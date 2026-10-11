// Thin MinHook wrapper keyed by static (unrebased) addresses.
#pragma once
#include <MinHook.h>

#include "log.hpp"
#include "ue.hpp"

namespace crab {

inline bool HookInit() {
    MH_STATUS s = MH_Initialize();
    return s == MH_OK || s == MH_ERROR_ALREADY_INITIALIZED;
}

template <class F>
inline bool Hook(uintptr_t staticVa, F detour, F* original, const char* name) {
    void* target = reinterpret_cast<void*>(Addr(staticVa));
    MH_STATUS s = MH_CreateHook(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(original));
    if (s == MH_OK) s = MH_EnableHook(target);
    CRAB_LOG("hook %s @ %p: %s", name, target, MH_StatusToString(s));
    return s == MH_OK;
}

// Replaces one entry of a class's virtual table, for functions too short for a MinHook detour. The slot is a
// static address (the table's address plus the slot's offset).
template <class F>
inline bool SwapVirtual(uintptr_t slotVa, F detour, F* original, const char* name) {
    auto* slot = reinterpret_cast<F*>(Addr(slotVa));
    DWORD old;
    bool ok = VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old);
    if (ok) {
        *original = *slot;
        *slot = detour;
        VirtualProtect(slot, sizeof *slot, old, &old);
    }
    CRAB_LOG("hook %s (virtual table) @ %p: %s", name, static_cast<void*>(slot), ok ? "MH_OK" : "failed");
    return ok;
}

}  // namespace crab
