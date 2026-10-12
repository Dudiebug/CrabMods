// Necromancer: the Soul Whip, a weapon picked in the lobby. Its cracks tag enemies for your undead, and while you
// hold it your ability is replaced by raising the dead: kills fill a soul bar, and the raise keys spend souls to
// bring back kinds of enemies you have killed this run, as undead that fight for you.
//
// Every machine: the whip's data asset (a copy of the Sniper's, with its own projectile) joins the lobby's weapon
// list before the game builds it. The player's machine reads the raise keys, sends raise and tag requests to the
// host (SendToHost), and draws the soul bar and the raise panel in place of the ability.
// Host: souls (per player, kept for the run), the undead (fresh enemies spawned outside the island's count),
// who may hurt whom (DamageInfo_CanDamage: undead and hostile enemies fight, players and undead never), who
// targets whom (ACrabEnemyAIC::SelectTarget), upkeep, and the whip's tags. It tells every player which enemies it
// raised, so their machines show them as undead too, and which are tagged. A raise is announced a moment before the
// undead come up ("rise"), so every machine can draw its circle on the ground (necro_runes.hpp) and leave out the
// enemy's own spawn effect, a flat one that cuts into slopes.
// Every machine also dresses the whip (necro_art.hpp: its meshes and sounds come inside CrabMods.dll): the handle
// is the weapon's mesh, and the lash is a chain of mesh pieces moved by a small simulation (necro_lash.hpp) that
// each shot (the weapon's replicated shot count) flings forward.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"
#include "necro_art.hpp"
#include "necro_lash.hpp"
#include "necro_loot.hpp"
#include "necro_runes.hpp"

namespace crabmods {
namespace {

namespace loot = necro_loot;

constexpr wchar_t kSpawnablesPath[] = L"/Game/Blueprint/DataAsset/DA_Spawnables.DA_Spawnables";
constexpr wchar_t kEnemyClassPath[] = L"/Script/CrabChampions.CrabEnemyC";
constexpr wchar_t kMsg[] = L"necro ";  // our messages, both ways
constexpr size_t kMsgLen = std::size(kMsg) - 1;
constexpr uint8_t kFight = 3;           // GS_MatchState during an island's fight
constexpr int kTiers = 4;               // fodder, medium, hard, elite: EnemyCategory 1..4
const wchar_t* const kTierNames[kTiers] = {L"Fodder", L"Medium", L"Hard", L"Elite"};
const char* const kTierKeys[kTiers] = {"raiseKeyFodder", "raiseKeyMedium", "raiseKeyHard", "raiseKeyElite"};
const char* const kTierKeyDefaults[kTiers] = {"V", "C", "X", "Z"};

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// A key name from the ini ("V", "5", "F3", "Mouse4", "Shift", ...) as a Windows virtual key, 0 if unknown.
int VirtualKey(std::string name) {
    for (char& ch : name) ch = char(toupper(uint8_t(ch)));
    if (name.size() == 1 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9'))) return name[0];
    if (name.size() >= 2 && name[0] == 'F') {
        int n = atoi(name.c_str() + 1);
        if (n >= 1 && n <= 24) return VK_F1 + n - 1;
    }
    static const std::pair<const char*, int> named[] = {
        {"MOUSE3", VK_MBUTTON}, {"MOUSE4", VK_XBUTTON1}, {"MOUSE5", VK_XBUTTON2}, {"TAB", VK_TAB},
        {"SHIFT", VK_SHIFT},     {"CTRL", VK_CONTROL},    {"CONTROL", VK_CONTROL}, {"ALT", VK_MENU},
        {"SPACE", VK_SPACE},     {"CAPSLOCK", VK_CAPITAL}, {"ENTER", VK_RETURN},  {"BACKSPACE", VK_BACK},
        {"INSERT", VK_INSERT},   {"DELETE", VK_DELETE},   {"HOME", VK_HOME},       {"END", VK_END},
        {"PAGEUP", VK_PRIOR},    {"PAGEDOWN", VK_NEXT},   {"UP", VK_UP},           {"DOWN", VK_DOWN},
        {"LEFT", VK_LEFT},       {"RIGHT", VK_RIGHT},     {"NUM0", VK_NUMPAD0},    {"NUM1", VK_NUMPAD1},
        {"NUM2", VK_NUMPAD2},    {"NUM3", VK_NUMPAD3},    {"NUM4", VK_NUMPAD4},    {"NUM5", VK_NUMPAD5},
        {"NUM6", VK_NUMPAD6},    {"NUM7", VK_NUMPAD7},    {"NUM8", VK_NUMPAD8},    {"NUM9", VK_NUMPAD9},
    };
    for (auto& [n, vk] : named)
        if (name == n) return vk;
    return 0;
}

std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring FStringOf(void* obj, int offset) {
    auto& s = At<TArray<wchar_t>>(obj, offset);
    std::wstring out;
    for (int i = 0; s.Data && i < s.Num && s.Data[i]; i++) out.push_back(s.Data[i]);
    return out;
}

// Every actor of a class in the world (UGameplayStatics::GetAllActorsOfClass).
std::vector<void*> AllActorsOf(void* world, void* cls) {
    static void* statics = ui::FindObject(va::GameplayStaticsPath);
    struct {
        void* context;
        void* cls;
        TArray<void*> out;
    } p{world, cls, {}};
    std::vector<void*> actors;
    if (!statics || !cls || !ui::Call(statics, L"GetAllActorsOfClass", &p)) return actors;
    for (int i = 0; p.out.Data && i < p.out.Num; i++)
        if (p.out.Data[i]) actors.push_back(p.out.Data[i]);
    if (p.out.Data) Fn<void (*)(void*)>(va::FMemory_Free)(p.out.Data);
    return actors;
}

bool Alive(void* c) {
    if (!c || (At<uint8_t>(c, off::C_bIsEliminated) & 1)) return false;
    void* hc = At<void*>(c, off::C_HC);
    return hc && At<float>(hc, off::HC_CurrentHealth) > 0.f;
}

class Necromancer : public Item {
public:
    Necromancer() : Item("necromancer", 200) {
        self_ = this;
        for (int t = 0; t < kTiers; t++) keyNames_[t] = kTierKeyDefaults[t], keys_[t] = VirtualKey(keyNames_[t]);
    }

    std::string DefaultIni() const override {
        return R"(# Necromancer: the Soul Whip, a weapon you pick in the lobby. Its cracks tag enemies for your undead. While
# you hold it, your ability is replaced by raising the dead: kills fill a soul bar, and each raise key spends
# souls to bring back a kind of enemy you have killed this run, as an undead that fights for you.
enabled = true

# --- Soul Whip ---
# Each crack sends a soul bolt toward your aim. It stops at the first enemy it hits, or the ground, or its
# reach, and bursts there, hurting every enemy around.
# Damage a second. The bolt does its damage with its burst (whipDps / whipFireRate each, so 80 at 1.25 cracks a
# second), to every enemy in it; those at its edge take less.
whipDps = 100
# Cracks a second.
whipFireRate = 1.25
# Reach in meters.
whipRangeMeters = 45
# The burst's radius in meters.
burstRadiusMeters = 4
# Chance-based weapon mods (Double Shot, Split Shot, ...) roll this many times as often with the whip. The Sniper,
# whose data the whip starts from, has 7; the SMG 1 and the AutoRifle 2.5. Every extra bolt is a whole burst.
procRollMultiplier = 3
# A crack tags every enemy within this many meters of where it lands, for this many seconds. Your undead go
# after tagged enemies first and hurt them more, and tagged kills give more souls.
tagRadiusMeters = 6
tagSeconds = 8
tagDamagePercent = 25
taggedSoulsPercent = 50

# --- Souls ---
maxSouls = 100
# Souls for a kill, by the enemy's rank in the game.
soulsEasy = 1
soulsMedium = 3
soulsHard = 6
soulsElite = 25
soulsBoss = 60

# --- Raising ---
# Soul cost of each raise. Fodder raises a pack.
costFodder = 10
fodderPack = 3
costMedium = 15
costHard = 40
costElite = 75
# Undead still standing when the fight ends crumble and give back this percent of their raise cost.
survivorRefundPercent = 50
# The most undead each player can have at once (Legion adds to it).
maxUndead = 20
# How far from you you can raise, in meters.
raiseRangeMeters = 40
# While enemies are alive, each undead costs this percent of its raise cost a second (Hollow Hunger cuts it).
# When the souls run out, your undead starve instead: each loses this percent of its health a second until you have souls again.
upkeepPercent = 1
starvePercent = 4
# Undead health and damage, as a multiple of the enemy's own.
undeadHealth = 2
undeadDamage = 1
# This percent of your own damage bonus counts for your undead too (at 50, your +40% gives them +20%).
ownerDamagePercent = 50
# Raised elites' health and damage as a multiple (on top of the above).
eliteHealth = 1
eliteDamage = 0.5
# Enemies go after an undead instead of a player when it is this many times closer.
aggroBias = 1.5
# Undead with no enemy near go for nests and spawners within this many meters, each doing this much damage a second.
nestRangeMeters = 25
nestDps = 30
# Kills by your undead give this percent of the souls yours would. They drop crystals as your own kills do,
# but don't set off your on-kill perks and relics (Scavenger, Exploding Enemies, Bounty Hunter, ...).
undeadKillSoulsPercent = 50

# --- Yours ---
# Each player's own: the host's settings never change these. Raise keys: A letter or digit, F1-F24, Mouse3-Mouse5, Num0-Num9,
# or Tab, Shift, Ctrl, Alt, Space, CapsLock, Enter, Insert, Delete, Home, End, PageUp, PageDown, Up, Down,
# Left, Right.
raiseKeyFodder = V
raiseKeyMedium = C
raiseKeyHard = X
raiseKeyElite = Z
# The kit's sounds (the whip, souls, raising), in percent, on top of the game's Master and Gameplay volume.
# Also your own.
soundVolume = 80
)";
    }

    void Set(const std::string& k, const std::string& v) override {
        auto num = [&](float lo, float hi) { return std::clamp(ParseFloat(v), lo, hi); };
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "whipDps") whipDps_ = num(0.f, 100000.f);
        else if (k == "whipFireRate") whipFireRate_ = num(0.1f, 20.f);
        else if (k == "whipRangeMeters") whipRange_ = num(1.f, 200.f) * 100.f;
        else if (k == "burstRadiusMeters") burstRadius_ = num(0.f, 50.f) * 100.f;
        else if (k == "procRollMultiplier") procRoll_ = num(0.f, 100.f);
        else if (k == "tagRadiusMeters") tagRadius_ = num(0.f, 100.f) * 100.f;
        else if (k == "tagSeconds") tagSeconds_ = num(0.f, 600.f);
        else if (k == "tagDamagePercent") tagDamage_ = num(0.f, 10000.f) / 100.f;
        else if (k == "taggedSoulsPercent") taggedSouls_ = num(0.f, 10000.f) / 100.f;
        else if (k == "maxSouls") maxSouls_ = num(1.f, 1e6f);
        else if (k == "soulsEasy") souls_[1] = num(0.f, 1e6f);
        else if (k == "soulsMedium") souls_[2] = num(0.f, 1e6f);
        else if (k == "soulsHard") souls_[3] = num(0.f, 1e6f);
        else if (k == "soulsElite") souls_[4] = num(0.f, 1e6f);
        else if (k == "soulsBoss") souls_[5] = num(0.f, 1e6f);
        else if (k == "costFodder") cost_[0] = num(0.f, 1e6f);
        else if (k == "fodderPack") fodderPack_ = int(num(1.f, 10.f));
        else if (k == "survivorRefundPercent") refund_ = num(0.f, 100.f) / 100.f;
        else if (k == "maxUndead") maxUndead_ = int(num(1.f, 200.f));
        else if (k == "costMedium") cost_[1] = num(0.f, 1e6f);
        else if (k == "costHard") cost_[2] = num(0.f, 1e6f);
        else if (k == "costElite") cost_[3] = num(0.f, 1e6f);
        else if (k == "raiseRangeMeters") raiseRange_ = num(1.f, 1000.f) * 100.f;
        else if (k == "upkeepPercent") upkeep_ = num(0.f, 100.f) / 100.f;
        else if (k == "undeadDamage") undeadDamage_ = num(0.f, 1000.f);
        else if (k == "undeadHealth") undeadHealth_ = num(0.01f, 100.f);
        else if (k == "ownerDamagePercent") ownerDamage_ = num(0.f, 1000.f) / 100.f;
        else if (k == "starvePercent") starve_ = num(0.f, 100.f) / 100.f;
        else if (k == "eliteHealth") eliteHealth_ = num(0.01f, 10.f);
        else if (k == "eliteDamage") eliteDamage_ = num(0.f, 10.f);
        else if (k == "aggroBias") aggroBias_ = num(0.f, 100.f);
        else if (k == "nestRangeMeters") nestRange_ = num(0.f, 500.f) * 100.f;
        else if (k == "nestDps") nestDps_ = num(0.f, 1e6f);
        else if (k == "undeadKillSoulsPercent") undeadKillSouls_ = num(0.f, 1000.f) / 100.f;
        else if (k == "soundVolume") mixer_.SetVolume(num(0.f, 200.f) / 100.f);
        else
            for (int t = 0; t < kTiers; t++)
                if (k == kTierKeys[t]) {
                    keyNames_[t] = v;
                    keys_[t] = VirtualKey(v);
                    if (!keys_[t]) CRAB_LOG("necromancer: unknown key name '%s' for %s", v.c_str(), k.c_str());
                }
    }
    bool LocalSetting(const std::string& k) const override {
        if (k == "soundVolume") return true;
        for (const char* key : kTierKeys)
            if (k == key) return true;
        return false;
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        std::string keys;
        for (int t = 0; t < kTiers; t++) keys += (t ? "/" : "") + keyNames_[t];
        return "Soul Whip " + Num(whipDps_) + " dps, " + Num(whipRange_ / 100.f) + " m, " + Num(burstRadius_ / 100.f) + " m burst of " + Num(whipDps_ / whipFireRate_) + "; " + Num(maxSouls_) +
               " souls; raise " + Num(cost_[0]) + "/" + Num(cost_[1]) + "/" + Num(cost_[2]) + "/" + Num(cost_[3]) +
               " on " + keys + ", up to " + std::to_string(maxUndead_) + " undead";
    }
    std::string Title() const override { return "Soul Whip (Necromancer)"; }
    static bool HoldsWhip(void* ps) { return self_ && self_->enabled_ && self_->Holds(ps); }
    const char* CatalogKind() const override { return "Weapon"; }

    void InstallHooks() override {
        Hook(va::UCrabGI_BuildLoadoutLists, &HkBuildLists, &oBuildLists, "UCrabGI lobby lists");
        Hook(va::DamageInfo_CanDamage, &HkCanDamage, &oCanDamage, "DamageInfo::CanDamage");
        Hook(va::ACrabEnemyAIC_SelectTarget, &HkSelectTarget, &oSelectTarget, "ACrabEnemyAIC::SelectTarget");
        Hook(va::ACrabPlayerC_AbilityPressed, &HkAbilityPressed, &oAbilityPressed, "ACrabPlayerC ability pressed");
    }

    // --- Level start ---------------------------------------------------------------------------------------

    void OnBeginPlay(void* lm) override {
        EnsureWhip();
        RefreshWhip();
        // The old level's actors are gone.
        undead_.clear();
        marked_.clear();
        pending_.clear();
        tags_.clear();
        seenTags_.clear();
        for (const Rising& r : rising_) Refund(r);
        rising_.clear();
        world_ = WorldOf(lm);
        lashes_.clear();  // the old level's weapons and pickups (and our pieces on them) are gone
        shows_.clear();
        crumbled_.clear();
        lastMelee_.clear();
        harvest_.clear();
        phylacteryUsed_.clear();
        hud_ = nullptr;
        // The pedestal, once the lobby's pickups are there (tried each tick for a few seconds).
    }

    // --- Kills (host) --------------------------------------------------------------------------------------

