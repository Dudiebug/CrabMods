// CrabMods: new items for Crab Champions.
//
// The game has none of these assets, so they are made at runtime as copies of its own (see AssetItem and
// MakeDA) and added to the loot lists, so chests roll them like any other pickup. Their effects come from the
// hooks below, which pass each game event to every item (crabmods/items/*.cpp):
//   ACrabLM::BeginPlay      level start: items make their assets and join the loot lists
//   ACrabPS::GetDamageBuff  the player's damage multiplier: items add their damage bonus
//   UCrabHC::TakeDamage     a character is hit
//   CalcOutgoingDamage      a player's hit, after their buffs, crits and elements
//   island-end rewards      the chests each player gets
//   LootRoll                every loot roll: items scale how likely each pickup is, per player
//   SpawnPortals            the next islands' rewards
//   ability cooldown tick   every 0.25 s while a player's ability recharges
//   SpawnSpectator          a player going down
//   destructible hits       a player breaking a pot, barrel, rock, ...
//   weapon fire             where each shot of a player's weapon starts and where it flies
//   heals                   healing lost to the max health cap
//   GetBuff lookups         max health and fire rate, as the game recomputes them
//   UGameEngine::Tick       four times a second
//   PostLogin, ClientMessage the host's settings, sent to every player who joins, and items' messages
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cwchar>
#include <random>
#include <unordered_map>
#include <vector>

#include <crab/hook.hpp>
#include <crab/log.hpp>
#include <crab/ue.hpp>

#include "config.hpp"
#include "item.hpp"
#include "world.hpp"

using namespace crab;
using namespace crabmods;

