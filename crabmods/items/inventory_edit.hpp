// Changing a player's inventory from code (host), with the game's own adders and removers: they replicate, tell
// the inventory menu (OnInventoryChanged) and pop up the pickup message on the player's machine. Used by the
// Greed gambles (snap_decision.cpp, wild_card.cpp).
//
// The adders merge an item already held into its entry (Level += the new one's, up to 255), so adding an item
// at its own level doubles it. The removers take out the entry of that type byte (DA+0xD8).
#pragma once
#include <algorithm>
#include <random>
#include <vector>

#include "../item.hpp"
#include "necro_loot.hpp"
#include "shell_loot.hpp"
#include "shared.hpp"

namespace crabmods::inv {

struct Entry {
    Kind kind;
    void* da;
    int level;
};

// The kinds the gambles touch, in the inventory menu's order.
inline constexpr Kind kLeveled[] = {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk};

inline std::vector<Entry> Held(void* ps, Kind kind) {
    std::vector<Entry> out;
    ForEachItem(ps, Info(kind).playerArray, [&](void* da, int level) {
        if (da) out.push_back({kind, da, level});
    });
    return out;
}

// FItem / FCrabRelic: the DA, then FCrabInventoryInfo {u8 Level, TArray<u8> Enhancements, float AccumulatedBuff}.
struct alignas(8) NewItem {
    void* da;
    uint8_t level;
    uint8_t pad[7];
    TArray<uint8_t> enhancements;
    float accumulated;
    uint32_t pad2;
};
static_assert(sizeof(NewItem) == 0x28);

inline void Add(void* ps, Kind kind, void* da, int level) {
    NewItem item{da, uint8_t(std::clamp(level, 1, 255)), {}, {}, 0.f, 0};
    uintptr_t fn = kind == Kind::WeaponMod    ? va::ACrabPS_AddWeaponMod
                   : kind == Kind::AbilityMod ? va::ACrabPS_AddAbilityMod
                   : kind == Kind::MeleeMod   ? va::ACrabPS_AddMeleeMod
                   : kind == Kind::Perk       ? va::ACrabPS_AddPerk
                                              : va::ACrabPS_AddRelic;
    Fn<void (*)(void*, void*)>(fn)(ps, &item);
}

inline void Remove(void* ps, Kind kind, void* da) {
    uintptr_t fn = kind == Kind::WeaponMod    ? va::ACrabPS_RemoveWeaponMod
                   : kind == Kind::AbilityMod ? va::ACrabPS_RemoveAbilityMod
                   : kind == Kind::MeleeMod   ? va::ACrabPS_RemoveMeleeMod
                   : kind == Kind::Perk       ? va::ACrabPS_RemovePerk
                                              : va::ACrabPS_RemoveRelic;
    Fn<void (*)(void*, uint8_t)>(fn)(ps, At<uint8_t>(da, off::DA_ItemType));
}

// What a random totem could give this player of a kind: the game's full list, less Greed items (the game's own
// randomize leaves those out too), charms, the Soul Whip's and Shellbreaker's items for players without that weapon, and
// whatever is in skip.
inline std::vector<void*> Pool(void* lm, void* ps, Kind kind, const std::vector<void*>& skip) {
    std::vector<void*> out;
    auto* list = Spawnables(lm, Info(kind).spawnablesList);
    bool whip = HoldsSoulWhip(ps), shell = HoldsShellbreaker(ps);
    for (int i = 0; list && list->Data && i < list->Num; i++) {
        void* da = list->Data[i];
        if (!da || At<uint8_t>(da, off::DA_Rarity) == rarity::Greed || IsCharm(da)) continue;
        if (!whip && necro_loot::IsNecroItem(da)) continue;
        if (!shell && shell_loot::IsShellItem(da)) continue;
        if (std::find(skip.begin(), skip.end(), da) != skip.end()) continue;
        out.push_back(da);
    }
    return out;
}

}  // namespace crabmods::inv
