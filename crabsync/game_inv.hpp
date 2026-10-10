// Reads an ACrabPS into a ledger Inventory and drives a live ACrabPS to a target Inventory using the
// game's own functions. Host (authority) only; must run on the game thread.
#pragma once
#include "ledger.hpp"

namespace crabsync {

void ReadInventory(void* ps, Inventory& out);

// announce: deliver item gains through ACrabPlayerC::GivePickup, exactly like a real pickup (popup,
// perk side effects). Otherwise items are written silently (used after travel/join, where the
// player's state is being restored rather than gained).
// At most one GivePickup is made per call: two pickups arriving in the same frame can lose one, so
// they are queued and handed out one per poll, in pickup order. Returns false while some are still queued.
bool ApplyTarget(void* ps, const Inventory& cur, const Inventory& tgt, bool announce);

}  // namespace crabsync