namespace {

constexpr const char kVersion[] = "0.8.2";

std::vector<Item*> g_items;  // enabled items, in ini order

template <class F>
void ForEachEnabled(F&& f) {
    for (Item* i : g_items)
        if (i->Enabled()) f(i);
}

// --- Level start -------------------------------------------------------------------------------------------

using BeginPlayFn = void (*)(void*);
BeginPlayFn oBeginPlay;

void HkBeginPlay(void* lm) {
    oBeginPlay(lm);
    ForEachEnabled([&](Item* i) { i->OnBeginPlay(lm); });
}

// --- Damage ------------------------------------------------------------------------------------------------

using DamageBuffFn = float (*)(void*);
DamageBuffFn oDamageBuff;

// What was last logged per player, so the log shows changes rather than every shot. Bonuses that change
// continuously (health, time) are logged at most every few seconds.
struct LoggedBonus {
    std::string names, text;
    std::chrono::steady_clock::time_point at;
};
std::unordered_map<void*, LoggedBonus> g_logged;

void LogBonus(void* ps, const std::string& names, const std::string& text, float buff) {
    auto& last = g_logged[ps];
    auto now = std::chrono::steady_clock::now();
    if (text == last.text) return;
    if (names == last.names && now - last.at < std::chrono::seconds(5)) return;
    last = {names, text, now};
    CRAB_LOG("player %p: %s damage (buff now %.2f)", ps, text.empty() ? "no bonus" : text.c_str(), buff);
}

// The bonus goes into DamageMultiplier for the length of the call, so it is added alongside the game's own
// damage perks (Collector included) and scaled by the same multipliers.
float HkDamageBuff(void* ps) {
    if (!ps) return oDamageBuff(ps);
    float bonus = 0.f;
    std::string names, text;
    ForEachEnabled([&](Item* i) {
        float b = i->DamageBonus(ps);
        if (b == 0.f) return;
        bonus += b;
        names += std::string(i->Section()) + " ";
        char part[64];
        snprintf(part, sizeof part, "%s%s %+.0f%%", text.empty() ? "" : ", ", i->Section(), b * 100.f);
        text += part;
    });
    float& mult = At<float>(ps, off::PS_DamageMultiplier);
    float saved = mult;
    mult += bonus;
    float r = oDamageBuff(ps);
    mult = saved;
    if (bonus != 0.f || g_logged.count(ps)) LogBonus(ps, names, text, r);
    return r;
}

using TakeDamageFn = void (*)(void*, void*);
TakeDamageFn oTakeDamage;

void HkTakeDamage(void* hc, void* info) {
    if (hc && info) ForEachEnabled([&](Item* i) { i->OnTakeDamage(hc, info); });
    oTakeDamage(hc, info);
}

using OutgoingDamageFn = void (*)(void*, void*);
OutgoingDamageFn oOutgoingDamage;

void HkOutgoingDamage(void* ps, void* info) {
    oOutgoingDamage(ps, info);
    if (ps && info) ForEachEnabled([&](Item* i) { i->OnOutgoingDamage(ps, info); });
}

// --- Loot --------------------------------------------------------------------------------------------------

using LootRollFn = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
using GetPickupsFn = void* (*)(void*, void*);
using WeightedPickFn = int (*)(TArray<float>*);
LootRollFn oLootRoll;
GetPickupsFn oGetPickups;
WeightedPickFn oWeightedPick;

int g_rollDepth = 0;            // inside LootRoll
void* g_rollSource = nullptr;   // LootRoll's world context
uint8_t g_rollPool = pool::None;  // LootRoll's loot pool
std::unordered_map<void*, float> g_baseWeight;  // SpawnWeights changed during this roll, to restore after
std::unordered_map<void*, uint8_t> g_basePool;  // LootPools changed during this roll, to restore after

// The chests of one loot pool, the ones an item can add pickups of another pool to.
bool RegularPool(uint8_t p) { return p >= pool::Damage && p <= pool::Skill; }
std::minstd_rand g_rng{std::random_device{}()};

// Sets every pickup's SpawnWeight for the player LootRoll is choosing for: its base weight times each item's
// LootWeight. The roll reads the weights right after asking the player what they hold, and then keeps the
// pickups whose LootPool (also read from the asset) is the chest's: a pickup an item adds to another pool's
// chest (OtherChestWeight) gets the chest's pool for the rest of the roll.
void ApplyWeights(void* ps) {
    void* lm = LevelManagerOf(g_rollSource);
    if (!lm) return;
    for (auto& [da, p] : g_basePool) At<uint8_t>(da, off::DA_LootPool) = p;  // as the items expect them
    g_basePool.clear();
    std::vector<std::pair<void*, uint8_t>> moves;
    auto apply = [&](TArray<void*>* list) {
        if (!list || !list->Data) return;
        for (int i = 0; i < list->Num; i++) {
            void* da = list->Data[i];
            if (!da) continue;
            auto [it, fresh] = g_baseWeight.try_emplace(da, At<float>(da, off::DA_SpawnWeight));
            float w = it->second;
            uint8_t p = At<uint8_t>(da, off::DA_LootPool);
            float other = 0.f;
            if (RegularPool(g_rollPool) && RegularPool(p) && p != g_rollPool)
                ForEachEnabled([&](Item* item) { other = std::max(other, item->OtherChestWeight(ps, da)); });
            if (other > 0.f) {
                ForEachEnabled([&](Item* item) {
                    if (item->OtherChestWeight(ps, da) <= 0.f) w *= item->LootWeight(ps, da);
                });
                w *= other;
                moves.emplace_back(da, p);
            } else {
                ForEachEnabled([&](Item* item) { w *= item->LootWeight(ps, da); });
            }
            At<float>(da, off::DA_SpawnWeight) = w;
        }
    };
    apply(&At<TArray<void*>>(lm, off::LM_AllUnlockedPickups));
    for (int list = off::Spawnables_WeaponMods; list <= off::Spawnables_Relics; list += 0x10)
        apply(Spawnables(lm, list));
    for (auto& [da, p] : moves)
        if (g_basePool.try_emplace(da, p).second) At<uint8_t>(da, off::DA_LootPool) = g_rollPool;
}

void* HkGetPickups(void* ps, void* out) {
    void* r = oGetPickups(ps, out);
    if (g_rollDepth > 0 && ps) ApplyWeights(ps);
    return r;
}

// The game's pick, except that a weight of 0 is never picked (the game's can pick it when rand() is 0).
int HkWeightedPick(TArray<float>* weights) {
    if (g_rollDepth == 0 || !weights || !weights->Data) return oWeightedPick(weights);
    float total = 0.f;
    for (int i = 0; i < weights->Num; i++) total += weights->Data[i] > 0.f ? weights->Data[i] : 0.f;
    if (total <= 0.f) return -1;
    float r = std::uniform_real_distribution<float>(0.f, total)(g_rng), sum = 0.f;
    int last = -1;
    for (int i = 0; i < weights->Num; i++) {
        if (weights->Data[i] <= 0.f) continue;
        sum += weights->Data[i];
        last = i;
        if (r < sum) return i;
    }
    return last;
}

// void(Source, uint8 Pool, uint8 Rarity, TArray<ACrabPS*>*, int Choices, float, TArray<FCrabPickupInfo>* Out),
// taken as raw 64-bit slots so every argument passes through untouched. The players array is the caller's
// by-value copy and LootRoll frees its data before returning (0x140d346d1), so the opener is read first.
void HkLootRoll(uint64_t source, uint64_t pool, uint64_t rarity, uint64_t players, uint64_t choices, uint64_t f,
                uint64_t out) {
    auto* who = reinterpret_cast<TArray<void*>*>(players);
    void* opener = who && who->Num > 0 && who->Data ? who->Data[0] : nullptr;
    void* savedSource = g_rollSource;
    uint8_t savedPool = g_rollPool;
    g_rollSource = reinterpret_cast<void*>(source);
    g_rollPool = uint8_t(pool);
    g_rollDepth++;
    oLootRoll(source, pool, rarity, players, choices, f, out);
    g_rollDepth--;
    g_rollSource = savedSource;
    g_rollPool = savedPool;
    if (g_rollDepth == 0) {
        for (auto& [da, w] : g_baseWeight) At<float>(da, off::DA_SpawnWeight) = w;
        g_baseWeight.clear();
        for (auto& [da, p] : g_basePool) At<uint8_t>(da, off::DA_LootPool) = p;
        g_basePool.clear();
        // An opened chest's roll fills its PickupsToSpawn for the opener; it spawns them right after.
        void* chest = reinterpret_cast<void*>(source);
        auto* loot = reinterpret_cast<TArray<uint8_t>*>(out);
        if (chest && loot == &At<TArray<uint8_t>>(chest, off::Chest_PickupsToSpawn) &&
            IsA(chest, StaticClass(va::ACrabChest_StaticClass)) && IsA(opener, StaticClass(va::ACrabPS_StaticClass)))
            ForEachEnabled([&](Item* i) { i->OnChestLoot(chest, opener, *loot); });
    }
}

// --- Island-end rewards ------------------------------------------------------------------------------------
//
// The host's rewards function builds each player's reward chests in a local array (Data/Num/Max at its
// rsp+0x40, also kept in r12/esi), then asks HasRelic(player, High Roller). HasRelic is hooked with a small
// thunk: at that one call it hands the array to the items, then loads the (possibly changed) array back into
// r12/esi, which HasRelic itself leaves untouched and the rewards function goes on using.

bool RewardChests(void* ps, char* frame, int num, void** data) {
    auto& chests = At<TArray<void*>>(frame, 0x40);
    if (chests.Num != num || chests.Data != data || num < 0 || num > chests.Max) {
        static bool warned = false;
        if (!warned) CRAB_LOG("island rewards: the chest list isn't where expected; left alone");
        warned = true;
        return false;
    }
    if (!ps || num == 0) return false;
    ForEachEnabled([&](Item* i) { i->OnRewardChests(ps, chests); });
    if (chests.Num == num) return true;
    CRAB_LOG("island rewards for %p: %d chests instead of %d", ps, chests.Num, num);
    // As when High Roller takes them all: with nothing to open, the portals come now.
    if (chests.Num == 0)
        if (void* lm = LevelManagerOf(ps)) Fn<void (*)(void*)>(va::ACrabLM_SpawnPortalsSoon)(lm);
    return true;
}

}  // namespace