    void OnEnemyEliminating(void* enemy) override {
        if (At<uint8_t>(enemy, off::Actor_Role) != ROLE_Authority) return;
        void*& killer = At<void*>(enemy, off::Enemy_EliminatingC);
        if (auto u = undead_.find(enemy); u != undead_.end()) {
            killer = nullptr;  // an undead's death drops nothing and credits nobody
            Vec3 at;
            if (LocationOf(enemy, at)) PlayAt("A_Undead_Crumble_", at);
            // Last Rites: part of its cost back, when it falls in a fight (not when the fight ends).
            if (void* owner = u->second.owner; owner && !fightOver_)
                if (float back = loot::Total(owner, loot::LastRites) / 100.f; back > 0.f)
                    AddSouls(owner, u->second.cost * std::min(back, 1.f));
            return;
        }
        void* ps = nullptr;
        bool byUndead = false;
        if (auto u = undead_.find(killer); killer && u != undead_.end()) {
            ps = u->second.owner;
            byUndead = true;
            // Its owner's crystals drop: the game only drops them for a player's kill. (The kill count isn't
            // touched: the game mode counts it from the hit, which was the undead's.)
            killer = PawnOf(ps);
        } else if (IsA(killer, StaticClass(va::ACrabPlayerC_StaticClass))) {
            ps = At<void*>(killer, off::Pawn_PlayerState);
        }
        Credit(enemy, ps, byUndead);
        // The owner's own on-kill perks and relics stay quiet: they're read in the same code as the crystals,
        // so the owner's perks and relics are hidden from it (after the souls above, which read them) until
        // OnEnemyEliminated.
        if (byUndead && killer) HideKit(enemy, ps);
    }
    // Souls, Soul Brand, Harvester and the roster, for a kill by the player or one of their undead.
    void Credit(void* enemy, void* ps, bool byUndead) {
        if (!Holds(ps) || (At<uint8_t>(enemy, off::Enemy_bIsLesserEnemy) & 1)) return;
        uint8_t category = At<uint8_t>(enemy, off::Enemy_Category);
        float gain = category < 6 ? souls_[category] : 0.f;
        if (byUndead) gain *= undeadKillSouls_;
        bool tagged = Tagged(enemy);
        if (tagged) gain *= 1.f + taggedSouls_ + loot::Total(ps, loot::GrimHarvest) / 100.f;
        gain *= 1.f + loot::Total(ps, loot::HungryGhosts) / 100.f;
        auto melee = lastMelee_.find(enemy);
        bool byClaw = !byUndead && melee != lastMelee_.end() && melee->second == ps;
        if (byClaw) gain *= 1.f + loot::Total(ps, loot::GraveClaws) / 100.f;
        AddSouls(ps, gain);
        PlayerData& d = players_[PlayerKey(ps)];
        // Soul Brand: the mark spreads to the enemies nearest the tagged kill.
        if (tagged)
            if (int n = int(loot::Total(ps, loot::SoulBrand) + 0.5f); n > 0) SpreadTag(enemy, n);
        // Harvester: a claw kill on fodder or medium can rise for free (raised on the next tick, not mid-death).
        if (byClaw && (category == 1 || category == 2) && CanRaise(enemy))
            if (int lv = loot::Level(ps, loot::Harvester); lv && Roll(loot::Amount(loot::Harvester) / 100.f, lv)) {
                Vec3 at;
                if (LocationOf(enemy, at)) harvest_.push_back({ps, category - 1, At<void*>(enemy, off::Object_Class), at});
            }
        if (category >= 1 && category <= kTiers && CanRaise(enemy)) {
            int t = category - 1;
            d.roster[t] = At<void*>(enemy, off::Object_Class);
            d.names[t] = FStringOf(enemy, off::Enemy_EnemyName);
        }
        d.dirty = true;
    }
    void OnEnemyEliminated(void* enemy) override {
        ShowKit(enemy);
        undead_.erase(enemy);
        tags_.erase(enemy);
        lastMelee_.erase(enemy);
    }

    // The game's elimination (ACrabEnemyC_OnEliminated) drops the crystals for a player's kill, then sets off
    // that player's on-kill perks (Scavenger, Bounty Hunter, Rare Treasure, Exploding/Freezing/... Enemies) and
    // relics (Lingering Fumes), each read from their perk and relic lists. For an undead's kill the lists look
    // empty for that one call (their counts, put back right after; nothing replicates in between). The crystal
    // drop itself reads neither.
    struct Hidden {
        void* enemy;
        void* ps;
        int perks, relics;
    };
    void HideKit(void* enemy, void* ps) {
        if (!ps) return;
        auto& perks = At<TArray<uint8_t>>(ps, off::PS_Perks);
        auto& relics = At<TArray<uint8_t>>(ps, off::PS_Relics);
        hidden_.push_back({enemy, ps, perks.Num, relics.Num});
        perks.Num = relics.Num = 0;
    }
    void ShowKit(void* enemy) {
        for (size_t i = hidden_.size(); i-- > 0;)  // the latest first: one elimination can run inside another
            if (hidden_[i].enemy == enemy) {
                Show(hidden_[i]);
                hidden_.erase(hidden_.begin() + i);
                return;
            }
    }
    static void Show(const Hidden& h) {
        At<TArray<uint8_t>>(h.ps, off::PS_Perks).Num = h.perks;
        At<TArray<uint8_t>>(h.ps, off::PS_Relics).Num = h.relics;
    }

    // --- Damage (host) -------------------------------------------------------------------------------------

    void OnTakeDamage(void* hc, void* info) override {
        void* victim = At<void*>(hc, off::HC_OwningC);
        void* src = At<void*>(info, off::DamageInfo_DamagingC);
        float& damage = At<float>(info, off::DamageInfo_Damage);
        if (void* ps = PlayerOfHealth(hc)) return Phylactery(ps, hc, damage);
        if (IsUndead(victim) && src && At<void*>(src, off::Pawn_PlayerState)) {
            damage = 0.f;  // players' hits that got here anyway (sent by their machine)
            return;
        }
        auto u = src ? undead_.find(src) : undead_.end();
        if (u == undead_.end()) return;
        float m = undeadDamage_;
        if (u->second.tier == 3) m *= eliteDamage_;
        // Part of the owner's damage bonus (ownerDamagePercent).
        if (void* ps = u->second.owner; ps && PawnOf(ps))
            m *= std::max(1.f + ownerDamage_ * (Fn<float (*)(void*)>(va::ACrabPS_GetDamageBuff)(ps) - 1.f), 0.f);
        if (Tagged(victim)) m *= 1.f + tagDamage_;
        void* owner = u->second.owner;
        m *= 1.f + loot::Total(owner, loot::RingOfTheGrave) / 100.f;
        damage *= m;
        // Plague Bearers: their hits poison.
        if (int stacks = int(loot::Total(owner, loot::PlagueBearers) + 0.5f); stacks > 0) AddDebuff(info, kPoison, stacks);
    }

    // Army of the Dead: the host weighs the player's undead by tier (Army); their own machine has the weight the
    // host sent.
    float DamageBonus(void* ps) override {
        float per = loot::Total(ps, loot::ArmyOfTheDead) / 100.f;
        if (per <= 0.f) return 0.f;
        float n = Army(ps);
        if (n <= 0.f && view_.known && LocalPlayer() == ps) n = view_.army;
        return per * n;
    }

    // The whip's hits, for its stats. (The bolt never hits directly: its burst does all of its damage.)
    void OnOutgoingDamageBase(void* ps, void* info) override {
        if (!Holds(ps)) return;
        stats_.in = stats_.set = At<float>(info, off::DamageInfo_Damage);
    }

    // What the whip really does, in the log every few seconds of use: shots a second and each kind of hit's damage
    // (as the game made it, as the whip set it, and after the player's buffs).
    struct Stats {
        double since = 0.0;
        int shots = 0;
        float in = 0.f, set = 0.f;
        struct Hit {
            int n = 0;
            float in = 0.f, set = 0.f, out = 0.f;
        } hits[16];
    } stats_;
    void StatsTick() {
        double now = Now();
        if (stats_.since == 0.0) stats_.since = now;
        if (now - stats_.since < 5.0) return;
        double secs = now - stats_.since;
        std::string text;
        float total = 0.f;
        for (int t = 0; t < 16; t++)
            if (const Stats::Hit& h = stats_.hits[t]; h.n) {
                char part[160];
                snprintf(part, sizeof part, "; type %d x%d: game %.1f, whip %.1f, buffed %.1f", t, h.n, h.in / h.n,
                         h.set / h.n, h.out / h.n);
                text += part;
                total += h.out;
            }
        CRAB_LOG("necromancer: whip stats over %.1f s: %d shots (%.2f a second), %.1f damage a second%s", secs,
                 stats_.shots, stats_.shots / secs, total / secs, text.c_str());
        stats_ = Stats{};
        stats_.since = now;
    }

    // The whip's loot that works on hits.
    void OnOutgoingDamage(void* ps, void* info) override {
        if (!Holds(ps)) return;
        uint8_t type = At<uint8_t>(info, off::DamageInfo_Type);
        float& damage = At<float>(info, off::DamageInfo_Damage);
        void* victim = At<void*>(info, off::DamageInfo_DamagedActor);
        if (Stats::Hit& h = stats_.hits[type & 15]; true) h.n++, h.in += stats_.in, h.set += stats_.set, h.out += damage;
        if (!victim || IsUndead(victim)) return;
        if (Tagged(victim)) damage *= 1.f + loot::Total(ps, loot::DeathMark) / 100.f;
        if (type == 3) lastMelee_[victim] = ps;
        else lastMelee_.erase(victim);
        // Soul Siphon: each enemy a bolt (or its burst) hits, once a crack.
        if (type <= 1)
            if (float n = loot::Total(ps, loot::SoulSiphon); n > 0.f) {
                double& last = siphoned_[victim];
                if (Now() - last > 0.5) last = Now(), AddSouls(ps, n);
            }
    }

    // --- Shots (the shooter's machine): where a crack lands is tagged ---------------------------------------

    void OnShot(void* weapon, void* ps, Vec3& start, Vec3& dir) override {
        if (whip_ && At<void*>(weapon, off::Weapon_WeaponDA) == whip_) stats_.shots++, StatsTick();
        if (!whip_ || At<void*>(weapon, off::Weapon_WeaponDA) != whip_ || Now() - lastTagSent_ < 0.1) return;
        void* c = At<void*>(weapon, off::Weapon_OwningC);
        void* pc = At<void*>(weapon, off::Weapon_OwningPC);
        // Long Lash and Branding Iron: this shooter's bolt (this machine fires the real one).
        float range = Reach(ps), radius = TagRadius(ps);
        if (projDA_) {
            At<float>(projDA_, off::ProjDA_Lifetime) = range / kBoltSpeed;
            At<float>(projDA_, off::ProjDA_ExplosionRadius) = burstRadius_ * (1.f + loot::Total(ps, loot::BrandingIron) / 100.f);
        }
        Vec3 at;
        if (!Aim(c, range, at)) return;
        lastTagSent_ = Now();
        wchar_t buf[96];
        swprintf(buf, std::size(buf), L"%lstag %.0f %.0f %.0f", kMsg, at.x, at.y, at.z);
        SendToHost(pc, buf);
        // The tag's sound, where it lands, if it catches anyone.
        if (enemyClass_)
            for (void* e : AllActorsOf(c, enemyClass_)) {
                Vec3 p;
                if (IsUndead(e) || !Alive(e) || !LocationOf(e, p) || DistanceSquared(p, at) > radius * radius)
                    continue;
                PlayAt("A_SoulWhip_Tag_", at);
                PlayAt("A_Necro_TagMark_", at, 0.6f);
                break;
            }
    }

    // --- Messages ------------------------------------------------------------------------------------------

    bool OnHostMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, kMsgLen, kMsg) != 0) return false;
        const wchar_t* s = text.c_str() + kMsgLen;
        void* ps = At<void*>(pc, off::Controller_PlayerState);
        if (!Holds(ps)) return true;
        int tier;
        Vec3 at;
        if (swscanf(s, L"raise %d %f %f %f", &tier, &at.x, &at.y, &at.z) == 4 && tier >= 0 && tier < kTiers)
            Raise(ps, tier, at);
        else if (swscanf(s, L"tag %f %f %f", &at.x, &at.y, &at.z) == 3)
            TagAround(ps, at);
        return true;
    }

    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, kMsgLen, kMsg) != 0) return false;
        const wchar_t* s = text.c_str() + kMsgLen;
        Vec3 at;
        float souls, max, c[kTiers];
        int tier, undead = 0;
        float army = -1.f;
        bool host = At<uint8_t>(pc, off::Actor_Role) == ROLE_Authority;  // the host's own: it knows already
        wchar_t path[200];
        if (swscanf(s, L"rise %d %f %f %f %199ls", &tier, &at.x, &at.y, &at.z, path) == 5) {
            if (!host && tier >= 0 && tier < kTiers) {
                runes_.Rise(world_, at, kRuneRadius[tier], kRiseDelay);
                Hush(ui::FindObject(path));
            }
        } else if (text.compare(kMsgLen, 5, L"tags ") == 0) {
            if (!host) SeeTags(s + 5);
        } else if (swscanf(s, L"raised %d %f %f %f", &tier, &at.x, &at.y, &at.z) == 4) {
            if (At<uint8_t>(pc, off::Actor_Role) != ROLE_Authority) pending_.push_back({at, Now() + 5.0});
            // One sound for a pack.
            if (tier >= 0 && tier < kTiers && Now() - lastRaiseSound_[tier] > 0.3) {
                lastRaiseSound_[tier] = Now();
                PlayAt((std::string("A_Necro_Raise_") + ui::Narrow(kTierNames[tier]) + "_").c_str(), at);
            }
        } else if (text.compare(kMsgLen, std::wstring::npos, L"denied") == 0) {
            Play("A_Necro_Raise_Denied_", 0.8f);
        } else if (swscanf(s, L"souls %f %f %f %f %f %f %d %f", &souls, &max, &c[0], &c[1], &c[2], &c[3], &undead, &army) >= 6) {
            view_.undead = undead;
            view_.army = army >= 0.f && army < 1e6f ? army : float(undead);  // an older host sends only the count
            if (view_.known && souls > view_.souls + 0.01f) {
                if (souls >= max - 1e-3f && view_.souls < max - 1e-3f) Play("A_Necro_SoulBar_Full_", 0.9f);
                else if (Now() - lastSoulSound_ > 0.08) Play("A_Soul_Pickup_", 0.55f), lastSoulSound_ = Now();
            }
            view_.souls = souls;
            view_.max = max;
            for (int t = 0; t < kTiers; t++) view_.cost[t] = c[t];
            // Then the tiers' kinds, one per line.
            size_t line = text.find(L'\n');
            for (int t = 0; t < kTiers; t++) {
                size_t next = line == std::wstring::npos ? line : text.find(L'\n', line + 1);
                view_.names[t] = line == std::wstring::npos ? L"" : text.substr(line + 1, next - line - 1);
                line = next;
            }
            view_.known = true;
        }
        return true;
    }

    // --- Ticks ---------------------------------------------------------------------------------------------

    void OnTick(void* world) override {
        void* gs = GameStateOf(world);
        if (!gs) return;
        enemyClass_ = enemyClass_ ? enemyClass_ : ui::FindObject(kEnemyClassPath);
        // The islands stream into the lobby's world, so the whip's stone and pickup would stay behind: gone once
        // the lobby is (each machine its own stone; the host the pickup, which the game removes everywhere).
        bool lobby = At<uint8_t>(gs, off::GS_CurrentIslandType) == uint8_t(IslandType::Lobby);
        if (!lobby) pedestalTries_ = 0;
        // Back in the lobby (the islands stream into its world, so this is seen here, not at BeginPlay): a new
        // run, with no souls and no kinds to raise. The host forgets them; every machine its HUD's copy.
        if (lobby && !wasLobby_) {
            if (At<uint8_t>(gs, off::Actor_Role) == ROLE_Authority && !players_.empty()) {
                CRAB_LOG("necromancer: back in the lobby, souls and rosters cleared");
                players_.clear();
            }
            view_ = View{};
        }
        wasLobby_ = lobby;
        if (!pedestal_.empty() && !lobby) {
            static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
            static void* meshActorClass = ui::FindObject(L"/Script/Engine.StaticMeshActor");
            std::vector<void*> present = AllActorsOf(world, meshActorClass), pickups = AllActorsOf(world, pickupClass);
            present.insert(present.end(), pickups.begin(), pickups.end());
            for (void* a : pedestal_)
                if (std::find(present.begin(), present.end(), a) != present.end()) ui::Call(a, L"K2_DestroyActor");
            pedestal_.clear();
        }
        // In the lobby, the whip's pedestal is checked once a second and put back if it's missing (the first
        // time, after a run, after a restart), for as long as the lobby lasts.
        if (lobby && Now() >= nextPedestalCheck_) {
            nextPedestalCheck_ = Now() + 1.0;
            void* lm = LevelManagerOf(world);
            if (lm && !AddLobbyPedestal(lm) && ++pedestalTries_ == 10)
                CRAB_LOG("necromancer: no Soul Whip pedestal yet: %s", pedestalWhy_.c_str());
        }
        if (At<uint8_t>(gs, off::Actor_Role) == ROLE_Authority) HostTick(world, gs);
        MarkRaised(world);
        MarkTagged(world, At<uint8_t>(gs, off::Actor_Role) == ROLE_Authority);
        UpdateHud(world);
    }

    // The raise keys, on the player's machine.
    void OnFrame(void* world, float dt) override {
        world_ = world;
        runes_.Frame(world);
        Unhush(false);
        FollowGameVolume(world);
        UpdateLashes(world, dt);
        UpdatePickups(world);
        void* lm = LevelManagerOf(world);
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        if (!IsA(c, StaticClass(va::ACrabPlayerC_StaticClass))) return;
        void* ps = At<void*>(c, off::Pawn_PlayerState);
        void* pc = At<void*>(c, off::Pawn_Controller);
        // Picking the whip up.
        void* held = ps ? At<void*>(ps, off::PS_WeaponDA) : nullptr;
        if (whip_ && held == whip_ && lastHeld_ && lastHeld_ != whip_) Play("A_SoulWhip_Equip_", 0.9f);
        lastHeld_ = held;
        bool ready = whip_ && held == whip_ && pc && InGame(pc);
        for (int t = 0; t < kTiers; t++) {
            bool down = ready && keys_[t] && (GetAsyncKeyState(keys_[t]) & 0x8000);
            if (down && !keyDown_[t]) {
                Vec3 at;
                if (Aim(c, raiseRange_, at)) {
                    wchar_t buf[96];
                    swprintf(buf, std::size(buf), L"%lsraise %d %.0f %.0f %.0f", kMsg, t, at.x, at.y, at.z);
                    SendToHost(pc, buf);
                }
            }
            keyDown_[t] = down;
        }
    }

