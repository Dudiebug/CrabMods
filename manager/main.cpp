// CrabMods Manager: a mod browser for Crab Champions. Every CrabMods item and CrabSync is a mod to install or
// uninstall, with its settings as real controls. It keeps the mods and itself up to date from this repository's
// GitHub releases, shares a whole setup as one short code, starts the game and, when it crashes, gathers the
// logs and the crash report for a GitHub issue. Dear ImGui on Direct3D 11; it runs under wine and Proton too.
#include "platform.hpp"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include <d3d11.h>

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using namespace mgr;

namespace {

constexpr const wchar_t kTitle[] = L"CrabMods Manager";
// The first versions whose DLL the manager may load to read its catalog. Older ones start hooking from DllMain.
constexpr const char kCatalogSince[] = "0.9.0", kSyncDefaultsSince[] = "1.0.5";

// --- The manager's own settings ------------------------------------------------------------------------------

struct Config {
    std::string lastCrash;      // the crash folder last reported (or seen), so each crash is offered once
    bool setupDone = false;     // the first start installed everything (or found mods installed already)
    bool autoMods = true;       // install mod updates as soon as they are out
    bool autoManager = true;    // update the manager itself
    bool linuxHintDone = false; // "I've set the launch option"
} g_cfg;

std::wstring OwnConfig() { return ExeDir() + L"CrabModsManager.ini"; }
std::wstring CacheDir() { return ExeDir() + L"cache"; }
std::wstring ExePath() {
    wchar_t buf[MAX_PATH];
    return std::wstring(buf, GetModuleFileNameW(nullptr, buf, MAX_PATH));
}

void SaveConfig() {
    auto b = [](bool v) { return v ? "true" : "false"; };
    WriteFile(OwnConfig(), "game=" + Utf8(g_game) + "\nlastCrash=" + g_cfg.lastCrash + "\nsetupDone=" + b(g_cfg.setupDone) +
                               "\nautoUpdateMods=" + b(g_cfg.autoMods) + "\nautoUpdateManager=" + b(g_cfg.autoManager) +
                               "\nlinuxHintDone=" + b(g_cfg.linuxHintDone) + "\n");
}

std::wstring LoadConfig() {
    auto t = ReadFile(OwnConfig());
    if (!t) return {};
    IniValues all = ReadIni(*t);
    auto& v = all[""];
    auto flag = [&](const char* k, bool def) { return v.count(k) ? v[k] == "true" : def; };
    g_cfg.lastCrash = v["lastCrash"];
    // A manager from before 0.3.0 was already used: it doesn't install everything over what is there.
    g_cfg.setupDone = flag("setupDone", !v["game"].empty());
    g_cfg.autoMods = flag("autoUpdateMods", true);
    g_cfg.autoManager = flag("autoUpdateManager", true);
    g_cfg.linuxHintDone = flag("linuxHintDone", false);
    return NormalizeGameDir(Wide(v["game"]));
}

// --- Background jobs -------------------------------------------------------------------------------------------
// One worker thread runs them in order (downloads, file copies). Each returns a status line and, optionally,
// something to do on the UI thread afterwards.

struct JobResult {
    std::string status;
    bool error = false;
    std::function<void()> then;
};

HWND g_wnd;
std::mutex g_jobMutex;
std::condition_variable g_jobCv;
std::deque<std::pair<std::string, std::function<JobResult()>>> g_jobs;
std::deque<JobResult> g_results;
std::atomic<int> g_pending{0};  // queued or running
std::string g_jobLabel;         // what runs now (under g_jobMutex)

void Worker() {
    for (;;) {
        std::pair<std::string, std::function<JobResult()>> job;
        {
            std::unique_lock lock(g_jobMutex);
            g_jobCv.wait(lock, [] { return !g_jobs.empty(); });
            job = std::move(g_jobs.front());
            g_jobs.pop_front();
            g_jobLabel = job.first;
        }
        JobResult r = job.second();
        {
            std::lock_guard lock(g_jobMutex);
            g_results.push_back(std::move(r));
            g_jobLabel.clear();
        }
        g_pending--;
        PostMessageW(g_wnd, WM_APP, 0, 0);
    }
}

void Enqueue(const std::string& label, std::function<JobResult()> fn) {
    g_pending++;
    std::lock_guard lock(g_jobMutex);
    g_jobs.push_back({label, std::move(fn)});
    g_jobCv.notify_one();
}

std::string JobLabel() {
    std::lock_guard lock(g_jobMutex);
    return g_jobLabel;
}

// --- Status line -----------------------------------------------------------------------------------------------

std::string g_status;
bool g_statusError = false;
double g_statusTime = -100;

void SetStatus(const std::string& s, bool error = false) {
    g_status = s;
    g_statusError = error;
    g_statusTime = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0;
}

// --- What is installed ---------------------------------------------------------------------------------------

std::map<std::string, Release> g_latest;
bool g_releasesLoaded = false, g_fetching = false;
std::string g_releaseError;

struct ModFiles {
    std::string version;  // installed, "" if not
    std::string iniText;  // its settings file, "" if none yet
    IniValues ini;
};
ModFiles g_mods, g_sync;  // CrabMods, CrabSync

std::optional<Catalog> g_catalog;      // CrabMods' items
std::optional<CatalogItem> g_syncItem; // CrabSync
std::string g_catalogKey, g_syncKey;   // which DLL they were read from
bool g_waitingForGame = false;          // updates wait until the game is closed
bool g_launchAfterJobs = false;

const ModInfo& ModsInfo() { return Mods()[0]; }
const ModInfo& SyncInfo() { return Mods()[1]; }

// A string export of a mod DLL, read by loading a copy of it (so the original isn't held open). Only versions
// whose DllMain does nothing outside the game.
std::optional<std::string> ReadExport(const std::wstring& dll, const char* fn, const char* since) {
    std::string v = FileVersion(dll);
    if (v.empty() || v == "?" || CompareVersions(v, since) < 0) return std::nullopt;
    CreateDirectoryW(CacheDir().c_str(), nullptr);
    std::wstring copy = CacheDir() + L"\\read-" + std::to_wstring(GetCurrentProcessId()) + L".dll";
    if (!CopyFileW(dll.c_str(), copy.c_str(), FALSE)) return std::nullopt;
    std::optional<std::string> r;
    if (HMODULE h = LoadLibraryExW(copy.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) {
        if (auto f = reinterpret_cast<const char* (*)()>(GetProcAddress(h, fn)))
            if (const char* s = f()) r = s;
        FreeLibrary(h);
    }
    DeleteFileW(copy.c_str());
    return r;
}

std::string FileKey(const std::wstring& p) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &a)) return {};
    return Utf8(p) + "|" + std::to_string(a.nFileSizeLow) + "|" + std::to_string(Ticks(a.ftLastWriteTime));
}

// The catalog from the installed DLL, or from the latest one in the cache when it isn't installed (or too old).
template <class Parse>
void LoadCatalog(const char* dll, const char* fn, const char* since, std::string& key, Parse parse) {
    for (const std::wstring& p : {g_game.empty() ? std::wstring() : DllPath(dll), CacheDir() + L"\\" + Wide(dll)}) {
        if (p.empty() || !Exists(p)) continue;
        std::string v = FileVersion(p);
        if (v.empty() || v == "?" || CompareVersions(v, since) < 0) continue;
        std::string k = FileKey(p);
        if (k == key) return;
        if (auto text = ReadExport(p, fn, since); text && parse(*text)) {
            key = k;
            return;
        }
    }
}

void ReadMod(ModFiles& f, const ModInfo& m) {
    f.version = g_game.empty() ? "" : FileVersion(DllPath(m.dll));
    auto t = g_game.empty() ? std::nullopt : ReadFile(g_game + L"\\" + Wide(m.ini));
    f.iniText = t ? *t : "";
    f.ini = ReadIni(f.iniText);
}

void Rescan() {
    ReadMod(g_mods, ModsInfo());
    ReadMod(g_sync, SyncInfo());
    LoadCatalog(ModsInfo().dll, "CrabModsCatalog", kCatalogSince, g_catalogKey, [](const std::string& s) {
        auto c = ParseCatalog(s);
        if (c) g_catalog = std::move(c);
        return bool(g_catalog);
    });
    LoadCatalog(SyncInfo().dll, "CrabSyncDefaultIni", kSyncDefaultsSince, g_syncKey, [](const std::string& s) {
        g_syncItem = CrabSyncItem(s);
        return true;
    });
}

bool ModsInstalled() { return !g_mods.version.empty(); }
bool SyncInstalled() { return !g_sync.version.empty(); }