extern "C" {
uintptr_t crabmods_rewardSite;  // Ret_RewardChestsHighRoller, rebased
void* crabmods_hasRelicOrig;    // the original HasRelic (MinHook trampoline)
void crabmods_HasRelicHook();
bool crabmods_OnRewardChests(void* ps, char* frame, uint32_t num, void** data) {
    return RewardChests(ps, frame, int(num), data);
}
}

// At entry rsp points at the return address; the caller's rsp is 8 above it. crabmods_RewardSite pushes
// 16 bytes and reserves 40 (shadow space, keeping rsp 16-byte aligned for the call), so the caller's rsp is
// its rsp + 64.
asm(R"(
.intel_syntax noprefix
.text
.globl crabmods_HasRelicHook
crabmods_HasRelicHook:
    mov rax, qword ptr [rip + crabmods_rewardSite]
    cmp qword ptr [rsp], rax
    je crabmods_RewardSite
    jmp qword ptr [rip + crabmods_hasRelicOrig]

.seh_proc crabmods_RewardSite
crabmods_RewardSite:
    push rcx
    .seh_pushreg rcx
    push rdx
    .seh_pushreg rdx
    sub rsp, 40
    .seh_stackalloc 40
    .seh_endprologue
    lea rdx, [rsp + 64]
    mov r8d, esi
    mov r9, r12
    call crabmods_OnRewardChests
    test al, al
    jz 1f
    mov esi, dword ptr [rsp + 64 + 0x48]
    mov r12, qword ptr [rsp + 64 + 0x40]
1:
    add rsp, 40
    pop rdx
    pop rcx
    jmp qword ptr [rip + crabmods_hasRelicOrig]
.seh_endproc
.att_syntax prefix
)");

