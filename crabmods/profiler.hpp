// The profiler (items/profiler.cpp): where the game's frame time goes. It hooks the game's ticks, script calls and
// garbage collection itself; main.cpp reports each frame and times every item call it dispatches while g_on is set
// (items' own hooks are not timed).
#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace crabmods {

class Item;

namespace prof {

// true while the profiler is switched on in the ini.
inline bool g_on = false;

// Nanoseconds, for differences only.
inline int64_t Now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Around one dispatch of an event to the items: only the outermost one counts toward CrabMods' total, so an
// event raised inside another item's event is not counted twice.
void Enter();
void Leave();
// One item's call: ns spent in event (a string literal).
void Add(Item* item, const char* event, int64_t ns);
// The end of UGameEngine::Tick (CrabMods' frame events included), which ran from tickStart to tickEnd.
void EndFrame(int64_t tickStart, int64_t tickEnd);
// The profiler's summary on the HUD, after CrabMods' version; empty when it is off.
std::wstring HudText();

}  // namespace prof
}  // namespace crabmods