bool UpdateFor(const ModFiles& f, const ModInfo& m) {
    auto it = g_latest.find(m.name);
    return !f.version.empty() && it != g_latest.end() && (f.version == "?" || CompareVersions(f.version, it->second.version) < 0);
}

// --- Changing files ---------------------------------------------------------------------------------------------

// Merges settings (a compact ini) into a mod's settings file, making the file first (with every default, as the
// mod would) if it doesn't exist yet.
bool WriteSettings(const ModInfo& m, const std::string& compact) {
    if (g_game.empty()) return false;
    ModFiles& f = &m == &ModsInfo() ? g_mods : g_sync;
    std::string base = f.iniText;
    if (base.empty()) {
        if (&m == &ModsInfo() && g_catalog) base = DefaultCrabModsIni(*g_catalog);
        else if (&m == &SyncInfo() && g_syncItem) base = g_syncItem->ini;
    }
    std::string text = MergeIni(base, compact);
    if (!WriteFile(g_game + L"\\" + Wide(m.ini), text)) {
        SetStatus(std::string("Could not save ") + m.ini + ".", true);
        return false;
    }
    f.iniText = text;
    f.ini = ReadIni(text);
    return true;
}

// The latest release's file in the cache (downloading it when the cache has another version), for installing
// and for reading the catalog of a mod that isn't installed.
std::string Cache(const Release& rel, const char* file, bool versioned, std::wstring& path) {
    CreateDirectoryW(CacheDir().c_str(), nullptr);
    path = CacheDir() + L"\\" + Wide(file);
    if (Exists(path) && (!versioned || FileVersion(path) == rel.version)) return {};
    auto url = rel.assets.find(file);
    if (url == rel.assets.end()) return std::string("the release has no ") + file;
    std::string error;
    Download(url->second, path, error);
    return error;
}

// Worker: installs or updates a mod and the mod loader with it, from the cache.
JobResult InstallJob(const ModInfo& m, const Release& rel) {
    std::wstring dll, loader;
    std::string error = Cache(rel, m.dll, true, dll);
    if (error.empty()) error = Cache(rel, kLoaderDll, false, loader);
    if (error.empty()) {
        // The loader is replaced only when it differs (it rarely changes, and the game holds it open).
        auto have = ReadFile(DllPath(kLoaderDll)), want = ReadFile(loader);
        if (!have || !want || *have != *want) CopyInto(loader, DllPath(kLoaderDll), error);
    }
    if (error.empty()) CopyInto(dll, DllPath(m.dll), error);
    if (!error.empty()) return {std::string(m.name) + ": " + error, true};
    DeleteFileW((DllPath(m.dll) + L".disabled").c_str());  // switched off by manager 0.1 / 0.2
    return {std::string(m.name) + " " + rel.version + " installed."};
}

JobResult UninstallJob(const ModInfo& m) {
    DeleteFileW(DllPath(m.dll).c_str());
    DeleteFileW((DllPath(m.dll) + L".disabled").c_str());
    if (Exists(DllPath(m.dll))) return {std::string("Could not remove ") + m.dll + " while the game is running.", true};
    bool any = false;
    for (const ModInfo& o : Mods()) any |= Exists(DllPath(o.dll));
    if (!any) DeleteFileW(DllPath(kLoaderDll).c_str());
    return {std::string(m.name) + " uninstalled."};
}

void Install(const ModInfo& m, std::function<void()> then = {}) {
    auto it = g_latest.find(m.name);
    if (it == g_latest.end()) {
        SetStatus(g_releasesLoaded ? std::string("No release of ") + m.name + " on GitHub."
                                   : "Still checking GitHub for the mods. Try again in a moment.",
                  true);
        return;
    }
    Release rel = it->second;
    Enqueue("Installing " + std::string(m.name), [&m, rel, then] {
        JobResult r = InstallJob(m, rel);
        if (!r.error) r.then = then;
        return r;
    });
}

void Uninstall(const ModInfo& m) {
    Enqueue("Uninstalling " + std::string(m.name), [&m] { return UninstallJob(m); });
}

// Every item on or off, as one compact ini.
std::string AllItems(bool on, const std::string& except = "") {
    std::string s;
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items)
            if (it.toggle) s += "[" + it.section + "]\nenabled = " + ((it.section == except) != on ? "true" : "false") + "\n";
    return s;
}

void InstallEverything() {
    if (!ModsInstalled() || UpdateFor(g_mods, ModsInfo()))
        Install(ModsInfo(), [] { WriteSettings(ModsInfo(), AllItems(true)); });
    else
        WriteSettings(ModsInfo(), AllItems(true));
    if (!SyncInstalled()) Install(SyncInfo());
}

// An item: on or off in CrabMods.ini. Installing one without CrabMods installs CrabMods with that item only.
void SetItem(const CatalogItem& it, bool on) {
    std::string section = it.section, name = it.name;
    if (on && !ModsInstalled()) {
        Install(ModsInfo(), [section, name] {
            WriteSettings(ModsInfo(), AllItems(false, section));
            SetStatus(name + " installed.");
        });
        return;
    }
    if (WriteSettings(ModsInfo(), "[" + section + "]\nenabled = " + (on ? "true" : "false") + "\n"))
        SetStatus(name + (on ? " installed." : " uninstalled."));
}

// --- Updates ---------------------------------------------------------------------------------------------------

void CheckForUpdates();

// Swaps in the new manager next to the running one and starts it.
void RestartInto(const std::wstring& fresh) {
    std::wstring exe = ExePath(), old = exe + L".old";
    if (!MoveFileExW(exe.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING) ||
        !MoveFileExW(fresh.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING);
        SetStatus("Could not update the manager: its folder can't be written to.", true);
        return;
    }
    STARTUPINFOW si{sizeof si};
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + exe + L"\" --updated";
    if (CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, ExeDir().c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        PostQuitMessage(0);
    }
}

// After the release list arrives: the cache gets the latest DLLs (so the catalog of a mod that isn't
// installed can be shown), installed mods are updated, a first start installs everything, and the manager
// updates itself.
void AfterReleases() {
    for (const ModInfo* m : {&ModsInfo(), &SyncInfo()}) {
        auto it = g_latest.find(m->name);
        if (it == g_latest.end()) continue;
        Release rel = it->second;
        Enqueue("Downloading " + std::string(m->name), [m, rel] {
            std::wstring p;
            std::string e = Cache(rel, m->dll, true, p);
            return JobResult{e.empty() ? "" : std::string(m->name) + ": " + e, !e.empty()};
        });
    }
    // Queued after the downloads, so the catalog is there for it.
    Enqueue("", [] {
        return JobResult{"", false, [] {
            Rescan();
            if (g_game.empty()) return;
            if (!g_cfg.setupDone) {
                if (!ModsInstalled() && !SyncInstalled()) {
                    InstallEverything();
                    SetStatus("Installing every mod. Uninstall the ones you don't want.");
                }
                g_cfg.setupDone = true;
                SaveConfig();
            }
            if (!g_cfg.autoMods) return;
            bool any = UpdateFor(g_mods, ModsInfo()) || UpdateFor(g_sync, SyncInfo());
            g_waitingForGame = any && GameRunning();
            if (g_waitingForGame) return;
            if (UpdateFor(g_mods, ModsInfo())) Install(ModsInfo());
            if (UpdateFor(g_sync, SyncInfo())) Install(SyncInfo());
        }};
    });
    auto mgr = g_latest.find("Manager");
    if (g_cfg.autoManager && mgr != g_latest.end() && CompareVersions(mgr->second.version, kVersion) > 0) {
        Release rel = mgr->second;
        Enqueue("Updating the manager", [rel] {
            std::wstring fresh = ExePath() + L".new";
            std::string error;
            if (!Download(rel.assets.at(kManagerExe), fresh, error)) return JobResult{"Manager update: " + error, true};
            return JobResult{"", false, [fresh] { RestartInto(fresh); }};
        });
    }
}

void CheckForUpdates() {
    if (g_fetching) return;
    g_fetching = true;
    Enqueue("Checking for updates", [] {
        std::string error;
        auto body = HttpGet(L"https://api.github.com/repos/" + Wide(kRepo) + L"/releases?per_page=100", error);
        return JobResult{body ? "" : "Could not reach GitHub: " + error, !body, [body, error] {
            g_fetching = false;
            g_releasesLoaded = true;
            g_releaseError = body ? "" : error;
            if (!body) return;
            g_latest = LatestReleases(*body);
            AfterReleases();
        }};
    });
}