namespace {

void HookRewardChests() {
    // Only the call this was written for: mov dl, 0x3F; mov rcx, rdi; call HasRelic.
    crabmods_rewardSite = Addr(va::Ret_RewardChestsHighRoller);
    auto* p = reinterpret_cast<const uint8_t*>(crabmods_rewardSite - 10);
    int32_t rel;
    memcpy(&rel, p + 6, 4);
    if (memcmp(p, "\xB2\x3F\x48\x8B\xCF\xE8", 6) != 0 || crabmods_rewardSite + rel != Addr(va::ACrabPS_HasRelic)) {
        CRAB_LOG("island rewards: the High Roller check isn't where expected; Double or Nothing can't work");
        return;
    }
    Hook(va::ACrabPS_HasRelic, &crabmods_HasRelicHook, reinterpret_cast<void (**)()>(&crabmods_hasRelicOrig),
         "ACrabPS::HasRelic (island rewards)");
}

// --- Portal rewards ----------------------------------------------------------------------------------------
//
// SpawnPortals picks a reward pool for each Arena, Horde and Demolition portal, by weight and without repeats,
// then calls SetInfo for the portal. When an item makes a pool more likely, the reward is picked again here
// with the same weights times the items' factors. Anvil and Spiked rewards come with their own rules, so
// those are left alone and never picked.

using SpawnPortalsFn = void (*)(void*);
using SetPortalInfoFn = void (*)(void*, void*);
SpawnPortalsFn oSpawnPortals;
SetPortalInfoFn oSetPortalInfo;

void* g_spawningPortals = nullptr;  // the level manager, while it spawns portals
bool g_rewardUsed[16];              // pools already given to a portal this time

float BaseRewardWeight(uint8_t p) {
    switch (p) {
    case pool::Damage: case pool::Health: case pool::Random: return 1.25f;
    case pool::Critical: case pool::Elemental: case pool::Speed: case pool::Luck:
    case pool::Economy: case pool::Skill: return 0.75f;
    case pool::Greed: return 0.2f;
    case pool::Upgrade: return 0.4f;
    case pool::Relic: return 0.5f;
    }
    return 0.f;  // not picked again here
}

void HkSpawnPortals(void* lm) {
    for (bool& b : g_rewardUsed) b = false;
    g_spawningPortals = lm;
    oSpawnPortals(lm);
    g_spawningPortals = nullptr;
}

void HkSetPortalInfo(void* portal, void* info) {
    if (g_spawningPortals && info) {
        uint8_t& reward = At<uint8_t>(info, off::PortalInfo_RewardLootPool);
        if (reward > 0 && reward < 16 && BaseRewardWeight(reward) > 0.f) {
            float w[16] = {}, total = 0.f;
            bool boosted = false;
            for (uint8_t p = 1; p < 16; p++) {
                if (g_rewardUsed[p] || BaseRewardWeight(p) == 0.f) continue;
                float m = 1.f;
                ForEachEnabled([&](Item* i) { m *= i->PortalRewardWeight(g_spawningPortals, p); });
                if (m != 1.f) boosted = true;
                w[p] = BaseRewardWeight(p) * m;
                total += w[p];
            }
            if (boosted && total > 0.f) {
                float r = std::uniform_real_distribution<float>(0.f, total)(g_rng), sum = 0.f;
                uint8_t pick = reward;
                for (uint8_t p = 1; p < 16; p++) {
                    if (w[p] <= 0.f) continue;
                    sum += w[p];
                    pick = p;
                    if (r < sum) break;
                }
                if (pick != reward) CRAB_LOG("portal reward pool %d instead of %d", pick, reward);
                reward = pick;
            }
        }
        if (reward < 16) g_rewardUsed[reward] = true;
    }
    oSetPortalInfo(portal, info);
}

// --- Ability cooldown --------------------------------------------------------------------------------------

using TickCooldownsFn = void (*)(void*);
TickCooldownsFn oTickCooldowns;

void HkTickCooldowns(void* c) {
    if (c) {
        void* ps = At<void*>(c, off::Pawn_PlayerState);
        if (IsA(ps, StaticClass(va::ACrabPS_StaticClass)))
            ForEachEnabled([&](Item* i) { i->OnCooldownTick(c, ps); });
    }
    oTickCooldowns(c);
}

// --- Healing -----------------------------------------------------------------------------------------------
//
// Every heal ends in UCrabHC::Heal, which caps health at max. Most item heals (perks, relics, Vampire, Health
// Shot) are first cut to the missing health by ClampHealToMissing, and skipped at full health. Between them, the
// two see every bit of healing that was lost to the cap (on the host, where all healing happens).

using HealFn = void (*)(void*, float);
using ClampHealFn = float (*)(void*, float);
HealFn oHeal;
ClampHealFn oClampHeal;

void Overheal(void* ps, float amount) {
    if (amount > 0.f && ps) ForEachEnabled([&](Item* i) { i->OnOverheal(ps, amount); });
}

void HkHeal(void* hc, float amount) {
    float over = 0.f;
    if (hc && amount > 0.f) over = At<float>(hc, off::HC_CurrentHealth) + amount - At<float>(hc, off::HC_CurrentMaxHealth);
    oHeal(hc, amount);
    if (over > 0.f) Overheal(PlayerOfHealth(hc), over);
}

float HkClampHeal(void* c, float requested) {
    float r = oClampHeal(c, requested);
    if (c && requested > r && At<uint8_t>(c, off::Actor_Role) == ROLE_Authority) {
        void* ps = At<void*>(c, off::Pawn_PlayerState);
        if (IsA(ps, StaticClass(va::ACrabPS_StaticClass))) Overheal(ps, requested - (r > 0.f ? r : 0.f));
    }
    return r;
}

// --- Destructibles ------------------------------------------------------------------------------------------
//
// On the host, a destructible's health component reports each hit; the one with hit type 5 breaks it. Only breaks
// during an island's fight count (not the clean-up when it is cleared), and not the landmines, homing barrels and
// mushrooms players' own items spawn.

using DestructibleHitFn = void (*)(void*, void*, uint8_t);
DestructibleHitFn oDestructibleHit;

void HkDestructibleHit(void* d, void* info, uint8_t hitType) {
    void* ps = nullptr;
    if (d && info && hitType == 5 && !At<uint8_t>(d, off::Destructible_bExploded)) {
        void* gs = GameStateOf(d);
        if (gs && At<uint8_t>(gs, off::GS_MatchState) == 3) {
            void* playerC = StaticClass(va::ACrabPlayerC_StaticClass);
            void* c = At<void*>(info, off::DamageInfo_DamagingC);
            if (!IsA(c, playerC)) c = At<void*>(d, off::Destructible_LastPlayer);
            bool own = false;
            if (void* lm = At<void*>(gs, off::GS_LM); lm && At<void*>(lm, off::LM_SpawnablesDA)) {
                void* sp = At<void*>(lm, off::LM_SpawnablesDA);
                for (int i = 0; i < off::Spawnables_NumPlayerDestructibles; i++)
                    own |= At<void*>(d, off::Object_Class) == At<void*>(sp, off::Spawnables_PlayerDestructibles + 8 * i);
            }
            if (!own && IsA(c, playerC)) {
                ps = At<void*>(c, off::Pawn_PlayerState);
                if (!IsA(ps, StaticClass(va::ACrabPS_StaticClass))) ps = nullptr;
            }
        }
    }
    oDestructibleHit(d, info, hitType);
    if (ps) ForEachEnabled([&](Item* i) { i->OnDestructibleBroken(ps); });
}

// --- Going down -----------------------------------------------------------------------------------------
//
// On the host, a player who goes down gets a spectator; just before that their character still has its player
// state, so whatever happens there (Martyr's blast) is credited to them.

using SpawnSpectatorFn = void (*)(void*);
SpawnSpectatorFn oSpawnSpectator;

void HkSpawnSpectator(void* pc) {
    if (pc && At<uint8_t>(pc, off::Actor_Role) == ROLE_Authority) {
        void* c = At<void*>(pc, off::Controller_Pawn);
        void* ps = IsA(c, StaticClass(va::ACrabPlayerC_StaticClass)) ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
        if (IsA(ps, StaticClass(va::ACrabPS_StaticClass))) ForEachEnabled([&](Item* i) { i->OnDown(c, ps); });
    }
    oSpawnSpectator(pc);
}

// --- Weapon fire -------------------------------------------------------------------------------------------
//
// Only on the machine that controls the player (OwningPC is set there): its projectiles deal the damage, and the
// game sends the others a copy with the same start and direction, so changing them here shows for everyone.

using FireProjectilesFn = void (*)(void*, void*, void*, uint8_t);
using SpawnProjectileFn = void* (*)(void*, void*, Vec3*, bool);
FireProjectilesFn oFireProjectiles;
SpawnProjectileFn oSpawnProjectile;
uintptr_t g_retSpawnShot;

void* ShooterOf(void* weapon) {
    if (!weapon || !At<void*>(weapon, off::Weapon_OwningPC) || !At<void*>(weapon, off::Weapon_WeaponDA)) return nullptr;
    void* ps = At<void*>(weapon, off::Weapon_OwningPS);
    return IsA(ps, StaticClass(va::ACrabPS_StaticClass)) ? ps : nullptr;
}

// The weapon's ShotSpawnType is switched to AboveCrosshair for the length of the shot (the asset is shared by
// everyone with that weapon, so it is put back at once).
void HkFireProjectiles(void* weapon, void* formation, void* projectile, uint8_t lesser) {
    void* ps = ShooterOf(weapon);
    uint8_t* type = ps ? &At<uint8_t>(At<void*>(weapon, off::Weapon_WeaponDA), off::WeaponDA_ShotSpawnType) : nullptr;
    bool above = false;
    if (type && *type == 0) ForEachEnabled([&](Item* i) { above |= i->ShotsFromAbove(ps); });
    if (above) *type = 1;
    oFireProjectiles(weapon, formation, projectile, lesser);
    if (above) *type = 0;
}

void* HkSpawnProjectile(void* info, void* owner, Vec3* start, bool send) {
    if (send && info && start && reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == g_retSpawnShot)
        if (void* ps = ShooterOf(owner)) {
            Vec3& dir = At<Vec3>(info, off::ProjInfo_Dir);
            ForEachEnabled([&](Item* i) { i->OnShot(owner, ps, *start, dir); });
        }
    return oSpawnProjectile(info, owner, start, send);
}

// --- Buff lookups ------------------------------------------------------------------------------------------
//
// The game asks for an item's buff (by type) from 100+ places; these hooks add the items' bonuses at the one call
// each cares about, told apart by return address.

using GetBuffFn = float (*)(void*, uint8_t);
GetBuffFn oGetBuff, oWeaponModBuff;
uintptr_t g_retBanana, g_retRapidFire;

// UpdateMaxHealth: max health multiplier += GetBuff(Perks, Banana).
float HkGetBuff(void* inventory, uint8_t type) {
    float r = oGetBuff(inventory, type);
    if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == g_retBanana && inventory) {
        void* ps = static_cast<char*>(inventory) - off::PS_Perks;
        ForEachEnabled([&](Item* i) { r += i->MaxHealthMultiplierBonus(ps); });
    }
    return r;
}

