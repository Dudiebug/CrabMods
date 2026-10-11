// CrabSync: shared team inventory for Crab Champions co-op, run by the host.
//
// The game is host-authoritative and replicates every PlayerState's inventory to its owner, so the
// host alone keeps everyone in sync; clients need no mod. Each poll the host folds every player's own
// changes into the team inventory (see ledger.hpp) and drives every player to it with the game's own
// functions. Island travel is handled by the game's autosave: each client re-uploads its inventory
// after the map loads, and the restore hook below treats that as a fresh baseline, not a pickup.
#include <windows.h>

#include <crab/hook.hpp>
#include <crab/log.hpp>
#include <crab/ue.hpp>

#include "config.hpp"
#include "game_inv.hpp"
#include "ledger.hpp"

using namespace crab;
using namespace crabsync;

namespace {

constexpr const char kVersion[] = "1.0.5";

Config g_cfg;
Ledger g_ledger;

struct Runtime {
    void* world = nullptr;
    float sinceLoad = 0.f;
    float sincePoll = 0.f;
    bool forceResync = false;
    bool dumpRequested = false;
    int mismatchLogs = 0;
    std::string status;
    bool lobbyFresh = false;  // the team was reset for the current lobby visit
    uint8_t lastIsland = 0;
    bool runStarting = false;  // left the lobby: rebuild the team once the game has set up the run
    std::unordered_map<PlayerKey, uintptr_t> bodies;  // each player's character at the last poll
    std::unordered_map<PlayerKey, Inventory> pending;  // players seen mid-change, as last read
    // In someone else's game: whether their CrabSync said hello, and for how long we have been waiting.
    bool hostAnnounced = false;
    bool hostWarned = false;
    float clientSeconds = 0.f;
} g;

// Max health added to every player's own max for the shared pool (healthPerPlayer per extra player).
// Applied inside ACrabPS::UpdateMaxHealth, the one place the game derives max health from items.
float g_extraHealth = 0.f;
int g_inUpdateMaxHealth = 0;

using TickFn = void (*)(void*, float, bool);
using PostLoginFn = void (*)(void*, void*);
using ClientMessageFn = void (*)(void*, TArray<wchar_t>*, uint64_t, float);
using RestoreFn = void (*)(void*, void*);
using UpdateMaxHealthFn = void (*)(void*);
using SetMaxHealthFn = void (*)(void*, float);
using AddBaseMaxHealthFn = void (*)(void*, float);
using AddBaseMaxHealthFromPerkFn = void (*)(void*, float, uint8_t);
using ChestPayFn = void (*)(void*, void*);
TickFn oTick = nullptr;
RestoreFn oRestore = nullptr;
UpdateMaxHealthFn oUpdateMaxHealth = nullptr;
SetMaxHealthFn oSetMaxHealth = nullptr;
AddBaseMaxHealthFn oAddBaseMaxHealth = nullptr;
AddBaseMaxHealthFromPerkFn oAddBaseMaxHealthFromPerk = nullptr;
ChestPayFn oChestPay = nullptr;
PostLoginFn oPostLogin = nullptr;
ClientMessageFn oClientMessage = nullptr;

uint64_t g_frame = 0;  // engine ticks, to spot one team event reaching every player in the same frame
uint64_t g_fortitudeFrame = ~0ull;
float g_chestBaseMax = 0.f;  // max health taken by the chest being paid (set by the AddBaseMaxHealth hook)

std::string PlayerName(void* ps) {
    std::string n = FStringToUtf8(At<TArray<wchar_t>>(ps, off::PS_PlayerName));
    return n.empty() ? "player " + std::to_string(At<int32_t>(ps, off::PS_PlayerId)) : n;
}

std::string ItemName(ItemId id) { return ObjectName(reinterpret_cast<void*>(id)); }

bool GameHasFocus() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

bool KeyPressed(int vk) {
    static bool down[256];
    bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool pressed = now && !down[vk];
    down[vk] = now;
    return pressed;
}

void DumpInventory(const char* who, const Inventory& inv) {
    static const char* life[] = {"no character", "alive", "dead"};
    CRAB_LOG("  %s: health=%.1f/%.1f (%s) base max=%.1f", who, inv.health, inv.maxHealth, life[int(inv.life)],
             inv.baseMax);
    CRAB_LOG("  %s: weapon=%s ability=%s melee=%s crystals=%u slots=%d/%d/%d/%d", who,
             ItemName(inv.equip[Weapon]).c_str(), ItemName(inv.equip[Ability]).c_str(),
             ItemName(inv.equip[Melee]).c_str(), inv.crystals, inv.slots[0], inv.slots[1], inv.slots[2], inv.slots[3]);
    for (int c = 0; c < NumCats; c++)
        for (auto& i : inv.items[c])
            CRAB_LOG("    %s %s x%d buff=%.3f enh=%zu", CatName(c), ItemName(i.id).c_str(), i.level, i.buff,
                     i.enhancements.size());
}

// Which parts of a player's inventory differ from what was written, for the retry log.
std::string Mismatch(const Inventory& cur, const Inventory& tgt) {
    std::string out;
    auto add = [&](const std::string& s) { out += (out.empty() ? "" : ", ") + s; };
    for (int e = 0; e < NumEquip; e++)
        if (cur.equip[e] != tgt.equip[e]) add(std::string(EquipName(e)) + " " + ItemName(tgt.equip[e]));
    if (cur.slots != tgt.slots) add("slots");
    if (cur.crystals != tgt.crystals) add("crystals " + std::to_string(cur.crystals) + " -> " + std::to_string(tgt.crystals));
    for (int c = 0; c < NumCats; c++)
        if (cur.items[c] != tgt.items[c]) add(std::string(CatName(c)) + "s");
    if (cur.baseMax != tgt.baseMax) {
        char buf[64];
        snprintf(buf, sizeof buf, "base max %.1f -> %.1f", cur.baseMax, tgt.baseMax);
        add(buf);
    }
    if (cur.life != tgt.life || cur.health != tgt.health) {
        char buf[96];
        snprintf(buf, sizeof buf, "health %.0f/%.0f -> %.0f%s", cur.health, cur.maxHealth, tgt.health,
                 tgt.life == Life::Dead ? " (dead)" : "");
        add(buf);
    }
    return out;
}

void Sync(void* gs) {
    static void* psClass = StaticClass(va::ACrabPS_StaticClass);
    auto& arr = At<TArray<void*>>(gs, off::GS_PlayerArray);
    std::vector<void*> players;
    std::vector<PlayerKey> keys;
    for (int i = 0; i < arr.Num; i++) {
        void* ps = arr.Data[i];
        if (!IsA(ps, psClass)) continue;
        players.push_back(ps);
        keys.push_back(reinterpret_cast<PlayerKey>(ps));
    }
    if (players.empty()) return;
    g_ledger.retain(keys);

    // Each player after the first adds healthPerPlayer to everyone's max, so the pool grows with the team.
    float extra = g_cfg.sync.health ? g_cfg.healthPerPlayer * float(players.size() - 1) : 0.f;
    bool extraChanged = extra != g_extraHealth;
    if (extraChanged)
        CRAB_LOG("%zu player(s): shared health pool max is everyone's own max + %.0f", players.size(), extra);
    g_extraHealth = extra;

    std::vector<Inventory> cur(players.size());
    std::unordered_map<PlayerKey, uintptr_t> bodies;
    for (size_t i = 0; i < players.size(); i++) {
        ReadInventory(players[i], cur[i]);
        // A new character takes its max from before; recompute it (with the extra) before it is used.
        // (A spectator has no health: maxHealth stays 0.)
        auto it = g.bodies.find(keys[i]);
        if (cur[i].maxHealth > 0.f && (extraChanged || it == g.bodies.end() || it->second != cur[i].body)) {
            Fn<void (*)(void*)>(va::ACrabPS_UpdateMaxHealth)(players[i]);
            ReadInventory(players[i], cur[i]);
        }
        bodies[keys[i]] = cur[i].body;
    }
    g.bodies = std::move(bodies);

    if (!g_ledger.hasTeam()) {
        std::vector<const Inventory*> all;
        for (auto& c : cur) all.push_back(&c);
        g_ledger.seed(all);
        CRAB_LOG("team inventory started from %zu player(s)", players.size());
    }

    // Input buffer: a player whose inventory is still changing (mid-pickup, several pickups in a row)
    // is left alone until it holds still for one poll, then everything they did is taken in at once.
    std::vector<bool> hold(players.size(), false);
    std::unordered_map<PlayerKey, Inventory> pending;
    for (size_t i = 0; i < players.size(); i++) {
        const Inventory* base = g_ledger.baseline(keys[i]);
        if (!base || SameItems(*base, cur[i])) continue;
        auto it = g.pending.find(keys[i]);
        if (it == g.pending.end() || !SameItems(it->second, cur[i])) {
            hold[i] = true;
            pending[keys[i]] = cur[i];
        }
    }
    g.pending = std::move(pending);

    std::vector<std::vector<Change>> changes;
    g_ledger.absorbAll(keys, cur, &changes, &hold);
    for (size_t i = 0; i < players.size(); i++) {
        if (changes[i].empty()) continue;
        std::string who = PlayerName(players[i]);
        for (auto& ch : changes[i])
            CRAB_LOG("%s: %s%s%s", who.c_str(), ch.what.c_str(), ch.id ? " " : "", ch.id ? ItemName(ch.id).c_str() : "");
    }

    // Output queue: ApplyTarget hands out at most one pickup per player per poll.
    for (size_t i = 0; i < players.size(); i++) {
        if (hold[i]) continue;
        bool fresh = !g_ledger.hasBaseline(keys[i]) || g.forceResync;
        Inventory tgt = g_ledger.target(cur[i]);
        if (!(cur[i] == tgt)) {
            bool done = ApplyTarget(players[i], cur[i], tgt, g_cfg.announce && !fresh);
            Inventory before = std::move(cur[i]);
            ReadInventory(players[i], cur[i]);
            for (int c = 0; c < NumCats; c++) {
                for (auto& it : cur[i].items[c]) {
                    const Item* b = FindItem(before.items[c], it.id);
                    if (!b || it.level > b->level)
                        CRAB_LOG("%s: received %s x%d %s", PlayerName(players[i]).c_str(), CatName(c),
                                 it.level - (b ? b->level : 0), ItemName(it.id).c_str());
                }
            }
            if (done && !(cur[i] == tgt) && g.mismatchLogs < 50) {
                g.mismatchLogs++;
                CRAB_LOG("%s: could not fully apply the team inventory yet (retrying): %s", PlayerName(players[i]).c_str(),
                         Mismatch(cur[i], tgt).c_str());
            }
        }
        // Whatever we wrote is now the baseline, so it is never mistaken for the player's own change.
        g_ledger.setBaseline(keys[i], cur[i]);
    }

    if (g.dumpRequested) {
        g.dumpRequested = false;
        CRAB_LOG("--- dump ---");
        DumpInventory("team", g_ledger.team());
        for (size_t i = 0; i < players.size(); i++) DumpInventory(PlayerName(players[i]).c_str(), cur[i]);
    }
    g.forceResync = false;
}

// --- Is the host syncing? -------------------------------------------------------------------------------
//
// Only the host's CrabSync syncs, so a player in someone else's game cannot tell from their own log whether
// anything is shared. The host's CrabSync (1.0.4 and newer) greets every player who joins with its version
// and whether health is shared, as a ClientMessage (the RPC behind console messages) starting with a marker;
// CrabSync on the player's machine logs it and keeps it off the console. A player who hears nothing is told
// on screen that the host's CrabSync is missing or too old.

constexpr wchar_t kHelloTag[] = L"CrabSync host\n";
constexpr size_t kHelloTagLen = std::size(kHelloTag) - 1;
constexpr float kHostWaitSeconds = 30.f;

void SendClientMessage(void* pc, std::wstring msg) {
    struct {
        TArray<wchar_t> s;
        uint64_t type;  // FName None
        float lifeTime;
        uint32_t pad;
    } params{{msg.data(), int32_t(msg.size() + 1), int32_t(msg.size() + 1)}, 0, 0.f, 0};
    uint64_t name = *reinterpret_cast<uint64_t*>(Addr(va::FName_ClientMessage));
    void* fn = Fn<void* (*)(void*, uint64_t)>(va::UObject_FindFunctionChecked)(pc, name);
    if (!fn) return;
    void** vtable = *reinterpret_cast<void***>(pc);
    reinterpret_cast<void (*)(void*, void*, void*)>(vtable[va::UObject_ProcessEvent_Slot / 8])(pc, fn, &params);
}

void HkPostLogin(void* gm, void* pc) {
    oPostLogin(gm, pc);
    if (!pc) return;
    std::wstring msg = kHelloTag;
    for (const char* c = kVersion; *c; c++) msg.push_back(wchar_t(*c));
    msg += g_cfg.sync.health ? L"\nsyncHealth=1" : L"\nsyncHealth=0";
    SendClientMessage(pc, msg);
}

void HkClientMessage(void* pc, TArray<wchar_t>* s, uint64_t type, float lifeTime) {
    if (!s || !s->Data || s->Num <= int(kHelloTagLen) || wcsncmp(s->Data, kHelloTag, kHelloTagLen) != 0)
        return oClientMessage(pc, s, type, lifeTime);
    std::string text;
    for (int i = int(kHelloTagLen); i < s->Num && s->Data[i]; i++) text.push_back(s->Data[i] == L'\n' ? ' ' : char(s->Data[i]));
    // The host greets its own controller too; only a client's controller is not the authority.
    if (At<uint8_t>(pc, off::Actor_Role) == ROLE_Authority || g.hostAnnounced) return;
    g.hostAnnounced = true;
    bool health = text.find("syncHealth=1") != std::string::npos;
    CRAB_LOG("the host runs CrabSync %s: %s", text.substr(0, text.find(' ')).c_str(),
             health ? "the host syncs everything, including shared health, max health and deaths"
                    : "the host has syncHealth = false, so health, max health and deaths are NOT shared");
}

// On this machine's own screen: of the player states a client sees, only its own has an owner (its controller).
void ShowOwnMessage(void* gs, const wchar_t* text) {
    static void* psClass = StaticClass(va::ACrabPS_StaticClass);
    std::wstring copy = text;
    TArray<wchar_t> s{copy.data(), int32_t(copy.size() + 1), int32_t(copy.size() + 1)};
    auto& arr = At<TArray<void*>>(gs, off::GS_PlayerArray);
    for (int i = 0; i < arr.Num; i++) {
        void* ps = arr.Data[i];
        if (IsA(ps, psClass) && At<void*>(ps, off::Actor_Owner))
            Fn<void (*)(void*, TArray<wchar_t>*)>(va::ACrabPS_ShowMessage)(ps, &s);
    }
}

void WatchHost(void* gs, float dt) {
    if (g.hostAnnounced || g.hostWarned) return;
    g.clientSeconds += dt;
    if (g.clientSeconds < kHostWaitSeconds) return;
    g.hostWarned = true;
    CRAB_LOG("the host never announced CrabSync 1.0.4 or newer: they don't run CrabSync, or an older one. Older "
             "versions don't share max health (before 1.0.2) or deaths; nothing is synced without it");
    ShowOwnMessage(gs, L"CrabSync: the host isn't running CrabSync 1.0.4 or newer, so max health and deaths "
                       L"may not be shared");
}

// Logs a line whenever what CrabSync is doing changes, so the log always says why it is (not) syncing.
void SetStatus(const std::string& s) {
    if (s == g.status) return;
    g.status = s;
    CRAB_LOG("%s", s.c_str());
}

// F10 outside of a sync (lobby, client, waiting): dump what this PC can see without changing anything.
void DumpVisible(void* gs, const char* why) {
    static void* psClass = StaticClass(va::ACrabPS_StaticClass);
    CRAB_LOG("--- dump (%s; read only) ---", why);
    if (g_ledger.hasTeam()) DumpInventory("team", g_ledger.team());
    auto& arr = At<TArray<void*>>(gs, off::GS_PlayerArray);
    for (int i = 0; i < arr.Num; i++) {
        void* ps = arr.Data[i];
        if (!IsA(ps, psClass)) continue;
        Inventory inv;
        ReadInventory(ps, inv);
        DumpInventory(PlayerName(ps).c_str(), inv);
    }
}

void OnTick(void* engine, float dt) {
    if (GameHasFocus()) {
        if (KeyPressed(VK_F9)) {
            g.forceResync = true;
            CRAB_LOG("F9: forcing a full resync");
        }
        if (KeyPressed(VK_F10)) g.dumpRequested = true;
    }

    void* vp = At<void*>(engine, off::Engine_GameViewport);
    if (!vp) return;
    void* world = At<void*>(vp, off::Viewport_World);
    if (!world) return;
    if (world != g.world) {
        g.world = world;
        g.sinceLoad = 0.f;
        g_ledger.clearBaselines();
        g.lobbyFresh = false;
        CRAB_LOG("map loaded (%s)", ObjectName(world).c_str());
    }

    static void* gsClass = StaticClass(va::ACrabGS_StaticClass);
    void* gs = At<void*>(world, off::World_GameState);
    if (!IsA(gs, gsClass)) {
        SetStatus("waiting for the game state");
        return;
    }
    if (At<uint8_t>(gs, off::Actor_Role) != ROLE_Authority) {
        g_extraHealth = 0.f;
        SetStatus("this PC joined someone else's game: the host does all syncing, nothing to do here");
        WatchHost(gs, dt);
        if (g.dumpRequested) {
            g.dumpRequested = false;
            DumpVisible(gs, "client");
        }
        return;
    }

    g.hostAnnounced = g.hostWarned = false;  // hosting: the next game joined is checked afresh
    g.clientSeconds = 0.f;

    uint8_t island = At<uint8_t>(gs, off::GS_CurrentIslandType);
    int islandNum = At<int32_t>(gs, off::GS_CurrentIsland);
    if (island != g.lastIsland) {
        // Starting a run resets slots, crystals and health for everyone; wait for that to finish, then
        // rebuild the team from it instead of counting the reset as everyone losing things.
        if (g.lastIsland == uint8_t(IslandType::Lobby)) g.runStarting = true;
        if (island == uint8_t(IslandType::Lobby)) {
            // Back from a run: let the lobby characters spawn before the new team is read from them.
            g.runStarting = false;
            g.sinceLoad = 0.f;
        }
        if (g.runStarting && island != uint8_t(IslandType::None)) g.sinceLoad = 0.f;
        g.lastIsland = island;
    }
    // The lobby shows the pool's size (everyone's max includes healthPerPlayer) at full health, but no
    // damage is shared there: a pool carried over from the run's last fight must not reach the fresh
    // lobby characters.
    g_ledger.setLobby(island == uint8_t(IslandType::Lobby));

    const char* pause = nullptr;
    if (g.sinceLoad < g_cfg.settleSeconds) {
        g.sinceLoad += dt;
        pause = "map loading";
    } else if (island == uint8_t(IslandType::Lobby)) {
        // Arriving in the lobby (after a run, or at startup) starts a fresh team, once per visit.
        if (!g.lobbyFresh) {
            g.lobbyFresh = true;
            if (g_ledger.hasTeam()) CRAB_LOG("back in the lobby: team inventory cleared");
            g_ledger.reset();
        }
        SetStatus("host, syncing in the lobby");
    } else if (island == uint8_t(IslandType::None)) {
        SetStatus("host, waiting for the island to start");
        pause = "no island yet";
    } else {
        g.lobbyFresh = false;
        if (g.runStarting) {
            g.runStarting = false;
            g_ledger.reset();
            CRAB_LOG("run started: team inventory rebuilt from everyone's starting state");
        }
        SetStatus("host, syncing on island " + std::to_string(islandNum) + " (" + IslandTypeName(island) + ")");
    }
    if (pause) {
        if (g.dumpRequested) {
            g.dumpRequested = false;
            DumpVisible(gs, pause);
        }
        return;
    }

    g.sincePoll += dt;
    if (g.sincePoll < g_cfg.pollSeconds && !g.forceResync && !g.dumpRequested) return;
    g.sincePoll = 0.f;
    Sync(gs);
}

void HkTick(void* engine, float dt, bool idle) {
    g_frame++;
    oTick(engine, dt, idle);
    OnTick(engine, dt);
}

void HkRestoreAutoSave(void* ps, void* save) {
    oRestore(ps, save);
    // The player's state was replaced wholesale (island travel re-upload). Treat it as a new baseline
    // rather than as items gained; the next poll overwrites it with the team inventory.
    if (At<uint8_t>(ps, off::Actor_Role) == ROLE_Authority) {
        g_ledger.forget(reinterpret_cast<PlayerKey>(ps));
        CRAB_LOG("%s: autosave restored", PlayerName(ps).c_str());
    }
}

void HkUpdateMaxHealth(void* ps) {
    g_inUpdateMaxHealth++;
    oUpdateMaxHealth(ps);
    g_inUpdateMaxHealth--;
}

// Only the item-derived max from UpdateMaxHealth gets the extra; spawning restores a saved max as is.
void HkSetMaxHealth(void* character, float max) {
    if (g_inUpdateMaxHealth && g_extraHealth > 0.f) max += g_extraHealth;
    oSetMaxHealth(character, max);
}

// A player's base max health changed by `d` (on the host). Everything but a chest's cost is the team's.
void OnBaseMaxHealth(void* ps, float d, uintptr_t ret) {
    if (d == 0.f) return;
    if (!g_ledger.hasTeam() || !g_ledger.maxShared()) {
        // Not an error in the lobby (nothing is shared there) or while a run is being set up; logged so a
        // max health change that stays with one player can be traced.
        CRAB_LOG("%s: max health %+.1f, not shared (%s)", PlayerName(ps).c_str(), d,
                 !g_ledger.hasTeam() ? "no team yet" : "in the lobby");
        return;
    }
    if (ret == Addr(va::Ret_ChestMaxHealthCost)) {
        g_chestBaseMax += d;  // HkChestPay decides what the team pays
        return;
    }
    // Fortitude: the game gives it to every player in one loop; the team gets it once.
    if (ret == Addr(va::Ret_FortitudeIsland)) {
        if (g_fortitudeFrame == g_frame) return;
        g_fortitudeFrame = g_frame;
    }
    g_ledger.addBaseMax(d);
    CRAB_LOG("%s: max health %+.1f, shared (team base max %.1f)", PlayerName(ps).c_str(), d, g_ledger.team().baseMax);
}

void HkAddBaseMaxHealth(void* ps, float amount) {
    float before = At<float>(ps, off::PS_BaseMaxHealth);
    oAddBaseMaxHealth(ps, amount);
    OnBaseMaxHealth(ps, At<float>(ps, off::PS_BaseMaxHealth) - before, reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
}

void HkAddBaseMaxHealthFromPerk(void* ps, float amount, uint8_t perk) {
    float before = At<float>(ps, off::PS_BaseMaxHealth);
    oAddBaseMaxHealthFromPerk(ps, amount, perk);
    OnBaseMaxHealth(ps, At<float>(ps, off::PS_BaseMaxHealth) - before, reinterpret_cast<uintptr_t>(__builtin_return_address(0)));
}

// A chest paid for with health or max health (spiked and greed chests): the team pays the share set by
// chestHealthCost instead of the opener's whole cost.
void HkChestPay(void* chest, void* opener) {
    static void* cClass = StaticClass(va::ACrabC_StaticClass);
    uint8_t currency = At<uint8_t>(chest, off::Chest_CurrencyType);
    void* ps = IsA(opener, cClass) ? At<void*>(opener, off::Pawn_PlayerState) : nullptr;
    void* hc = IsA(opener, cClass) ? At<void*>(opener, off::C_HC) : nullptr;
    if (!ps || !hc || At<uint8_t>(chest, off::Chest_bIsChestOpened) || At<int32_t>(chest, off::Chest_Cost) <= 0 ||
        (currency != 2 && currency != 3) || At<uint8_t>(chest, off::Actor_Role) != ROLE_Authority) {
        oChestPay(chest, opener);
        return;
    }
    float health = At<float>(hc, off::HC_CurrentHealth);
    g_chestBaseMax = 0.f;
    oChestPay(chest, opener);
    float taken = std::max(0.f, health - At<float>(hc, off::HC_CurrentHealth));
    float baseMax = g_chestBaseMax;
    g_chestBaseMax = 0.f;
    if (!g_ledger.hasTeam() || (taken == 0.f && baseMax == 0.f)) return;
    float fraction = currency == 3 ? float(At<int32_t>(chest, off::Chest_Cost)) * 0.01f : 0.f;
    g_ledger.chestCost(reinterpret_cast<PlayerKey>(ps), baseMax, currency == 2 ? taken : 0.f, fraction);
    if (currency == 2)
        CRAB_LOG("%s: opened a chest for %.0f health; the team pays %.0f", PlayerName(ps).c_str(), taken,
                 g_ledger.chestShare(taken));
    else
        CRAB_LOG("%s: opened a chest for %.1f max health; the team pays %.1f (team base max %.1f)",
                 PlayerName(ps).c_str(), -baseMax, -g_ledger.chestMaxShare(baseMax, fraction), g_ledger.team().baseMax);

}

DWORD WINAPI Init(LPVOID) {
    std::wstring dir = ModuleDir();
    Log::Get().Open(dir + L"CrabSync.log");
    bool created = false;
    g_cfg = LoadConfig(dir + L"CrabSync.ini", &created);
    if (created) CRAB_LOG("wrote default CrabSync.ini");
    if (!InitImage()) {
        CRAB_LOG("unsupported game build (PE timestamp mismatch); CrabSync is disabled");
        return 0;
    }
    if (!g_cfg.enabled) {
        CRAB_LOG("disabled in CrabSync.ini");
        return 0;
    }
    g_ledger.setSettings(g_cfg.sync);
    if (!HookInit()) return 0;
    Hook(va::UGameEngine_Tick, &HkTick, &oTick, "UGameEngine::Tick");
    Hook(va::ACrabPS_RestoreAutoSave, &HkRestoreAutoSave, &oRestore, "ACrabPS::RestoreAutoSave");
    Hook(va::AGameModeBase_K2_PostLogin, &HkPostLogin, &oPostLogin, "AGameModeBase::K2_PostLogin");
    Hook(va::APlayerController_ClientMessage_Impl, &HkClientMessage, &oClientMessage, "ClientMessage");
    if (g_cfg.sync.health) {
        Hook(va::ACrabPS_UpdateMaxHealth, &HkUpdateMaxHealth, &oUpdateMaxHealth, "ACrabPS::UpdateMaxHealth");
        Hook(va::ACrabC_SetMaxHealth, &HkSetMaxHealth, &oSetMaxHealth, "ACrabC::SetMaxHealth");
        Hook(va::ACrabPS_AddBaseMaxHealth, &HkAddBaseMaxHealth, &oAddBaseMaxHealth, "ACrabPS::AddBaseMaxHealth");
        Hook(va::ACrabPS_AddBaseMaxHealthFromPerk, &HkAddBaseMaxHealthFromPerk, &oAddBaseMaxHealthFromPerk,
             "ACrabPS::AddBaseMaxHealthFromPerk");
        Hook(va::ACrabChest_Pay, &HkChestPay, &oChestPay, "ACrabChest::Pay");

    }
    CRAB_LOG("CrabSync %s loaded", kVersion);
    return 0;
}

}  // namespace

// For CrabMods Manager, which loads the DLL to read it (outside the game, so DllMain does nothing).
extern "C" __declspec(dllexport) const char* CrabSyncDefaultIni() { return kDefaultIni; }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH && GetModuleHandleW(L"CrabChampions-Win64-Shipping.exe")) {
        DisableThreadLibraryCalls(inst);
        // Hooks go in off the loader lock.
        CloseHandle(CreateThread(nullptr, 0, Init, nullptr, 0, nullptr));
    }
    return TRUE;
}