// --- Setups ----------------------------------------------------------------------------------------------------

Setup CurrentSetup() {
    Setup s;
    s.crabSync = SyncInstalled();
    s.crabMods = ModsInstalled();
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items) {
            s.on[it.section] = ItemOn(g_mods.ini, it);
            for (const Setting& st : it.settings) s.values[it.section][st.key] = ValueOf(g_mods.ini, it.section, st);
        }
    if (g_syncItem)
        for (const Setting& st : g_syncItem->settings) s.values["S"][st.key] = ValueOf(g_sync.ini, "", st);
    return s;
}

std::string CurrentCode() {
    static const Catalog none;
    return MakeSetupCode(g_catalog ? *g_catalog : none, g_syncItem ? &*g_syncItem : nullptr, CurrentSetup());
}

void ApplySetup(const Setup& s) {
    auto settings = [s] {
        if (s.crabMods && g_catalog) {
            std::vector<const CatalogItem*> items;
            for (const CatalogItem& it : g_catalog->items) items.push_back(&it);
            WriteSettings(ModsInfo(), SetupIni(s, items));
        }
        if (s.crabSync && g_syncItem) WriteSettings(SyncInfo(), SetupIni(s, {&*g_syncItem}));
        SetStatus("Setup applied. It is used the next time the game starts.");
    };
    if (s.crabMods && (!ModsInstalled() || UpdateFor(g_mods, ModsInfo()))) Install(ModsInfo());
    if (s.crabSync && !SyncInstalled()) Install(SyncInfo());
    if (!s.crabMods && ModsInstalled()) Uninstall(ModsInfo());
    if (!s.crabSync && SyncInstalled()) Uninstall(SyncInfo());
    Enqueue("Applying the setup", [settings] { return JobResult{"", false, settings}; });
}

// --- Crashes ---------------------------------------------------------------------------------------------------

// Says what was saved and offers to report it: Yes opens a filled-in GitHub issue and the report's folder (to drag
// the zip into the issue), No only the folder.
void OfferReport(const BuiltReport& b) {
    if (!b.error.empty()) {
        SetStatus("Could not save the report: " + b.error, true);
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
    if (!crash || crash->name == g_cfg.lastCrash) return;
    if (crash->time + 3 * 24 * 3600 * kSecond < Now()) {
        g_cfg.lastCrash = crash->name;
        SaveConfig();
        return;
    }
    if (pending != crash->name || pendingSize != crash->size) {
        pending = crash->name;
        pendingSize = crash->size;
        return;
    }
    busy = true;
    g_cfg.lastCrash = crash->name;
    SaveConfig();
    OfferReport(BuildReport(crash));
    busy = false;
}

void CollectLogs() {
    auto crash = NewestCrash();  // in too if it's from the last day
    if (crash && crash->time + 24 * 3600 * kSecond < Now()) crash.reset();
    OfferReport(BuildReport(crash));
}

// --- Small Windows helpers --------------------------------------------------------------------------------------

bool SetClipboard(const std::string& utf8) {
    std::wstring text = Wide(utf8);
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

std::string GetClipboard() {
    std::wstring text;
    if (!OpenClipboard(g_wnd)) return {};
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            text = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return Utf8(text);
}

void SetGameDir(const std::wstring& dir) {
    g_game = dir;
    g_catalogKey.clear();
    g_syncKey.clear();
    Rescan();
}

void BrowseForGame() {
    BROWSEINFOW bi{};
    bi.hwndOwner = g_wnd;
    bi.lpszTitle = L"Pick the Crab Champions folder (or its CrabChampions\\Binaries\\Win64 folder)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) {
        std::wstring dir = NormalizeGameDir(path);
        if (dir.empty()) {
            SetStatus("Crab Champions is not in that folder (no CrabChampions-Win64-Shipping.exe).", true);
        } else {
            SetGameDir(dir);
            SaveConfig();
            SetStatus("Found Crab Champions.");
            if (!g_cfg.setupDone && g_releasesLoaded) AfterReleases();
        }
    }
    CoTaskMemFree(pidl);
}

void Play() {
    if (g_pending > 0) {
        g_launchAfterJobs = true;
        SetStatus("Starting the game when the updates are done...");
        return;
    }
    SetStatus(LaunchGame());
}

constexpr const char kLaunchOption[] = "WINEDLLOVERRIDES=\"xinput1_3=n,b\" %command%";

// ================================================================================================================
// The look: a plain desktop tool. Tabs on top, a table of mods, details beside it, a status bar.

namespace col {
constexpr ImU32 Bg = IM_COL32(22, 24, 28, 255);
constexpr ImU32 Bar = IM_COL32(30, 33, 38, 255);
constexpr ImU32 Panel = IM_COL32(28, 31, 36, 255);
constexpr ImU32 Line = IM_COL32(46, 50, 57, 255);
constexpr ImU32 Text = IM_COL32(226, 228, 231, 255);
constexpr ImU32 Dim = IM_COL32(150, 155, 163, 255);
constexpr ImU32 Faint = IM_COL32(105, 110, 118, 255);
constexpr ImU32 Accent = IM_COL32(222, 98, 48, 255);
constexpr ImU32 AccentHover = IM_COL32(236, 116, 66, 255);
constexpr ImU32 Green = IM_COL32(110, 180, 100, 255);
constexpr ImU32 Red = IM_COL32(214, 88, 80, 255);
constexpr ImU32 Yellow = IM_COL32(222, 180, 70, 255);
}  // namespace col

ImVec4 V4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
ImU32 Alpha(ImU32 c, float a) { return (c & 0x00FFFFFF) | (ImU32(a * 255) << 24); }

ImFont *g_fBody, *g_fSmall, *g_fTitle, *g_fBold;
float g_s = 1;  // DPI scale
float S(float v) { return v * g_s; }

// The game's rarity colors.
ImU32 RarityColor(const std::string& rarity) {
    if (rarity == "common") return IM_COL32(190, 196, 204, 255);
    if (rarity == "epic") return IM_COL32(176, 120, 240, 255);
    if (rarity == "legendary") return IM_COL32(240, 176, 60, 255);
    if (rarity == "greed") return IM_COL32(100, 200, 110, 255);
    return col::Dim;
}

void Style() {
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 0;
    st.ChildRounding = S(3);
    st.FrameRounding = S(3);
    st.PopupRounding = S(3);
    st.GrabRounding = S(2);
    st.ScrollbarRounding = S(3);
    st.TabRounding = 0;
    st.ScrollbarSize = S(12);
    st.FramePadding = ImVec2(S(8), S(5));
    st.ItemSpacing = ImVec2(S(8), S(6));
    st.CellPadding = ImVec2(S(8), S(5));
    st.WindowPadding = ImVec2(S(14), S(12));
    st.WindowBorderSize = 0;
    st.ChildBorderSize = S(1);
    st.FrameBorderSize = 0;
    st.GrabMinSize = S(10);
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = V4(col::Text);
    c[ImGuiCol_TextDisabled] = V4(col::Faint);
    c[ImGuiCol_WindowBg] = V4(col::Bg);
    c[ImGuiCol_ChildBg] = V4(0);
    c[ImGuiCol_PopupBg] = V4(IM_COL32(34, 37, 43, 255));
    c[ImGuiCol_Border] = V4(col::Line);
    c[ImGuiCol_FrameBg] = V4(IM_COL32(38, 41, 47, 255));
    c[ImGuiCol_FrameBgHovered] = V4(IM_COL32(46, 50, 57, 255));
    c[ImGuiCol_FrameBgActive] = V4(IM_COL32(52, 56, 64, 255));
    c[ImGuiCol_Button] = V4(IM_COL32(48, 52, 59, 255));
    c[ImGuiCol_ButtonHovered] = V4(IM_COL32(60, 65, 73, 255));
    c[ImGuiCol_ButtonActive] = V4(IM_COL32(68, 73, 82, 255));
    c[ImGuiCol_Header] = V4(IM_COL32(52, 57, 66, 255));
    c[ImGuiCol_HeaderHovered] = V4(IM_COL32(40, 44, 51, 255));
    c[ImGuiCol_HeaderActive] = V4(IM_COL32(56, 61, 70, 255));
    c[ImGuiCol_TableHeaderBg] = V4(col::Bar);
    c[ImGuiCol_TableBorderStrong] = V4(col::Line);
    c[ImGuiCol_TableBorderLight] = V4(IM_COL32(38, 41, 47, 255));
    c[ImGuiCol_TableRowBg] = V4(0);
    c[ImGuiCol_TableRowBgAlt] = V4(IM_COL32(255, 255, 255, 6));
    c[ImGuiCol_SliderGrab] = V4(col::Accent);
    c[ImGuiCol_SliderGrabActive] = V4(col::AccentHover);
    c[ImGuiCol_CheckMark] = V4(col::Text);
    c[ImGuiCol_ScrollbarBg] = V4(0);
    c[ImGuiCol_ScrollbarGrab] = V4(IM_COL32(56, 60, 68, 255));
    c[ImGuiCol_ScrollbarGrabHovered] = V4(IM_COL32(70, 75, 84, 255));
    c[ImGuiCol_ScrollbarGrabActive] = V4(IM_COL32(84, 90, 100, 255));
    c[ImGuiCol_TextSelectedBg] = V4(Alpha(col::Accent, 0.35f));
    c[ImGuiCol_NavHighlight] = V4(Alpha(col::Accent, 0.6f));
    c[ImGuiCol_Separator] = V4(col::Line);
}

// Install is the one colored button; everything else is plain.
bool Button(const char* label, ImVec2 size = {0, 0}, bool primary = false, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, col::Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col::AccentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, col::Accent);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    }
    bool r = ImGui::Button(label, size);
    if (primary) ImGui::PopStyleColor(4);
    if (!enabled) ImGui::EndDisabled();
    return r && enabled;
}

