// Shellbreaker's own loot (shell_loot.cpp): pickups that only drop for players holding Shellbreaker. The items
// only carry a level and a number per level; shellbreaker.cpp asks for them here and does what they say.
#pragma once

namespace crabmods::shell_loot {

enum Id {
    LongChain,     // weapon mod: links added to the chain (reach, circle and hook range grow with it)
    WideArc,       // weapon mod: swing hit radius, percent
    Whirlwind,     // weapon mod: the overhead circle drags enemies in, cm/s
    Executioner,   // weapon mod: the chop and its rebound deal more, percent
    SearingEdge,   // weapon mod: fire stacks per swing hit
    Riptide,       // perk: damage for a few seconds after the hook lands, percent
    QuickDraw,     // perk: hook cooldown cut, percent (hyperbolic, never below a sixth)
    Undertow,      // perk: the hook's own hit deals more, percent
    Rhythm,        // perk: the overhead circle deals more, percent
    Grappler,      // relic: extra hook charges
    KrakensGrasp,  // relic: yanks reach hard and elite enemies too, and drag this many more along
    FrostChain,    // relic: ice stacks from the hook's hit
    kCount
};

// One of the items' assets.
bool IsShellItem(void* da);
// The player's level of the item (0: not held, or switched off).
int Level(void* ps, Id id);
// Its number per level, from CrabMods.ini.
float Amount(Id id);
// Level x amount.
inline float Total(void* ps, Id id) { return float(Level(ps, id)) * Amount(id); }

}  // namespace crabmods::shell_loot

namespace crabmods {
// In shellbreaker.cpp: true while the player holds Shellbreaker.
bool HoldsShellbreaker(void* ps);
}
