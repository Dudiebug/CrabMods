// CrabMods Manager, the Windows side of platform.hpp: the registry, WinHTTP, processes, the shell. The same exe
// runs under wine (Proton) for Linux players who prefer it; there the Linux Steam is reached through Z:.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <winhttp.h>

#include <tuple>

#include "platform.hpp"

namespace mgr::plat {

HWND g_window = nullptr;  // set by win_main.cpp

namespace {

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

std::wstring Env(const wchar_t* name) {
    wchar_t buf[1024];
    DWORD n = GetEnvironmentVariableW(name, buf, 1024);
    return n && n < 1024 ? std::wstring(buf, n) : std::wstring();
}

std::wstring RegString(HKEY root, const wchar_t* key, const wchar_t* value) {
    wchar_t buf[1024];
    DWORD size = sizeof buf;
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return {};
    return buf;
}

// A path from Steam's files: a Linux one ("/home/me/...") is "Z:\home\me\..." under wine.
fs::path WinePath(const std::string& p) {
    std::string s = p;
    for (char& c : s)
        if (c == '/') c = '\\';
    if (!p.empty() && p[0] == '/') s = "Z:" + s;
    return PathU8(s);
}

std::string HeaderValue(HINTERNET r, const wchar_t* name) {
    wchar_t buf[512];
    DWORD size = sizeof buf;
    if (!WinHttpQueryHeaders(r, WINHTTP_QUERY_CUSTOM, name, buf, &size, WINHTTP_NO_HEADER_INDEX)) return {};
    return Utf8(std::wstring(buf, size / sizeof(wchar_t)));
}

}  // namespace

bool RunningOnWine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version");
}
bool NativeLinux() { return false; }
std::string PlatformName() { return RunningOnWine() ? "Linux (Proton)" : "Windows"; }

fs::path ExePath() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(std::size(buf)));
    return fs::path(std::wstring(buf, n));
}
// Portable: everything sits next to the exe, as in every manager before.
fs::path DataDir() { return ExePath().parent_path(); }
fs::path CacheDir() { return DataDir() / "cache"; }