void Spinner(ImVec2 c, float r, ImU32 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float t = float(ImGui::GetTime()) * 6.f;
    dl->PathClear();
    dl->PathArcTo(c, r, t, t + 4.2f, 16);
    dl->PathStroke(color, 0, S(2));
}

void TextColored(ImU32 c, const std::string& s) {
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopStyleColor();
}

void Wrapped(ImU32 c, const std::string& s) {
    ImGui::PushStyleColor(ImGuiCol_Text, c);
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(s.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void Small(ImU32 c, const std::string& s) {
    ImGui::PushFont(g_fSmall);
    Wrapped(c, s);
    ImGui::PopFont();
}

// A section heading with a rule under it.
void Heading(const char* s) {
    ImGui::Dummy(ImVec2(0, S(6)));
    ImGui::PushFont(g_fBold);
    ImGui::TextUnformatted(s);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, S(2)));
}

// "damagePercent" -> "Damage (%)", "radiusMeters" -> "Radius (m)", "syncWeaponMods" -> "Sync weapon mods".
std::string Pretty(const std::string& key) {
    std::string words;
    for (size_t i = 0; i < key.size(); i++) {
        char c = key[i];
        if (c >= 'A' && c <= 'Z' && i > 0) words += ' ', c = char(c - 'A' + 'a');
        words += c;
    }
    if (!words.empty()) words[0] = char(std::toupper(uint8_t(words[0])));
    for (auto [suffix, unit] : {std::pair{" percent", " (%)"}, {" meters", " (m)"}, {" seconds", " (s)"},
                                {" multiplier", " (x)"}}) {
        std::string s = suffix;
        if (words.size() > s.size() && words.compare(words.size() - s.size(), s.size(), s) == 0)
            words = words.substr(0, words.size() - s.size()) + unit;
    }
    return words;
}

std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(uint8_t(c)));
    return s;
}

std::string Capital(std::string s) {
    if (!s.empty()) s[0] = char(std::toupper(uint8_t(s[0])));
    return s;
}

// ================================================================================================================
// Pages

const char* const kKinds[] = {"Perk", "Relic", "Weapon mod", "Ability mod", "Melee mod", "Tweak"};
const char* const kKindPlurals[] = {"Perks", "Relics", "Weapon mods", "Ability mods", "Melee mods", "Tweaks"};

enum class Page { Mods, Share, Settings };
Page g_page = Page::Mods;
std::string g_select = "pack";  // the mod in the details pane: "pack" (CrabMods itself), "sync", or an item section
char g_search[128] = "";
int g_show = 0;  // 0 all, 1 installed, 2 not installed
int g_kind = -1; // index into kKinds, -1 all
std::map<std::string, std::string> g_editing;  // "section|key" -> value being typed or dragged

struct Entry {
    std::string id, name, kind, rarity, about;
    bool installed;
    const CatalogItem* item;  // nullptr for CrabMods and CrabSync themselves
};

std::vector<Entry> Entries() {
    std::vector<Entry> e;
    std::string count = g_catalog ? std::to_string(g_catalog->items.size()) + " " : "";
    e.push_back({"pack", "CrabMods", "Mod pack", "",
                 "Adds " + count + "perks, relics, weapon mods and tweaks. Everyone in a game needs it.", ModsInstalled(), nullptr});
    e.push_back({"sync", "CrabSync", "Multiplayer", "", g_syncItem ? g_syncItem->about : "Shared team inventory and health.",
                 SyncInstalled(), nullptr});
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items)
            e.push_back({it.section, it.name, it.kind, it.rarity, it.about, ModsInstalled() && ItemOn(g_mods.ini, it), &it});
    auto rank = [](const std::string& k) { return int(std::find(std::begin(kKinds), std::end(kKinds), k) - std::begin(kKinds)); };
    std::stable_sort(e.begin() + 2, e.end(), [&](const Entry& a, const Entry& b) {
        return rank(a.kind) != rank(b.kind) ? rank(a.kind) < rank(b.kind) : a.name < b.name;
    });
    return e;
}

void Toggle(const Entry& e, bool on) {
    if (e.id == "pack") {
        if (on) InstallEverything();
        else Uninstall(ModsInfo());
    } else if (e.id == "sync") {
        if (on) Install(SyncInfo());
        else Uninstall(SyncInfo());
    } else if (e.item) {
        SetItem(*e.item, on);
    }
}

bool CanChange(const Entry& e) {
    return !g_game.empty() && !(g_pending > 0 && (e.id == "pack" || e.id == "sync" || !ModsInstalled()));
}

// A setting: its name and control on one line, what it does under it. Saved as soon as it's chosen (typed
// values when leaving the box).
void SettingRow(const ModInfo& mod, const std::string& section, const Setting& st, const IniValues& ini) {
    std::string id = section + "|" + st.key, value = ValueOf(ini, section, st);
    auto editing = g_editing.find(id);
    std::string shown = editing != g_editing.end() ? editing->second : value;
    ImGui::PushID(id.c_str());
    ImGui::Dummy(ImVec2(0, S(4)));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Pretty(st.key).c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.45f);
    std::optional<std::string> commit;
    bool changedFromDefault = shown != st.def;
    float resetW = changedFromDefault ? ImGui::CalcTextSize("Reset").x + S(16) + S(6) : 0;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetW);
    switch (st.type) {
    case Setting::Bool: {
        bool v = shown == "true";
        if (ImGui::Checkbox("##v", &v)) commit = v ? "true" : "false";
        break;
    }
    case Setting::Choice: {
        if (ImGui::BeginCombo("##v", shown.c_str())) {
            for (const std::string& c : st.choices)
                if (ImGui::Selectable(c.c_str(), c == shown)) commit = c;
            ImGui::EndCombo();
        }
        break;
    }
    case Setting::Int:
    case Setting::Float: {
        double v = std::strtod(shown.c_str(), nullptr);
        bool changed;
        if (st.range && st.type == Setting::Int) {
            int i = int(std::lround(v));
            changed = ImGui::SliderInt("##v", &i, int(st.lo), int(st.hi));
            v = i;
        } else if (st.range) {
            float f = float(v);
            changed = ImGui::SliderFloat("##v", &f, float(st.lo), float(st.hi), "%g");
            v = f;
        } else if (st.type == Setting::Int) {
            int i = int(std::lround(v));
            changed = ImGui::InputInt("##v", &i, 1, 10);
            v = i;
        } else {
            changed = ImGui::InputDouble("##v", &v, 0.1, 1, "%g");
        }
        if (changed) {
            char b[64];
            std::snprintf(b, sizeof b, st.type == Setting::Int ? "%.0f" : "%g", v);
            g_editing[id] = b;
        }
        if (g_editing.count(id) && !ImGui::IsItemActive()) commit = g_editing[id];
        break;
    }
    case Setting::Text: {
        char buf[256];
        std::snprintf(buf, sizeof buf, "%s", shown.c_str());
        if (ImGui::InputText("##v", buf, sizeof buf)) g_editing[id] = buf;
        if (ImGui::IsItemDeactivated() && g_editing.count(id)) commit = g_editing[id];
        break;
    }
    }
    if (changedFromDefault) {
        ImGui::SameLine(0, S(6));
        if (ImGui::Button("Reset")) commit = st.def;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Default: %s", st.def.c_str());
    }
    if (commit) {
        g_editing.erase(id);
        if (*commit != value) {
            std::string compact = (section.empty() ? "" : "[" + section + "]\n") + st.key + " = " + *commit + "\n";
            if (WriteSettings(mod, compact)) SetStatus("Saved " + Pretty(st.key) + ". Takes effect next time the game starts.");
        }
    }
    if (!st.help.empty()) {
        std::string help = st.help;  // the ini comment, its lines joined
        for (char& c : help)
            if (c == '\n') c = ' ';
        Small(col::Faint, help);
    }
    ImGui::PopID();
}

