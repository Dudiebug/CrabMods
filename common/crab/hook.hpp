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

}  // namespace crab
