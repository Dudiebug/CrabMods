// What items read from each other.
#pragma once

namespace crabmods {

// [arsenal] countStacks: a x3 stack of a weapon mod counts as three mods (Arsenal and Hoarder).
bool ArsenalCountStacks();

// The character using a totem (ACrabC*), while ACrabTotem::Interact runs (host; key_totem.cpp sets it for every
// totem).
inline void* g_totemUser = nullptr;

// Encore (encore.cpp): a roll of its chance for a player, true on success (only if they hold it and it is on).
bool EncoreRoll(void* ps);

}  // namespace crabmods
