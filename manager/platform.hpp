// CrabMods Manager: what differs between Windows and Linux. Implemented by platform_win.cpp (Win32, WinHTTP) and
// platform_linux.cpp (POSIX, libcurl). Everything else is shared (app.cpp, files.hpp, core.hpp).
#pragma once
#include "files.hpp"

namespace mgr {

constexpr const char kVersion[] = "0.5.0";

namespace plat {

// "Windows", "Linux (Proton)" for the Windows build under wine, or "Linux".
std::string PlatformName();
bool RunningOnWine();
// The Windows build runs as a Linux player's program too (under Proton), but only the Linux build is native.
bool NativeLinux();

fs::path ExePath();
// The manager's settings and its state/ (installed.json, seen.json, backups/). Windows: next to the exe (it is
// portable). Linux: $XDG_CONFIG_HOME/CrabModsManager, since an AppImage's own folder is read-only.
fs::path DataDir();
// Downloads (cache/<mod>/<version>/) and the release list. Windows: next to the exe. Linux: $XDG_CACHE_HOME.
fs::path CacheDir();

// Every place Steam may be installed, and how to open a library path from Steam's own files.
std::vector<fs::path> SteamRoots();
fs::path LibraryPath(const std::string& steamPath);
// The game folder, if it can be found without asking.
fs::path FindGame();
// The game's Saved folders (crash reports and its log).
std::vector<SavedDir> SavedDirs(const fs::path& game);
// User and computer names, to take out of crash reports.
std::vector<std::string> Secrets();

bool GameRunning();

// GET over HTTPS, following redirects. `etag` sends If-None-Match. The response has the status (0 without a
// connection), the body, the ETag and GitHub's rate limit.
HttpResponse Http(const std::string& url, const std::string& etag = "", const std::string& accept = "application/vnd.github+json");

// Opens an address or a folder with the system's own program.
void OpenExternal(const std::string& target);
// Starts the game through Steam. A message for the status bar.
std::string LaunchGame(const fs::path& game);
// A folder picker, if the system has one. nullopt when there is none (the app asks for a path instead); an empty
// path when the player cancelled.
std::optional<fs::path> PickFolder(const std::string& title);

// Wakes the UI thread (after a job finished in the background).
void Wake();
// Asks for the player's attention (flashes the taskbar entry) if the window isn't in front.
void Attention();

// --- The manager's own updates ----------------------------------------------------------------------------------
// The release file this build updates from: CrabModsManager.exe or CrabModsManager-x86_64.AppImage.
const char* ManagerAsset();
// Where a downloaded update waits until the manager closes.
fs::path StagedUpdate();
// Puts the staged update in place of the running manager. True when it did.
bool SwapInUpdate(std::string& error);
// Starts the (new) manager with these arguments; the caller then quits.
bool Relaunch(const std::vector<std::string>& args);

}  // namespace plat
}  // namespace mgr
