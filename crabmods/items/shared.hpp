// What items read from each other.
#pragma once

namespace crabmods {

// The number of different pickup tags (Fire, Critical, Speed, ...) among everything a player holds (Jack of All
// Trades and Minimalist; jack_of_all_trades.cpp).
int DistinctTags(void* ps);

// The character using a totem (ACrabC*), while ACrabTotem::Interact runs (host; key_totem.cpp sets it for every
// totem).
inline void* g_totemUser = nullptr;

// Necromancer (necromancer.cpp): is this enemy one of the players' undead (raised or being raised)?
bool IsUndead(void* c);

// Encore (encore.cpp): a roll of its chance for a player, true on success (only if they hold it and it is on).
bool EncoreRoll(void* ps);

// Charms (charms.cpp): items that give inventory slots, held as relics. Their cards say "Charm", in this colour.
bool IsCharm(void* da);
constexpr float kCharmColor[4] = {0.3f, 1.f, 0.8f, 1.f};

// Greed Run (greed_run.cpp): the game mode where everything is Greed.
bool GreedRunActive();
// A pickup's card is being filled (ACrabInteractPickup::SetupUI, every machine; lucky_charm.cpp has the hook): before
// is true just before the game fills it, false after the other items changed it.
void GreedRunCard(void* pickup, bool before);
// true while a player drops an item (cursed_lottery.cpp sets it around ServerDropPickup, host): the pickup that
// spawns is one they already had.
inline bool g_droppingPickup = false;

}  // namespace crabmods