std::vector<fs::path> SteamRoots() {
    std::vector<fs::path> roots;
    for (auto [root, key, value] : {std::tuple{HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath"},
                                    std::tuple{HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath"}})
        if (std::wstring p = RegString(root, key, value); !p.empty()) roots.push_back(WinePath(Utf8(p)));
    if (RunningOnWine())
        for (const wchar_t* var : {L"HOME", L"USER", L"USERNAME"}) {
            std::wstring v = Env(var);
            if (v.empty()) continue;
            fs::path home = v[0] == L'/' ? WinePath(Utf8(v)) : fs::path(L"Z:\\home\\" + v);
            roots.push_back(home / ".local" / "share" / "Steam");
            roots.push_back(home / ".steam" / "steam");
            roots.push_back(home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam");
        }
    roots.push_back(L"C:\\Program Files (x86)\\Steam");
    return roots;
}

fs::path LibraryPath(const std::string& steamPath) { return WinePath(steamPath); }

fs::path FindGame() {
    if (fs::path here = NormalizeGameDir(ExePath().parent_path()); !here.empty()) return here;
    return FindGameIn(SteamRoots(), LibraryPath);
}

// The game's Saved folder in this Windows profile, and for Proton the one in the game's own prefix (the manager
// may run in another one).
std::vector<SavedDir> SavedDirs(const fs::path& game) {
    std::vector<SavedDir> dirs;
    if (std::wstring local = Env(L"LOCALAPPDATA"); !local.empty())
        dirs.push_back({fs::path(local) / "CrabChampions" / "Saved", RunningOnWine()});
    if (fs::path p = ProtonSaved(game); !p.empty()) dirs.push_back({p, true});
    std::erase_if(dirs, [](const SavedDir& d) { return !Exists(d.path); });
    return dirs;
}

std::vector<std::string> Secrets() {
    std::vector<std::string> secrets;
    for (const wchar_t* var : {L"USERNAME", L"USER", L"LOGNAME", L"COMPUTERNAME", L"HOSTNAME"})
        if (std::wstring v = Env(var); !v.empty()) secrets.push_back(Utf8(v));
    wchar_t buf[256];
    DWORD n = 256;
    if (GetComputerNameW(buf, &n)) secrets.push_back(Utf8(std::wstring(buf, n)));
    n = 256;
    if (GetUserNameW(buf, &n) && n > 1) secrets.push_back(Utf8(std::wstring(buf, n - 1)));
    std::erase_if(secrets, [](const std::string& s) { return s == "steamuser"; });  // Proton's, the same for all
    return secrets;
}

// On Windows; under wine only in the same prefix.
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

HttpResponse Http(const std::string& url, const std::string& etag, const std::string& accept) {
    HttpResponse out;
    std::wstring wurl = Wide(url);
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof u;
    wchar_t host[256], path[4096];
    u.lpszHostName = host;
    u.dwHostNameLength = DWORD(std::size(host));
    u.lpszUrlPath = path;
    u.dwUrlPathLength = DWORD(std::size(path));
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &u)) {
        out.error = "bad address";
        return out;
    }
    std::string ua = std::string("CrabModsManager/") + kVersion;
    HINTERNET s = WinHttpOpen(Wide(ua).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(Wide(ua).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (s) WinHttpSetTimeouts(s, 15000, 15000, 30000, 60000);
    HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                    : nullptr;
    std::wstring headers = L"Accept: " + Wide(accept) + L"\r\n";
    if (!etag.empty()) headers += L"If-None-Match: " + Wide(etag) + L"\r\n";
    if (r && WinHttpSendRequest(r, headers.c_str(), DWORD(-1), nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, size = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
        out.status = int(status);
        out.etag = HeaderValue(r, L"ETag");
        if (std::string v = HeaderValue(r, L"X-RateLimit-Remaining"); !v.empty()) out.rateRemaining = std::atoll(v.c_str());
        if (std::string v = HeaderValue(r, L"X-RateLimit-Reset"); !v.empty()) out.rateReset = std::atoll(v.c_str());
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(r, &avail) && avail) {
            std::string chunk(avail, '\0');
            DWORD got = 0;
            if (!WinHttpReadData(r, chunk.data(), avail, &got)) break;
            out.body.append(chunk, 0, got);
        }
    } else {
        out.error = "no connection (error " + std::to_string(GetLastError()) + ")";
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return out;
}

// Under wine the Linux program opens it, through winebrowser.
void OpenExternal(const std::string& target) {
    std::wstring t = Wide(target);
    if (RunningOnWine()) ShellExecuteW(nullptr, L"open", L"winebrowser.exe", (L"\"" + t + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
    else ShellExecuteW(nullptr, L"open", t.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// Through Steam (so the overlay, cloud saves and multiplayer work); without Steam, the game itself.
std::string LaunchGame(const fs::path& game) {
    std::wstring url = L"steam://rungameid/" + Wide(kAppId);
    if (RunningOnWine()) {
        OpenExternal(Utf8(url));
    } else if (INT_PTR(ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32) {
        if (game.empty()) return "Could not start Steam, and the game folder is not known.";
        fs::path exe = game / kGameExe;
        if (INT_PTR(ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, game.c_str(), SW_SHOWNORMAL)) <= 32)
            return "Could not start the game.";
    }
    return "Starting Crab Champions through Steam. While this window is open, it watches for crashes.";
}

std::optional<fs::path> PickFolder(const std::string& title) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return std::nullopt;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(Wide(title).c_str());
    fs::path picked;
    if (SUCCEEDED(dlg->Show(g_window))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                picked = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    return picked;
}

void Wake() {
    if (g_window) PostMessageW(g_window, WM_APP, 0, 0);
}

void Attention() {
    if (!g_window || GetForegroundWindow() == g_window) return;
    FLASHWINFO f{sizeof f, g_window, FLASHW_ALL | FLASHW_TIMERNOFG, 0, 0};
    FlashWindowEx(&f);
}

const char* ManagerAsset() { return kManagerExe; }
fs::path StagedUpdate() { return DataDir() / "update" / kManagerExe; }

// Windows lets a running exe be renamed, not replaced: the old one moves aside (deleted at the next start) and the
// new one takes its name.
bool SwapInUpdate(std::string& error) {
    fs::path exe = ExePath(), old = exe, staged = StagedUpdate();
    old += ".old";
    if (!Exists(staged)) {
        error = "the downloaded update is gone";
        return false;
    }
    DeleteFileW(old.c_str());
    if (!MoveFileExW(exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        error = "could not move the running manager aside (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    if (!MoveFileExW(staged.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        error = "could not put the new manager in place (error " + std::to_string(GetLastError()) + ")";
        MoveFileExW(old.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING);
        return false;
    }
    std::error_code ec;
    fs::path v = staged;
    v += ".version";
    fs::remove(v, ec);
    fs::remove(staged.parent_path(), ec);  // only if empty
    return true;
}

bool Relaunch(const std::vector<std::string>& args) {
    std::wstring cmd = L"\"" + ExePath().wstring() + L"\"";
    for (const std::string& a : args) cmd += L" \"" + Wide(a) + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::wstring dir = DataDir().wstring();
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

}  // namespace mgr::plat