void Details(const Entry& e) {
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted(e.name.c_str());
    ImGui::PopFont();
    std::string sub = e.kind + (e.rarity.empty() ? "" : ", " + Capital(e.rarity));
    if (e.id == "pack" && ModsInstalled()) sub += ", version " + g_mods.version;
    if (e.id == "sync" && SyncInstalled()) sub += ", version " + g_sync.version;
    TextColored(e.rarity.empty() ? col::Dim : RarityColor(e.rarity), sub);
    ImGui::Dummy(ImVec2(0, S(2)));
    Wrapped(col::Text, e.about);
    ImGui::Dummy(ImVec2(0, S(4)));

    if (e.installed ? Button("Uninstall", ImVec2(S(110), 0), false, CanChange(e))
                    : Button("Install", ImVec2(S(110), 0), true, CanChange(e)))
        Toggle(e, !e.installed);
    if (e.id == "pack" && ModsInstalled() && g_catalog) {
        int on = 0;
        for (const CatalogItem& it : g_catalog->items) on += ItemOn(g_mods.ini, it);
        ImGui::SameLine();
        if (on < int(g_catalog->items.size()) && Button("Install all items", ImVec2(0, 0), false, g_pending == 0)) {
            WriteSettings(ModsInfo(), AllItems(true));
            SetStatus("All CrabMods items installed.");
        }
    }
    if (e.id == "pack" && UpdateFor(g_mods, ModsInfo())) Small(col::Yellow, "Update " + g_latest["CrabMods"].version + " available.");
    if (e.id == "sync" && UpdateFor(g_sync, SyncInfo())) Small(col::Yellow, "Update " + g_latest["CrabSync"].version + " available.");

    ImGui::Dummy(ImVec2(0, S(2)));
    if (e.id == "pack")
        Small(col::Faint, "Removing an item takes it out of the loot pool. In multiplayer the host's items and settings apply to everyone.");
    else if (e.id == "sync")
        Small(col::Faint, "Only the host needs it.");
    else
        Small(col::Faint, "Part of CrabMods. In multiplayer the host's settings apply.");

    const CatalogItem* item = e.item ? e.item : e.id == "sync" && g_syncItem ? &*g_syncItem : nullptr;
    if (!item || item->settings.empty()) return;
    Heading("Settings");
    const ModInfo& mod = e.item ? ModsInfo() : SyncInfo();
    const IniValues& ini = e.item ? g_mods.ini : g_sync.ini;
    for (const Setting& st : item->settings) SettingRow(mod, item->section, st, ini);
    bool changed = false;
    for (const Setting& st : item->settings) changed |= ValueOf(ini, item->section, st) != st.def;
    if (changed) {
        ImGui::Dummy(ImVec2(0, S(4)));
        if (Button("Reset all to defaults")) {
            std::string compact = item->section.empty() ? "" : "[" + item->section + "]\n";
            for (const Setting& st : item->settings) compact += st.key + " = " + st.def + "\n";
            WriteSettings(mod, compact);
            SetStatus(e.name + " settings reset.");
        }
    }
}

// A notice across the top of the Mods page.
void Notice(ImU32 color, const std::string& text, const std::function<void()>& buttons = {}) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, pad = S(8);
    ImGui::SetCursorScreenPos(ImVec2(p.x + pad + S(4), p.y + pad));
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + w - 2 * pad - S(4));
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    if (buttons) buttons();
    ImGui::EndGroup();
    float bottom = ImGui::GetItemRectMax().y + pad;
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(p, ImVec2(p.x + w, bottom), Alpha(color, 0.10f));
    dl->AddRectFilled(p, ImVec2(p.x + S(3), bottom), color);
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(p.x, bottom + S(6)));
    ImGui::Dummy(ImVec2(0, 0));
}

void Notices() {
    if (g_game.empty())
        Notice(col::Red, "Crab Champions wasn't found.", [] {
            if (Button("Choose game folder...", ImVec2(0, 0), true)) BrowseForGame();
        });
    static std::optional<bool> launchOption;
    static double checked = -100;
    if (RunningOnWine() && !g_cfg.linuxHintDone && (ModsInstalled() || SyncInstalled())) {
        if (ImGui::GetTime() - checked > 10) launchOption = LaunchOptionSet(), checked = ImGui::GetTime();
        if (launchOption != true)
            Notice(col::Yellow, "Linux / Steam Deck: add this to Crab Champions' launch options in Steam (Properties > General), "
                                "or the mods won't load:",
                   [] {
                       TextColored(col::Yellow, kLaunchOption);
                       if (Button("Copy")) SetStatus(SetClipboard(kLaunchOption) ? "Copied." : "Could not copy.");
                       ImGui::SameLine();
                       if (Button("Done")) {
                           g_cfg.linuxHintDone = true;
                           SaveConfig();
                       }
                   });
    }
    if (g_waitingForGame) Notice(col::Yellow, "Updates are downloaded and will install when the game closes.");
}

void ModsPage() {
    Notices();
    std::vector<Entry> all = Entries();
    int installed = 0;
    for (auto& e : all) installed += e.installed;

    // Toolbar.
    ImGui::SetNextItemWidth(S(240));
    ImGui::InputTextWithHint("##search", "Search", g_search, sizeof g_search);
    ImGui::SameLine();
    const char* shows[] = {"All", "Installed", "Not installed"};
    ImGui::SetNextItemWidth(S(130));
    ImGui::Combo("##show", &g_show, shows, 3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(S(140));
    if (ImGui::BeginCombo("##kind", g_kind < 0 ? "All types" : kKindPlurals[g_kind])) {
        if (ImGui::Selectable("All types", g_kind < 0)) g_kind = -1;
        for (int i = 0; i < 6; i++)
            if (ImGui::Selectable(kKindPlurals[i], g_kind == i)) g_kind = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    TextColored(col::Faint, std::to_string(installed) + " of " + std::to_string(all.size()) + " installed");

    std::string q = Lower(g_search);
    std::vector<const Entry*> shown;
    for (const Entry& e : all) {
        if ((g_show == 1 && !e.installed) || (g_show == 2 && e.installed)) continue;
        if (g_kind >= 0 && e.kind != kKinds[g_kind]) continue;
        if (!q.empty() && Lower(e.name + " " + e.kind + " " + e.rarity + " " + e.about + " " + e.id).find(q) == std::string::npos)
            continue;
        shown.push_back(&e);
    }

    float h = ImGui::GetContentRegionAvail().y;
    float detailsW = std::max(S(360), ImGui::GetContentRegionAvail().x * 0.38f);
    float listW = ImGui::GetContentRegionAvail().x - detailsW - S(10);
    ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                         ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("mods", 4, tf, ImVec2(listW, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, S(100));
        ImGui::TableSetupColumn("Rarity", ImGuiTableColumnFlags_WidthFixed, S(76));
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, S(86));
        ImGui::TableHeadersRow();
        for (const Entry* e : shown) {
            ImGui::PushID(e->id.c_str());
            ImGui::TableNextRow(0, S(30));
            ImGui::TableNextColumn();
            float y = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(y + S(3));
            ImGui::PushStyleColor(ImGuiCol_Text, e->installed ? col::Text : col::Dim);
            if (!e->item) ImGui::PushFont(g_fBold);
            if (ImGui::Selectable(e->name.c_str(), g_select == e->id,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, S(22))))
                g_select = e->id;
            if (!e->item) ImGui::PopFont();
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::SetCursorPosY(y + S(5));
            TextColored(col::Dim, e->kind);
            ImGui::TableNextColumn();
            ImGui::SetCursorPosY(y + S(5));
            if (!e->rarity.empty()) TextColored(RarityColor(e->rarity), Capital(e->rarity));
            else if (e->id == "pack" && ModsInstalled()) TextColored(col::Faint, g_mods.version);
            else if (e->id == "sync" && SyncInstalled()) TextColored(col::Faint, g_sync.version);
            ImGui::TableNextColumn();
            ImGui::SetCursorPosY(y + S(2));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(6), S(3)));
            if (e->installed ? Button("Uninstall", ImVec2(-FLT_MIN, 0), false, CanChange(*e))
                             : Button("Install", ImVec2(-FLT_MIN, 0), true, CanChange(*e)))
                Toggle(*e, !e->installed);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (shown.empty()) {
        ImVec2 p = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + S(10), p.y + S(40)), col::Faint,
                                            g_catalog || !q.empty() ? "No matches." : "Downloading the mod list...");
    }
    ImGui::SameLine(0, S(10));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, col::Panel);
    ImGui::BeginChild("details", ImVec2(detailsW, h), ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding);
    for (const Entry& e : all)
        if (e.id == g_select) Details(e);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// --- Share page --------------------------------------------------------------------------------------------------