// UpdateFireRate: fire rate += GetWeaponModBuff(RapidFire).
float HkWeaponModBuff(void* ps, uint8_t type) {
    float r = oWeaponModBuff(ps, type);
    if (reinterpret_cast<uintptr_t>(__builtin_return_address(0)) == g_retRapidFire && ps)
        ForEachEnabled([&](Item* i) { r += i->FireRateBonus(ps); });
    return r;
}

// --- Tick --------------------------------------------------------------------------------------------------

using EngineTickFn = void (*)(void*, float, bool);
EngineTickFn oEngineTick;
float g_sinceTick = 0.f;

void HkEngineTick(void* engine, float dt, bool idle) {
    oEngineTick(engine, dt, idle);
    g_sinceTick += dt;
    if (g_sinceTick < 0.25f) return;
    g_sinceTick = 0.f;
    void* viewport = engine ? At<void*>(engine, off::Engine_GameViewport) : nullptr;
    void* world = viewport ? At<void*>(viewport, off::Viewport_World) : nullptr;
    if (world && GameStateOf(world)) ForEachEnabled([&](Item* i) { i->OnTick(world); });
}

// --- Host settings -----------------------------------------------------------------------------------------
//
// In multiplayer everyone plays by the host's CrabMods.ini. When a player joins, the host sends its settings
// as a ClientMessage (the RPC behind console messages) starting with a marker; CrabMods on the player's
// machine applies them and keeps the message off the console. The host sends them to itself too, so hosting
// a game puts your own settings back after playing in someone else's. A player without CrabMods would just
// get the text in their console.

