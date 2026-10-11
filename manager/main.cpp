// CrabMods Manager: installs, updates, switches on and off and removes the Crab Champions mods from this
// repository's GitHub releases, edits and shares their settings, starts the game and, when it crashes, gathers the
// logs and the crash report for a GitHub issue. One window, plain Win32, works under wine.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>

#include <atomic>
#include <cwchar>
#include <tuple>
#include <fstream>
#include <thread>

#include "core.hpp"

using namespace mgr;

namespace {

constexpr const char kVersion[] = "0.2.0";
constexpr const wchar_t kTitle[] = L"CrabMods Manager";

// --- Strings and files ---------------------------------------------------------------------------------------

std::wstring Wide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::optional<std::string> ReadFile(const std::wstring& p) {
    std::ifstream f(p.c_str(), std::ios::binary);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool WriteFile(const std::wstring& p, const std::string& data) {
    std::ofstream f(p.c_str(), std::ios::binary | std::ios::trunc);
    f << data;
    return bool(f);
}

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

// "\n" text <-> "\r\n" for the edit control.
std::wstring ToCrLf(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s) {
        if (c == L'\r') continue;
        if (c == L'\n') r += L'\r';
        r += c;
    }
    return r;
}
std::wstring FromCrLf(const std::wstring& s) {
    std::wstring r;
    for (wchar_t c : s)
        if (c != L'\r') r += c;
    return r;
}

// --- The game folder ---------------------------------------------------------------------------------------

constexpr const wchar_t kGameSub[] = L"\\steamapps\\common\\Crab Champions\\CrabChampions\\Binaries\\Win64";
constexpr const wchar_t kGameExe[] = L"CrabChampions-Win64-Shipping.exe";

bool IsGameDir(const std::wstring& d) { return Exists(d + L"\\" + kGameExe); }

// The Win64 folder for whatever the user picked: Win64 itself, the game folder, or a Steam library.
std::wstring NormalizeGameDir(std::wstring d) {
    while (!d.empty() && (d.back() == L'\\' || d.back() == L'/')) d.pop_back();
    for (const wchar_t* sub : {L"", L"\\CrabChampions\\Binaries\\Win64", L"\\Binaries\\Win64", kGameSub,
                               L"\\common\\Crab Champions\\CrabChampions\\Binaries\\Win64"})
        if (IsGameDir(d + sub)) return d + sub;
    return {};
}

std::wstring RegString(HKEY root, const wchar_t* key, const wchar_t* value) {
    wchar_t buf[1024];
    DWORD size = sizeof buf;
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

// A Linux path from Steam's files ("/home/me/...") as wine sees it ("Z:\home\me\...").
std::wstring WinePath(const std::string& p) {
    std::wstring w = Wide(p);
    for (auto& c : w)
        if (c == L'/') c = L'\\';
    if (!p.empty() && p[0] == '/') w = L"Z:" + w;
    return w;
}

std::wstring FindGameDir() {
    if (std::wstring here = NormalizeGameDir(ExeDir()); !here.empty()) return here;
    std::vector<std::wstring> steamRoots;
    for (auto [root, key, value] : {std::tuple{HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath"}})
        if (std::wstring p = RegString(root, key, value); !p.empty()) steamRoots.push_back(WinePath(Utf8(p)));
    // Under Proton or wine the Linux Steam is reached through Z:.
    for (const wchar_t* var : {L"HOME", L"USER", L"USERNAME"}) {
        wchar_t buf[512];
        DWORD n = GetEnvironmentVariableW(var, buf, 512);
        if (!n || n >= 512) continue;
        std::wstring v(buf, n), home = v[0] == L'/' ? L"Z:" + v : L"Z:\\home\\" + v;
        for (auto& c : home)
            if (c == L'/') c = L'\\';
        steamRoots.push_back(home + L"\\.local\\share\\Steam");
        steamRoots.push_back(home + L"\\.steam\\steam");
        steamRoots.push_back(home + L"\\.var\\app\\com.valvesoftware.Steam\\.local\\share\\Steam");
    }
    steamRoots.push_back(L"C:\\Program Files (x86)\\Steam");
    for (const auto& root : steamRoots) {
        std::vector<std::wstring> libs{root};
        if (auto vdf = ReadFile(root + L"\\steamapps\\libraryfolders.vdf"))
            for (const auto& lib : SteamLibraries(*vdf)) libs.push_back(WinePath(lib));
        for (const auto& lib : libs)
            if (IsGameDir(lib + kGameSub)) return lib + kGameSub;
    }
    return {};
}

// --- Downloads ---------------------------------------------------------------------------------------------

// GET over HTTPS; redirects are followed (release files are served from another host).
std::optional<std::string> HttpGet(const std::wstring& url, std::string& error) {
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof u;
    wchar_t host[256], path[2048];
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) {
        error = "bad address";
        return std::nullopt;
    }
    std::string ua = std::string("CrabModsManager/") + kVersion;
    HINTERNET s = WinHttpOpen(Wide(ua).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    std::optional<std::string> body;
    if (r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", DWORD(-1), nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, size = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size,
                            nullptr);
        std::string data;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail) {
            std::string chunk(avail, '\0');
            DWORD got = 0;
            if (!WinHttpReadData(r, chunk.data(), avail, &got)) break;
            data.append(chunk, 0, got);
        }
        if (status == 200) body = std::move(data);
        else error = "the server answered " + std::to_string(status);
    } else {
        error = "no connection (error " + std::to_string(GetLastError()) + ")";
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return body;
}

// --- Installed mods ----------------------------------------------------------------------------------------

std::wstring g_game;  // the game's Win64 folder, empty until found
std::map<std::string, Release> g_latest;
std::string g_releaseError;
bool g_releasesLoaded = false;

std::wstring DllPath(const char* dll) { return g_game + L"\\" + Wide(dll); }
std::wstring DisabledPath(const char* dll) { return DllPath(dll) + L".disabled"; }

enum class State { Missing, Enabled, Disabled };
State StateOf(const ModInfo& m) {
    if (g_game.empty()) return State::Missing;
    if (Exists(DllPath(m.dll))) return State::Enabled;
    if (Exists(DisabledPath(m.dll))) return State::Disabled;
    return State::Missing;
}

// "0.3.0" from the DLL's version resource; "?" if it has none (releases before the manager).
std::string InstalledVersion(const ModInfo& m) {
    State st = StateOf(m);
    if (st == State::Missing) return {};
    std::wstring p = st == State::Enabled ? DllPath(m.dll) : DisabledPath(m.dll);
    DWORD dummy = 0, size = GetFileVersionInfoSizeW(p.c_str(), &dummy);
    if (!size) return "?";
    std::vector<char> buf(size);
    VS_FIXEDFILEINFO* fi = nullptr;
    UINT len = 0;
    if (!GetFileVersionInfoW(p.c_str(), 0, size, buf.data()) ||
        !VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void**>(&fi), &len) || !fi)
        return "?";
    return std::to_string(HIWORD(fi->dwFileVersionMS)) + "." + std::to_string(LOWORD(fi->dwFileVersionMS)) + "." +
           std::to_string(HIWORD(fi->dwFileVersionLS));
}

bool UpdateAvailable(const ModInfo& m) {
    auto it = g_latest.find(m.name);
    if (it == g_latest.end() || StateOf(m) == State::Missing) return false;
    std::string v = InstalledVersion(m);
    return v == "?" || CompareVersions(v, it->second.version) < 0;
}

// Replaces `target` with the download, through a temporary file so a failed download leaves it untouched.
bool Download(const std::string& url, const std::wstring& target, std::string& error) {
    auto data = HttpGet(Wide(url), error);
    if (!data) return false;
    if (data->size() < 2 || (*data)[0] != 'M' || (*data)[1] != 'Z') {
        error = "the download is not a DLL";
        return false;
    }
    std::wstring tmp = target + L".download";
    if (!WriteFile(tmp, *data)) {
        error = "could not write to the game folder";
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        error = "could not replace " + Utf8(target.substr(target.find_last_of(L'\\') + 1)) +
                ". Close the game and try again.";
        return false;
    }
    return true;
}

// Wine (Proton) has ntdll!wine_get_version. There the game needs a launch option to load the mod loader.
bool RunningOnWine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version");
}

// Installs or updates a mod, and the mod loader with it. A switched-off mod stays switched off.
std::string Install(const ModInfo& m) {
    auto it = g_latest.find(m.name);
    if (it == g_latest.end()) return "No release of " + std::string(m.name) + " to install.";
    const Release& r = it->second;
    std::string error;
    std::wstring target = StateOf(m) == State::Disabled ? DisabledPath(m.dll) : DllPath(m.dll);
    if (auto loader = r.assets.find(kLoaderDll); loader != r.assets.end())
        if (!Download(loader->second, DllPath(kLoaderDll), error)) return "Mod loader: " + error;
    if (!Exists(DllPath(kLoaderDll))) return "This release has no mod loader (" + std::string(kLoaderDll) + ").";
    if (!Download(r.assets.at(m.dll), target, error)) return std::string(m.name) + ": " + error;
    std::string msg = std::string(m.name) + " " + r.version + " installed.";
    if (RunningOnWine()) msg += " On Linux / Steam Deck, set the game's Launch Options to: WINEDLLOVERRIDES=\"xinput1_3=n,b\" %command%";
    return msg;
}

std::string Uninstall(const ModInfo& m) {
    DeleteFileW(DllPath(m.dll).c_str());
    DeleteFileW(DisabledPath(m.dll).c_str());
    if (StateOf(m) != State::Missing) return "Could not remove " + std::string(m.dll) + ". Close the game and try again.";
    bool any = false;
    for (const ModInfo& o : Mods()) any |= StateOf(o) != State::Missing;
    std::string msg = std::string(m.name) + " removed (its " + m.ini + " is kept).";
    if (!any && DeleteFileW(DllPath(kLoaderDll).c_str())) msg += " No mods left, so the mod loader was removed too.";
    return msg;
}

std::string Toggle(const ModInfo& m) {
    State st = StateOf(m);
    if (st == State::Missing) return std::string(m.name) + " is not installed.";
    bool on = st == State::Enabled;
    std::wstring from = on ? DllPath(m.dll) : DisabledPath(m.dll), to = on ? DisabledPath(m.dll) : DllPath(m.dll);
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING))
        return "Could not rename " + Utf8(from.substr(from.find_last_of(L'\\') + 1)) + ". Close the game and try again.";
    return std::string(m.name) + (on ? " switched off." : " switched on.");
}

