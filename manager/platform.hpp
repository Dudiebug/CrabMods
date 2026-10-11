// CrabMods Manager, the Windows side: files, the game folder, downloads, crash reports, starting the game.
// Included by main.cpp only.
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <winhttp.h>

#include <cwchar>
#include <fstream>
#include <tuple>

#include "core.hpp"

namespace mgr {

constexpr const char kVersion[] = "0.3.0";

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

// Every place Steam may be installed: the registry, and under Proton or wine the Linux Steam through Z:.
std::vector<std::wstring> SteamRoots() {
    std::vector<std::wstring> steamRoots;
    for (auto [root, key, value] : {std::tuple{HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath"}})
        if (std::wstring p = RegString(root, key, value); !p.empty()) steamRoots.push_back(WinePath(Utf8(p)));
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
    return steamRoots;
}

std::wstring FindGameDir() {
    if (std::wstring here = NormalizeGameDir(ExeDir()); !here.empty()) return here;
    for (const auto& root : SteamRoots()) {
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

inline std::wstring g_game;  // the game's Win64 folder, empty until found

std::wstring DllPath(const char* dll) { return g_game + L"\\" + Wide(dll); }
std::wstring FileName(const std::wstring& p) { return p.substr(p.find_last_of(L"\\/") + 1); }

// "0.3.0" from a program's version resource; "" without the file, "?" if it has none (releases before the
// manager).
std::string FileVersion(const std::wstring& p) {
    if (!Exists(p)) return {};
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

// Replaces `to` with a copy of `from`, through a temporary file so a failure leaves it as it was. Windows refuses
// to replace a DLL the running game has loaded; Linux lets it, and the game keeps the old one until it restarts.
bool CopyInto(const std::wstring& from, const std::wstring& to, std::string& error) {
    std::wstring tmp = to + L".download";
    if (!CopyFileW(from.c_str(), tmp.c_str(), FALSE)) {
        error = "could not write to " + Utf8(to.substr(0, to.find_last_of(L'\\')));
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileW(tmp.c_str());
        error = "could not replace " + Utf8(FileName(to)) + " while the game is running";
        return false;
    }
    return true;
}

// Whether Crab Champions runs (on Windows; under wine only in the same prefix).
bool GameRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
        found = _wcsicmp(pe.szExeFile, L"CrabChampions-Win64-Shipping.exe") == 0;
    CloseHandle(snap);
    return found;
}

// On Linux the game needs WINEDLLOVERRIDES in its Steam launch options to load the mod loader. Reads them from
// Steam's localconfig.vdf: true if set, false if not, nothing when Steam's files aren't found.
std::optional<bool> LaunchOptionSet() {
    std::optional<bool> result;
    for (const auto& root : SteamRoots()) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((root + L"\\userdata\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.cFileName[0] == L'.') continue;
            auto vdf = ReadFile(root + L"\\userdata\\" + fd.cFileName + L"\\config\\localconfig.vdf");
            if (!vdf) continue;
            if (!result) result = false;
            // "774801" { ... "LaunchOptions" "..." ... }: the app's own block, up to its closing brace.
            for (size_t i = vdf->find("\"774801\""); i != std::string::npos; i = vdf->find("\"774801\"", i + 1)) {
                size_t open = vdf->find('{', i), next = vdf->find('"', i + 8);
                if (open == std::string::npos || (next != std::string::npos && next < open)) continue;
                int depth = 0;
                size_t end = open;
                for (; end < vdf->size(); end++) {
                    if ((*vdf)[end] == '{') depth++;
                    if ((*vdf)[end] == '}' && --depth == 0) break;
                }
                std::string block = vdf->substr(open, end - open);
                size_t lo = block.find("\"LaunchOptions\"");
                if (lo != std::string::npos && block.find("xinput1_3", lo) != std::string::npos) result = true;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        if (result) break;
    }
    return result;
}

// Replaces `target` with the download, through a temporary file so a failed download leaves it untouched.
bool Download(const std::string& url, const std::wstring& target, std::string& error) {
    auto data = HttpGet(Wide(url), error);
    if (!data) return false;
    if (data->size() < 2 || (*data)[0] != 'M' || (*data)[1] != 'Z') {
        error = "the download is not a Windows program";
        return false;
    }
    std::wstring tmp = target + L".download";
    if (!WriteFile(tmp, *data)) {
        error = "could not write " + Utf8(tmp);
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
        std::string ver = g_game.empty() ? "" : FileVersion(DllPath(m.dll));
        v += std::string(m.name) + " " + (ver.empty() ? "not installed" : ver) + ", ";
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

}  // namespace mgr
