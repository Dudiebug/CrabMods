// CrabMods Manager: the app itself (state, background jobs, every page), shared by the Windows and Linux windows
// (win_main.cpp, linux_main.cpp). The frontends make the window and the ImGui context, then call these.
#pragma once
#include <string>
#include <vector>

namespace app {

struct Options {
    std::string screenshot;  // draw once, save a PNG there, quit; nothing is written or downloaded
    std::string page;        // "share", "settings", "help"
    std::string select;      // a mod or item in the details pane
    std::string demo;        // for screenshots: "wizard", "review", "banner", "updated", "crash"
    bool updated = false;    // started by the old manager after an update
};

// After the ImGui context exists: fonts, style, settings, the first update check.
void Init(const Options& o, float scale);
// Every pass of the frontend's loop: finished jobs, timers (update checks, crash watch, waiting for the game).
void Tick();
void Frame();
// Whether to keep drawing (a spinner, a fresh status line) rather than wait for input.
bool Animating();
// The app asked to close (to restart into an update).
bool WantsQuit();
// The window is closing: a staged manager update goes in now if the player allowed it.
void Shutdown();
// No window: checks GitHub, the game and the catalogs and writes CrabModsManager-selftest.txt.
int SelfTest(bool report);

// For tests (tests/manager_app_test.cpp), which drive the real pages without a screen:
// the next Frame() presses every enabled button with this label (all of them, when several share it).
void Press(const std::string& label);
// An enabled button with this label was drawn in the last Frame().
bool Shown(const std::string& label);
// The notices across the top of the last Frame().
std::vector<std::string> NoticeTexts();
std::string StatusLine();
// Background jobs are queued or running.
bool Busy();

// The smallest window that fits, in pixels at scale 1.
constexpr int kMinWidth = 960, kMinHeight = 620;
constexpr const char kTitle[] = "CrabMods Manager";

}  // namespace app
