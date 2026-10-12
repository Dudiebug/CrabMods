// The Soul Whip's own loot (necro_loot.cpp): pickups that only drop for players holding the whip. The items
// themselves only carry a level and a number per level; necromancer.cpp, which owns souls, tags and undead, asks
// for them here and does what they say.
#pragma once

namespace crabmods::necro_loot {

enum Id {
    LongLash,       // weapon mod: bolt reach, percent
    SoulSiphon,     // weapon mod: souls per enemy a bolt hits
    BrandingIron,   // weapon mod: burst and tag radius, percent
    DeathMark,      // weapon mod: your damage to tagged enemies, percent
    SoulBrand,      // weapon mod: enemies a tagged kill tags
    SturdyBones,    // ability mod: undead health, percent
    SoulDiscount,   // ability mod: raise cost, percent off (hyperbolic, never below half)
    MassGrave,      // ability mod: fodder per raise
    TwinRising,     // ability mod: chance of a raise doubling, percent (stacks as 1 - (1 - p)^level)
    GraveClaws,     // melee mod: souls from melee kills, percent
    Harvester,      // melee mod: chance a melee kill on fodder or medium rises free, percent (stacks like Twin Rising)
    SoulJar,        // perk: max souls
    HungryGhosts,   // perk: souls from kills, percent
    LastRites,      // perk: souls back when an undead dies, percent of its cost
    ArmyOfTheDead,  // perk: damage per undead, percent
    PlagueBearers,  // perk: poison stacks per undead hit
    GrimHarvest,    // perk: more souls from tagged kills, percent
    Legion,         // perk: more undead at once
    HollowHunger,   // perk: undead upkeep cut, percent (multiplies per level, never below an eighth)
    RingOfSouls,    // relic: souls from everything, percent
    RingOfTheGrave, // relic: undead health and damage, percent
    Phylactery,     // relic: least souls it takes to cancel a death
    kCount
};

// One of the items' assets.
bool IsNecroItem(void* da);
// The player's level of the item (0: not held, or switched off).
int Level(void* ps, Id id);
// Its number per level, from CrabMods.ini.
float Amount(Id id);
// Level x amount.
inline float Total(void* ps, Id id) { return float(Level(ps, id)) * Amount(id); }

}  // namespace crabmods::necro_loot

namespace crabmods {
// In necromancer.cpp: true while the player holds the Soul Whip.
bool HoldsSoulWhip(void* ps);
}
