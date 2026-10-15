// What the chest luck items (Appraiser, Clean Sweep, Fixation) share.
#pragma once

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {

constexpr int kKeyChest = 0x5A0, kSecretChest = 0x5A8;  // SpawnablesDA

// A chest's roll, other than a key or secret chest's.
inline bool LuckyChest(void* source) {
    if (!source || !IsA(source, StaticClass(va::ACrabChest_StaticClass))) return false;
    void* lm = LevelManagerOf(source);
    void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
    void* cls = At<void*>(source, off::Object_Class);
    return !sp || (cls != At<void*>(sp, kKeyChest) && cls != At<void*>(sp, kSecretChest));
}

}  // namespace crabmods