private:
    struct PlayerData {
        float souls = 0.f;
        void* roster[kTiers] = {};  // the enemy class each tier raises: the last one killed
        std::wstring names[kTiers];
        bool dirty = true;
        int sentUndead = -1;
        float sentArmy = -1.f;
        float sentMax = -1.f, sentCost[kTiers] = {};  // what the HUD was told, to resend when loot changes them
    };
    struct UndeadInfo {
        void* owner;  // ACrabPS*
        int tier;
        float cost;
    };
    struct Pending {
        Vec3 at;
        double until;
    };
    struct Rising {  // host: an undead about to come up
        void* ps;
        std::string key;  // the owner's, for a refund
        int tier;
        void* cls;
        Vec3 loc;
        float yaw, upkeep, paid;
        double at;
    };
    struct View {  // what the host last said, on the player's machine
        bool known = false;
        float souls = 0.f, max = 100.f, cost[kTiers] = {};
        int undead = 0;
        float army = 0.f;  // Army of the Dead's weighted count
        std::wstring names[kTiers];
    };

    bool Holds(void* ps) const {
        return whip_ && IsA(ps, StaticClass(va::ACrabPS_StaticClass)) && At<void*>(ps, off::PS_WeaponDA) == whip_;
    }
    bool IsUndead(void* c) const { return c && (undead_.count(c) || marked_.count(c)); }
    bool Tagged(void* enemy) const {
        auto it = tags_.find(enemy);
        return it != tags_.end() && it->second > Now();
    }
    static std::string PlayerKey(void* ps) { return FStringToUtf8(At<TArray<wchar_t>>(ps, off::PS_PlayerName)); }

    // In a level, with the game window in front and no menu or chat open: keys are for raising.
    static bool InGame(void* pc) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (!fg || (GetWindowThreadProcessId(fg, &pid), pid != GetCurrentProcessId())) return false;
        if (At<void*>(pc, off::PC_OpenMenu)) return false;
        void* gsui = At<void*>(pc, off::PC_GameStateUI);
        void* chat = gsui ? At<void*>(gsui, off::GameStateUI_ChatInput) : nullptr;
        bool typing = false;
        if (chat) ui::Call(chat, L"HasKeyboardFocus", &typing);
        return !typing;
    }

    // Where the player's crosshair points, up to range away (the ground below it if it hits nothing).
    static bool Aim(void* c, float range, Vec3& out) {
        if (!IsA(c, StaticClass(va::ACrabPlayerC_StaticClass))) return false;
        alignas(16) uint8_t hit[off::Hit_Size] = {};
        At<float>(hit, off::Hit_Time) = 1.f;
        Fn<void (*)(void*, void*, float)>(va::ACrabPlayerC_CameraTrace)(c, hit, range);
        out = (hit[off::Hit_bBlockingHit] & 1) ? At<Vec3>(hit, off::Hit_ImpactPoint) : At<Vec3>(hit, off::Hit_TraceEnd);
        Fn<void (*)(void*, Vec3*)>(va::SnapToGround)(c, &out);
        return true;
    }

    // --- The whip's asset ----------------------------------------------------------------------------------

    // Made once, on every machine, under the same name: the game replicates it by path.
    void EnsureWhip() {
        if (whip_ || !enabled_) return;
        void* sp = ui::FindObject(kSpawnablesPath);
        if (!sp) return;
        auto& weapons = At<TArray<void*>>(sp, off::Spawnables_Weapons);
        void *sniper = nullptr, *scepter = nullptr, *wand = nullptr;
        for (int i = 0; weapons.Data && i < weapons.Num; i++) {
            std::string n = ObjectName(weapons.Data[i]);
            if (n == "DA_Weapon_Sniper") sniper = weapons.Data[i];
            else if (n == "DA_Weapon_LightningScepter") scepter = weapons.Data[i];
            else if (n == "DA_Weapon_ArcaneWand") wand = weapons.Data[i];
        }
        if (!sniper) {
            CRAB_LOG("necromancer: the Sniper isn't in the lobby list; no Soul Whip");
            return;
        }
        whip_ = MakeDA(At<void*>(sniper, off::Object_Class), sniper, L"DA_Weapon_SoulWhip");
        if (!whip_) return;
        if (void* proj = At<void*>(sniper, off::WeaponDA_ProjectileDA)) {
            projDA_ = MakeDA(At<void*>(proj, off::Object_Class), proj, L"DA_Projectile_SoulWhip");
            if (projDA_) At<void*>(whip_, off::WeaponDA_ProjectileDA) = projDA_;
        }
        // The crack's bolt: a soul, the Arcane Wand's purple orb and burst, flying straight and slow enough to see
        // (the Sniper's 1000 m/s would show it for a frame). Its reach, burst and piercing: RefreshWhip.
        if (projDA_) {
            if (void* orb = wand ? At<void*>(wand, off::WeaponDA_ProjectileDA) : nullptr) {
                for (int f : {off::ProjDA_FX, off::ProjDA_TrailFX, off::ProjDA_ExplosionFX, off::ProjDA_ExplosionSound})
                    At<void*>(projDA_, f) = At<void*>(orb, f);
                At<bool>(projDA_, off::ProjDA_bRandomizeTint) = At<bool>(orb, off::ProjDA_bRandomizeTint);
            } else {
                CRAB_LOG("necromancer: no Arcane Wand to borrow the soul bolt's look from; it keeps the Sniper's");
            }
            At<void*>(projDA_, off::ProjDA_Sound) = nullptr;
            At<float>(projDA_, off::ProjDA_Speed) = kBoltSpeed;
            At<float>(projDA_, off::ProjDA_MaxSpeed) = 0.f;
            At<float>(projDA_, off::ProjDA_GravityScale) = 0.f;
            At<float>(projDA_, off::ProjDA_CollisionRadius) = 40.f;
            At<bool>(projDA_, off::ProjDA_bExplodeAfterLifetime) = true;
        }
        SetFString(whip_, off::DA_Name, L"Soul Whip");
        SetFString(whip_, off::DA_Description,
                   L"Cracks tag enemies for your undead. Replaces your ability with raising the dead");
        At<bool>(whip_, off::DA_bRequiresUnlock) = false;
        At<bool>(whip_, off::WeaponDA_bInfiniteClipSize) = true;
        // The Lightning Scepter's lash of an arm swing, crosshair and aiming, instead of the Sniper's scope and bolt.
        if (scepter) {
            std::memcpy(&At<uint8_t>(whip_, 0xF0), &At<uint8_t>(scepter, 0xF0), 0x10);  // aiming FOV, arm offset
            At<uint8_t>(whip_, off::WeaponDA_CrosshairType) = At<uint8_t>(scepter, off::WeaponDA_CrosshairType);
            At<void*>(whip_, off::WeaponDA_FireMontage) = At<void*>(scepter, off::WeaponDA_FireMontage);
        }
        At<void*>(whip_, off::WeaponDA_PostFireClearChamberMontage) = nullptr;
        At<void*>(whip_, off::WeaponDA_PostFireClearChamberSound) = nullptr;
        At<float>(whip_, off::WeaponDA_VerticalRecoil) = 0.f;
        At<float>(whip_, off::WeaponDA_HorizontalRecoil) = 0.f;
        Append(weapons, whip_);
        CRAB_LOG("necromancer: created %s from %s and added it to the lobby", ObjectName(whip_).c_str(),
                 ObjectName(sniper).c_str());
        EnsureArt();
    }

    // The whip's meshes, the HUD's pictures and the sounds, from the pack inside the DLL. Once.
    void EnsureArt() {
        if (artTried_ || !whip_) return;
        artTried_ = true;
        if (!pack_.Load()) return;
        void* palette = necro::Texture(pack_.Get("palette.png"));
        mid_ = necro::TextureMaterial(palette);
        void* handle = necro::Mesh(pack_.Get("whip_handle.mesh"), mid_, L"SM_SoulWhip_Handle");
        if (handle) {
            At<void*>(whip_, off::WeaponDA_Mesh) = handle;
        } else {
            CRAB_LOG("necromancer: couldn't build the whip's handle; it keeps the Sniper's look");
            return;
        }
        // The lash: its pieces and the rig (where it leaves the handle, its segments' lengths).
        necro::Blob rig = pack_.Get("whip.rig");
        uint32_t n = 0;
        if (rig && rig.size >= 28) std::memcpy(&n, rig.data + 24, 4);
        if (!n || n > uint32_t(necro::Lash::kMaxSegments) || rig.size < 28 + n * 4) {
            CRAB_LOG("necromancer: the whip's rig is missing; no lash");
            return;
        }
        std::memcpy(&lashAnchor_, rig.data, 12);
        std::memcpy(&lashDir_, rig.data + 12, 12);
        segLength_.resize(n);
        std::memcpy(segLength_.data(), rig.data + 28, n * 4);
        for (uint32_t i = 0; i < n; i++) {
            char name[32];
            if (i + 1 < n) snprintf(name, sizeof name, "whip_lash_%02u.mesh", i + 1);
            else snprintf(name, sizeof name, "whip_tip.mesh");
            wchar_t wname[40];
            swprintf(wname, std::size(wname), L"SM_SoulWhip_%u", i + 1);
            void* m = necro::Mesh(pack_.Get(name), mid_, wname);
            if (!m) {
                CRAB_LOG("necromancer: couldn't build %s; no lash", name);
                segMesh_.clear();
                return;
            }
            segMesh_.push_back(m);
        }
        // The soul glow's steps: a material each, held by the handle mesh (extra slots it doesn't draw).
        for (int k = 0; k < kPulseSteps; k++) {
            char name[16];
            snprintf(name, sizeof name, "pulse_%d.png", k);
            pulse_[k] = necro::TextureMaterial(necro::Texture(pack_.Get(name)));
            struct {
                void* material;
                uint32_t ret[2];
            } p{pulse_[k], {}};
            if (pulse_[k]) ui::Call(handle, L"AddMaterial", &p);
            else pulse_[0] = nullptr;
        }
        // The HUD's pictures, held by the whip's material so they outlive any one HUD.
        const char* pictures[] = {"soul.png", "pip.png", "crown.png", "icon_tier1.png", "icon_tier2.png",
                                  "icon_tier3.png", "icon_tier4.png"};
        void** slots[] = {&texSoul_, &texPip_, &texCrown_, &texTier_[0], &texTier_[1], &texTier_[2], &texTier_[3]};
        for (int i = 0; i < 7; i++) {
            *slots[i] = necro::Texture(pack_.Get(pictures[i]));
            wchar_t param[32];
            swprintf(param, std::size(param), L"Keep%d", i);
            if (*slots[i] && mid_) necro::Keep(mid_, param, *slots[i]);
        }
        // The marks on the ground: the raise circle's two layers, kept the same way; the decal material by the
        // handle mesh.
        void* ring = necro::Texture(pack_.Get("raise_ring.png"));
        void* hex = necro::Texture(pack_.Get("raise_hex.png"));
        if (ring && mid_) necro::Keep(mid_, L"Keep7", ring);
        if (hex && mid_) necro::Keep(mid_, L"Keep8", hex);
        // The tag's halo: a flat square one unit across, the picture on it see-through where it's clear.
        void* halo = nullptr;
        if (void* haloMat = necro::TextureMaterial(necro::Texture(pack_.Get("tag_halo.png")), true)) {
            const float quad[6][5] = {{-.5f, -.5f, 0, 0, 0}, {.5f, -.5f, 0, 1, 0}, {.5f, .5f, 0, 1, 1},
                                      {-.5f, -.5f, 0, 0, 0}, {.5f, .5f, 0, 1, 1}, {-.5f, .5f, 0, 0, 1}};
            uint8_t blob[4 + sizeof quad];
            uint32_t n = 6;
            std::memcpy(blob, &n, 4);
            std::memcpy(blob + 4, quad, sizeof quad);
            halo = necro::Mesh({blob, uint32_t(sizeof blob)}, haloMat, L"SM_TagHalo");
        }
        runes_.Init(ring, hex, handle, halo, &AddPiece);
        // Our crack and equip sounds instead of the Sniper's; no muzzle flash or scope glint.
        At<void*>(whip_, off::WeaponDA_GunshotSound) = nullptr;
        At<void*>(whip_, off::WeaponDA_EquipSound) = nullptr;
        At<void*>(whip_, off::WeaponDA_MuzzleFlashFX) = nullptr;
        At<void*>(whip_, off::WeaponDA_CosmeticFX) = nullptr;
        // Its pickup: not the Sniper floating in the Sniper's red, but the whip itself (UpdatePickups) in violet.
        At<void*>(whip_, off::DA_PrimaryFX) = nullptr;
        At<ui::LinearColor>(whip_, off::DA_Tint) = {0.55f, 0.22f, 1.f, 1.f};
        CRAB_LOG("necromancer: whip art ready: handle and %zu lash pieces", segMesh_.size());
    }

    // --- The lash (every machine) ----------------------------------------------------------------------------
    //
    // Every whip held by anyone gets the lash pieces as components of its weapon actor, placed each frame along a
    // simulated chain that starts where the handle ends. The weapons are found through the players' characters, so
    // a weapon that's gone is never touched again (the engine destroys its components with it).

    struct LashState {
        necro::Lash sim;
        std::vector<void*> pieces;
        std::vector<int> glow;  // each piece's soul glow step, and the handle's last
        int32_t fired = 0;
        bool hidden = false;
        bool seen = false;
    };

    void UpdateLashes(void* world, float dt) {
        if (segMesh_.empty()) return;
        for (auto& [w, l] : lashes_) l.seen = false;
        ForEachPlayer(world, [&](void* ps) {
            void* c = PawnOf(ps);
            if (!c) return;
            auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
            for (int i = 0; weapons.Data && i < weapons.Num; i++) {
                void* w = weapons.Data[i];
                if (!w || At<void*>(w, off::Weapon_WeaponDA) != whip_) continue;
                auto it = lashes_.find(w);
                if (it == lashes_.end()) {
                    it = lashes_.emplace(w, LashState{}).first;
                    AddPieces(w, it->second);  // if it fails, the whip goes without (once)
                }
                it->second.seen = true;
                StepLash(w, c, it->second, dt);
            }
        });
        for (auto it = lashes_.begin(); it != lashes_.end();) {
            if (it->second.seen) {
                ++it;
                continue;
            }
            // Its weapon is gone, and the engine took the pieces with it.
            it = lashes_.erase(it);
        }
    }

    bool AddPieces(void* weapon, LashState& l) {
        l.sim.Init(segLength_.data(), int(segLength_.size()));
        l.fired = At<int32_t>(weapon, off::Weapon_TimesFired);
        for (void* mesh : segMesh_) {
            void* piece = AddPiece(weapon, mesh);
            if (!piece) {
                CRAB_LOG("necromancer: couldn't add the lash to a whip");
                return false;
            }
            l.pieces.push_back(piece);
        }
        l.glow.assign(l.pieces.size() + 1, -1);
        return true;
    }

    // A mesh component on an actor, placed in the world by us each frame: no collision, no shadow.
    static void* AddPiece(void* actor, void* mesh) {
        static void* componentClass = ui::FindObject(L"/Script/Engine.StaticMeshComponent");
        {
            struct alignas(16) {
                void* cls;
                bool manual;  // not attached: placed in the world each frame
                uint8_t pad[7];
                necro::Transform t;
                bool deferred;
                uint8_t pad2[7];
                void* ret;
            } p{componentClass, true, {}, {}, false, {}, nullptr};
            static_assert(offsetof(decltype(p), t) == 0x10 && offsetof(decltype(p), deferred) == 0x40 &&
                          offsetof(decltype(p), ret) == 0x48);
            if (!ui::Call(actor, L"AddComponentByClass", &p) || !p.ret) return nullptr;
            void* piece = p.ret;
            uint8_t movable = 2, noCollision = 0;
            bool no = false;
            ui::Call(piece, L"SetMobility", &movable);
            ui::Call(piece, L"SetCollisionEnabled", &noCollision);
            ui::Call(piece, L"SetCastShadow", &no);
            struct {
                void* mesh;
                bool ret;
            } m{mesh, false};
            ui::Call(piece, L"SetStaticMesh", &m);
            return piece;
        }
    }

    void StepLash(void* weapon, void* c, LashState& l, float dt) {
        void* mesh = At<void*>(weapon, off::Weapon_Mesh);
        if (!mesh || l.pieces.size() != segMesh_.size()) return;
        static necro::Method isVisible(L"IsVisible"),
            setHidden(L"SetHiddenInGame"), aimRotation(L"GetBaseAimRotation"), halfHeight(L"GetScaledCapsuleHalfHeight");
        bool visible = false;
        isVisible(mesh, &visible);
        if (visible == l.hidden) {
            l.hidden = !visible;
            for (void* piece : l.pieces) {
                struct {
                    bool hidden, children;
                } h{l.hidden, false};
                setHidden(piece, &h);
            }
        }
        if (l.hidden) return;

        // Where the handle is: the weapon mesh's world transform (the mesh is in the hand socket's space).
        necro::Q4 q;
        std::memcpy(&q, &At<uint8_t>(mesh, off::SceneComponent_Rotation), 16);
        necro::V3 loc = Of(At<Vec3>(mesh, off::SceneComponent_Location));
        necro::V3 scale = Of(At<Vec3>(mesh, off::SceneComponent_Scale));
        float s = (std::fabs(scale.x) + std::fabs(scale.y) + std::fabs(scale.z)) / 3.f;
        necro::V3 anchor = loc + necro::Rotate(q, Of(lashAnchor_) * s);
        necro::V3 dir = necro::Normal(necro::Rotate(q, Of(lashDir_)));

        // A shot: the crack, out toward where it lands (the soul bolt flies the rest of the way).
        int32_t fired = At<int32_t>(weapon, off::Weapon_TimesFired);
        if (fired != l.fired) {
            l.fired = fired;
            Vec3 at;
            necro::V3 target;
            if (Aim(c, whipRange_, at)) {
                target = Of(at);
            } else {
                Vec3 r{};
                aimRotation(c, &r);
                const float d = 3.14159265f / 180.f;
                necro::V3 forward{std::cos(r.x * d) * std::cos(r.y * d), std::cos(r.x * d) * std::sin(r.y * d),
                                  std::sin(r.x * d)};
                target = anchor + forward * whipRange_;
            }
            l.sim.Crack(target);
            PlayAt("A_SoulWhip_Crack_", {anchor.x, anchor.y, anchor.z}, 1.f, 0.45f);
        }

        Vec3 feet{};
        LocationOf(c, feet);
        float half = 0.f;
        if (void* capsule = At<void*>(c, off::Character_Capsule)) halfHeight(capsule, &half);
        l.sim.Step(anchor, dir, s, feet.z - half, dt);

        // The soul glow: breathing, as a wave running down the cord from the handle; a crack flares it.
        for (size_t i = 0; i <= l.pieces.size(); i++)  // the handle (last) leads
            Glow(i < l.pieces.size() ? l.pieces[i] : mesh, l.glow[i], i < l.pieces.size() ? int(i + 1) : 0,
                 l.sim.Striking());

        // The pieces along the chain: each from its point to the next, stretched to fit, rolled as little as
        // possible from the handle's.
        necro::V3 up = necro::Rotate(q, {1, 0, 0});
        for (int i = 0; i < l.sim.Segments(); i++)
            PlaceSegment(l.pieces[i], l.sim.Point(i), l.sim.Point(i + 1), l.sim.RestLength(i) * s, s, dir, up);
    }

    // A lash piece from a to b (its rest length stretched to fit), rolled as little as possible from the last.
    static void PlaceSegment(void* piece, necro::V3 a, necro::V3 b, float rest, float s, necro::V3 dir, necro::V3& up) {
        necro::V3 x = necro::Normal(b - a, dir);
        necro::V3 z = up - x * necro::Dot(up, x);
        if (necro::Length(z) < 1e-3f) z = necro::Cross(x, {0, 1, 0});
        z = necro::Normal(z);
        necro::V3 y = necro::Cross(z, x);
        up = z;
        float stretch = rest > 0.f ? necro::Length(b - a) / rest * s : s;
        SetWorld(piece, a, necro::FromAxes(x, y, z), {stretch, s, s});
    }

    static void SetWorld(void* component, necro::V3 loc, necro::Q4 rot, necro::V3 scale) {
        static necro::Method setTransform(L"K2_SetWorldTransform");
        struct alignas(16) {
            necro::Transform t;
            bool sweep;
            uint8_t hit[off::Hit_Size + 3];
            bool teleport;
            uint8_t pad[3];
        } p{};
        static_assert(offsetof(decltype(p), hit) == 0x31 && offsetof(decltype(p), teleport) == 0xBC);
        std::memcpy(p.t.rot, &rot, 16);
        p.t.loc[0] = loc.x, p.t.loc[1] = loc.y, p.t.loc[2] = loc.z;
        p.t.scale[0] = scale.x, p.t.scale[1] = scale.y, p.t.scale[2] = scale.z;
        p.teleport = true;
        setTransform(component, &p);
    }

    // One part's soul glow: along = its place from the handle (the wave runs down the cord); flare: a crack.
    void Glow(void* part, int& current, int along, bool flare) {
        if (!pulse_[0]) return;
        static necro::Method setMaterial(L"SetMaterial");
        int step = kPulseSteps - 1;
        if (!flare) {
            double wave = 0.5 + 0.5 * std::sin(6.2831853 * (0.7 * Now() - 0.06 * along));
            step = int(std::lround(wave * (kPulseSteps - 2)));
        }
        if (step == current) return;
        current = step;
        struct {
            int32_t index;
            void* material;
        } m{0, pulse_[step]};
        setMaterial(part, &m);
    }

    // --- The whip's pickups (every machine) ------------------------------------------------------------------
    //
    // A pickup holding the whip (its pedestal, or one from a key totem) shows the whip itself: the handle with the
    // lash coiled under it, turning and bobbing, glowing like the held one.

    struct ShowState {
        std::vector<void*> parts;  // the handle, then the lash pieces
        std::vector<int> glow;
        bool hidden = false;
        bool seen = false;
    };

    void UpdatePickups(void* world) {
        void* handle = whip_ ? At<void*>(whip_, off::WeaponDA_Mesh) : nullptr;
        if (segMesh_.empty() || !handle) return;
        static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
        for (auto& [a, sh] : shows_) sh.seen = false;
        for (void* a : AllActorsOf(world, pickupClass)) {
            if (At<void*>(a, off::InteractPickup_PickupInfo + off::PickupInfo_DA) != whip_) continue;
            auto it = shows_.find(a);
            if (it == shows_.end()) {
                it = shows_.emplace(a, ShowState{}).first;
                for (size_t i = 0; i <= segMesh_.size(); i++) {
                    void* part = AddPiece(a, i ? segMesh_[i - 1] : handle);
                    if (!part) {
                        CRAB_LOG("necromancer: couldn't show the whip on its pickup");
                        it->second.parts.clear();
                        break;
                    }
                    it->second.parts.push_back(part);
                }
                it->second.glow.assign(it->second.parts.size(), -1);
            }
            it->second.seen = true;
            StepShow(a, it->second);
        }
        for (auto it = shows_.begin(); it != shows_.end();) it = it->second.seen ? std::next(it) : shows_.erase(it);
    }

    void StepShow(void* pickup, ShowState& sh) {
        if (sh.parts.size() != segMesh_.size() + 1) return;
        bool hidden = At<bool>(pickup, off::InteractPickup_bDeactivated);
        if (hidden != sh.hidden) {
            sh.hidden = hidden;
            static necro::Method setHidden(L"SetHiddenInGame");
            for (void* part : sh.parts) {
                struct {
                    bool hidden, children;
                } h{hidden, false};
                setHidden(part, &h);
            }
        }
        if (hidden) return;
        Vec3 at;
        if (!LocationOf(pickup, at)) return;
        const float k = 0.8f;  // a little smaller than in the hand
        double t = Now();
        float yaw = float(std::fmod(t * 1.2, 6.2831853));
        necro::Q4 spin{0.f, 0.f, std::sin(yaw / 2), std::cos(yaw / 2)};
        // The handle's grip at the pickup's spot, bobbing.
        necro::V3 center{at.x, at.y, at.z + 8.f * float(std::sin(t * 2.0))};
        necro::V3 origin = center - necro::Rotate(spin, Of(lashAnchor_) * (0.5f * k));
        SetWorld(sh.parts[0], origin, spin, {k, k, k});
        Glow(sh.parts[0], sh.glow[0], 0, false);
        // The lash: out of the handle, then round in a loop and a bit, drifting sideways so it doesn't overlap.
        necro::V3 a = origin + necro::Rotate(spin, Of(lashAnchor_) * k);
        necro::V3 d0 = necro::Normal(necro::Rotate(spin, Of(lashDir_)));
        necro::V3 axis = necro::Normal(necro::Cross(d0, {0, 0, 1}), necro::Rotate(spin, {0, 1, 0}));
        size_t n = segMesh_.size();
        float turn = 6.2831853f * 1.15f / float(n);
        necro::V3 up = necro::Rotate(spin, {1, 0, 0});
        for (size_t i = 0; i < n; i++) {
            float th = turn * float(i + 1);
            // d0 turned about the axis by th (Rodrigues), plus the drift.
            necro::V3 d = d0 * std::cos(th) + necro::Cross(axis, d0) * std::sin(th) +
                          axis * (necro::Dot(axis, d0) * (1.f - std::cos(th)));
            d = necro::Normal(d + axis * 0.12f);
            necro::V3 b = a + d * (segLength_[i] * k);
            PlaceSegment(sh.parts[i + 1], a, b, segLength_[i] * k, k, d0, up);
            Glow(sh.parts[i + 1], sh.glow[i + 1], int(i + 1), false);
            a = b;
        }
    }

    static necro::V3 Of(const Vec3& v) { return {v.x, v.y, v.z}; }

    // --- Sounds (every machine) --------------------------------------------------------------------------------

    // One of the sounds whose names start with prefix, at random.
    necro::Blob Pick(const char* prefix) {
        auto all = pack_.All(prefix);
        if (all.empty()) return {};
        return all[rng_() % all.size()];
    }
    // A sound for this machine's player alone (souls, a refused raise).
    void Play(const char* prefix, float gain = 1.f) {
        if (pack_.Load()) mixer_.Play(Pick((std::string(prefix)).c_str()), gain, 0.f);
    }
    // A sound in the world: quieter with distance from this machine's camera, and panned.
    void PlayAt(const char* prefix, Vec3 at, float gain = 1.f, float skip = 0.f) {
        if (!pack_.Load()) return;
        static necro::Method cameraLocation(L"GetCameraLocation"), cameraRotation(L"GetCameraRotation");
        void* lm = world_ ? LevelManagerOf(world_) : nullptr;
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        void* pc = c ? At<void*>(c, off::Pawn_Controller) : nullptr;
        void* cam = pc ? At<void*>(pc, off::PC_CameraManager) : nullptr;
        float pan = 0.f;
        if (cam) {
            Vec3 from{}, rot{};
            cameraLocation(cam, &from);
            cameraRotation(cam, &rot);
            necro::V3 to = Of(at) - Of(from);
            float dist = necro::Length(to);
            float fade = std::clamp(1.f - (dist - 1000.f) / 5000.f, 0.f, 1.f);
            gain *= fade * fade;
            float yaw = rot.y * 3.14159265f / 180.f;
            if (dist > 1.f) pan = necro::Dot(to * (1.f / dist), {-std::sin(yaw), std::cos(yaw), 0.f}) * 0.8f;
        }
        mixer_.Play(Pick(prefix), gain, pan, skip);
    }

    // The settings that can change with the host's.
    void RefreshWhip() {
        if (!whip_) return;
        At<float>(whip_, off::WeaponDA_BaseFireRate) = whipFireRate_;
        At<float>(whip_, off::WeaponDA_ModTriggerRollMultiplier) = procRoll_;
        if (projDA_) {
            At<float>(projDA_, off::ProjDA_Lifetime) = whipRange_ / kBoltSpeed;
            At<int32_t>(projDA_, off::ProjDA_MaxPiercings) = 0;  // stops at the first enemy, and bursts
            At<float>(projDA_, off::ProjDA_ExplosionRadius) = burstRadius_;
            At<float>(projDA_, off::ProjDA_ExplosionDamage) = whipDps_ / whipFireRate_;  // the bolt's only damage
        }
    }

    // --- The lobby's pedestal -------------------------------------------------------------------------------
    //
    // The lobby's weapons are pickups placed in the map (BP_Pickup_Lobby) over stone pedestals (plain mesh
    // actors). The whip gets its own, one pedestal to the left of the Crossbow: every machine adds the stone
    // (the map's own copy, nothing to send), and the host spawns the pickup, which the game sends to everyone.

    struct alignas(16) Transform {  // FTransform: the engine copies it with aligned loads
        float rot[4] = {0, 0, 0, 1};
        Vec3 loc{};
        float pad0 = 0;
        Vec3 scale{1, 1, 1};
        float pad1 = 0;
    };

    static void* Spawn(void* context, void* cls, const Transform& t, void (*setup)(void*, void*), void* arg) {
        static void* statics = ui::FindObject(va::GameplayStaticsPath);
        struct {
            void* context;
            void* cls;
            Transform t;
            uint8_t collision;  // AlwaysSpawn
            void* owner;
            void* ret;
        } p{context, cls, t, 1, nullptr, nullptr};
        if (!statics || !cls || !ui::Call(statics, L"BeginDeferredActorSpawnFromClass", &p) || !p.ret) return nullptr;
        if (setup) setup(p.ret, arg);
        struct {
            void* actor;
            Transform t;
            void* ret;
        } f{p.ret, t, nullptr};
        ui::Call(statics, L"FinishSpawningActor", &f);
        return p.ret;
    }

    static Vec3 Rotation(void* actor) {
        Vec3 r{};
        ui::Call(actor, L"K2_GetActorRotation", &r);
        return r;
    }

    // A quaternion from a rotator (pitch, yaw, roll in degrees), as FRotator::Quaternion.
    static void Quat(const Vec3& r, float q[4]) {
        const float d = 3.14159265f / 360.f;
        float sp = std::sin(r.x * d), cp = std::cos(r.x * d), sy = std::sin(r.y * d), cy = std::cos(r.y * d),
              sr = std::sin(r.z * d), cr = std::cos(r.z * d);
        q[0] = cr * sp * sy - sr * cp * cy;
        q[1] = -cr * sp * cy - sr * cp * sy;
        q[2] = cr * cp * sy - sr * sp * cy;
        q[3] = cr * cp * cy + sr * sp * sy;
    }

    // Adds what's missing of the pedestal (in the lobby). true when it's all there; false with the reason in
    // pedestalWhy_.
    bool AddLobbyPedestal(void* lm) {
        if (!whip_) return pedestalWhy_ = "no whip", false;
        static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
        static void* meshActorClass = ui::FindObject(L"/Script/Engine.StaticMeshActor");
        void *crossbow = nullptr, *mine = nullptr;
        std::vector<std::pair<void*, Vec3>> weapons;  // the other lobby weapons
        for (void* a : AllActorsOf(lm, pickupClass)) {
            void* da = At<void*>(a, off::InteractPickup_PickupInfo + off::PickupInfo_DA);
            if (da == whip_) mine = a;
            if (!da || ObjectName(At<void*>(a, off::Object_Class)) != "BP_Pickup_Lobby_C") continue;
            if (ObjectName(da) == "DA_Weapon_Crossbow") crossbow = a;
            else if (ObjectName(At<void*>(da, off::Object_Class)) == ObjectName(At<void*>(whip_, off::Object_Class))) {
                Vec3 p;
                if (LocationOf(a, p)) weapons.push_back({a, p});
            }
        }
        int pickups = 0;
        for (void* a : AllActorsOf(lm, pickupClass)) pickups += a != nullptr;
        Vec3 at;
        if (!crossbow || !LocationOf(crossbow, at)) {
            char why[96];
            snprintf(why, sizeof why, "no Crossbow pickup among %d (class %s)", pickups, pickupClass ? "found" : "missing");
            pedestalWhy_ = why;
            return false;  // not the lobby, or not loaded yet
        }
        // Left of the Crossbow, facing it: away from its nearest neighbour, one gap further.
        Vec3 next{};
        float best = 1e30f;
        for (auto& [a, p] : weapons)
            if (DistanceSquared(p, at) < best) best = DistanceSquared(p, at), next = p;
        if (best == 1e30f) return pedestalWhy_ = "no other lobby weapon to measure the gap", false;
        Vec3 step{at.x - next.x, at.y - next.y, 0.f};
        Vec3 spot{at.x + step.x, at.y + step.y, at.z};

        // The stone under the Crossbow, copied.
        void* stone = nullptr;
        bool haveStone = false;
        best = 1e30f;
        for (void* a : AllActorsOf(lm, meshActorClass)) {
            haveStone |= a == stone_;
            Vec3 p;
            if (ObjectName(a).rfind("S_Pedestal_Generic", 0) != 0 || !LocationOf(a, p)) continue;
            float d = (p.x - at.x) * (p.x - at.x) + (p.y - at.y) * (p.y - at.y);
            if (d < best) best = d, stone = a;
        }
        if (haveStone) {
        } else if (stone && best < 400.f * 400.f) {
            Transform t;
            LocationOf(stone, t.loc);
            t.loc.x += step.x, t.loc.y += step.y;
            Quat(Rotation(stone), t.rot);
            ui::Call(stone, L"GetActorScale3D", &t.scale);
            void* copy = Spawn(lm, meshActorClass, t, [](void* copy, void* from) {
                void* to = At<void*>(copy, off::StaticMeshActor_Component);
                void* src = At<void*>(from, off::StaticMeshActor_Component);
                if (!to || !src) return;
                uint8_t movable = 2;
                ui::Call(to, L"SetMobility", &movable);
                struct { void* mesh; bool ret; } m{At<void*>(src, off::StaticMeshComponent_StaticMesh), false};
                ui::Call(to, L"SetStaticMesh", &m);
                int32_t n = 0;
                ui::Call(src, L"GetNumMaterials", &n);
                for (int32_t i = 0; i < n; i++) {
                    struct { int32_t i; void* ret; } g{i, nullptr};
                    ui::Call(src, L"GetMaterial", &g);
                    struct { int32_t i; void* mat; } sm{i, g.ret};
                    ui::Call(to, L"SetMaterial", &sm);
                }
            }, stone);
            if (copy) pedestal_.push_back(copy);
            stone_ = copy;
        } else if (!stoneWarned_) {
            stoneWarned_ = true;
            CRAB_LOG("necromancer: no pedestal stone under the Crossbow; the Soul Whip floats");
        }

        // The pickup: the host's, sent to everyone (already there after a settings change).
        if (At<uint8_t>(lm, off::Actor_Role) != ROLE_Authority || mine) return true;
        Transform t;
        t.loc = spot;
        Quat(Rotation(crossbow), t.rot);
        void* pickup = Spawn(lm, At<void*>(crossbow, off::Object_Class), t, [](void* pickup, void* from) {
                uint8_t* to = &At<uint8_t>(pickup, off::InteractPickup_PickupInfo);
                const uint8_t* src = &At<uint8_t>(from, off::InteractPickup_PickupInfo);
                // The inventory part (level, enhancements: heap arrays) the game's way, the rest as is. The
                // fresh pickup's arrays are empty, so nothing is lost by constructing over them.
                Fn<void* (*)(void*, const void*)>(va::FCrabInventoryInfo_CopyConstruct)(to + off::PickupInfo_Level,
                                                                                        src + off::PickupInfo_Level);
                std::memcpy(to + off::PickupInfo_MaxPickups, src + off::PickupInfo_MaxPickups,
                            off::PickupInfo_Size - off::PickupInfo_MaxPickups);
                At<void*>(to, off::PickupInfo_DA) = self_->whip_;
            }, crossbow);
        if (pickup) pedestal_.push_back(pickup);
        if (pickup)
            CRAB_LOG("necromancer: Soul Whip pedestal at %.0f %.0f %.0f, left of the Crossbow", spot.x, spot.y, spot.z);
        else
            CRAB_LOG("necromancer: couldn't spawn the Soul Whip's pickup");
        return true;
    }

    // --- Host ----------------------------------------------------------------------------------------------

    void HostTick(void* world, void* gs) {
        // Never left hidden past an elimination (ShowKit puts them back; this is only in case it didn't run).
        for (size_t i = hidden_.size(); i-- > 0;) Show(hidden_[i]);
        if (!hidden_.empty()) {
            CRAB_LOG("necromancer: %zu hidden perk lists put back late", hidden_.size());
            hidden_.clear();
        }
        bool fight = At<uint8_t>(gs, off::GS_MatchState) == kFight;
        std::vector<void*> enemies = enemyClass_ ? AllActorsOf(world, enemyClass_) : std::vector<void*>{};
        std::unordered_set<void*> present(enemies.begin(), enemies.end());
        for (auto it = undead_.begin(); it != undead_.end();)  // gone without dying (should not happen)
            it = present.count(it->first) ? std::next(it) : undead_.erase(it);
        for (auto it = tags_.begin(); it != tags_.end();)
            it = present.count(it->first) && it->second > Now() ? std::next(it) : tags_.erase(it);
        hostiles_.clear();
        for (void* e : enemies)
            if (!undead_.count(e) && Alive(e) && !IsSnail(e)) hostiles_.push_back(e);

        if (!fight && !undead_.empty()) {
            CRAB_LOG("necromancer: the fight is over, %zu undead crumble", undead_.size());
            std::vector<void*> all;
            std::unordered_map<void*, float> back;  // per owner: the survivors' cost they get back
            for (auto& [u, info] : undead_) {
                all.push_back(u);
                if (info.owner && Alive(u)) back[info.owner] += info.cost * refund_;
            }
            for (auto& [ps, n] : back) {
                if (n <= 0.f) continue;
                PlayerData& d = players_[PlayerKey(ps)];
                d.souls = std::min(d.souls + n, MaxSouls(ps));
                d.dirty = true;
                CRAB_LOG("necromancer: %s gets %.1f souls back for undead still standing", PlayerKey(ps).c_str(), n);
            }
            fightOver_ = true;
            for (void* u : all) Crumble(u);
            fightOver_ = false;
        }
        for (auto it = siphoned_.begin(); it != siphoned_.end();) it = present.count(it->first) ? std::next(it) : siphoned_.erase(it);
        for (auto it = lastMelee_.begin(); it != lastMelee_.end();) it = present.count(it->first) ? std::next(it) : lastMelee_.erase(it);
        if (fight)
            for (const Harvest& h : harvest_) {
                if (!h.cls) continue;
                QueueRise(h.ps, h.tier, h.cls, h.at, 0.f, cost_[h.tier] / (h.tier ? 1 : fodderPack_), 0.f);
                CRAB_LOG("necromancer: Harvester raises a %ls for %s", kTierNames[h.tier], PlayerKey(h.ps).c_str());
            }
        harvest_.clear();
        RiseDue(world, fight);
        if (fight && tagsDirty_ && Now() >= nextTagSync_) SendTags(world);
        if (fight && !hostiles_.empty()) Upkeep();
        if (fight && !undead_.empty()) HitNests(world);
        else nests_.clear();
        if (Now() >= nextGhost_) {  // again now and then: some meshes aren't ready at spawn, and buffs re-tint
            nextGhost_ = Now() + 1.0;
            for (auto& [u, info] : undead_) Ghost(u);
        }

        // Undead chase tagged enemies first, and anything when their target is gone; hostiles turn on undead
        // that come close (once a second).
        bool aggro = ++tick_ % 4 == 0;
        for (auto& [u, info] : undead_) {
            void* aic = At<void*>(u, off::Enemy_OwningAIC);
            if (!aic) continue;
            void* now = At<void*>(aic, off::EnemyAIC_BestEnemyC);
            void* want = PickForUndead(u);
            if (want != now && (!now || !Alive(now) || undead_.count(now) || (want && Tagged(want) && !Tagged(now))))
                Retarget(aic, u, want);
        }
        if (aggro && !undead_.empty())
            for (void* e : hostiles_)
                if (void* aic = At<void*>(e, off::Enemy_OwningAIC); aic && !undead_.count(At<void*>(aic, off::EnemyAIC_BestEnemyC)))
                    if (void* u = UndeadInReach(e)) Retarget(aic, e, u);

        // Souls to their players.
        ForEachPlayer(world, [&](void* ps) {
            if (!Holds(ps)) return;
            PlayerData& d = players_[PlayerKey(ps)];
            int count = 0;
            for (auto& [u, info] : undead_) count += info.owner == ps;
            if (count != d.sentUndead) d.sentUndead = count, d.dirty = true;
            if (float a = Army(ps); a != d.sentArmy) d.sentArmy = a, d.dirty = true;  // Army of the Dead
            if (float m = MaxSouls(ps); m != d.sentMax) d.sentMax = m, d.dirty = true;  // Soul Jar
            for (int t = 0; t < kTiers; t++)
                if (float c = Cost(ps, t); c != d.sentCost[t]) d.sentCost[t] = c, d.dirty = true;  // Soul Discount
            if (!d.dirty) return;
            d.dirty = false;
            void* pc = At<void*>(ps, off::Actor_Owner);
            if (!pc) return;
            int mine = 0;
            for (auto& [u, info] : undead_) mine += info.owner == ps;
            wchar_t buf[192];
            swprintf(buf, std::size(buf), L"%lssouls %.2f %.0f %.0f %.0f %.0f %.0f %d %.2f", kMsg, d.souls, MaxSouls(ps),
                     Cost(ps, 0), Cost(ps, 1), Cost(ps, 2), Cost(ps, 3), mine, Army(ps));
            std::wstring msg = buf;
            for (int t = 0; t < kTiers; t++) msg += L"\n" + (d.roster[t] ? d.names[t] : std::wstring());
            SendToPlayer(pc, msg);
        });
    }

    void Upkeep() {
        std::unordered_map<std::string, std::vector<void*>> byOwner;
        for (auto& [u, info] : undead_)
            if (info.owner) byOwner[PlayerKey(info.owner)].push_back(u);
        for (auto& [key, list] : byOwner) {
            PlayerData& d = players_[key];
            float drain = 0.f;
            for (void* u : list) drain += undead_[u].cost * upkeep_ * 0.25f;
            drain *= UpkeepShare(undead_[list.front()].owner);  // Hollow Hunger
            if (drain <= 0.f) continue;
            if (d.souls >= drain) {
                d.souls -= drain;
                d.dirty = true;
                continue;
            }
            if (d.souls > 0.f) d.souls = 0.f, d.dirty = true;
            // Out of souls: they starve, a little health at a time (this runs four times a second).
            for (void* u : list) {
                void* hc = At<void*>(u, off::C_HC);
                if (!hc || !Alive(u)) continue;
                float& health = At<float>(hc, off::HC_CurrentHealth);
                float loss = At<float>(hc, off::HC_CurrentMaxHealth) * starve_ * 0.25f;
                if (health - loss <= 1.f) Crumble(u);  // the ordinary death, while it still has health to lose
                else health -= loss;
            }
        }
    }

    void Raise(void* ps, int tier, Vec3 at) {
        void* gs = GameStateOf(ps);
        void* lm = LevelManagerOf(ps);
        void* c = PawnOf(ps);
        if (!gs || !lm || !c || At<uint8_t>(gs, off::GS_MatchState) != kFight) return Deny(ps, L"Nothing to raise against here");
        PlayerData& d = players_[PlayerKey(ps)];
        void* cls = d.roster[tier];
        if (!cls) return Deny(ps, (L"Kill a " + Lower(kTierNames[tier]) + L" enemy first").c_str());
        float cost = Cost(ps, tier);
        if (d.souls + 1e-3f < cost) return Deny(ps, L"Not enough souls");
        int mine = 0;
        for (auto& [u, info] : undead_) mine += info.owner == ps;
        for (const Rising& r : rising_) mine += r.ps == ps;
        if (mine >= MaxUndead(ps)) return Deny(ps, L"Your army is as big as it gets");
        Vec3 from;
        LocationOf(c, from);
        float dx = at.x - from.x, dy = at.y - from.y, dist = std::sqrt(dx * dx + dy * dy);
        if (dist > raiseRange_ && dist > 0.f) at = {from.x + dx / dist * raiseRange_, from.y + dy / dist * raiseRange_, at.z};
        float yaw = std::atan2(dy, dx) * 57.2957795f;
        int count = tier == 0 ? fodderPack_ + int(loot::Total(ps, loot::MassGrave) + 0.5f) : 1;
        float each = cost / count;  // what each one costs in upkeep
        // Twin Rising: twice as many, for the same souls.
        bool twin = false;
        if (int lv = loot::Level(ps, loot::TwinRising); lv && Roll(loot::Amount(loot::TwinRising) / 100.f, lv))
            twin = true, count *= 2;
        int paid = twin ? count / 2 : count;  // a twin's extra ones are free
        for (int i = 0; i < count; i++) {
            float a = 6.2831853f * i / count;
            float r = count > 1 ? 160.f + 25.f * count : 0.f;
            Vec3 loc = count > 1 ? Vec3{at.x + r * std::cos(a), at.y + r * std::sin(a), at.z} : at;
            QueueRise(ps, tier, cls, loc, yaw, each, i < paid ? each : 0.f);
        }
        d.souls -= each * float(paid);  // back for any that don't come up (Refund)
        d.dirty = true;
        if (twin) ShowMessage(ps, L"Twin Rising!");
        CRAB_LOG("necromancer: %s raises %d %ls (%ls)%s for %g souls, %g left", PlayerKey(ps).c_str(), count,
                 kTierNames[tier], d.names[tier].c_str(), twin ? " (twin)" : "", cost, d.souls);
    }

    // A raise to come: its circle now, the undead kRiseDelay later (HostTick). Every machine is told, to draw the
    // circle and leave out the enemy's own spawn effect (Hush).
    void QueueRise(void* ps, int tier, void* cls, Vec3 loc, float yaw, float upkeep, float paid) {
        rising_.push_back({ps, PlayerKey(ps), tier, cls, loc, yaw, upkeep, paid, Now() + kRiseDelay});
        runes_.Rise(world_, loc, kRuneRadius[tier], kRiseDelay);
        Hush(cls);
        std::wstring path = PathOf(cls);
        if (path.empty() || path.size() > 199) return;
        wchar_t buf[300];
        swprintf(buf, std::size(buf), L"%lsrise %d %.0f %.0f %.0f %ls", kMsg, tier, loc.x, loc.y, loc.z, path.c_str());
        ForEachPlayer(ps, [&](void* other) {
            if (void* pc = At<void*>(other, off::Actor_Owner)) SendToPlayer(pc, buf);
        });
    }

    // Host: the risings that are due come up; those that can't give their souls back.
    void RiseDue(void* world, bool fight) {
        double now = Now();
        std::vector<Rising> due;
        for (auto it = rising_.begin(); it != rising_.end();)
            if (it->at <= now || !fight) due.push_back(*it), it = rising_.erase(it);
            else ++it;
        if (due.empty()) return;
        std::unordered_set<void*> here;
        ForEachPlayer(world, [&](void* ps) { here.insert(ps); });
        void* lm = LevelManagerOf(world);
        for (const Rising& r : due)
            if (!fight || !lm || !here.count(r.ps) || !SpawnUndead(lm, r.ps, r.tier, r.cls, r.loc, r.yaw, r.upkeep))
                Refund(r);
    }

    void Refund(const Rising& r) {
        if (r.paid <= 0.f) return;
        auto it = players_.find(r.key);
        if (it == players_.end()) return;
        it->second.souls += r.paid;  // under the cap: it was there a moment ago
        it->second.dirty = true;
        CRAB_LOG("necromancer: a %ls didn't rise for %s; %g souls back", kTierNames[r.tier], r.key.c_str(), r.paid);
    }

    // The enemy's own spawn effect (EnemySpawnFX, played on the ground at BeginPlay) is left out of undead: taken
    // off the class's defaults for a few seconds around the raise, which every machine's copy is built from.
    void Hush(void* cls) {
        void* cdo = cls ? At<void*>(cls, off::Class_DefaultObject) : nullptr;
        if (!cdo || !enemyClass_ || !IsA(cdo, enemyClass_)) return;
        double until = Now() + 3.0;
        for (Hushed& h : hushed_)
            if (h.cdo == cdo) return void(h.until = until);
        void* fx = At<void*>(cdo, off::Enemy_SpawnFX);
        if (!fx) return;
        hushed_.push_back({cdo, fx, until});
        At<void*>(cdo, off::Enemy_SpawnFX) = nullptr;
    }
    void Unhush(bool all) {
        double now = Now();
        for (auto it = hushed_.begin(); it != hushed_.end();) {
            if (!all && it->until > now) {
                ++it;
                continue;
            }
            At<void*>(it->cdo, off::Enemy_SpawnFX) = it->fx;
            it = hushed_.erase(it);
        }
    }

    std::wstring PathOf(void* obj) {
        auto it = paths_.find(obj);
        if (it != paths_.end()) return it->second;
        static void* lib = ui::FindObject(L"/Script/Engine.Default__KismetSystemLibrary");
        struct {
            void* obj;
            TArray<wchar_t> ret;
        } p{obj, {}};
        std::wstring path;
        if (lib && ui::Call(lib, L"GetPathName", &p) && p.ret.Data) path = p.ret.Data;  // the engine's copy stays
        return paths_[obj] = path;
    }

    // One undead of that class at loc, owned by ps; upkeep: its share of the raise cost. null if it didn't spawn.
    void* SpawnUndead(void* lm, void* ps, int tier, void* cls, Vec3 loc, float yaw, float upkeep) {
        float rot[3] = {0.f, yaw, 0.f};
        void* e = Fn<void* (*)(void*, void*, Vec3*, float*, bool)>(va::ACrabLM_SpawnEnemy)(lm, cls, &loc, rot, false);
        if (!e) return nullptr;
        undead_[e] = {ps, tier, upkeep};
        At<uint8_t>(e, off::Enemy_bIgnoredByPlayerTargeting) = 1;
        At<float>(e, off::Enemy_ChanceToSpawnOnEliminated) = 0.f;
        // No shields for undead: the game may have rolled it the Shielded or Armored buff as it spawned.
        uint8_t& buff = At<uint8_t>(e, off::Enemy_EnemyBuff);
        if (buff == 1 || buff == 2) {
            buff = 0;
            Fn<void (*)(void*)>(va::ACrabEnemyC_OnRep_EnemyBuff)(e);
        }
        if (void* hc = At<void*>(e, off::C_HC)) {
            if (At<int>(hc, off::HC_ArmorPlates)) {  // Armored's plates, or the enemy's own
                At<int>(hc, off::HC_ArmorPlates) = 0;
                At<float>(hc, off::HC_ArmorPlateHealth) = 0.f;
                Fn<void (*)(void*)>(va::UCrabHC_OnRep_HealthInfo)(hc);
            }
            float health = undeadHealth_ * (tier == 3 ? eliteHealth_ : 1.f) *
                           (1.f + loot::Total(ps, loot::SturdyBones) / 100.f) *
                           (1.f + loot::Total(ps, loot::RingOfTheGrave) / 100.f);
            if (health != 1.f) {
                Fn<void (*)(void*, float)>(va::ACrabC_SetMaxHealth)(e, At<float>(hc, off::HC_CurrentMaxHealth) * health);
                At<float>(hc, off::HC_CurrentHealth) = At<float>(hc, off::HC_CurrentMaxHealth);  // risen whole
            }
        }
        Ghost(e);
        Vec3 where = loc;
        LocationOf(e, where);
        wchar_t buf[96];
        swprintf(buf, std::size(buf), L"%lsraised %d %.0f %.0f %.0f", kMsg, tier, where.x, where.y, where.z);
        ForEachPlayer(ps, [&](void* other) {  // the host's own machine ignores it: it knows already
            if (void* pc = At<void*>(other, off::Actor_Owner)) SendToPlayer(pc, buf);
        });
        return e;
    }

    // --- The whip's loot (host unless said) -------------------------------------------------------------------

    float MaxSouls(void* ps) const { return maxSouls_ + loot::Total(ps, loot::SoulJar); }
    // Legion.
    int MaxUndead(void* ps) const { return maxUndead_ + int(loot::Total(ps, loot::Legion) + 0.5f); }
    // Hollow Hunger: the share of upkeep left, cut again for each level, never below an eighth.
    float UpkeepShare(void* ps) const {
        float keep = 1.f - std::clamp(loot::Amount(loot::HollowHunger) / 100.f, 0.f, 1.f);
        return std::max(std::pow(keep, float(loot::Level(ps, loot::HollowHunger))), 0.125f);
    }
    // Army of the Dead's count: each undead weighs by its tier.
    float Army(void* ps) const {
        static constexpr float weight[kTiers] = {0.25f, 1.f, 2.f, 4.f};
        float n = 0.f;
        for (auto& [u, info] : undead_)
            if (info.owner == ps && info.tier >= 0 && info.tier < kTiers) n += weight[info.tier];
        return n;
    }
    // Soul Discount: hyperbolic, never below half.
    float Cost(void* ps, int tier) const {
        float cut = loot::Total(ps, loot::SoulDiscount) / 100.f;
        return cost_[tier] * (0.5f + 0.5f / (1.f + 2.f * cut));
    }
    // Long Lash and Branding Iron (every machine: the inventory replicates).
    float Reach(void* ps) const { return whipRange_ * (1.f + loot::Total(ps, loot::LongLash) / 100.f); }
    float TagRadius(void* ps) const { return tagRadius_ * (1.f + loot::Total(ps, loot::BrandingIron) / 100.f); }

    // Souls from anywhere, with Ring of Souls.
    void AddSouls(void* ps, float n) {
        if (n <= 0.f) return;
        n *= 1.f + loot::Total(ps, loot::RingOfSouls) / 100.f;
        PlayerData& d = players_[PlayerKey(ps)];
        d.souls = std::min(d.souls + n, MaxSouls(ps));
        d.dirty = true;
    }

    // true with chance p, rolled once per level.
    bool Roll(float p, int levels) {
        std::uniform_real_distribution<float> u(0.f, 1.f);
        for (int i = 0; i < levels; i++)
            if (u(rng_) < p) return true;
        return false;
    }

    // Soul Brand: the n hostile enemies nearest the dead one get its tag.
    void SpreadTag(void* from, int n) {
        Vec3 at;
        if (!LocationOf(from, at)) return;
        std::vector<std::pair<float, void*>> closest;
        for (void* e : hostiles_) {
            Vec3 p;
            if (e != from && !Tagged(e) && Alive(e) && LocationOf(e, p)) closest.push_back({DistanceSquared(p, at), e});
        }
        std::sort(closest.begin(), closest.end());
        double until = Now() + tagSeconds_;
        for (int i = 0; i < n && i < int(closest.size()); i++) tags_[closest[i].second] = until, tagsDirty_ = true;
    }

    // Phylactery: a hit that would down its holder takes their souls instead, once per island.
    void Phylactery(void* ps, void* hc, float& damage) {
        if (damage <= 0.f || !loot::Level(ps, loot::Phylactery)) return;
        float health = At<float>(hc, off::HC_CurrentHealth);
        if (damage < health || health <= 0.f) return;
        std::string key = PlayerKey(ps);
        PlayerData& d = players_[key];
        if (phylacteryUsed_.count(key) || d.souls + 1e-3f < std::max(loot::Amount(loot::Phylactery), 1.f)) return;
        phylacteryUsed_.insert(key);
        damage = 0.f;
        float max = At<float>(hc, off::HC_CurrentMaxHealth);
        At<float>(hc, off::HC_CurrentHealth) = std::max(health, max * 0.5f);
        CRAB_LOG("necromancer: %s's Phylactery spent %g souls to save them", key.c_str(), d.souls);
        d.souls = 0.f;
        d.dirty = true;
        ShowMessage(ps, L"Your Phylactery shattered and saved you!");
        if (Vec3 at; LocationOf(At<void*>(hc, off::HC_OwningC), at)) PlayAt("A_Necro_Raise_Elite_", at);
    }

    // Adds stacks of an element to a hit (FCrabDebuff: uint8 type, int32 stacks at +4).
    static constexpr uint8_t kPoison = 4;  // ECrabDebuffType
    static void AddDebuff(void* info, uint8_t type, int stacks) {
        auto& a = At<TArray<uint8_t>>(info, off::DamageInfo_Debuffs);
        for (int i = 0; a.Data && i < a.Num; i++)
            if (a.Data[i * 8] == type) {
                At<int32_t>(a.Data + i * 8, 4) += stacks;
                return;
            }
        if (a.Num == a.Max) {
            int max = a.Max ? a.Max * 2 : 2;
            auto* p = Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(a.Data, size_t(max) * 8, 0);
            if (!p) return;
            a.Data = static_cast<uint8_t*>(p);
            a.Max = max;
        }
        uint8_t* e = a.Data + a.Num++ * 8;
        std::memset(e, 0, 8);
        e[0] = type;
        At<int32_t>(e, 4) = stacks;
    }

    // Our sounds don't go through the engine, so they follow the game's Master and Gameplay volume sliders here.
    void FollowGameVolume(void* world) {
        void* gi = world ? At<void*>(world, off::World_GameInstance) : nullptr;
        void* sg = gi && IsA(gi, StaticClass(va::UCrabGI_StaticClass)) ? At<void*>(gi, off::GI_SettingsSG) : nullptr;
        if (!sg) return;
        auto unit = [](float v) { return std::clamp(v > 1.001f ? v / 100.f : v, 0.f, 1.f); };  // 0..1 or percent
        float master = At<float>(sg, off::SettingsSG_MasterVolume), gameplay = At<float>(sg, off::SettingsSG_GameplayVolume);
        mixer_.SetGameVolume(unit(master) * unit(gameplay));
        if (master != loggedVolume_[0] || gameplay != loggedVolume_[1]) {
            loggedVolume_[0] = master, loggedVolume_[1] = gameplay;
            CRAB_LOG("necromancer: following the game's volume (master %g, gameplay %g)", master, gameplay);
        }
    }

    // This machine's player (ACrabPS*), or null.
    void* LocalPlayer() const {
        void* lm = world_ ? LevelManagerOf(world_) : nullptr;
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        return c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
    }

    // A raise refused: why, on the player's screen, and its sound.
    void Deny(void* ps, const wchar_t* why) {
        ShowMessage(ps, why);
        if (void* pc = At<void*>(ps, off::Actor_Owner)) SendToPlayer(pc, std::wstring(kMsg) + L"denied");
    }

    static std::wstring Lower(const wchar_t* s) {
        std::wstring r(s);
        for (wchar_t& ch : r) ch = towlower(ch);
        return r;
    }

    void TagAround(void* ps, const Vec3& at) {
        void* c = PawnOf(ps);
        Vec3 from;
        float reach = Reach(ps) * 1.3f, radius = TagRadius(ps);
        if (!c || !LocationOf(c, from) || DistanceSquared(from, at) > reach * reach) return;
        double until = Now() + tagSeconds_;
        for (void* e : hostiles_) {
            Vec3 p;
            if (LocationOf(e, p) && DistanceSquared(p, at) <= radius * radius) tags_[e] = until, tagsDirty_ = true;
        }
    }

    // --- Tags on every machine: the host says where its tagged enemies are and for how long ("tags x y z s ..."),
    // a few times a second at most while they change; each machine finds them and brands them (necro_runes.hpp).

    void SendTags(void* world) {
        tagsDirty_ = false;
        nextTagSync_ = Now() + 0.25;
        std::wstring msg = std::wstring(kMsg) + L"tags";
        int n = 0;
        double now = Now();
        for (auto& [e, until] : tags_) {
            Vec3 p;
            if (until <= now || !Alive(e) || !LocationOf(e, p)) continue;
            wchar_t one[64];
            swprintf(one, std::size(one), L" %.0f %.0f %.0f %.1f", p.x, p.y, p.z, until - now);
            msg += one;
            if (++n == 24) break;
        }
        ForEachPlayer(world, [&](void* ps) {
            if (void* pc = At<void*>(ps, off::Actor_Owner)) SendToPlayer(pc, msg);
        });
    }

    // A player's machine: the tagged enemies are those nearest the host's spots.
    void SeeTags(const wchar_t* s) {
        if (!enemyClass_ || !world_) return;
        std::vector<void*> enemies = AllActorsOf(world_, enemyClass_);
        double now = Now();
        Vec3 at;
        float left;
        int used = 0;
        while (swscanf(s, L"%f %f %f %f%n", &at.x, &at.y, &at.z, &left, &used) == 4) {
            s += used;
            void* best = nullptr;
            float d = 250.f * 250.f;
            for (void* e : enemies) {
                Vec3 p;
                if (!marked_.count(e) && LocationOf(e, p) && DistanceSquared(p, at) < d) d = DistanceSquared(p, at), best = e;
            }
            if (best) seenTags_[best] = now + left;
        }
    }

    // Every machine: a brand under each living tagged enemy.
    void MarkTagged(void* world, bool host) {
        auto& tags = host ? tags_ : seenTags_;
        std::unordered_map<void*, float> brand;
        if (!tags.empty() && enemyClass_) {
            std::vector<void*> enemies = AllActorsOf(world, enemyClass_);
            std::unordered_set<void*> present(enemies.begin(), enemies.end());
            double now = Now();
            for (auto it = tags.begin(); it != tags.end();) {
                if (!present.count(it->first) || it->second <= now) {
                    it = host ? std::next(it) : tags.erase(it);  // the host's own list keeps its own rules
                    continue;
                }
                if (Alive(it->first) && !IsUndead(it->first)) {
                    static const float size[] = {110.f, 110.f, 140.f, 170.f, 220.f, 320.f};
                    uint8_t cat = At<uint8_t>(it->first, off::Enemy_Category);
                    brand[it->first] = size[cat < 6 ? cat : 2];
                }
                ++it;
            }
        }
        runes_.Brands(world, brand);
    }

    // Kills an undead the ordinary way (its death animation and effects), dropping nothing.
    void Crumble(void* u) {
        void* hc = At<void*>(u, off::C_HC);
        if (!hc || !Alive(u)) {
            undead_.erase(u);
            return;
        }
        alignas(8) uint8_t info[off::DamageInfo_Size] = {};
        At<void*>(info, off::DamageInfo_DamagedActor) = u;
        At<float>(info, off::DamageInfo_Damage) = At<float>(hc, off::HC_CurrentHealth) + At<float>(hc, off::HC_CurrentMaxHealth) + 1e6f;
        LocationOf(u, At<Vec3>(info, off::DamageInfo_Location));
        Fn<void (*)(void*, void*)>(va::UCrabHC_TakeDamage)(hc, info);
        if (Alive(u)) {
            CRAB_LOG("necromancer: an undead didn't die when told; removed");
            ui::Call(u, L"K2_DestroyActor");
        }
        undead_.erase(u);
    }

    // --- Kinds of enemy -------------------------------------------------------------------------------------

    enum KindBits { kBarrel = 1, kSnail = 2 };
    int KindOf(void* e) {
        void* cls = At<void*>(e, off::Object_Class);
        auto it = kinds_.find(cls);
        if (it != kinds_.end()) return it->second;
        std::string name = ObjectName(cls);
        int k = (name.find("Barrel") != std::string::npos ? kBarrel : 0) |
                (name.find("ImmortalSnail") != std::string::npos ? kSnail : 0);
        return kinds_[cls] = k;
    }
    // The Immortal Snail hunts players only: undead leave it be, and it them.
    bool IsSnail(void* e) { return e && (KindOf(e) & kSnail); }
    // Kinds that make poor undead don't join the raise list: barrel skulls, the snail, and the ones that blow
    // themselves up (slugs, pufferfish).
    bool CanRaise(void* e) { return !(KindOf(e) & (kBarrel | kSnail)) && !(At<uint8_t>(e, off::Enemy_bIsExplosiveEnemy) & 1); }

    // --- Nests (host) --------------------------------------------------------------------------------------
    //
    // Nests and spawners are destructibles, not enemies, so the enemy AI can't aim at them. An undead with no enemy
    // within nestRange walks to the nearest one, and hurts it while next to it.

    static bool IsNest(void* d) {
        return At<int32_t>(d, off::Destructible_NestEnemies + 8) > 0 || At<int32_t>(d, off::Destructible_ChallengeEnemies + 8) > 0;
    }
    static bool Standing(void* d) {
        void* hc = At<void*>(d, off::Destructible_HC);
        return hc && !(At<uint8_t>(d, off::Destructible_bExploded) & 1) && At<float>(hc, off::HC_CurrentHealth) > 0.f;
    }
    bool IsNestActor(void* d) const { return std::find(nests_.begin(), nests_.end(), d) != nests_.end(); }

    void HitNests(void* world) {
        double now = Now();
        if (now >= nextNests_) {
            nextNests_ = now + 1.0;
            nests_.clear();
            if (void* cls = StaticClass(va::ACrabDestructible_StaticClass))
                for (void* d : AllActorsOf(world, cls))
                    if (IsNest(d) && Standing(d)) nests_.push_back(d);
        }
        if (nests_.empty() || nestDps_ <= 0.f) return;
        float dt = 0.25f;  // HostTick runs at the game's tick; damage is dealt four times a second
        if (now < nextNestHit_) return;
        nextNestHit_ = now + dt;
        for (auto& [u, info] : undead_) {
            Vec3 at;
            if (!Alive(u) || !LocationOf(u, at)) continue;
            // An enemy near enough to fight comes first.
            bool busy = false;
            for (void* e : hostiles_) {
                Vec3 p;
                if (Alive(e) && LocationOf(e, p) && DistanceSquared(at, p) < nestRange_ * nestRange_) { busy = true; break; }
            }
            if (busy) continue;
            void* best = nullptr;
            float d = nestRange_ * nestRange_;
            for (void* n : nests_) {
                Vec3 p;
                if (Standing(n) && LocationOf(n, p) && DistanceSquared(at, p) < d) d = DistanceSquared(at, p), best = n;
            }
            if (!best) continue;
            void* aic = At<void*>(u, off::Enemy_OwningAIC);
            if (std::sqrt(d) > 350.f) {
                if (aic) {
                    struct {
                        void* goal;
                        float radius;
                        bool stopOnOverlap, pathfinding, strafe;
                        void* filter;
                        bool partial;
                        uint8_t result;
                    } m{best, 150.f, true, true, false, nullptr, true, 0};
                    ui::Call(aic, L"MoveToActor", &m);
                }
                continue;
            }
            void* hc = At<void*>(best, off::Destructible_HC);
            alignas(8) uint8_t hit[off::DamageInfo_Size] = {};
            At<void*>(hit, off::DamageInfo_DamagedActor) = best;
            At<void*>(hit, off::DamageInfo_DamagingC) = u;
            float m = undeadDamage_ * (info.tier == 3 ? eliteDamage_ : 1.f) * (1.f + loot::Total(info.owner, loot::RingOfTheGrave) / 100.f);
            At<float>(hit, off::DamageInfo_Damage) = nestDps_ * m * dt;
            LocationOf(best, At<Vec3>(hit, off::DamageInfo_Location));
            if (void* owner = info.owner ? PawnOf(info.owner) : nullptr) At<void*>(best, off::Destructible_LastPlayer) = owner;
            Fn<void (*)(void*, void*)>(va::UCrabHC_TakeDamage)(hc, hit);
        }
    }

    // --- Targeting (host) ----------------------------------------------------------------------------------

    void* PickForUndead(void* u) const {
        Vec3 at;
        if (!LocationOf(u, at)) return nullptr;
        void *best = nullptr, *bestTagged = nullptr;
        float d = 1e30f, dt = 1e30f;
        for (void* e : hostiles_) {
            Vec3 p;
            if (!Alive(e) || !LocationOf(e, p)) continue;
            float s = DistanceSquared(at, p);
            if (s < d) d = s, best = e;
            if (s < dt && Tagged(e)) dt = s, bestTagged = e;
        }
        return bestTagged ? bestTagged : best;
    }

    // The nearest undead to a hostile enemy, if it is aggroBias times closer than the nearest player.
    void* UndeadInReach(void* e) const {
        Vec3 at;
        if (!LocationOf(e, at)) return nullptr;
        void* best = nullptr;
        float du = 1e30f, dp = 1e30f;
        for (auto& [u, info] : undead_) {
            Vec3 p;
            if (Alive(u) && LocationOf(u, p) && DistanceSquared(at, p) < du) du = DistanceSquared(at, p), best = u;
        }
        ForEachPlayer(e, [&](void* ps) {
            Vec3 p;
            if (void* c = PawnOf(ps); c && Alive(c) && LocationOf(c, p)) dp = std::min(dp, DistanceSquared(at, p));
        });
        return best && std::sqrt(du) * aggroBias_ < std::sqrt(dp) ? best : nullptr;
    }

    static void Retarget(void* aic, void* c, void* target) {
        At<void*>(aic, off::EnemyAIC_BestEnemyC) = target;
        Fn<void (*)(void*, void*)>(va::ACrabEnemyC_SetBestEnemy)(c, target);
        Fn<void (*)(void*)>(va::ACrabEnemyAIC_FocusBestEnemy)(aic);
    }

    // --- Looks (every machine) -----------------------------------------------------------------------------

    // A pale, glowing ghost of the enemy. Each kind's material names its colors its own way (crabs SkinTintA,
    // skulls Tint_Inner, grubs Body_Tint, ...), so all of them are set, on every material of the body; a name a
    // material doesn't have is ignored.
    static void Ghost(void* e) {
        if (void* mesh = At<void*>(e, off::Character_Mesh)) {
            static const wchar_t* const light[] = {L"SkinTintA", L"Tint", L"Tint_Outer", L"Base_Tint", L"Body_Tint",
                                                   L"BodyTint", L"PrimaryTint", L"AltTint", L"Emissive_Tint",
                                                   L"CrystalFresnel_Tint"};
            static const wchar_t* const dark[] = {L"SkinTintB", L"Tint_Inner", L"SecondaryTint", L"StemTint"};
            static const wchar_t* const glow[] = {L"Emissive_Brightness", L"Brightness", L"Emissive_Amount",
                                                  L"CrystalFresnel_Amount", L"Fresnel_Brightness"};
            struct {
                uint64_t name;
                Vec3 color;
            } v{};
            v.color = {0.35f, 0.95f, 0.75f};
            for (const wchar_t* p : light) v.name = ui::Name(p), ui::Call(mesh, L"SetVectorParameterValueOnMaterials", &v);
            v.color = {0.1f, 0.45f, 0.4f};
            for (const wchar_t* p : dark) v.name = ui::Name(p), ui::Call(mesh, L"SetVectorParameterValueOnMaterials", &v);
            v.color = {0.6f, 1.f, 0.85f};
            v.name = ui::Name(L"Eyes_Tint");
            ui::Call(mesh, L"SetVectorParameterValueOnMaterials", &v);
            struct {
                uint64_t name;
                float value;
            } s{0, 2.f};
            for (const wchar_t* p : glow) s.name = ui::Name(p), ui::Call(mesh, L"SetScalarParameterValueOnMaterials", &s);
        }
        void* dmi = At<void*>(e, off::Enemy_MeshDMI);
        if (!dmi) return;
        struct {
            uint64_t name;
            ui::LinearColor color;
        } v{};
        v.color = {0.35f, 0.95f, 0.75f, 1.f};
        v.name = ui::Name(L"SkinTintA");
        ui::Call(dmi, L"SetVectorParameterValue", &v);
        v.color = {0.1f, 0.45f, 0.4f, 1.f};
        v.name = ui::Name(L"SkinTintB");
        ui::Call(dmi, L"SetVectorParameterValue", &v);
        struct {
            uint64_t name;
            float value;
        } s{ui::Name(L"Emissive_Brightness"), 2.f};
        ui::Call(dmi, L"SetScalarParameterValue", &s);
    }

    // Players' machines: the enemies the host said it raised.
    void MarkRaised(void* world) {
        if (!enemyClass_ || (pending_.empty() && marked_.empty())) return;
        std::vector<void*> enemies = AllActorsOf(world, enemyClass_);
        std::unordered_set<void*> present(enemies.begin(), enemies.end());
        for (auto it = marked_.begin(); it != marked_.end();) it = present.count(*it) ? std::next(it) : marked_.erase(it);
        for (auto it = crumbled_.begin(); it != crumbled_.end();) it = marked_.count(*it) ? std::next(it) : crumbled_.erase(it);
        for (void* e : marked_)  // an undead falling, as the host's machine heard it
            if (!Alive(e) && crumbled_.insert(e).second)
                if (Vec3 at; LocationOf(e, at)) PlayAt("A_Undead_Crumble_", at);
        double now = Now();
        if (now >= nextMarkGhost_) {
            nextMarkGhost_ = now + 1.0;
            for (void* e : marked_)
                if (Alive(e)) Ghost(e);
        }
        for (auto it = pending_.begin(); it != pending_.end();) {
            void* best = nullptr;
            float d = 400.f * 400.f;
            for (void* e : enemies) {
                Vec3 p;
                if (!marked_.count(e) && LocationOf(e, p) && DistanceSquared(p, it->at) < d) d = DistanceSquared(p, it->at), best = e;
            }
            if (best) {
                marked_.insert(best);
                At<uint8_t>(best, off::Enemy_bIgnoredByPlayerTargeting) = 1;
                Ghost(best);
            }
            it = best || it->until < now ? pending_.erase(it) : std::next(it);
        }
    }

    // --- HUD (the player's machine) ------------------------------------------------------------------------
    //
    // While the whip is held, the ability's widgets are hidden. Bottom right, left of the melee slot, the raise
    // panel: a picture of each tier's enemy with its rank marks above and its key and cost below, bright when you can
    // raise it. Bottom left, above the health bar, the soul bar: a soul, the count, and the bar with a notch at
    // each raise cost. Positions are the HUD's 1920 x 1080 layout, from its bottom corners.

    static constexpr float kBarX = 38.f, kBarY = -105.f, kBarW = 333.f, kBarH = 7.f;
    // The raise panel ends left of the melee slot (which starts about 335 from the right edge).
    static constexpr float kTierRight = -392.f, kTierStep = 58.f, kTierY = -80.f, kIcon = 46.f;
    static constexpr float kMarkY = -33.f, kKeyY = 34.f;  // the rank marks above an icon, the key and cost below

    struct TierWidgets {
        void* icon = nullptr;
        void* key = nullptr;
        void* cost = nullptr;
        std::vector<void*> marks;  // the rank marks above
    };

    void UpdateHud(void* world) {
        void* lm = LevelManagerOf(world);
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        if (!IsA(c, StaticClass(va::ACrabPlayerC_StaticClass))) return;
        void* ps = At<void*>(c, off::Pawn_PlayerState);
        void* pc = At<void*>(c, off::Pawn_Controller);
        void* hud = pc ? At<void*>(pc, off::PC_HUD) : nullptr;
        bool holds = Holds(ps);
        if (hud != hud_) {
            hud_ = hud;
            widgets_.clear();
            abilityHidden_ = false;
        }
        if (!hud) return;
        if (holds != abilityHidden_) {
            const int widgets[] = {off::GameplayUI_AbilityKeyText, off::GameplayUI_AbilityIcon,
                                   off::GameplayUI_AbilityCooldownText, off::GameplayUI_AbilityStackText};
            for (int i = 0; i < 4; i++) {
                void* w = At<void*>(hud, widgets[i]);
                if (!w) continue;
                if (holds) {
                    abilityVis_[i] = ui::SelfHitTestInvisible;
                    ui::Call(w, L"GetVisibility", &abilityVis_[i]);
                }
                ui::SetVisibility(w, holds ? ui::Hidden : ui::Visibility(abilityVis_[i]));
            }
            abilityHidden_ = holds;
        }
        if (widgets_.empty() && holds && !BuildHud(hud)) return;
        for (void* w : widgets_) ui::SetVisibility(w, holds ? ui::HitTestInvisible : ui::Hidden);
        if (!holds || widgets_.empty()) return;

        float max = view_.max > 0.f ? view_.max : 100.f;
        float souls = std::clamp(view_.souls, 0.f, max);
        bool full = souls >= max - 1e-3f;
        wchar_t buf[64];
        swprintf(buf, std::size(buf), L"%d / %d", int(souls), int(max));
        ui::SetText(soulText_, view_.known ? buf : L"-");
        SetSize(barFill_, kBarW * souls / max, kBarH);
        Tint(barFill_, full ? ui::LinearColor{0.78f, 0.62f, 1.f, 1.f} : ui::LinearColor{0.55f, 0.38f, 1.f, 1.f});
        Tint(soulIcon_, full ? ui::LinearColor{0.85f, 0.75f, 1.f, 1.f} : ui::LinearColor{0.68f, 0.55f, 1.f, 1.f});
        for (int t = 0; t < kTiers; t++) {
            float cost = view_.cost[t];
            if (notch_[t]) {
                ui::SetVisibility(notch_[t], cost > 0.f && cost < max ? ui::HitTestInvisible : ui::Hidden);
                SetPosition(notch_[t], kBarX + kBarW * std::clamp(cost / max, 0.f, 1.f), kBarY);
            }
            TierWidgets& w = tiers_[t];
            bool known = !view_.names[t].empty();
            bool ready = known && souls + 1e-3f >= cost;
            ui::LinearColor look = !known ? ui::LinearColor{0.55f, 0.55f, 0.6f, 0.3f}
                                   : ready ? ui::LinearColor{1.f, 1.f, 1.f, 1.f}
                                           : ui::LinearColor{0.8f, 0.8f, 0.85f, 0.5f};
            Tint(w.icon, look);
            static const ui::LinearColor rank[kTiers] = {
                {0.45f, 0.9f, 0.5f, 1.f}, {0.45f, 0.7f, 1.f, 1.f}, {0.75f, 0.5f, 1.f, 1.f}, {1.f, 0.85f, 0.4f, 1.f}};
            for (void* m : w.marks) Tint(m, ready ? rank[t] : ui::LinearColor{0.7f, 0.7f, 0.75f, 0.45f});
            swprintf(buf, std::size(buf), L"%d", int(cost));
            ui::SetText(w.cost, known ? buf : L"-");
            float opacity = ready ? 1.f : known ? 0.6f : 0.35f;
            ui::Call(w.key, L"SetOpacity", &opacity);
            ui::Call(w.cost, L"SetOpacity", &opacity);
        }
    }

    bool BuildHud(void* hud) {
        void* tree = At<void*>(hud, off::UserWidget_WidgetTree);
        void* root = tree ? At<void*>(tree, off::WidgetTree_RootWidget) : nullptr;
        if (!root) return false;
        for (TierWidgets& t : tiers_) t = {};
        ui::Font font{}, medium{}, small{};
        void* crystals = At<void*>(hud, 0x340 /* CrystalsText */);
        bool haveFont = ui::FontOf(crystals, 24, font) && ui::FontOf(crystals, 19, medium) &&
                        ui::FontOf(crystals, 12, small);
        auto text = [&](float ax, float x, float y, float alignX, const ui::Font& f) {
            void* t = Place(hud, root, va::UTextBlock_StaticClass, ax, x, y, alignX, 0.5f, 0.f, 0.f);
            if (t && haveFont) ui::SetFont(t, f);
            return t;
        };
        auto image = [&](float ax, float x, float y, float alignX, float alignY, float w, float h, void* texture) {
            void* i = Place(hud, root, va::UImage_StaticClass, ax, x, y, alignX, alignY, w, h);
            if (i && texture) {
                struct {
                    void* texture;
                    bool matchSize;
                } b{texture, false};
                ui::Call(i, L"SetBrushFromTexture", &b);
            }
            return i;
        };

        // The soul bar, bottom left.
        void* trough = image(0.f, kBarX, kBarY, 0.f, 0.5f, kBarW, kBarH, nullptr);
        if (!trough) {
            CRAB_LOG("necromancer: the HUD's root isn't a canvas (%s); no soul display", ObjectName(root).c_str());
            widgets_.clear();
            return false;
        }
        Tint(trough, {0.42f, 0.37f, 0.36f, 0.85f});
        barFill_ = image(0.f, kBarX, kBarY, 0.f, 0.5f, kBarW, kBarH, nullptr);
        for (int t = 0; t < kTiers; t++) {
            notch_[t] = image(0.f, kBarX, kBarY, 0.5f, 0.5f, 3.f, kBarH + 4.f, nullptr);
            Tint(notch_[t], {0.93f, 0.9f, 1.f, 0.9f});
        }
        soulIcon_ = image(0.f, kBarX + 14.f, kBarY - 24.f, 0.5f, 0.5f, 30.f, 30.f, texSoul_);
        soulText_ = text(0.f, kBarX + 36.f, kBarY - 23.f, 0.f, font);

        // The raise panel, bottom right.
        for (int t = 0; t < kTiers; t++) {
            TierWidgets& w = tiers_[t];
            float x = kTierRight - kTierStep * (kTiers - 1 - t);
            w.icon = image(1.f, x, kTierY, 0.5f, 0.5f, kIcon, kIcon, texTier_[t]);
            if (t < 3) {
                for (int m = 0; m <= t; m++)
                    w.marks.push_back(image(1.f, x + (m - t / 2.f) * 11.f, kTierY + kMarkY, 0.5f, 0.5f, 9.f, 9.f, texPip_));
            } else {
                w.marks.push_back(image(1.f, x, kTierY + kMarkY, 0.5f, 0.5f, 18.f, 18.f, texCrown_));
            }
            std::wstring key = Widen(keyNames_[t]);
            if (!texTier_[t]) key += L" " + std::wstring(kTierNames[t]);  // no picture: say which
            w.key = text(1.f, x - 1.f, kTierY + kKeyY, 1.f, medium);
            ui::SetText(w.key, key.c_str());
            w.cost = text(1.f, x + 2.f, kTierY + kKeyY + 3.f, 0.f, small);
        }
        CRAB_LOG("necromancer: soul bar and raise panel added to the HUD");
        return true;
    }

    // A widget in the HUD's canvas, anchored to the bottom left (anchorX 0) or bottom right (1) corner; w = 0:
    // sized to its content.
    void* Place(void* hud, void* root, uintptr_t cls, float anchorX, float x, float y, float alignX, float alignY,
                float w, float h) {
        void* widget = ui::NewWidget(hud, cls);
        void* slot = widget ? ui::AddToCanvas(root, widget) : nullptr;
        if (!slot) {
            if (widget) ui::RemoveFromParent(widget);
            return nullptr;
        }
        float anchors[4] = {anchorX, 1.f, anchorX, 1.f};
        ui::Call(slot, L"SetAnchors", anchors);
        float align[2] = {alignX, alignY};
        ui::Call(slot, L"SetAlignment", align);
        bool autoSize = w <= 0.f;
        ui::Call(slot, L"SetAutoSize", &autoSize);
        widgets_.push_back(widget);
        SetPosition(widget, x, y);
        if (!autoSize) SetSize(widget, w, h);
        return widget;
    }
    static void SetPosition(void* widget, float x, float y) {
        float pos[2] = {x, y};
        ui::Call(ui::SlotOf(widget), L"SetPosition", pos);
    }
    static void SetSize(void* widget, float w, float h) {
        float size[2] = {std::max(w, 0.f), h};
        ui::Call(ui::SlotOf(widget), L"SetSize", size);
    }
    static void Tint(void* image, ui::LinearColor c) {
        if (image) ui::Call(image, L"SetColorAndOpacity", &c);
    }

    // --- Hooks ---------------------------------------------------------------------------------------------

    using BuildListsFn = void (*)(void*);
    using CanDamageFn = bool (*)(void*, void*);
    using SelectTargetFn = void (*)(void*);
    using AbilityPressedFn = void (*)(void*);
    static inline BuildListsFn oBuildLists;
    static inline CanDamageFn oCanDamage;
    static inline SelectTargetFn oSelectTarget;
    static inline AbilityPressedFn oAbilityPressed;
    static inline Necromancer* self_;

    // Before the lobby's lists are built from SpawnablesDA: the whip joins its weapons.
    static void HkBuildLists(void* gi) {
        self_->EnsureWhip();
        oBuildLists(gi);
    }

    static bool HkCanDamage(void* info, void* victim) {
        Necromancer& self = *self_;
        if (info && victim && (!self.undead_.empty() || !self.marked_.empty())) {
            void* src = At<void*>(info, off::DamageInfo_DamagingC);
            bool su = self.IsUndead(src), vu = self.IsUndead(victim);
            if ((su || vu) && src != victim) {
                if (su && vu) return false;
                void* other = su ? victim : src;
                if (!other) return oCanDamage(info, victim);
                if (At<void*>(other, off::Pawn_PlayerState)) return false;  // players and undead leave each other be
                if (su && self.IsNestActor(other)) return true;             // undead break nests
                if (self.IsSnail(other)) return false;                      // the snail is the players' problem
                return IsA(other, self.enemyClass_);                        // undead and hostile enemies fight
            }
        }
        return oCanDamage(info, victim);
    }

    static void HkSelectTarget(void* aic) {
        oSelectTarget(aic);
        Necromancer& self = *self_;
        if (!aic || self.undead_.empty()) return;
        void* c = At<void*>(aic, off::EnemyAIC_OwnedC);
        if (!c) return;
        if (self.undead_.count(c)) Retarget(aic, c, self.PickForUndead(c));
        else if (self.IsSnail(c)) return;
        else if (void* u = self.UndeadInReach(c)) Retarget(aic, c, u);
    }

    // The whip replaces the ability: its action does nothing, whatever key it's on (the raise keys raise). Each
    // player's own machine fires their ability, so it's stopped there, before any charge or RPC.
    static void HkAbilityPressed(void* c) {
        if (c && self_->enabled_ && self_->Holds(At<void*>(c, off::Pawn_PlayerState))) return;
        oAbilityPressed(c);
    }

    // The game's own ability mods do nothing while the whip is held: they don't drop for its holders.
    float LootWeight(void* ps, void* da) override {
        if (!da || !Holds(ps) || loot::IsNecroItem(da)) return 1.f;
        return IsA(da, StaticClass(va::UCrabAbilityModDA_StaticClass)) ? 0.f : 1.f;
    }

    // Settings
    bool enabled_ = true;
    float whipDps_ = 100.f, whipFireRate_ = 1.25f, whipRange_ = 4500.f, burstRadius_ = 400.f;
    float procRoll_ = 3.f;  // WeaponModTriggerRollMultiplier (the Sniper's is 7)
    float tagRadius_ = 600.f, tagSeconds_ = 8.f, tagDamage_ = 0.25f, taggedSouls_ = 0.5f;
    float maxSouls_ = 100.f;
    float souls_[6] = {0.f, 1.f, 3.f, 6.f, 25.f, 60.f};  // by EnemyCategory
    float cost_[kTiers] = {10.f, 15.f, 40.f, 75.f};
    int fodderPack_ = 3;
    float refund_ = 0.5f;  // share of a surviving undead's cost given back when the fight ends
    float raiseRange_ = 4000.f, upkeep_ = 0.01f, undeadDamage_ = 1.f, undeadHealth_ = 2.f, starve_ = 0.04f, eliteHealth_ = 1.f, eliteDamage_ = 0.5f;
    float aggroBias_ = 1.5f, nestRange_ = 2500.f, nestDps_ = 30.f;
    int maxUndead_ = 20;            // per player, before Legion
    float ownerDamage_ = 0.5f;      // share of the owner's damage bonus their undead get
    float undeadKillSouls_ = 0.5f;  // share of a kill's souls when an undead lands it
    std::string keyNames_[kTiers];
    int keys_[kTiers] = {};

    // State
    void* whip_ = nullptr;    // DA_Weapon_SoulWhip
    void* world_ = nullptr;   // this frame's
    double nextPedestalCheck_ = 0.0;
    int pedestalTries_ = 0;
    bool wasLobby_ = false;
    std::string pedestalWhy_;
    void* stone_ = nullptr;  // our copy of the stone (compared, never touched unless still in the world)
    bool stoneWarned_ = false;
    std::vector<void*> pedestal_;  // the stone and pickup we added to the lobby, to remove when it's gone
    bool artTried_ = false;
    necro::Pack pack_;
    necro::Mixer mixer_;
    static constexpr int kPulseSteps = 8;  // 0..6 the glow's breathing, 7 a crack's flare (pulse_N.png)
    void* pulse_[kPulseSteps] = {};
    void* mid_ = nullptr;  // the whip's material (rooted through its meshes); also keeps the HUD's textures
    Vec3 lashAnchor_{}, lashDir_{0, 0, 1};  // where the lash leaves the handle, in the hand socket's space
    std::vector<float> segLength_;
    std::vector<void*> segMesh_;
    std::unordered_map<void*, LashState> lashes_;  // whip weapon -> its lash
    std::unordered_map<void*, ShowState> shows_;   // whip pickup -> the whip shown on it
    void *texSoul_ = nullptr, *texPip_ = nullptr, *texCrown_ = nullptr, *texTier_[kTiers] = {};
    std::unordered_set<void*> crumbled_;  // players' machines: undead already heard falling
    void* lastHeld_ = nullptr;
    double lastRaiseSound_[kTiers] = {}, lastSoulSound_ = 0.0;
    std::mt19937 rng_{std::random_device{}()};
    void* projDA_ = nullptr;  // DA_Projectile_SoulWhip
    static constexpr float kBoltSpeed = 9000.f;  // cm/s
    void* enemyClass_ = nullptr;
    std::unordered_map<std::string, PlayerData> players_;  // host: by player name, for the run
    std::unordered_map<void*, UndeadInfo> undead_;         // host: the undead in this level
    std::unordered_map<void*, double> tags_;               // host: tagged enemy -> until
    std::unordered_map<void*, void*> lastMelee_;           // host: enemy -> the player whose claw hit it last
    std::unordered_map<void*, double> siphoned_;           // host: enemy -> when Soul Siphon last paid for it
    std::vector<Hidden> hidden_;                           // host: perk lists hidden during an undead's kill
    struct Harvest {
        void* ps;
        int tier;
        void* cls;
        Vec3 at;
    };
    std::vector<Harvest> harvest_;                         // host: Harvester's free raises, for the next tick
    static constexpr float kRiseDelay = 0.35f;              // from the circle to the undead coming up, seconds
    static constexpr float kRuneRadius[kTiers] = {120.f, 160.f, 200.f, 260.f};  // the circle's, per tier, cm
    std::vector<Rising> rising_;                           // host: undead about to come up
    struct Hushed {
        void* cdo;  // an enemy class's defaults
        void* fx;   // its EnemySpawnFX, to put back
        double until;
    };
    std::vector<Hushed> hushed_;                           // every machine: spawn effects left out for a raise
    std::unordered_map<void*, std::wstring> paths_;        // host: enemy class -> its path, for "rise"
    necro::Runes runes_;                                   // every machine: the circles and brands
    std::unordered_map<void*, double> seenTags_;           // players' machines: tagged enemies, as the host said
    bool tagsDirty_ = false;                               // host: tags_ changed since last sent
    double nextTagSync_ = 0.0;
    std::unordered_set<std::string> phylacteryUsed_;       // host: players whose Phylactery went off this island
    bool fightOver_ = false;
    float loggedVolume_[2] = {-1.f, -1.f};                               // host: crumbling everyone at the fight's end
    std::vector<void*> hostiles_;                          // host: living enemies that aren't undead, this tick
    std::unordered_set<void*> marked_;                     // players' machines: enemies the host raised
    std::vector<Pending> pending_;                         // raised, not yet matched to an enemy here
    int tick_ = 0;
    double nextGhost_ = 0.0, nextMarkGhost_ = 0.0, nextNests_ = 0.0, nextNestHit_ = 0.0;
    std::vector<void*> nests_;                             // host: nests and spawners, refreshed once a second
    std::unordered_map<void*, int> kinds_;                 // enemy class -> Kind bits, by its name
    double lastTagSent_ = 0.0;
    bool keyDown_[kTiers] = {};
    View view_;
    void* hud_ = nullptr;
    std::vector<void*> widgets_;  // all of ours in it
    void *barFill_ = nullptr, *soulIcon_ = nullptr, *soulText_ = nullptr, *notch_[kTiers] = {};
    TierWidgets tiers_[kTiers];
    bool abilityHidden_ = false;
    uint8_t abilityVis_[4] = {4, 4, 4, 4};  // their visibility before the whip hid them
};

Necromancer g_item;

}  // namespace

bool HoldsSoulWhip(void* ps) { return Necromancer::HoldsWhip(ps); }

}  // namespace crabmods