char g_codeInput[4096] = "";

std::string Summary(const Setup& s) {
    std::vector<std::string> parts;
    if (s.crabMods) {
        int on = 0;
        for (auto& [k, v] : s.on) on += v;
        parts.push_back("CrabMods (" + std::to_string(on) + "/" + std::to_string(s.on.size()) + " items)");
    }
    if (s.crabSync) parts.push_back("CrabSync");
    std::string r;
    for (size_t i = 0; i < parts.size(); i++) r += (i ? " + " : "") + parts[i];
    return r.empty() ? "no mods" : r;
}

int ChangedSettings(const Setup& s) {
    int n = 0;
    auto count = [&](const CatalogItem& it, const std::string& name) {
        auto v = s.values.find(name);
        if (v == s.values.end()) return;
        for (const Setting& st : it.settings)
            if (auto k = v->second.find(st.key); k != v->second.end() && k->second != st.def) n++;
    };
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items) count(it, it.section);
    if (g_syncItem) count(*g_syncItem, "S");
    return n;
}

std::string Describe(const Setup& s) {
    int changed = ChangedSettings(s);
    return Summary(s) + ", " + (changed ? std::to_string(changed) + " setting" + (changed == 1 ? "" : "s") + " changed" : "default settings");
}

void SharePage() {
    float w = std::min(ImGui::GetContentRegionAvail().x, S(760));
    ImGui::BeginChild("share", ImVec2(w, 0));
    Heading("Your setup code");
    Small(col::Dim, "Which mods you have installed and every setting you changed. Send it to someone and they can load it below.");
    std::string code = CurrentCode();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - S(90));
    ImGui::InputText("##code", code.data(), code.size() + 1, ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (Button("Copy", ImVec2(-FLT_MIN, 0), true)) SetStatus(SetClipboard(code) ? "Copied " + code : "Could not copy.");
    TextColored(col::Faint, Capital(Describe(CurrentSetup())));

    Heading("Load a code");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - S(90));
    ImGui::InputTextWithHint("##in", "CRAB2-...", g_codeInput, sizeof g_codeInput);
    ImGui::SameLine();
    if (Button("Paste", ImVec2(-FLT_MIN, 0))) std::snprintf(g_codeInput, sizeof g_codeInput, "%s", Trim(GetClipboard()).c_str());
    std::string in = Trim(g_codeInput);
    if (!in.empty()) {
        static const Catalog none;
        auto setup = ReadSetupCode(g_catalog ? *g_catalog : none, g_syncItem ? &*g_syncItem : nullptr, in);
        auto old = setup ? std::nullopt : ReadShareCode(in);
        if (setup) {
            Setup cur = CurrentSetup();
            std::vector<std::string> add, remove;
            if (setup->crabSync != cur.crabSync) (setup->crabSync ? add : remove).push_back("CrabSync");
            if (setup->crabMods != cur.crabMods) (setup->crabMods ? add : remove).push_back("CrabMods");
            if (g_catalog && setup->crabMods)
                for (const CatalogItem& it : g_catalog->items) {
                    bool was = cur.crabMods && cur.on[it.section], now = setup->on[it.section];
                    if (was != now) (now ? add : remove).push_back(it.name);
                }
            auto list = [](const std::vector<std::string>& v) {
                std::string r;
                for (size_t i = 0; i < v.size(); i++) r += (i ? ", " : "") + v[i];
                return r;
            };
            TextColored(col::Text, Capital(Describe(*setup)));
            if (!add.empty()) Wrapped(col::Green, "Installs " + list(add));
            if (!remove.empty()) Wrapped(col::Red, "Uninstalls " + list(remove));
            if (setup->unknown) Wrapped(col::Yellow, std::to_string(setup->unknown) + " part(s) need a newer CrabMods and are skipped.");
            if (Button("Load", ImVec2(S(90), 0), true, g_pending == 0 && !g_game.empty())) {
                ApplySetup(*setup);
                g_codeInput[0] = 0;
            }
        } else if (old) {
            TextColored(col::Text, "Old-style code with settings for " + old->iniName + ".");
            if (Button("Load", ImVec2(S(90), 0), true, !g_game.empty())) {
                for (const ModInfo& m : Mods())
                    if (old->iniName == m.ini && WriteSettings(m, old->settings)) SetStatus("Settings loaded.");
                g_codeInput[0] = 0;
            }
        } else {
            TextColored(col::Red, "Not a valid code (it may be cut off).");
        }
    }

    Heading("Reset");
    Small(col::Dim, "Install everything with default settings.");
    static double armed = -100;
    bool sure = ImGui::GetTime() - armed < 4;
    if (Button(sure ? "Click again to confirm" : "Reset to defaults", ImVec2(0, 0), sure, g_pending == 0 && !g_game.empty())) {
        if (sure) {
            Setup s;
            s.crabMods = s.crabSync = true;
            if (g_catalog)
                for (const CatalogItem& it : g_catalog->items) s.on[it.section] = true;
            ApplySetup(s);
            armed = -100;
        } else {
            armed = ImGui::GetTime();
        }
    }
    ImGui::EndChild();
}

// --- Settings page ----------------------------------------------------------------------------------------------