// --- Crash reports -----------------------------------------------------------------------------------------

// The game's Saved folder (its crash reports and log): in this Windows profile, and, for Proton, in the game's
// own wine prefix in the Steam library, since the manager may run in another prefix.
struct SavedDir {
    std::wstring path;
    bool proton;
};
std::vector<SavedDir> SavedDirs() {
    std::vector<SavedDir> dirs;
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n && n < MAX_PATH) dirs.push_back({std::wstring(buf, n) + L"\\CrabChampions\\Saved", RunningOnWine()});
    if (size_t i = g_game.rfind(L"\\steamapps\\common\\"); i != std::wstring::npos)
        dirs.push_back({g_game.substr(0, i) + L"\\steamapps\\compatdata\\" + Wide(kAppId) +
                            L"\\pfx\\drive_c\\users\\steamuser\\AppData\\Local\\CrabChampions\\Saved",
                        true});
    std::erase_if(dirs, [](const SavedDir& d) { return !Exists(d.path); });
    return dirs;
}

ULONGLONG Ticks(const FILETIME& f) { return (ULONGLONG(f.dwHighDateTime) << 32) | f.dwLowDateTime; }

// The files in a folder (not its subfolders): name, size, last write.
std::vector<WIN32_FIND_DATAW> FilesIn(const std::wstring& dir) {
    std::vector<WIN32_FIND_DATAW> files;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;
    do
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.push_back(fd);
    while (FindNextFileW(h, &fd));
    FindClose(h);
    return files;
}