// Items' own messages go the same way, starting with kItemTag.

constexpr wchar_t kSettingsTag[] = L"CrabMods settings\n";
constexpr size_t kSettingsTagLen = std::size(kSettingsTag) - 1;
constexpr wchar_t kItemTag[] = L"CrabMods item\n";
constexpr size_t kItemTagLen = std::size(kItemTag) - 1;
std::string g_ownSettings;  // this machine's CrabMods.ini, settings only
std::string g_settings;     // what is in use now
void* g_settingsPC;         // the player controller the host's settings last came through

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

using PostLoginFn = void (*)(void*, void*);
using ClientMessageFn = void (*)(void*, TArray<wchar_t>*, uint64_t, float);
PostLoginFn oPostLogin;
ClientMessageFn oClientMessage;

void SendSettings(void* pc) {
    std::wstring msg = kSettingsTag;
    for (char ch : g_ownSettings) msg.push_back(wchar_t(uint8_t(ch)));
    SendClientMessage(pc, msg);
}

void HkPostLogin(void* gm, void* pc) {
    oPostLogin(gm, pc);
    if (pc) SendSettings(pc);
}

void HkClientMessage(void* pc, TArray<wchar_t>* s, uint64_t type, float lifeTime) {
    if (s && s->Data && s->Num > int(kItemTagLen) && wcsncmp(s->Data, kItemTag, kItemTagLen) == 0) {
        std::wstring text(s->Data + kItemTagLen);
        for (Item* i : g_items)
            if (i->Enabled() && i->OnClientMessage(pc, text)) break;
        return;
    }
    if (!s || !s->Data || s->Num <= int(kSettingsTagLen) || wcsncmp(s->Data, kSettingsTag, kSettingsTagLen) != 0)
        return oClientMessage(pc, s, type, lifeTime);
    g_settingsPC = pc;
    std::string text;
    for (int i = int(kSettingsTagLen); i < s->Num && s->Data[i]; i++) text.push_back(char(s->Data[i]));
    if (text == g_settings) return;
    g_settings = text;
    ApplyAllSettings(text);
    CRAB_LOG("%s", text == g_ownSettings ? "using your own settings" : "using the host's settings:");
    for (Item* i : g_items)
        if (text != g_ownSettings) CRAB_LOG("  %s: %s", i->Section(), i->Enabled() ? i->Describe().c_str() : "off");
    // Already in a level: update the items' assets now rather than at the next level.
    void* lm = LevelManagerOf(pc);
    if (lm && At<TArray<void*>>(lm, off::LM_AllUnlockedPickups).Num > 0)
        ForEachEnabled([&](Item* i) { i->OnBeginPlay(lm); });
}

