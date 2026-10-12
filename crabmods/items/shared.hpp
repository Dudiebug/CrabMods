// What items read from each other.
#pragma once

namespace crabmods {

// The number of different pickup tags (Fire, Critical, Speed, ...) among everything a player holds (Jack of All
// Trades and Minimalist; jack_of_all_trades.cpp).
int DistinctTags(void* ps);

// The character using a totem (ACrabC*), while ACrabTotem::Interact runs (host; key_totem.cpp sets it for every
// totem).
inline void* g_totemUser = nullptr;

// Encore (encore.cpp): a roll of its chance for a player, true on success (only if they hold it and it is on).
bool EncoreRoll(void* ps);

// Charms (charms.cpp): items that give inventory slots, held as relics. Their cards say "Charm", in this colour.
bool IsCharm(void* da);
constexpr float kCharmColor[4] = {0.3f, 1.f, 0.8f, 1.f};

}  // namespace crabmods