void SettingsPage() {
    float w = std::min(ImGui::GetContentRegionAvail().x, S(760));
    ImGui::BeginChild("settings", ImVec2(w, 0));
    Heading("Game folder");
    Wrapped(g_game.empty() ? col::Red : col::Text, g_game.empty() ? "Not found" : Utf8(g_game));
    if (Button("Change...")) BrowseForGame();
    ImGui::SameLine();
    if (Button("Open", ImVec2(0, 0), false, !g_game.empty()))
        ShellExecuteW(g_wnd, L"open", g_game.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    Heading("Updates");
    if (ImGui::Checkbox("Install mod updates automatically", &g_cfg.autoMods)) SaveConfig();
    if (ImGui::Checkbox("Update the manager automatically", &g_cfg.autoManager)) SaveConfig();
    ImGui::Dummy(ImVec2(0, S(2)));
    if (ImGui::BeginTable("versions", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("", 0, S(140));
        ImGui::TableSetupColumn("Installed", 0, S(110));
        ImGui::TableSetupColumn("Latest", 0, S(110));
        ImGui::TableHeadersRow();
        auto line = [](const std::string& name, const std::string& have, const std::string& latest) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name.c_str());
            ImGui::TableNextColumn();
            TextColored(have.empty() ? col::Faint : col::Text, have.empty() ? "-" : have);
            ImGui::TableNextColumn();
            bool newer = !have.empty() && !latest.empty() && CompareVersions(have, latest) < 0;
            TextColored(newer ? col::Yellow : col::Dim, latest.empty() ? (g_releasesLoaded ? "-" : "...") : latest);
        };
        auto latest = [](const char* n) { return g_latest.count(n) ? g_latest[n].version : std::string(); };
        line("CrabMods", g_mods.version, latest("CrabMods"));
        line("CrabSync", g_sync.version, latest("CrabSync"));
        line("Manager", kVersion, latest("Manager"));
        ImGui::EndTable();
    }
    if (Button("Check now", ImVec2(0, 0), false, !g_fetching)) CheckForUpdates();
    if (!g_releaseError.empty()) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        TextColored(col::Red, "GitHub: " + g_releaseError);
    }

    if (RunningOnWine()) {
        Heading("Linux / Steam Deck");
        Small(col::Dim, "Steam launch options for Crab Champions (the mods don't load without it):");
        TextColored(col::Yellow, kLaunchOption);
        if (Button("Copy")) SetStatus(SetClipboard(kLaunchOption) ? "Copied." : "Could not copy.");
        auto set = LaunchOptionSet();
        if (set) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            TextColored(*set ? col::Green : col::Yellow, *set ? "Set in Steam." : "Not set in Steam yet.");
        }
    }

    Heading("Crash reports");
    Small(col::Dim, "While the manager is open it watches for game crashes and offers to save the logs (with names and paths "
                    "removed) and open a GitHub issue. Nothing is uploaded unless you post it.");
    if (Button("Collect logs...")) CollectLogs();
    ImGui::SameLine();
    if (Button("Open reports folder")) {
        CreateDirectoryW((ExeDir() + L"CrashReports").c_str(), nullptr);
        ShellExecuteW(g_wnd, L"open", (ExeDir() + L"CrashReports").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    Heading("About");
    TextColored(col::Dim, std::string("CrabMods Manager ") + kVersion + ". Fan-made, not affiliated with Noisestorm.");
    if (Button("GitHub")) OpenExternal(L"https://github.com/" + Wide(kRepo));
    ImGui::SameLine();
    if (Button("Report a bug")) OpenExternal(L"https://github.com/" + Wide(kRepo) + L"/issues/new");
    ImGui::EndChild();
}

// --- The frame ---------------------------------------------------------------------------------------------------

void TopBar(float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), col::Bar);
    dl->AddLine(ImVec2(0, h - 1), ImVec2(w, h - 1), col::Line);
    ImGui::SetCursorPos(ImVec2(S(16), (h - g_fTitle->FontSize) * 0.5f));
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted("CrabMods");
    ImGui::PopFont();
    float x = S(16) + g_fTitle->CalcTextSizeA(g_fTitle->FontSize, FLT_MAX, 0, "CrabMods").x + S(28);
    auto tab = [&](const char* label, Page page) {
        bool on = g_page == page;
        float tw = g_fBold->CalcTextSizeA(g_fBold->FontSize, FLT_MAX, 0, label).x + S(28);
        ImGui::SetCursorPos(ImVec2(x, 0));
        if (ImGui::InvisibleButton(label, ImVec2(tw, h))) g_page = page;
        bool hover = ImGui::IsItemHovered();
        dl->AddText(g_fBold, g_fBold->FontSize, ImVec2(x + S(14), (h - g_fBold->FontSize) * 0.5f),
                    on ? col::Text : hover ? IM_COL32(200, 203, 208, 255) : col::Dim, label);
        if (on) dl->AddRectFilled(ImVec2(x + S(8), h - S(3)), ImVec2(x + tw - S(8), h), col::Accent);
        x += tw;
    };
    tab("Mods", Page::Mods);
    tab("Share", Page::Share);
    tab("Settings", Page::Settings);

    float pw = S(120), ph = h - S(16);
    ImGui::SetCursorPos(ImVec2(w - pw - S(12), S(8)));
    ImGui::PushFont(g_fBold);
    bool ok = !g_game.empty();
    if (Button(g_launchAfterJobs ? "Starting..." : "Play", ImVec2(pw, ph), true, ok && !g_launchAfterJobs)) Play();
    ImGui::PopFont();
}

void StatusBar(float w, float y, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, y), ImVec2(w, y + h), col::Bar);
    dl->AddLine(ImVec2(0, y), ImVec2(w, y), col::Line);
    ImGui::PushFont(g_fSmall);
    float ty = y + (h - g_fSmall->FontSize) * 0.5f;
    float x = S(12);
    std::string left;
    ImU32 c = col::Dim;
    if (g_pending > 0) {
        Spinner(ImVec2(x + S(5), y + h * 0.5f), S(5), col::Dim);
        x += S(16);
        std::string job = JobLabel();
        left = job.empty() ? "Working..." : job + "...";
    } else if (!g_status.empty() && ImGui::GetTime() - g_statusTime < 12) {
        left = g_status;
        c = g_statusError ? col::Red : col::Text;
    } else if (g_game.empty()) {
        left = "Game not found", c = col::Red;
    } else if (!g_releaseError.empty()) {
        left = "Offline, couldn't check for updates", c = col::Yellow;
    } else if (g_waitingForGame) {
        left = "Updates will install when the game closes", c = col::Yellow;
    } else if (!g_releasesLoaded) {
        left = "Checking for updates...";
    } else {
        left = "Up to date";
    }
    dl->AddText(g_fSmall, g_fSmall->FontSize, ImVec2(x, ty), c, left.c_str());
    std::string right = std::string("Manager ") + kVersion;
    float rw = g_fSmall->CalcTextSizeA(g_fSmall->FontSize, FLT_MAX, 0, right.c_str()).x;
    dl->AddText(g_fSmall, g_fSmall->FontSize, ImVec2(w - rw - S(12), ty), col::Faint, right.c_str());
    ImGui::PopFont();
}

void Frame() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    float top = S(48), bottom = S(26);
    TopBar(io.DisplaySize.x, top);
    ImGui::SetCursorPos(ImVec2(0, top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(14), S(12)));
    ImGui::BeginChild("page", ImVec2(io.DisplaySize.x, io.DisplaySize.y - top - bottom), ImGuiChildFlags_AlwaysUseWindowPadding,
                      g_page == Page::Mods ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0);
    ImGui::PopStyleVar();
    switch (g_page) {
    case Page::Mods: ModsPage(); break;
    case Page::Share: SharePage(); break;
    case Page::Settings: SettingsPage(); break;
    }
    ImGui::EndChild();
    StatusBar(io.DisplaySize.x, io.DisplaySize.y - bottom, bottom);
    ImGui::End();
}

// ================================================================================================================
// Direct3D 11 and the window

ID3D11Device* g_device;
ID3D11DeviceContext* g_ctx;
IDXGISwapChain* g_swap;
ID3D11RenderTargetView* g_rtv;

void CreateTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}
void DropTarget() {
    if (g_rtv) g_rtv->Release(), g_rtv = nullptr;
}

bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    // A machine without a usable GPU driver still gets the software renderer.
    for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
        if (D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got,
                                          &g_ctx) == S_OK) {
            CreateTarget();
            return true;
        }
    return false;
}

// --- Screenshots (--screenshot file.png), to check the look without a person at the screen ---------------------

std::string Png(int w, int h, const std::vector<uint8_t>& rgba) {
    auto be32 = [](std::string& o, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) o += char((v >> s) & 0xFF);
    };
    std::string raw;
    for (int y = 0; y < h; y++) {
        raw += '\0';
        for (int x = 0; x < w; x++) raw.append(reinterpret_cast<const char*>(&rgba[(y * w + x) * 4]), 3);
    }
    std::string z = "\x78\x01";  // zlib, stored blocks
    for (size_t i = 0; i < raw.size(); i += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - i);
        z += char(i + n == raw.size());
        z += char(n & 0xFF), z += char(n >> 8), z += char(~n & 0xFF), z += char((~n >> 8) & 0xFF);
        z.append(raw, i, n);
    }
    uint32_t a = 1, b = 0;
    for (unsigned char c : raw) a = (a + c) % 65521, b = (b + a) % 65521;
    be32(z, b << 16 | a);
    std::string png = "\x89PNG\r\n\x1a\n";
    auto chunk = [&](const char* type, const std::string& data) {
        be32(png, uint32_t(data.size()));
        std::string body = std::string(type, 4) + data;
        png += body;
        be32(png, Crc32(body));
    };
    std::string ihdr;
    be32(ihdr, w), be32(ihdr, h);
    ihdr += std::string("\x08\x02\x00\x00\x00", 5);
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", "");
    return png;
}

bool SaveScreenshot(const std::wstring& path) {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (!back) return false;
    D3D11_TEXTURE2D_DESC d;
    back->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    bool ok = false;
    if (g_device->CreateTexture2D(&d, nullptr, &staging) == S_OK) {
        g_ctx->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE m;
        if (g_ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m) == S_OK) {
            std::vector<uint8_t> px(d.Width * d.Height * 4);
            for (UINT y = 0; y < d.Height; y++) memcpy(&px[y * d.Width * 4], static_cast<uint8_t*>(m.pData) + y * m.RowPitch, d.Width * 4);
            g_ctx->Unmap(staging, 0);
            ok = WriteFile(path, Png(int(d.Width), int(d.Height), px));
        }
        staging->Release();
    }
    back->Release();
    return ok;
}

constexpr UINT_PTR kCrashTimer = 1, kUpdateTimer = 2;

LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(w, msg, wp, lp)) return 1;
    switch (msg) {
    case WM_SIZE:
        if (g_device && wp != SIZE_MINIMIZED) {
            DropTarget();
            g_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp), DXGI_FORMAT_UNKNOWN, 0);
            CreateTarget();
        }
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {LONG(S(960)), LONG(S(620))};
        return 0;
    case WM_TIMER:
        if (wp == kCrashTimer) CheckForCrash();
        // Every 30 minutes, and every 30 seconds while updates wait for the game to close.
        if (wp == kUpdateTimer) {
            static int ticks = 0;
            if (g_pending == 0 && (++ticks >= 60 || (g_waitingForGame && !GameRunning()))) {
                ticks = 0;
                CheckForUpdates();
            }
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

// The font, built into the exe (resource 2).
void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(10) /*RT_RCDATA*/);
    void* data = r ? LockResource(LoadResource(nullptr, r)) : nullptr;
    int size = r ? int(SizeofResource(nullptr, r)) : 0;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    auto font = [&](float px) {
        return data ? io.Fonts->AddFontFromMemoryTTF(data, size, S(px), &cfg) : io.Fonts->AddFontDefault();
    };
    g_fBody = font(16);
    g_fSmall = font(13.5f);
    g_fBold = font(16);
    g_fTitle = font(20);
}

// "CrabModsManager.exe --selftest": no window. Finds the game, reads GitHub, the installed versions and the
// catalogs, and writes what it found to CrabModsManager-selftest.txt next to the exe. With --report it also saves
// a report of the newest crash (or of the logs) as "Collect logs" does, and adds the issue address it would open.
int SelfTest(bool report) {
    std::string r = std::string("CrabMods Manager ") + kVersion + "\n";
    g_game = LoadConfig();
    if (g_game.empty()) g_game = FindGameDir();
    r += "game: " + (g_game.empty() ? std::string("not found") : Utf8(g_game)) + "\n";
    std::string error;
    auto body = HttpGet(L"https://api.github.com/repos/" + Wide(kRepo) + L"/releases?per_page=100", error);
    r += body ? "releases: " + std::to_string(body->size()) + " bytes\n" : "releases: failed, " + error + "\n";
    if (body) g_latest = LatestReleases(*body);
    Rescan();
    for (auto& [name, rel] : g_latest) r += name + ": latest " + rel.version + "\n";
    r += "installed: CrabMods " + (g_mods.version.empty() ? "no" : g_mods.version) + ", CrabSync " +
         (g_sync.version.empty() ? "no" : g_sync.version) + "\n";
    if (g_catalog) {
        r += "catalog: " + std::to_string(g_catalog->items.size()) + " items\n";
        const char* types[] = {"bool", "int", "float", "choice", "text"};
        for (const CatalogItem& it : g_catalog->items) {
            r += "  " + it.section + " = " + it.name + " (" + it.kind + (it.rarity.empty() ? "" : ", " + it.rarity) + ")" +
                 (ItemOn(g_mods.ini, it) ? "" : " [off]") + ": " + it.about + "\n";
            for (const Setting& s : it.settings) {
                r += "      " + s.key + " = " + s.def + " " + types[s.type];
                if (s.range) r += " [" + std::to_string(s.lo) + ".." + std::to_string(s.hi) + "]";
                for (auto& c : s.choices) r += " |" + c;
                r += "\n";
            }
        }
    } else {
        r += "catalog: none\n";
    }
    if (g_syncItem) {
        r += "crabsync settings: " + std::to_string(g_syncItem->settings.size()) + "\n";
        for (const Setting& s : g_syncItem->settings) {
            r += "      " + s.key + " = " + s.def;
            for (auto& c : s.choices) r += " |" + c;
            r += "\n";
        }
    }
    r += "setup code: " + CurrentCode() + "\n";
    for (const SavedDir& d : SavedDirs()) r += "saved: " + Utf8(d.path) + (d.proton ? " (proton)" : "") + "\n";
    auto crash = NewestCrash();
    r += "newest crash: " + (crash ? crash->name + " " + LocalTime(crash->time, false) : std::string("none")) + "\n";
    auto lo = LaunchOptionSet();
    r += std::string("launch option: ") + (lo ? (*lo ? "set" : "not set") : "unknown") + "\n";
    if (report) {
        BuiltReport b = BuildReport(crash);
        r += "report: " + (b.error.empty() ? Utf8(b.zip) : b.error) + "\nissue: " + IssueUrl(b.report) + "\n";
    }
    WriteFile(ExeDir() + L"CrabModsManager-selftest.txt", r);
    return body ? 0 : 1;
}

std::wstring Arg(const wchar_t* cmd, const wchar_t* name) {
    const wchar_t* p = wcsstr(cmd, name);
    if (!p) return {};
    p += wcslen(name);
    while (*p == L' ' || *p == L'=') p++;
    std::wstring v;
    bool quoted = *p == L'"';
    if (quoted) p++;
    while (*p && (quoted ? *p != L'"' : *p != L' ')) v += *p++;
    return v;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int show) {
    if (cmd && wcsstr(cmd, L"--selftest")) return SelfTest(wcsstr(cmd, L"--report") != nullptr);
    std::wstring shot = cmd ? Arg(cmd, L"--screenshot") : L"";
    DeleteFileW((ExePath() + L".old").c_str());  // left by a self-update
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hbrBackground = CreateSolidBrush(RGB(22, 24, 28));
    wc.lpszClassName = L"CrabModsManager";
    RegisterClassW(&wc);
    // Sized for the screen's scale.
    HDC screen = GetDC(nullptr);
    g_s = std::max(1.f, GetDeviceCaps(screen, LOGPIXELSX) / 96.f);
    ReleaseDC(nullptr, screen);
    std::wstring title = std::wstring(kTitle) + L" " + Wide(kVersion);
    g_wnd = CreateWindowExW(0, wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, int(S(1240)),
                            int(S(800)), nullptr, nullptr, inst, nullptr);
    if (!CreateDevice(g_wnd)) {
        MessageBoxW(g_wnd, L"Direct3D 11 isn't available, so the window can't be drawn.", kTitle, MB_ICONERROR);
        return 1;
    }
    g_s = std::max(g_s, ImGui_ImplWin32_GetDpiScaleForHwnd(g_wnd));
    ShowWindow(g_wnd, shot.empty() ? show : SW_SHOWNOACTIVATE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    Style();
    LoadFonts();
    ImGui::GetIO().FontDefault = g_fBody;
    ImGui_ImplWin32_Init(g_wnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    std::thread(Worker).detach();
    std::wstring dir = LoadConfig();
    if (dir.empty()) dir = FindGameDir();
    SetGameDir(dir);
    if (!dir.empty()) SaveConfig();
    if (cmd && wcsstr(cmd, L"--updated")) SetStatus(std::string("CrabMods Manager updated to ") + kVersion + ".");
    std::wstring page = cmd ? Arg(cmd, L"--page") : L"";
    if (page == L"share") g_page = Page::Share;
    if (page == L"settings") g_page = Page::Settings;
    if (std::wstring sel = cmd ? Arg(cmd, L"--select") : L""; !sel.empty()) g_select = Utf8(sel);
    // A screenshot shows the window as it is; it changes nothing.
    if (shot.empty()) {
        CheckForUpdates();
        SetTimer(g_wnd, kCrashTimer, 3000, nullptr);
        SetTimer(g_wnd, kUpdateTimer, 30000, nullptr);
    }

    int busyFrames = 3;
    double started = ImGui::GetTime();
    for (bool running = true; running;) {
        bool animate = g_pending > 0 || busyFrames > 0 || ImGui::GetTime() - g_statusTime < 9 || !shot.empty();
        MsgWaitForMultipleObjects(0, nullptr, FALSE, animate ? 16 : 500, QS_ALLINPUT);
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            busyFrames = 3;
        }
        if (!running) break;
        // Finished jobs.
        for (;;) {
            JobResult r;
            {
                std::lock_guard lock(g_jobMutex);
                if (g_results.empty()) break;
                r = std::move(g_results.front());
                g_results.pop_front();
            }
            if (!r.status.empty()) SetStatus(r.status, r.error);
            if (r.error && r.status.find("game is running") != std::string::npos) g_waitingForGame = true;
            Rescan();
            if (r.then) r.then();
            busyFrames = 3;
        }
        if (g_pending == 0 && g_launchAfterJobs) {
            g_launchAfterJobs = false;
            SetStatus(LaunchGame());
        }
        busyFrames--;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        Frame();
        ImGui::Render();
        const float clear[4] = {22 / 255.f, 24 / 255.f, 28 / 255.f, 1};
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (!shot.empty() && ImGui::GetTime() - started > 1.5) {
            SaveScreenshot(shot);
            break;
        }
        g_swap->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