// --- Start -------------------------------------------------------------------------------------------------

DWORD WINAPI Init(LPVOID) {
    std::wstring dir = ModuleDir();
    Log::Get().Open(dir + L"CrabMods.log");
    LoadResult cfg = LoadConfig(dir + L"CrabMods.ini", dir + L"Arsenal.ini");
    if (cfg.created)
        CRAB_LOG("wrote default CrabMods.ini%s", cfg.migrated ? " (Arsenal settings taken from Arsenal.ini)" : "");
    if (!cfg.added.empty()) CRAB_LOG("added new items to CrabMods.ini: %s", cfg.added.c_str());
    if (!InitImage()) {
        CRAB_LOG("unsupported game build (PE timestamp mismatch); CrabMods is disabled");
        return 0;
    }
    g_items = SortedItems();
    g_ownSettings = g_settings = CompactIni(cfg.text);
    bool any = false;
    for (Item* i : g_items) any |= i->Enabled();
    // Hooked anyway: in someone else's game their settings are used.
    if (!any) CRAB_LOG("everything is disabled in CrabMods.ini (a host's settings still apply in their games)");
    if (!HookInit()) return 0;
    Hook(va::ACrabLM_BeginPlay, &HkBeginPlay, &oBeginPlay, "ACrabLM::BeginPlay");
    Hook(va::ACrabPS_GetDamageBuff, &HkDamageBuff, &oDamageBuff, "ACrabPS::GetDamageBuff");
    Hook(va::UCrabHC_TakeDamage, &HkTakeDamage, &oTakeDamage, "UCrabHC::TakeDamage");
    Hook(va::ACrabPS_CalcOutgoingDamage, &HkOutgoingDamage, &oOutgoingDamage, "ACrabPS::CalcOutgoingDamage");
    HookRewardChests();
    Hook(va::LootRoll, &HkLootRoll, &oLootRoll, "LootRoll");
    Hook(va::ACrabPS_GetPickups, &HkGetPickups, &oGetPickups, "ACrabPS::GetPickups");
    Hook(va::WeightedPick, &HkWeightedPick, &oWeightedPick, "WeightedPick");
    Hook(va::ACrabLM_SpawnPortals, &HkSpawnPortals, &oSpawnPortals, "ACrabLM::SpawnPortals");
    Hook(va::ACrabPortal_SetInfo, &HkSetPortalInfo, &oSetPortalInfo, "ACrabPortal::SetInfo");
    Hook(va::ACrabPlayerC_TickCooldowns, &HkTickCooldowns, &oTickCooldowns, "ACrabPlayerC cooldown tick");
    Hook(va::UCrabHC_Heal, &HkHeal, &oHeal, "UCrabHC::Heal");
    Hook(va::ACrabC_ClampHealToMissing, &HkClampHeal, &oClampHeal, "ACrabC::ClampHealToMissing");
    Hook(va::ACrabDestructible_OnHCDamaged, &HkDestructibleHit, &oDestructibleHit, "ACrabDestructible::OnHCDamaged");
    Hook(va::ACrabPC_SpawnSpectator, &HkSpawnSpectator, &oSpawnSpectator, "ACrabPC::SpawnSpectator");
    g_retSpawnShot = Addr(va::Ret_FireProjectilesSpawn);
    Hook(va::ACrabWeapon_FireProjectiles, &HkFireProjectiles, &oFireProjectiles, "ACrabWeapon::FireProjectiles");
    Hook(va::SpawnProjectile, &HkSpawnProjectile, &oSpawnProjectile, "SpawnProjectile");
    g_retBanana = Addr(va::Ret_MaxHealthMultiplierBanana);
    g_retRapidFire = Addr(va::Ret_FireRateRapidFire);
    Hook(va::InvArr_GetBuff, &HkGetBuff, &oGetBuff, "InvArr::GetBuff (max health)");
    Hook(va::ACrabPS_GetWeaponModBuff, &HkWeaponModBuff, &oWeaponModBuff, "ACrabPS::GetWeaponModBuff (fire rate)");
    Hook(va::UGameEngine_Tick, &HkEngineTick, &oEngineTick, "UGameEngine::Tick");
    Hook(va::AGameModeBase_K2_PostLogin, &HkPostLogin, &oPostLogin, "AGameModeBase::K2_PostLogin");
    Hook(va::APlayerController_ClientMessage_Impl, &HkClientMessage, &oClientMessage, "ClientMessage");
    for (Item* i : g_items) i->InstallHooks();
    CRAB_LOG("CrabMods %s loaded", kVersion);
    for (Item* i : g_items)
        CRAB_LOG("  %s: %s", i->Section(), i->Enabled() ? i->Describe().c_str() : "off");
    return 0;
}

}  // namespace

bool crabmods::HostHasCrabMods(void* pc) { return pc && pc == g_settingsPC; }

void crabmods::SendToPlayer(void* pc, const std::wstring& text) { SendClientMessage(pc, kItemTag + text); }

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        // Hooks go in off the loader lock.
        CloseHandle(CreateThread(nullptr, 0, Init, nullptr, 0, nullptr));
    }
    return TRUE;
}