// One of UE's crash folders (Saved\Crashes\UE4CC-Windows-...).
struct Crash {
    std::wstring dir;
    std::string name;
    ULONGLONG time = 0, size = 0;  // the newest file in it, and all of them together (to see it's finished)
    bool proton = false;
};

std::optional<Crash> NewestCrash() {
    std::optional<Crash> newest;
    for (const SavedDir& saved : SavedDirs()) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((saved.path + L"\\Crashes\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            Crash c{saved.path + L"\\Crashes\\" + fd.cFileName, Utf8(fd.cFileName), 0, 0, saved.proton};
            bool context = false;
            for (const auto& f : FilesIn(c.dir)) {
                c.time = std::max(c.time, Ticks(f.ftLastWriteTime));
                c.size += (ULONGLONG(f.nFileSizeHigh) << 32) | f.nFileSizeLow;
                context |= _wcsicmp(f.cFileName, L"CrashContext.runtime-xml") == 0;
            }
            if (context && (!newest || c.time > newest->time)) newest = c;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return newest;
}

ULONGLONG Now() {
    FILETIME f;
    GetSystemTimeAsFileTime(&f);
    return Ticks(f);
}
constexpr ULONGLONG kSecond = 10'000'000;  // FILETIME ticks

// "2026-10-11 14:03" (or "2026-10-11_14-03-05" for file names) in local time.
std::string LocalTime(ULONGLONG ticks, bool forFile) {
    FILETIME utc{DWORD(ticks), DWORD(ticks >> 32)}, local;
    SYSTEMTIME t;
    FileTimeToLocalFileTime(&utc, &local);
    FileTimeToSystemTime(&local, &t);
    char buf[32];
    std::snprintf(buf, sizeof buf, forFile ? "%04d-%02d-%02d_%02d-%02d-%02d" : "%04d-%02d-%02d %02d:%02d", t.wYear,
                  t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return buf;
}

// What would name the player's account or machine in the files: user and computer names.
std::vector<std::string> Secrets() {
    std::vector<std::string> secrets;
    for (const wchar_t* var : {L"USERNAME", L"USER", L"LOGNAME", L"COMPUTERNAME", L"HOSTNAME"}) {
        wchar_t buf[256];
        DWORD n = GetEnvironmentVariableW(var, buf, 256);
        if (n && n < 256) secrets.push_back(Utf8(std::wstring(buf, n)));
    }
    wchar_t buf[256];
    DWORD n = 256;
    if (GetComputerNameW(buf, &n)) secrets.push_back(Utf8(std::wstring(buf, n)));
    n = 256;
    if (GetUserNameW(buf, &n) && n > 1) secrets.push_back(Utf8(std::wstring(buf, n - 1)));
    std::erase_if(secrets, [](const std::string& s) { return s == "steamuser"; });  // Proton's, the same for all
    return secrets;
}

std::string Versions() {
    std::string v;
    for (const ModInfo& m : Mods()) {
        State st = StateOf(m);
        v += std::string(m.name) + " " +
             (st == State::Missing ? "not installed" : InstalledVersion(m) + (st == State::Disabled ? " (off)" : "")) +
             ", ";
    }
    return v + "Manager " + kVersion;
}

struct BuiltReport {
    CrashReport report;
    std::wstring folder, zip;
    std::string error;
};

// Copies the mod logs and settings, the game's log and the crash folder (if any) into
// CrashReports\CrabCrash-<time> next to the manager, with names and ids taken out, and zips them there.
BuiltReport BuildReport(const std::optional<Crash>& crash) {
    BuiltReport b;
    CrashReport& r = b.report;
    std::vector<std::pair<std::string, std::string>> files;  // name in the report, contents
    if (!g_game.empty())
        for (const char* name : {"CrabMods.log", "CrabSync.log", "CrabMods.ini", "CrabSync.ini"})
            if (auto t = ReadFile(g_game + L"\\" + Wide(name))) files.push_back({name, *t});
    for (const SavedDir& saved : SavedDirs())
        if (auto t = ReadFile(saved.path + L"\\Logs\\CrabChampions.log")) {
            files.push_back({"CrabChampions.log", *t});
            break;
        }
    if (crash)
        for (const auto& f : FilesIn(crash->dir)) {
            ULONGLONG size = (ULONGLONG(f.nFileSizeHigh) << 32) | f.nFileSizeLow;
            bool dump = _wcsicmp(f.cFileName + std::max<size_t>(wcslen(f.cFileName), 4) - 4, L".dmp") == 0;
            if (dump && (size == 0 || size > 20'000'000)) continue;  // wine writes empty ones; GitHub takes 25 MB
            if (auto t = ReadFile(crash->dir + L"\\" + f.cFileName)) files.push_back({"crash/" + Utf8(f.cFileName), *t});
        }

    std::vector<std::string> players, secrets = Secrets();
    for (const auto& [name, text] : files)
        if (name == "CrabSync.log") players = PlayerNames(text);
    for (auto& [name, text] : files) {
        if (name.ends_with(".dmp")) continue;
        if (name.ends_with(".runtime-xml")) text = StripCrashIds(text);
        text = Scrub(text, secrets, players);
        if (name == "CrabMods.log") r.crabModsLog = text;
        if (name == "CrabSync.log") r.crabSyncLog = text;
        if (name == "crash/CrashContext.runtime-xml") ReadCrashContext(r, text);
    }
    r.when = LocalTime(crash ? crash->time : Now(), false);
    r.platform = (crash ? crash->proton : RunningOnWine()) ? "Linux (Proton)" : "Windows";
    r.versions = Versions();
    WIN32_FILE_ATTRIBUTE_DATA log;
    if (crash && !g_game.empty() &&
        GetFileAttributesExW((g_game + L"\\CrabMods.log").c_str(), GetFileExInfoStandard, &log) &&
        Ticks(log.ftLastWriteTime) > crash->time + 60 * kSecond)
        r.note = "the game was started again after the crash, so the mod logs are from that later session";

    std::string stamp = (crash ? "CrabCrash-" : "CrabLogs-") + LocalTime(crash ? crash->time : Now(), true);
    std::wstring root = ExeDir() + L"CrashReports";
    CreateDirectoryW(root.c_str(), nullptr);
    b.folder = root + L"\\" + Wide(stamp);
    if (!CreateDirectoryW(b.folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        b.error = "could not make the folder " + Utf8(b.folder);
        return b;
    }
    r.zipName = stamp + ".zip";
    files.push_back({"report.md", "# " + IssueTitle(r) + "\n\n" + IssueBody(r, 200)});
    CreateDirectoryW((b.folder + L"\\crash").c_str(), nullptr);
    for (const auto& [name, text] : files) {
        std::wstring path = b.folder + L"\\" + Wide(name);
        std::replace(path.begin(), path.end(), L'/', L'\\');
        WriteFile(path, text);
    }
    if (!crash) RemoveDirectoryW((b.folder + L"\\crash").c_str());  // only if empty
    SYSTEMTIME t;
    GetLocalTime(&t);
    uint16_t dosTime = (t.wHour << 11) | (t.wMinute << 5) | (t.wSecond / 2);
    uint16_t dosDate = ((t.wYear - 1980) << 9) | (t.wMonth << 5) | t.wDay;
    b.zip = b.folder + L"\\" + Wide(r.zipName);
    if (!WriteFile(b.zip, StoreZip(files, dosTime, dosDate))) b.error = "could not write " + r.zipName;
    return b;
}

// Opens an address or a folder with the system's own program (under wine, the Linux one, through winebrowser).
void OpenExternal(const std::wstring& target) {
    if (RunningOnWine()) ShellExecuteW(nullptr, L"open", L"winebrowser.exe", (L"\"" + target + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    else ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// Starts the game through Steam (so the overlay, cloud saves and multiplayer work); without Steam, the game itself.
std::string LaunchGame() {
    std::wstring url = L"steam://rungameid/" + Wide(kAppId);
    if (RunningOnWine()) {
        OpenExternal(url);
    } else if (INT_PTR(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        if (g_game.empty()) return "Could not start Steam, and the game folder is not known.";
        std::wstring exe = g_game + L"\\" + kGameExe;
        if (INT_PTR(ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, g_game.c_str(), SW_SHOWNORMAL)) <= 32)
            return "Could not start the game.";
    }
    return "Starting Crab Champions through Steam. While this window is open, it watches for crashes.";
}

// --- Window ------------------------------------------------------------------------------------------------

enum : int {
    IdGame = 100, IdBrowse, IdList, IdInstall, IdToggle, IdRemove, IdRefresh, IdIni, IdSave, IdReload, IdCopyCode,
    IdPasteCode, IdOpenFolder, IdEditor, IdStatus, IdLaunch, IdCollect,
};
constexpr UINT_PTR kCrashTimer = 1;
constexpr UINT WM_APP_RELEASES = WM_APP + 1, WM_APP_DONE = WM_APP + 2;

HWND g_wnd, g_gameEdit, g_list, g_iniCombo, g_editor, g_status;
HFONT g_font, g_mono;
std::atomic<bool> g_busy{false};
int g_iniShown = -1;     // index into Mods() of the ini in the editor
bool g_editorDirty = false;
bool g_loadingEditor = false;

void SetStatus(const std::string& s) { SetWindowTextW(g_status, Wide(s).c_str()); }

std::wstring OwnConfig() { return ExeDir() + L"CrabModsManager.ini"; }

std::string g_lastCrash;  // the crash folder last reported (or seen), so each crash is offered once

void SaveConfig() { WriteFile(OwnConfig(), "game=" + Utf8(g_game) + "\nlastCrash=" + g_lastCrash + "\n"); }
std::wstring LoadGameDir() {
    auto t = ReadFile(OwnConfig());
    if (!t) return {};
    IniValues v = ReadIni(*t);
    g_lastCrash = v[""]["lastCrash"];
    return NormalizeGameDir(Wide(v[""]["game"]));
}

int Selected() { return int(SendMessageW(g_list, LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED)); }

void SetCell(int row, int col, const std::string& text) {
    std::wstring w = Wide(text);
    LVITEMW it{};
    it.iSubItem = col;
    it.pszText = w.data();
    SendMessageW(g_list, LVM_SETITEMTEXTW, row, LPARAM(&it));
}

void UpdateButtons() {
    int sel = Selected();
    bool busy = g_busy, have = !g_game.empty();
    bool installed = have && sel >= 0 && StateOf(Mods()[sel]) != State::Missing;
    bool canInstall = have && sel >= 0 && g_latest.count(Mods()[sel].name) &&
                      (!installed || UpdateAvailable(Mods()[sel]));
    const wchar_t* label = installed ? L"Update" : L"Install";
    SetWindowTextW(GetDlgItem(g_wnd, IdInstall), label);
    EnableWindow(GetDlgItem(g_wnd, IdInstall), !busy && canInstall);
    EnableWindow(GetDlgItem(g_wnd, IdToggle), !busy && installed);
    SetWindowTextW(GetDlgItem(g_wnd, IdToggle),
                   installed && StateOf(Mods()[sel]) == State::Disabled ? L"Switch on" : L"Switch off");
    EnableWindow(GetDlgItem(g_wnd, IdRemove), !busy && installed);
    EnableWindow(GetDlgItem(g_wnd, IdRefresh), !busy);
    bool ini = have && g_iniShown >= 0;
    for (int id : {IdSave, IdReload, IdCopyCode, IdPasteCode}) EnableWindow(GetDlgItem(g_wnd, id), ini);
    EnableWindow(GetDlgItem(g_wnd, IdOpenFolder), have);
    EnableWindow(GetDlgItem(g_wnd, IdLaunch), have);
}

void RefreshList() {
    for (size_t i = 0; i < Mods().size(); i++) {
        const ModInfo& m = Mods()[i];
        State st = StateOf(m);
        std::string inst = st == State::Missing ? "-" : InstalledVersion(m);
        if (inst == "?") inst = "older";
        auto it = g_latest.find(m.name);
        std::string latest = it != g_latest.end() ? it->second.version : g_releasesLoaded ? "-" : "checking...";
        std::string state = g_game.empty()                ? "game not found"
                            : st == State::Missing         ? "not installed"
                            : st == State::Disabled        ? "switched off"
                            : UpdateAvailable(m)           ? "update available"
                                                           : "on";
        SetCell(int(i), 1, inst);
        SetCell(int(i), 2, latest);
        SetCell(int(i), 3, state);
        SetCell(int(i), 4, m.description);
    }
    UpdateButtons();
}

bool ConfirmDiscard() {
    if (!g_editorDirty) return true;
    return MessageBoxW(g_wnd, L"The settings have unsaved changes. Discard them?", kTitle,
                       MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void ShowIni(int index) {
    g_iniShown = index;
    std::wstring text;
    if (index >= 0 && !g_game.empty()) {
        const ModInfo& m = Mods()[index];
        if (auto t = ReadFile(g_game + L"\\" + Wide(m.ini)))
            text = Wide(*t);
        else
            text = L"# " + Wide(m.ini) + L" does not exist yet. " + Wide(m.name) +
                   L" writes it, with every setting explained, the first time the game starts with it installed.\n";
    }
    g_loadingEditor = true;
    SetWindowTextW(g_editor, ToCrLf(text).c_str());
    g_loadingEditor = false;
    g_editorDirty = false;
    SendMessageW(g_iniCombo, CB_SETCURSEL, index, 0);
    UpdateButtons();
}

std::wstring EditorText() {
    int n = GetWindowTextLengthW(g_editor);
    std::wstring w(n + 1, L'\0');
    GetWindowTextW(g_editor, w.data(), n + 1);
    w.resize(n);
    return FromCrLf(w);
}

void SetGameDir(const std::wstring& dir) {
    g_game = dir;
    SetWindowTextW(g_gameEdit, dir.empty() ? L"(not found: press Browse and pick the Crab Champions folder)" : dir.c_str());
    ShowIni(g_iniShown < 0 ? 0 : g_iniShown);
    RefreshList();
}

void FetchReleases() {
    g_busy = true;
    UpdateButtons();
    SetStatus("Checking GitHub for releases...");
    std::thread([] {
        std::string error;
        auto body = HttpGet(L"https://api.github.com/repos/" + Wide(kRepo) + L"/releases?per_page=100", error);
        auto* result = new std::pair<std::optional<std::string>, std::string>(std::move(body), error);
        PostMessageW(g_wnd, WM_APP_RELEASES, 0, LPARAM(result));
    }).detach();
}

void RunJob(std::string (*job)(const ModInfo&), const ModInfo& m) {
    g_busy = true;
    UpdateButtons();
    SetStatus("Working on " + std::string(m.name) + "...");
    std::thread([job, &m] { PostMessageW(g_wnd, WM_APP_DONE, 0, LPARAM(new std::string(job(m)))); }).detach();
}

bool SetClipboard(const std::wstring& text) {
    if (!OpenClipboard(g_wnd)) return false;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    bool ok = false;
    if (h) {
        memcpy(GlobalLock(h), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(h);
        ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
        if (!ok) GlobalFree(h);
    }
    CloseClipboard();
    return ok;
}

std::wstring GetClipboard() {
    std::wstring text;
    if (!OpenClipboard(g_wnd)) return text;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            text = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return text;
}

// Says what was saved and offers to report it: Yes opens a filled-in GitHub issue and the report's folder (to drag
// the zip into the issue), No only the folder.
void OfferReport(const BuiltReport& b) {
    if (!b.error.empty()) {
        SetStatus("Could not save the report: " + b.error);
        return;
    }
    const CrashReport& r = b.report;
    std::wstring msg;
    if (r.crashed)
        msg = L"Crab Champions crashed (" + Wide(r.when) + L")" +
              (r.error.empty() ? L"" : L":\n" + Wide(r.error) + L" at " + Wide(r.callStack.substr(0, r.callStack.find('\n')))) +
              L"\n\n";
    msg += L"The mod logs and settings" + std::wstring(r.crashed ? L" and the game's crash report" : L"") +
           L" are saved, with player names, user folders and machine ids taken out, in:\n" + b.zip +
           L"\n\nReport it on GitHub?\n\nYes: opens a filled-in issue in your browser (you need a GitHub account) "
           L"and the folder with the zip, to drag into the issue.\nNo: only opens the folder (report.md in it has the "
           L"same text, to post anywhere else).";
    SetStatus("Saved " + Utf8(b.zip));
    int answer = MessageBoxW(g_wnd, msg.c_str(), kTitle,
                             MB_YESNOCANCEL | MB_SETFOREGROUND | (r.crashed ? MB_ICONWARNING : MB_ICONINFORMATION));
    if (answer == IDYES) OpenExternal(Wide(IssueUrl(r)));
    if (answer == IDYES || answer == IDNO) OpenExternal(b.folder);
}

// Every few seconds: a crash folder the manager hasn't offered yet, once it has stopped growing (the crash
// reporter writes it over a few seconds). Ones from before the manager was used, over 3 days old, are skipped.
void CheckForCrash() {
    static bool busy = false;
    static std::string pending;
    static ULONGLONG pendingSize = 0;
    if (busy) return;
    auto crash = NewestCrash();
    if (!crash || crash->name == g_lastCrash) return;
    if (crash->time + 3 * 24 * 3600 * kSecond < Now()) {
        g_lastCrash = crash->name;
        SaveConfig();
        return;
    }
    if (pending != crash->name || pendingSize != crash->size) {
        pending = crash->name;
        pendingSize = crash->size;
        return;
    }
    busy = true;
    g_lastCrash = crash->name;
    SaveConfig();
    OfferReport(BuildReport(crash));
    busy = false;
}

void OnCommand(int id, int code) {
    if (id >= 0 && g_busy && id != IdEditor) return;
    int sel = Selected();
    switch (id) {
    case IdBrowse: {
        BROWSEINFOW bi{};
        bi.hwndOwner = g_wnd;
        bi.lpszTitle = L"Pick the Crab Champions folder (or its CrabChampions\\Binaries\\Win64 folder)";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                std::wstring dir = NormalizeGameDir(path);
                if (dir.empty()) {
                    MessageBoxW(g_wnd, (L"Crab Champions is not in that folder (no " + std::wstring(kGameExe) + L").").c_str(),
                                kTitle, MB_ICONWARNING);
                } else if (ConfirmDiscard()) {
                    SetGameDir(dir);
                    SaveConfig();
                }
            }
            CoTaskMemFree(pidl);
        }
        break;
    }
    case IdInstall:
        if (sel >= 0) RunJob(Install, Mods()[sel]);
        break;
    case IdToggle:
        if (sel >= 0) {
            SetStatus(Toggle(Mods()[sel]));
            RefreshList();
        }
        break;
    case IdRemove:
        if (sel >= 0) {
            const ModInfo& m = Mods()[sel];
            std::wstring q = L"Remove " + Wide(m.name) + L" from the game? Its settings file stays.";
            if (MessageBoxW(g_wnd, q.c_str(), kTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                SetStatus(Uninstall(m));
                RefreshList();
            }
        }
        break;
    case IdRefresh:
        FetchReleases();
        break;
    case IdIni:
        if (code == CBN_SELCHANGE) {
            int i = int(SendMessageW(g_iniCombo, CB_GETCURSEL, 0, 0));
            if (i != g_iniShown && ConfirmDiscard()) ShowIni(i);
            else SendMessageW(g_iniCombo, CB_SETCURSEL, g_iniShown, 0);
        }
        break;
    case IdEditor:
        if (code == EN_CHANGE && !g_loadingEditor) g_editorDirty = true;
        break;
    case IdSave:
        if (g_iniShown >= 0) {
            const ModInfo& m = Mods()[g_iniShown];
            if (WriteFile(g_game + L"\\" + Wide(m.ini), Utf8(EditorText()))) {
                g_editorDirty = false;
                SetStatus(std::string("Saved ") + m.ini + ". It is read when the game starts.");
            } else {
                SetStatus(std::string("Could not save ") + m.ini + ".");
            }
        }
        break;
    case IdReload:
        if (ConfirmDiscard()) ShowIni(g_iniShown);
        break;
    case IdCopyCode:
        if (g_iniShown >= 0) {
            std::string code = MakeShareCode(Mods()[g_iniShown].ini, Utf8(EditorText()));
            SetStatus(SetClipboard(Wide(code)) ? "Share code copied. Anyone can paste it into their CrabMods Manager."
                                               : "Could not use the clipboard.");
        }
        break;
    case IdPasteCode: {
        auto shared = ReadShareCode(Utf8(GetClipboard()));
        if (!shared) {
            MessageBoxW(g_wnd, L"Copy a share code (it starts with CRAB1-) first, then press this again.", kTitle,
                        MB_ICONINFORMATION);
            break;
        }
        int target = -1;
        for (size_t i = 0; i < Mods().size(); i++)
            if (shared->iniName == Mods()[i].ini) target = int(i);
        if (target < 0) {
            MessageBoxW(g_wnd, (L"That code is for " + Wide(shared->iniName) + L", which this manager does not know.").c_str(),
                        kTitle, MB_ICONWARNING);
            break;
        }
        if (target != g_iniShown) {
            if (!ConfirmDiscard()) break;
            ShowIni(target);
        }
        std::string merged = MergeIni(Utf8(EditorText()), shared->settings);
        SetWindowTextW(g_editor, ToCrLf(Wide(merged)).c_str());
        g_editorDirty = true;
        SetStatus("Settings from the share code are in the editor. Press Save to keep them.");
        break;
    }
    case IdOpenFolder:
        ShellExecuteW(g_wnd, L"open", g_game.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IdLaunch:
        SetStatus(LaunchGame());
        break;
    case IdCollect: {
        // The newest crash report goes in too if it's from the last day.
        auto crash = NewestCrash();
        if (crash && crash->time + 24 * 3600 * kSecond < Now()) crash.reset();
        OfferReport(BuildReport(crash));
        break;
    }
    }
}

HWND Make(const wchar_t* cls, const wchar_t* text, DWORD style, int id, DWORD ex = 0) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, g_wnd, HMENU(INT_PTR(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(h, WM_SETFONT, WPARAM(g_font), TRUE);
    return h;
}

void Layout() {
    RECT rc;
    GetClientRect(g_wnd, &rc);
    int w = rc.right, h = rc.bottom, m = 10, bh = 26, y = m;
    auto place = [](int id, int x, int y, int cx, int cy) { MoveWindow(GetDlgItem(g_wnd, id), x, y, cx, cy, TRUE); };
    place(IdGame - 1, m, y + 5, 90, 20);
    place(IdGame, m + 95, y, w - 2 * m - 95 - 100, bh);
    place(IdBrowse, w - m - 95, y, 95, bh);
    y += bh + m;
    int listH = 110;
    place(IdList, m, y, w - 2 * m, listH);
    y += listH + 6;
    int x = m;
    for (int id : {IdInstall, IdToggle, IdRemove, IdRefresh}) {
        place(id, x, y, 115, bh);
        x += 121;
    }
    place(IdLaunch, w - m - 120 - 6 - 120, y, 120, bh);
    place(IdCollect, w - m - 120, y, 120, bh);
    y += bh + m + 4;
    place(IdIni - 1, m, y + 5, 60, 20);
    place(IdIni, m + 65, y, 150, 200);
    x = m + 225;
    for (int id : {IdSave, IdReload, IdCopyCode, IdPasteCode, IdOpenFolder}) {
        int cw = id == IdCopyCode || id == IdPasteCode ? 130 : 90;
        place(id, x, y, cw, bh);
        x += cw + 6;
    }
    y += bh + 6;
    int statusH = 22;
    place(IdEditor, m, y, w - 2 * m, h - y - statusH - m);
    place(IdStatus, m, h - statusH - 4, w - 2 * m, statusH);
}

void Create() {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    g_mono = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                         FIXED_PITCH | FF_MODERN, L"Consolas");

    Make(L"STATIC", L"Game folder:", 0, IdGame - 1);
    g_gameEdit = Make(L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY, IdGame, WS_EX_CLIENTEDGE);
    Make(L"BUTTON", L"Browse...", BS_PUSHBUTTON, IdBrowse);

    g_list = Make(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, IdList, WS_EX_CLIENTEDGE);
    SendMessageW(g_list, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    const std::pair<const wchar_t*, int> cols[] = {{L"Mod", 90}, {L"Installed", 75}, {L"Latest", 70},
                                                   {L"Status", 115}, {L"What it does", 520}};
    for (int i = 0; i < 5; i++) {
        LVCOLUMNW c{};
        c.mask = LVCF_TEXT | LVCF_WIDTH;
        c.pszText = const_cast<wchar_t*>(cols[i].first);
        c.cx = cols[i].second;
        SendMessageW(g_list, LVM_INSERTCOLUMNW, i, LPARAM(&c));
    }
    for (size_t i = 0; i < Mods().size(); i++) {
        std::wstring name = Wide(Mods()[i].name);
        LVITEMW it{};
        it.mask = LVIF_TEXT;
        it.iItem = int(i);
        it.pszText = name.data();
        SendMessageW(g_list, LVM_INSERTITEMW, 0, LPARAM(&it));
    }
    LVITEMW sel{};
    sel.stateMask = sel.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(g_list, LVM_SETITEMSTATE, 0, LPARAM(&sel));

    Make(L"BUTTON", L"Install", BS_PUSHBUTTON, IdInstall);
    Make(L"BUTTON", L"Switch off", BS_PUSHBUTTON, IdToggle);
    Make(L"BUTTON", L"Remove", BS_PUSHBUTTON, IdRemove);
    Make(L"BUTTON", L"Check for updates", BS_PUSHBUTTON, IdRefresh);
    Make(L"BUTTON", L"Launch game", BS_PUSHBUTTON, IdLaunch);
    Make(L"BUTTON", L"Collect logs...", BS_PUSHBUTTON, IdCollect);

    Make(L"STATIC", L"Settings:", 0, IdIni - 1);
    g_iniCombo = Make(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, IdIni);
    for (const ModInfo& m : Mods()) SendMessageW(g_iniCombo, CB_ADDSTRING, 0, LPARAM(Wide(m.ini).c_str()));
    Make(L"BUTTON", L"Save", BS_PUSHBUTTON, IdSave);
    Make(L"BUTTON", L"Reload", BS_PUSHBUTTON, IdReload);
    Make(L"BUTTON", L"Copy share code", BS_PUSHBUTTON, IdCopyCode);
    Make(L"BUTTON", L"Paste share code", BS_PUSHBUTTON, IdPasteCode);
    Make(L"BUTTON", L"Open folder", BS_PUSHBUTTON, IdOpenFolder);
    g_editor = Make(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN,
                    IdEditor, WS_EX_CLIENTEDGE);
    SendMessageW(g_editor, WM_SETFONT, WPARAM(g_mono), TRUE);
    SendMessageW(g_editor, EM_SETLIMITTEXT, 1 << 20, 0);
    g_status = Make(L"STATIC", L"", SS_LEFTNOWORDWRAP, IdStatus);
    Layout();

    std::wstring dir = LoadGameDir();
    if (dir.empty()) dir = FindGameDir();
    SetGameDir(dir);
    FetchReleases();
    SetTimer(g_wnd, kCrashTimer, 3000, nullptr);
}

LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_wnd = w;
        Create();
        return 0;
    case WM_SIZE:
        Layout();
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {800, 480};
        return 0;
    case WM_COMMAND:
        OnCommand(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_TIMER:
        if (wp == kCrashTimer && !g_busy) CheckForCrash();
        return 0;
    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(lp)->idFrom == IdList && reinterpret_cast<NMHDR*>(lp)->code == LVN_ITEMCHANGED)
            UpdateButtons();
        return 0;
    case WM_APP_RELEASES: {
        std::unique_ptr<std::pair<std::optional<std::string>, std::string>> r(
            reinterpret_cast<std::pair<std::optional<std::string>, std::string>*>(lp));
        g_busy = false;
        g_releasesLoaded = true;
        if (r->first) {
            g_latest = LatestReleases(*r->first);
            std::string s = "Latest on GitHub:";
            for (const ModInfo& m : Mods())
                s += " " + std::string(m.name) + " " + (g_latest.count(m.name) ? g_latest[m.name].version : "-") + ".";
            SetStatus(s);
        } else {
            SetStatus("Could not reach GitHub: " + r->second);
        }
        RefreshList();
        return 0;
    }
    case WM_APP_DONE: {
        std::unique_ptr<std::string> s(reinterpret_cast<std::string*>(lp));
        g_busy = false;
        SetStatus(*s);
        RefreshList();
        if (g_iniShown >= 0 && !g_editorDirty) ShowIni(g_iniShown);
        return 0;
    }
    case WM_CLOSE:
        if (!ConfirmDiscard()) return 0;
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

// "CrabModsManager.exe --selftest": no window. Finds the game, reads GitHub and the installed versions, and
// writes what it found to CrabModsManager-selftest.txt next to the exe. With --report it also saves a report of
// the newest crash (or of the logs) as "Collect logs" does, and adds the issue address it would open.
int SelfTest(bool report) {
    std::string r = std::string("CrabMods Manager ") + kVersion + "\n";
    g_game = LoadGameDir();
    if (g_game.empty()) g_game = FindGameDir();
    r += "game: " + (g_game.empty() ? std::string("not found") : Utf8(g_game)) + "\n";
    std::string error;
    auto body = HttpGet(L"https://api.github.com/repos/" + Wide(kRepo) + L"/releases?per_page=100", error);
    r += body ? "releases: " + std::to_string(body->size()) + " bytes\n" : "releases: failed, " + error + "\n";
    if (body) g_latest = LatestReleases(*body);
    for (const ModInfo& m : Mods()) {
        auto it = g_latest.find(m.name);
        State st = StateOf(m);
        r += std::string(m.name) + ": latest " + (it != g_latest.end() ? it->second.version : "-") + ", installed " +
             (st == State::Missing ? "no" : InstalledVersion(m) + (st == State::Disabled ? " (off)" : "")) +
             (UpdateAvailable(m) ? ", update available" : "") + "\n";
    }
    for (const SavedDir& d : SavedDirs()) r += "saved: " + Utf8(d.path) + (d.proton ? " (proton)" : "") + "\n";
    auto crash = NewestCrash();
    r += "newest crash: " + (crash ? crash->name + " " + LocalTime(crash->time, false) : std::string("none")) + "\n";
    if (report) {
        BuiltReport b = BuildReport(crash);
        r += "report: " + (b.error.empty() ? Utf8(b.zip) : b.error) + "\nissue: " + IssueUrl(b.report) + "\n";
    }
    WriteFile(ExeDir() + L"CrabModsManager-selftest.txt", r);
    return body ? 0 : 1;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    if (cmd && wcsstr(cmd, L"--selftest")) return SelfTest(wcsstr(cmd, L"--report") != nullptr);
    INITCOMMONCONTROLSEX icc{sizeof icc, ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = HBRUSH(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"CrabModsManager";
    RegisterClassW(&wc);
    std::wstring title = std::wstring(kTitle) + L" " + Wide(kVersion);
    HWND w = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             900, 640, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
