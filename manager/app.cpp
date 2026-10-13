// CrabMods Manager: a mod browser for Crab Champions. Every CrabMods item and CrabSync is a mod to install or
// uninstall, with its settings as real controls. It changes nothing in the game folder without asking (a review of
// every file first), checks each download against the checksum GitHub publishes for it, keeps a copy of what it
// replaces, and updates the mods and itself only if the player said so. It shares a whole setup as one short code,
// starts the game and, when it crashes, gathers the logs and the crash report for a GitHub issue.
// Dear ImGui; the window comes from win_main.cpp (Direct3D 11) or linux_main.cpp (SDL2 + OpenGL).
#include "app.hpp"

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "font_data.hpp"
#include "imgui.h"
#include "platform.hpp"

#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

using namespace mgr;

namespace app {
namespace {

Options g_opt;
bool g_readOnly = false;  // a screenshot: nothing is written, downloaded or changed
bool g_quit = false;

const ModInfo& ModsInfo() { return Mods()[0]; }
const ModInfo& SyncInfo() { return Mods()[1]; }
const ModInfo* ModNamed(const std::string& name) {
    for (const ModInfo& m : Mods())
        if (name == m.name || name == m.ini) return &m;
    return nullptr;
}

// --- Where things are ----------------------------------------------------------------------------------------------

fs::path StateDir() { return plat::DataDir() / "state"; }
fs::path CacheRoot() { return plat::CacheDir(); }
fs::path ConfigPath() { return plat::DataDir() / "CrabModsManager.ini"; }
fs::path ReportsDir() { return plat::DataDir() / "CrashReports"; }
fs::path LogPath() { return plat::DataDir() / "CrabModsManager.log"; }
constexpr const char kReleasesUrl[] = "https://api.github.com/repos/Dudiebug/CrabMods/releases?per_page=100";
std::string RepoUrl(const std::string& rest = "") { return "https://github.com/" + std::string(kRepo) + rest; }

// The manager's own log: what it changed, downloaded and checked (and the 304s of unchanged release lists).
void Log(const std::string& line) {
    if (g_readOnly) return;
    static std::mutex m;
    std::lock_guard lock(m);
    std::error_code ec;
    fs::create_directories(plat::DataDir(), ec);
    std::ofstream f(LogPath(), std::ios::app | std::ios::binary);
    f << LocalTime(FileNow(), false) << "  " << line << "\n";
}

// --- The manager's settings ----------------------------------------------------------------------------------------

Config g_cfg;
fs::path g_game;

// Nothing is saved before the first-run screen is done (it writes nothing until Install).
void SaveConfig() {
    if (g_readOnly || !g_cfg.setupDone) return;
    g_cfg.game = U8(g_game);
    if (!WriteFile(ConfigPath(), ConfigText(g_cfg))) Log("could not save " + U8(ConfigPath()));
}

// --- Background jobs ---------------------------------------------------------------------------------------------
// One worker thread runs them in order (downloads, file changes). Each returns a status line and, optionally,
// something to do on the UI thread afterwards.

struct JobResult {
    std::string status;
    bool error = false;
    std::function<void()> then;
};

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
        plat::Wake();
    }
}

void Enqueue(const std::string& label, std::function<JobResult()> fn) {
    if (g_readOnly) return;
    g_pending++;
    std::lock_guard lock(g_jobMutex);
    g_jobs.push_back({label, std::move(fn)});
    g_jobCv.notify_one();
}

std::string JobLabel() {
    std::lock_guard lock(g_jobMutex);
    return g_jobLabel;
}

HttpResponse Get(const std::string& url) { return plat::Http(url, "", "application/octet-stream"); }

// --- Status line -------------------------------------------------------------------------------------------------

std::string g_status;
bool g_statusError = false;
double g_statusTime = -100;

void SetStatus(const std::string& s, bool error = false) {
    g_status = s;
    g_statusError = error;
    g_statusTime = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0;
    if (error) Log("error: " + s);
}

// --- Notices: updates done, things waiting -----------------------------------------------------------------------

struct Note {
    std::string text;
    std::string title, notes, page;  // "What's new"
};
std::vector<Note> g_notes;

struct WhatsNew {
    std::string title, notes, page;
};
std::optional<WhatsNew> g_whatsNew;

void ShowWhatsNew(const Release& r, const std::string& name) {
    g_whatsNew = WhatsNew{name + " " + r.version, r.notes, r.page};
}

// --- What is installed ---------------------------------------------------------------------------------------------

ReleaseList g_releases;
bool g_releasesLoaded = false, g_fetching = false;
std::string g_releaseError, g_releasesBody, g_releasesEtag;
int64_t g_lastCheck = 0;
Poller g_poller;

struct ModFiles {
    std::string version;  // installed, "" if not
    std::string iniText;  // its settings file, "" if none yet
    IniValues ini;
};
ModFiles g_mods, g_sync;  // CrabMods, CrabSync
ModFiles& FilesOf(const ModInfo& m) { return &m == &ModsInfo() ? g_mods : g_sync; }

InstallRecord g_rec;
std::optional<Catalog> g_catalog;       // CrabMods' items
std::string g_catalogVersion, g_catalogKey;
std::optional<CatalogItem> g_syncItem;  // CrabSync
std::string g_syncKey;
Seen g_seen;
bool g_catalogsQueued = false;

const Release* Latest(const ModInfo& m) { return g_releases.Latest(m.name, m.dll); }
const Release* LatestManager() { return g_releases.Latest("Manager", plat::ManagerAsset()); }

bool ModsInstalled() { return !g_mods.version.empty(); }
bool SyncInstalled() { return !g_sync.version.empty(); }
bool Installed(const ModInfo& m) { return !FilesOf(m).version.empty(); }

fs::path CatalogFile(const ModInfo& m, const std::string& version) { return CachedFile(CacheRoot(), m.name, version, CatalogAsset(m)); }

// The versions whose catalog would do, best first: the installed one, the latest, then any other in the cache.
std::vector<std::string> CatalogVersions(const ModInfo& m) {
    std::vector<std::string> v;
    const std::string& installed = FilesOf(m).version;
    if (!installed.empty() && installed != "?") v.push_back(installed);
    if (const Release* r = Latest(m)) v.push_back(r->version);
    std::vector<std::string> cached;
    std::error_code ec;
    for (auto it = fs::directory_iterator(CacheRoot() / m.name, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        if (it->is_directory()) cached.push_back(U8(it->path().filename()));
    std::sort(cached.begin(), cached.end(), [](auto& a, auto& b) { return CompareVersions(a, b) > 0; });
    v.insert(v.end(), cached.begin(), cached.end());
    return v;
}

void LoadCatalogs() {
    bool firstRun = !Exists(StateDir() / "seen.json");
    for (const std::string& v : CatalogVersions(ModsInfo())) {
        fs::path p = CatalogFile(ModsInfo(), v);
        if (!Exists(p)) continue;
        std::string key = U8(p);
        if (key == g_catalogKey) break;
        auto text = ReadFile(p);
        auto c = text ? ParseCatalog(*text) : std::nullopt;
        if (!c) continue;
        g_catalog = std::move(c);
        g_catalogVersion = v;
        g_catalogKey = key;
        if (g_seen.empty()) g_seen = ParseSeen(ReadFile(StateDir() / "seen.json").value_or(""));
        if (NoteSeen(g_seen, *g_catalog, v, firstRun) && !g_readOnly && g_cfg.setupDone)
            WriteFile(StateDir() / "seen.json", SeenJson(g_seen));
        break;
    }
    for (const std::string& v : CatalogVersions(SyncInfo())) {
        fs::path p = CatalogFile(SyncInfo(), v);
        if (!Exists(p)) continue;
        std::string key = U8(p);
        if (key == g_syncKey) break;
        auto text = ReadFile(p);
        auto ini = text ? ParseSyncCatalog(*text) : std::nullopt;
        if (!ini) continue;
        g_syncItem = CrabSyncItem(*ini);
        g_syncKey = key;
        break;
    }
}

void ReadMod(ModFiles& f, const ModInfo& m) {
    f.version = g_game.empty() ? "" : FileVersion(g_game / m.dll);
    auto t = g_game.empty() ? std::nullopt : ReadFile(g_game / m.ini);
    f.iniText = t ? *t : "";
    f.ini = ReadIni(f.iniText);
}

void Rescan() {
    ReadMod(g_mods, ModsInfo());
    ReadMod(g_sync, SyncInfo());
    LoadCatalogs();
}

// Downloads the catalogs (small JSON files, checked like everything else) of the installed and latest versions.
// They are only read, never run.
void FetchCatalogs() {
    if (g_catalogsQueued || g_readOnly || g_cfg.offline) return;
    std::vector<std::pair<Asset, fs::path>> want;
    for (const ModInfo& m : Mods()) {
        std::set<std::string> versions;
        if (!FilesOf(m).version.empty() && FilesOf(m).version != "?") versions.insert(FilesOf(m).version);
        if (const Release* r = Latest(m)) versions.insert(r->version);
        for (const std::string& v : versions)
            if (const Release* r = g_releases.Version(m.name, v); r && r->Has(CatalogAsset(m)) && !Exists(CatalogFile(m, v)))
                want.push_back({*r->Get(CatalogAsset(m)), CatalogFile(m, v)});
    }
    if (want.empty()) return;
    g_catalogsQueued = true;
    Enqueue("Downloading the mod list", [want] {
        for (const auto& [asset, target] : want) {
            std::string error;
            if (DownloadChecked(Get, asset, target, error)) Log("downloaded " + U8(target));
            else Log("catalog: " + error);
        }
        return JobResult{"", false, [] {
            g_catalogsQueued = false;
            g_catalogKey.clear();
            g_syncKey.clear();
        }};
    });
}

// --- Settings files ------------------------------------------------------------------------------------------------

// Merges settings (a compact ini) into a mod's settings file, making the file first (with every default, as the
// mod would) if it doesn't exist yet.
bool WriteSettings(const ModInfo& m, const std::string& compact) {
    if (g_game.empty() || g_readOnly) return false;
    ModFiles& f = FilesOf(m);
    std::string base = f.iniText;
    if (base.empty()) {
        if (&m == &ModsInfo() && g_catalog) base = DefaultCrabModsIni(*g_catalog);
        else if (&m == &SyncInfo() && g_syncItem) base = g_syncItem->ini;
    }
    std::string text = MergeIni(base, compact);
    if (!WriteFile(g_game / m.ini, text)) {
        SetStatus(std::string("Could not save ") + m.ini + ".", true);
        return false;
    }
    f.iniText = text;
    f.ini = ReadIni(text);
    return true;
}

// Every item on or off, as one compact ini.
std::string AllItems(bool on, const std::string& except = "") {
    std::string s;
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items)
            if (it.toggle) s += "[" + it.section + "]\nenabled = " + ((it.section == except) != on ? "true" : "false") + "\n";
    return s;
}

std::map<std::string, std::string> ItemNames() {
    std::map<std::string, std::string> n;
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items) n[it.section] = it.name;
    return n;
}

// --- Changes to the game folder ------------------------------------------------------------------------------------
// Each is worked out first (an Action: its ChangePlan and what to do afterwards), shown for review, and only then
// carried out. Opted-in automatic updates skip the review but are always announced.

struct Action {
    ChangePlan plan;
    std::string done;                         // the status line when it worked
    std::vector<Note> notes;                  // announced when it worked
    std::map<std::string, std::string> skip;  // versions to skip afterwards (a rollback)
    bool setupDone = false;                   // the first-run screen's Install
};

std::optional<Action> g_review;
bool g_openReview = false;
std::deque<Action> g_waiting;  // for the game to close
std::set<std::string> g_autoTried;

void Finish(const Action& a) {
    Rescan();
    for (const auto& [ini, compact] : a.plan.iniWrites)
        if (const ModInfo* m = ModNamed(ini)) WriteSettings(*m, compact);
    for (const auto& [mod, v] : a.skip) g_cfg.skip[mod] = v;
    if (a.setupDone) g_cfg.setupDone = true;
    SaveConfig();
    for (const Note& n : a.notes) g_notes.push_back(n);
    if (!a.notes.empty()) plat::Attention();
    if (!a.done.empty()) SetStatus(a.done);
    Log("done: " + a.plan.title);
    for (const ModInfo& m : Mods()) {
        std::set<std::string> keep;
        if (!FilesOf(m).version.empty()) keep.insert(FilesOf(m).version);
        PruneCache(CacheRoot() / m.name, 2, keep);
    }
    FetchCatalogs();
}

bool TouchesFiles(const ChangePlan& p) {
    for (const FileChange& f : p.files)
        if (f.op != FileChange::Keep) return true;
    return !p.deletes.empty();
}

void Run(Action a) {
    if (g_readOnly) return;
    if (!TouchesFiles(a.plan)) {
        Finish(a);
        return;
    }
    if (plat::GameRunning()) {
        Log("waiting for the game to close: " + a.plan.title);
        g_waiting.push_back(std::move(a));
        SetStatus("Waiting for Crab Champions to close.");
        return;
    }
    Log("start: " + a.plan.title);
    for (const std::string& l : a.plan.Lines()) Log("  " + l);
    fs::path game = g_game, state = StateDir();
    Enqueue(a.plan.title, [a, game, state] {
        std::string error;
        for (const FileChange& c : a.plan.files)
            if ((c.op == FileChange::Add || c.op == FileChange::Replace) && !c.url.empty()) {
                if (!DownloadChecked(Get, Asset{c.name, c.url, c.sha256, c.size}, c.source, error))
                    return JobResult{a.plan.title + ": " + error, true};
                Log("checked " + c.name + " " + c.version + " sha256 " + c.sha256);
            }
        InstallRecord rec = LoadRecord(state);
        bool ok = ApplyFiles(game, state, a.plan, rec, LocalTime(FileNow(), true), error);
        bool running = !ok && error.find("game is running") != std::string::npos;
        return JobResult{ok ? "" : a.plan.title + ": " + error, !ok, [a, ok, running, rec] {
            g_rec = rec;
            if (ok) Finish(a);
            else if (running) g_waiting.push_back(a);
            else Rescan();
        }};
    });
}

// Shows the review, or runs it right away when there is nothing to look at.
void Review(Action a) {
    if (a.plan.Empty() && a.plan.iniWrites.empty()) {
        Finish(a);
        return;
    }
    g_review = std::move(a);
    g_openReview = true;
}

// The files of a mod release, into the plan. The loader is left alone when the one there is the manager's own
// (it is the same in every release).
bool PlanMod(ChangePlan& p, const ModInfo& m, const Release& rel, std::string& error) {
    for (const char* file : {m.dll, kLoaderDll}) {
        const Asset* a = rel.Get(file);
        if (!a) {
            error = rel.tag + " has no " + file;
            return false;
        }
        if (a->sha256.empty()) {
            error = std::string(file) + " in " + rel.tag + " has no published checksum, so the manager won't install it";
            return false;
        }
        bool loader = std::string(file) == kLoaderDll;
        if (loader && std::any_of(p.files.begin(), p.files.end(), [](const FileChange& f) { return f.name == kLoaderDll; })) continue;
        if (loader && Exists(g_game / kLoaderDll) && g_rec.Owns(kLoaderDll, FileSha(g_game / kLoaderDll))) continue;
        WantFile w;
        w.name = file;
        w.mod = loader ? "loader" : m.name;
        w.version = loader ? "" : rel.version;
        w.sha256 = a->sha256;
        w.url = a->url;
        w.source = CachedFile(CacheRoot(), m.name, rel.version, file);
        w.size = a->size;
        PlanAdd(p, g_game, g_rec, w);
    }
    // Switched off by manager 0.1 / 0.2.
    if (Exists(g_game / (std::string(m.dll) + ".disabled"))) p.deletes.push_back(std::string(m.dll) + ".disabled");
    return true;
}

void PlanUninstall(ChangePlan& p, const ModInfo& m, bool loaderStays) {
    PlanRemove(p, g_game, g_rec, m.dll);
    if (Exists(g_game / (std::string(m.dll) + ".disabled"))) p.deletes.push_back(std::string(m.dll) + ".disabled");
    if (!loaderStays && g_rec.Find(kLoaderDll)) PlanRemove(p, g_game, g_rec, kLoaderDll);
}

bool Ready() {
    if (g_game.empty()) {
        SetStatus("Choose the game folder first.", true);
        return false;
    }
    if (!g_releasesLoaded) {
        SetStatus(g_releaseError.empty() ? "Still checking GitHub for the mods. Try again in a moment."
                                         : "Can't reach GitHub: " + g_releaseError,
                  true);
        return false;
    }
    return true;
}

void InstallMod(const ModInfo& m, const std::string& compact = "") {
    if (!Ready()) return;
    const Release* rel = Latest(m);
    if (!rel) return SetStatus(std::string("No release of ") + m.name + " on GitHub.", true);
    Action a;
    a.plan.title = std::string("Install ") + m.name + " " + rel->version;
    std::string error;
    if (!PlanMod(a.plan, m, *rel, error)) return SetStatus(error, true);
    if (!compact.empty()) {
        a.plan.iniWrites.push_back({m.ini, compact});
        a.plan.settings = DescribeSettings(FilesOf(m).ini, compact, ItemNames());
    }
    a.done = std::string(m.name) + " " + rel->version + " installed.";
    Review(std::move(a));
}

void UninstallMod(const ModInfo& m) {
    if (g_game.empty()) return;
    Action a;
    a.plan.title = std::string("Uninstall ") + m.name;
    bool other = false;
    for (const ModInfo& o : Mods())
        if (&o != &m) other |= Installed(o);
    PlanUninstall(a.plan, m, other);
    a.done = std::string(m.name) + " uninstalled.";
    Review(std::move(a));
}

// An update to `rel`: reviewed when asked for, announced either way.
Action UpdateTo(const ModInfo& m, const Release& rel, std::string& error) {
    Action a;
    std::string from = FilesOf(m).version;
    a.plan.title = std::string("Update ") + m.name + " to " + rel.version;
    PlanMod(a.plan, m, rel, error);
    a.done = std::string(m.name) + " updated to " + rel.version + ".";
    a.notes.push_back({std::string(m.name) + " updated " + (from == "?" ? "" : from + " \xE2\x86\x92 ") + rel.version + ".",
                       std::string(m.name) + " " + rel.version, rel.notes, rel.page});
    return a;
}

void UpdateMod(const ModInfo& m) {
    if (!Ready()) return;
    const Release* rel = Latest(m);
    if (!rel) return;
    std::string error;
    Action a = UpdateTo(m, *rel, error);
    if (!error.empty()) return SetStatus(error, true);
    Review(std::move(a));
}

// Back to the release before the installed one; the newest is skipped, so an automatic update doesn't undo it.
void Rollback(const ModInfo& m) {
    if (!Ready()) return;
    const Release* prev = g_releases.Before(m.name, FilesOf(m).version, m.dll);
    const Release* latest = Latest(m);
    if (!prev) return SetStatus(std::string("No earlier release of ") + m.name + " to go back to.", true);
    Action a;
    a.plan.title = std::string("Roll back ") + m.name + " to " + prev->version;
    std::string error;
    if (!PlanMod(a.plan, m, *prev, error)) return SetStatus(error, true);
    if (latest) {
        a.skip[m.name] = latest->version;
        a.plan.settings.push_back("Skip " + std::string(m.name) + " " + latest->version + " (no automatic update back to it)");
    }
    a.done = std::string(m.name) + " is back to " + prev->version + ".";
    Review(std::move(a));
}

// Everything the manager put into the game folder comes out, and what it replaced goes back.
bool g_removeSettingsToo = false;
void RemoveAll() {
    if (g_game.empty()) return;
    Action a;
    a.plan.title = "Remove all mods";
    std::set<std::string> names;
    for (const RecordEntry& e : g_rec.files) names.insert(e.name);
    for (const ModInfo& m : Mods())
        if (Exists(g_game / m.dll)) names.insert(m.dll);
    for (const std::string& n : names) PlanRemove(a.plan, g_game, g_rec, n);
    for (const ModInfo& m : Mods()) {
        std::string dll = m.dll, base = dll.substr(0, dll.size() - 4);
        std::vector<std::string> extra = {dll + ".disabled"};
        if (g_removeSettingsToo) extra.insert(extra.end(), {m.ini, base + ".log"});
        for (const std::string& f : extra)
            if (Exists(g_game / f)) a.plan.deletes.push_back(f);
    }
    a.done = "All mods removed. The game folder is back as it was.";
    Review(std::move(a));
}

// --- Updates -----------------------------------------------------------------------------------------------------

std::string g_staged;  // a manager version downloaded and waiting
bool g_stageQueued = false;

void RestartNow() {
    std::string error;
    if (!plat::SwapInUpdate(error)) return SetStatus("Could not update the manager: " + error, true);
    Log("manager: swapped in " + g_staged + ", restarting");
    g_staged.clear();
    if (plat::Relaunch({"--updated"})) g_quit = true;
    else SetStatus("The manager is updated; start it again to use the new version.");
}

void StageManager(const Release& rel, bool restartAfter) {
    const Asset* a = rel.Get(plat::ManagerAsset());
    if (!a || g_stageQueued) return;
    g_stageQueued = true;
    Asset asset = *a;
    std::string v = rel.version;
    Enqueue("Downloading manager " + v, [asset, v, restartAfter] {
        std::string error;
        fs::path target = plat::StagedUpdate();
        bool ok = DownloadChecked(Get, asset, target, error);
        if (ok) {
            fs::path vf = target;
            vf += ".version";
            WriteFile(vf, v);
            Log("manager " + v + " downloaded and checked (sha256 " + asset.sha256 + ")");
        }
        return JobResult{ok ? "" : "Manager update: " + error, !ok, [ok, v, restartAfter] {
            g_stageQueued = false;
            if (!ok) return;
            g_staged = v;
            if (restartAfter) RestartNow();
            else plat::Attention();
        }};
    });
}

std::string ShownVersion(const ModInfo& m) {
    if (g_opt.demo == "banner") return "0.1.0";
    return FilesOf(m).version;
}
std::string ManagerVersion() { return g_opt.demo == "banner" ? "0.1.0" : kVersion; }

// After a new release list: opted-in updates go ahead (once per version), the rest show as banners.
void AfterReleases() {
    FetchCatalogs();
    if (g_game.empty() || !g_cfg.setupDone) return;
    for (const ModInfo& m : Mods()) {
        const Release* rel = Latest(m);
        if (!rel) continue;
        if (DecideUpdate(FilesOf(m).version, rel->version, g_cfg.autoMods, g_cfg.skip[m.name]) != UpdateAction::Auto) continue;
        std::string key = std::string(m.name) + "@" + rel->version;
        if (!g_autoTried.insert(key).second) continue;
        std::string error;
        Action a = UpdateTo(m, *rel, error);
        if (!error.empty()) {
            SetStatus(error, true);
            continue;
        }
        Log("automatic update: " + key);
        Run(std::move(a));
    }
    if (const Release* rel = LatestManager())
        if (DecideUpdate(kVersion, rel->version, g_cfg.autoManager, g_cfg.skip["Manager"]) == UpdateAction::Auto &&
            g_staged != rel->version && g_autoTried.insert("Manager@" + rel->version).second)
            StageManager(*rel, false);
}

void CheckForUpdates() {
    if (g_fetching || g_readOnly) return;
    g_fetching = true;
    std::string etag = g_releasesEtag, cached = g_releasesBody;
    Enqueue("Checking for updates", [etag, cached] {
        HttpResponse r = plat::Http(kReleasesUrl, cached.empty() ? "" : etag);
        CheckResult c = ReadCheck(r, cached, etag);
        if (r.status == 304) Log("releases: 304 not modified");
        else Log("releases: " + std::to_string(r.status) + (c.failed ? " (" + c.error + ")" : ""));
        if (c.changed) {
            WriteFile(CacheRoot() / "releases.json", c.body);
            WriteFile(CacheRoot() / "releases.etag", c.etag);
        }
        return JobResult{"", false, [r, c] {
            g_fetching = false;
            g_poller.Done(UnixNow(), g_cfg.checkMinutes, r);
            g_lastCheck = UnixNow();
            g_releaseError = c.failed ? c.error : "";
            if (c.body.empty()) return;
            g_releasesBody = c.body;
            g_releasesEtag = c.etag;
            g_releases = ParseReleases(c.body);
            g_releasesLoaded = g_releases.ok;
            AfterReleases();
        }};
    });
}

// The release list from the last check, so the window has something to show before (or without) the network.
void LoadCachedReleases() {
    auto body = ReadFile(CacheRoot() / "releases.json");
    if (!body) return;
    ReleaseList list = ParseReleases(*body);
    if (!list.ok) return;
    g_releases = std::move(list);
    g_releasesBody = *body;
    g_releasesEtag = ReadFile(CacheRoot() / "releases.etag").value_or("");
    g_releasesLoaded = true;
}

// --- Setups ------------------------------------------------------------------------------------------------------

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

// A setup (a loaded code, or the defaults): which mods to install or remove, and the settings, in one review.
void ApplySetup(const Setup& s, const std::string& title) {
    if (!Ready()) return;
    Action a;
    a.plan.title = title;
    std::string error;
    bool modsStay = s.crabMods, syncStays = s.crabSync;
    for (const ModInfo* m : {&ModsInfo(), &SyncInfo()}) {
        bool want = m == &ModsInfo() ? s.crabMods : s.crabSync;
        const Release* rel = Latest(*m);
        if (want && (!Installed(*m) || DecideUpdate(FilesOf(*m).version, rel ? rel->version : "", true, "") != UpdateAction::None)) {
            if (!rel || !PlanMod(a.plan, *m, *rel, error)) return SetStatus(error.empty() ? std::string("No release of ") + m->name : error, true);
        }
        if (!want && Installed(*m)) PlanUninstall(a.plan, *m, m == &ModsInfo() ? syncStays : modsStay);
    }
    if (s.crabMods && g_catalog) {
        std::vector<const CatalogItem*> items;
        for (const CatalogItem& it : g_catalog->items) items.push_back(&it);
        std::string ini = SetupIni(s, items);
        a.plan.iniWrites.push_back({ModsInfo().ini, ini});
        for (const std::string& w : DescribeSettings(g_mods.ini, ini, ItemNames())) a.plan.settings.push_back(w);
    }
    if (s.crabSync && g_syncItem) {
        std::string ini = SetupIni(s, {&*g_syncItem});
        a.plan.iniWrites.push_back({SyncInfo().ini, ini});
        std::map<std::string, std::map<std::string, std::string>> defaults;
        for (const Setting& st : g_syncItem->settings) defaults[""][st.key] = st.def;
        for (const std::string& w : DescribeSettings(g_sync.ini, ini, {}, defaults)) a.plan.settings.push_back("CrabSync: " + w);
    }
    a.done = "Setup applied. It is used the next time the game starts.";
    Review(std::move(a));
}

// --- Crashes -------------------------------------------------------------------------------------------------------

std::optional<BuiltReport> g_crashOffer;
bool g_openCrash = false;

std::string Versions() {
    std::string v;
    for (const ModInfo& m : Mods()) {
        const std::string& ver = FilesOf(m).version;
        v += std::string(m.name) + " " + (ver.empty() ? "not installed" : ver) + ", ";
    }
    return v + "Manager " + kVersion;
}

BuiltReport MakeReport(const std::optional<Crash>& crash) {
    return BuildReport({g_game, plat::SavedDirs(g_game), crash, ReportsDir(), plat::Secrets(), Versions(), plat::PlatformName()});
}

void OfferReport(const BuiltReport& b) {
    if (!b.error.empty()) return SetStatus("Could not save the report: " + b.error, true);
    SetStatus("Saved " + U8(b.zip));
    Log("report saved: " + U8(b.zip));
    g_crashOffer = b;
    g_openCrash = true;
    if (b.report.crashed) plat::Attention();
}

// Every few seconds: a crash folder the manager hasn't offered yet, once it has stopped growing (the crash
// reporter writes it over a few seconds). Ones over 3 days old (from before the manager was used) are skipped.
void CheckForCrash() {
    static std::string pending;
    static uint64_t pendingSize = 0;
    if (g_crashOffer || g_readOnly) return;
    auto crash = NewestCrash(plat::SavedDirs(g_game));
    if (!crash || crash->name == g_cfg.lastCrash) return;
    if (crash->time + std::chrono::hours(72) < FileNow()) {
        g_cfg.lastCrash = crash->name;
        SaveConfig();
        return;
    }
    if (pending != crash->name || pendingSize != crash->size) {
        pending = crash->name;
        pendingSize = crash->size;
        return;
    }
    g_cfg.lastCrash = crash->name;
    SaveConfig();
    OfferReport(MakeReport(crash));
}

void CollectLogs() {
    auto crash = NewestCrash(plat::SavedDirs(g_game));  // in too if it's from the last day
    if (crash && crash->time + std::chrono::hours(24) < FileNow()) crash.reset();
    OfferReport(MakeReport(crash));
}

// --- The game folder -----------------------------------------------------------------------------------------------

void SetGameDir(const fs::path& dir) {
    g_game = dir;
    g_rec = LoadRecord(StateDir());
    // A manager before 0.4 kept no record: the mods installed now are taken over as they are.
    if (!g_rec.exists && !g_game.empty() && AdoptInstalled(g_game, g_rec) && !g_rec.files.empty() && !g_readOnly) {
        SaveRecord(StateDir(), g_rec);
        for (const RecordEntry& e : g_rec.files) Log("adopted " + e.name + " " + e.version + " sha256 " + e.sha256);
    }
    Rescan();
}

void UseGameDir(const fs::path& picked) {
    fs::path dir = NormalizeGameDir(picked);
    if (dir.empty()) return SetStatus("Crab Champions is not in that folder (no CrabChampions-Win64-Shipping.exe).", true);
    SetGameDir(dir);
    SaveConfig();
    SetStatus("Found Crab Champions.");
    FetchCatalogs();
}

char g_pathInput[1024] = "";
bool g_openPath = false;

void BrowseForGame() {
    auto picked = plat::PickFolder("Pick the Crab Champions folder (or its CrabChampions/Binaries/Win64 folder)");
    if (!picked) {
        std::snprintf(g_pathInput, sizeof g_pathInput, "%s", U8(g_game).c_str());
        g_openPath = true;
    } else if (!picked->empty()) {
        UseGameDir(*picked);
    }
}

bool g_launchAfterJobs = false;

void Play() {
    if (g_pending > 0) {
        g_launchAfterJobs = true;
        SetStatus("Starting the game when the manager is done...");
        return;
    }
    SetStatus(plat::LaunchGame(g_game));
}

bool LinuxPlayer() { return plat::NativeLinux() || plat::RunningOnWine(); }

std::optional<bool> LaunchOptionSet() {
    static std::optional<bool> cached;
    static double checked = -100;
    if (ImGui::GetTime() - checked > 10) cached = LaunchOptionSetIn(plat::SteamRoots()), checked = ImGui::GetTime();
    return cached;
}

void Copy(const std::string& text) {
    ImGui::SetClipboardText(text.c_str());
    SetStatus("Copied.");
}

// =================================================================================================================
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
constexpr ImU32 Blue = IM_COL32(96, 150, 230, 255);
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
    st.WindowRounding = S(4);
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
    st.PopupBorderSize = S(1);
    st.ChildBorderSize = S(1);
    st.FrameBorderSize = 0;
    st.GrabMinSize = S(10);
    ImVec4* c = st.Colors;
    c[ImGuiCol_Text] = V4(col::Text);
    c[ImGuiCol_TextDisabled] = V4(col::Faint);
    c[ImGuiCol_WindowBg] = V4(col::Bg);
    c[ImGuiCol_ChildBg] = V4(0);
    c[ImGuiCol_PopupBg] = V4(IM_COL32(34, 37, 43, 255));
    c[ImGuiCol_ModalWindowDimBg] = V4(IM_COL32(0, 0, 0, 140));
    c[ImGuiCol_TitleBg] = V4(col::Bar);
    c[ImGuiCol_TitleBgActive] = V4(col::Bar);
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
    c[ImGuiCol_NavCursor] = V4(Alpha(col::Accent, 0.6f));
    c[ImGuiCol_Separator] = V4(col::Line);
}

// Install is the one colored button; everything else is plain.
// Tests press buttons by their label (Press), and see which were drawn and the notices shown.
std::set<std::string> g_press, g_pressing;
std::map<std::string, bool> g_drawn, g_drawing;  // label -> enabled
std::vector<std::string> g_noticeTexts, g_noticing;
bool Pressed(const char* label, bool enabled) {
    bool& d = g_drawing[label];
    d = d || enabled;
    return enabled && g_pressing.count(label);
}

bool Button(const char* label, ImVec2 size = {0, 0}, bool primary = false, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    if (primary) {
        ImGui::PushStyleColor(ImGuiCol_Button, col::Accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col::AccentHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, col::Accent);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    }
    bool r = ImGui::Button(label, size) || Pressed(label, enabled);
    if (primary) ImGui::PopStyleColor(4);
    if (!enabled) ImGui::EndDisabled();
    return r && enabled;
}

// One of two answers, neither chosen until the player picks.
bool Choice(const char* label, bool chosen) {
    if (chosen) {
        ImGui::PushStyleColor(ImGuiCol_Button, Alpha(col::Accent, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col::AccentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    }
    bool r = ImGui::Button(label) || Pressed(label, true);
    if (chosen) ImGui::PopStyleColor(3);
    return r;
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

std::string Capital(std::string s) {
    if (!s.empty()) s[0] = char(std::toupper(uint8_t(s[0])));
    return s;
}

std::string Join(const std::vector<std::string>& v, const char* sep = ", ") {
    std::string r;
    for (size_t i = 0; i < v.size(); i++) r += (i ? sep : "") + v[i];
    return r;
}

// A notice across the top of a page: a colored bar, its text, and buttons under it.
void Notice(ImU32 color, const std::string& text, const std::function<void()>& buttons = {}) {
    g_noticing.push_back(text);
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

// =================================================================================================================
// Pages

enum class Page { Mods, Share, Settings, Help };
Page g_page = Page::Mods;
std::string g_select = "pack";  // the mod in the details pane: "pack" (CrabMods itself), "sync", or an item section
char g_search[128] = "";
int g_show = 0;   // 0 all, 1 installed, 2 not installed
int g_kind = -1;  // index into kKinds, -1 all
SortBy g_sort = SortBy::Type;
bool g_sortDesc = false;
std::map<std::string, std::string> g_editing;  // "section|key" -> value being typed or dragged

std::vector<Entry> Entries() {
    std::vector<Entry> e;
    std::string count = g_catalog ? std::to_string(g_catalog->items.size()) + " " : "";
    e.push_back({"pack", "CrabMods", "Mod pack", "",
                 "Adds " + count + "perks, relics, weapons, weapon mods and tweaks. Everyone in a game needs it.", ModsInstalled()});
    e.push_back({"sync", "CrabSync", "Multiplayer", "", g_syncItem ? g_syncItem->about : "Shared team inventory and health.",
                 SyncInstalled()});
    if (g_catalog)
        for (const CatalogItem& it : g_catalog->items) {
            Entry x{it.section, it.name, it.kind, it.rarity, it.about, ModsInstalled() && ItemOn(g_mods.ini, it), &it};
            auto s = g_seen.find(it.section);
            x.since = s == g_seen.end() ? "" : s->second;
            x.isNew = IsNew(g_seen, it.section, g_catalogVersion);
            e.push_back(std::move(x));
        }
    SortEntries(e, g_sort, g_sortDesc);
    return e;
}

// An item: on or off in CrabMods.ini (a settings change, no files). Without CrabMods, installing one installs
// CrabMods with that item only (reviewed first).
void SetItem(const CatalogItem& it, bool on) {
    if (on && !ModsInstalled()) return InstallMod(ModsInfo(), AllItems(false, it.section));
    if (WriteSettings(ModsInfo(), "[" + it.section + "]\nenabled = " + (on ? "true" : "false") + "\n"))
        SetStatus(it.name + (on ? " installed." : " uninstalled.") + " Takes effect next time the game starts.");
}

void Toggle(const Entry& e, bool on) {
    if (e.id == "pack") on ? InstallMod(ModsInfo()) : UninstallMod(ModsInfo());
    else if (e.id == "sync") on ? InstallMod(SyncInfo()) : UninstallMod(SyncInfo());
    else if (e.item) SetItem(*e.item, on);
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
    if (e.isNew) sub += ", new in " + e.since;
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
        if (on < int(g_catalog->items.size()) && Button("Install all items", ImVec2(0, 0), false, g_pending == 0) &&
            WriteSettings(ModsInfo(), AllItems(true)))
            SetStatus("All CrabMods items installed.");
    }
    for (const ModInfo* m : {&ModsInfo(), &SyncInfo()}) {
        if (e.id != (m == &ModsInfo() ? "pack" : "sync") || !Installed(*m)) continue;
        const Release* rel = Latest(*m);
        if (rel && DecideUpdate(FilesOf(*m).version, rel->version, false, "") != UpdateAction::None) {
            ImGui::SameLine();
            if (Button(("Update to " + rel->version).c_str(), ImVec2(0, 0), false, g_pending == 0)) UpdateMod(*m);
        }
        if (const Release* prev = g_releases.Before(m->name, FilesOf(*m).version, m->dll)) {
            ImGui::SameLine();
            if (Button(("Roll back to " + prev->version).c_str(), ImVec2(0, 0), false, g_pending == 0)) Rollback(*m);
        }
        if (auto s = g_cfg.skip.find(m->name); s != g_cfg.skip.end())
            Small(col::Faint, "Skipping " + s->second + ". Updates resume with the next release.");
    }

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
            if (WriteSettings(mod, compact)) SetStatus(e.name + " settings reset.");
        }
    }
}

// --- Notices on the Mods page ----------------------------------------------------------------------------------------

void UpdateQuestion(const char* question, std::optional<bool>& answer) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(question);
    ImGui::SameLine(S(300));
    ImGui::PushID(question);
    if (Choice("Yes, automatically", answer == true)) answer = true;
    ImGui::SameLine();
    if (Choice("No, ask me first", answer == false)) answer = false;
    ImGui::PopID();
}

void Notices() {
    if (g_game.empty())
        Notice(col::Red, "Crab Champions wasn't found.", [] {
            if (Button("Choose game folder...", ImVec2(0, 0), true)) BrowseForGame();
        });
    // Players coming from manager 0.3 (which updated everything on its own) are asked once.
    if (!g_cfg.autoMods || !g_cfg.autoManager)
        Notice(col::Blue, "How should updates work? Until you choose, the manager only tells you about them.", [] {
            std::optional<bool> mods = g_cfg.autoMods, manager = g_cfg.autoManager;
            UpdateQuestion("Install mod updates automatically?", mods);
            UpdateQuestion("Update the manager automatically?", manager);
            if (mods != g_cfg.autoMods || manager != g_cfg.autoManager) {
                g_cfg.autoMods = mods;
                g_cfg.autoManager = manager;
                SaveConfig();
                if (g_cfg.autoMods && g_cfg.autoManager) AfterReleases();
            }
        });
    for (size_t i = 0; i < g_notes.size(); i++) {
        const Note n = g_notes[i];
        bool close = false;
        ImGui::PushID(int(i));
        Notice(col::Green, n.text, [&] {
            if (!n.notes.empty() || !n.page.empty()) {
                if (Button("What's new")) g_whatsNew = WhatsNew{n.title, n.notes, n.page};
                ImGui::SameLine();
            }
            close = Button("OK");
        });
        ImGui::PopID();
        if (close) {
            g_notes.erase(g_notes.begin() + long(i));
            break;
        }
    }
    // Updates the player hasn't let happen on their own.
    for (const ModInfo& m : Mods()) {
        const Release* rel = Latest(m);
        std::string have = ShownVersion(m);
        if (!rel || DecideUpdate(have, rel->version, g_opt.demo == "banner" ? std::optional<bool>(false) : g_cfg.autoMods,
                                 g_cfg.skip[m.name]) != UpdateAction::Banner)
            continue;
        ImGui::PushID(m.name);
        Notice(col::Yellow, std::string(m.name) + " " + rel->version + " is available (you have " + (have == "?" ? "an older one" : have) + ").",
               [&] {
                   if (Button("Update", ImVec2(0, 0), true, g_pending == 0)) UpdateMod(m);
                   ImGui::SameLine();
                   if (Button("Skip this version")) {
                       g_cfg.skip[m.name] = rel->version;
                       SaveConfig();
                   }
                   ImGui::SameLine();
                   if (Button("What's new")) ShowWhatsNew(*rel, m.name);
               });
        ImGui::PopID();
    }
    if (const Release* rel = LatestManager()) {
        std::string have = ManagerVersion();
        if (g_staged == rel->version) {
            Notice(col::Green,
                   "CrabMods Manager " + rel->version + " is downloaded and checked." +
                       (g_cfg.autoManager == true ? " It goes in when you close the manager." : ""),
                   [&] {
                       if (Button("Restart now", ImVec2(0, 0), true, g_pending == 0)) RestartNow();
                       ImGui::SameLine();
                       if (Button("What's new")) ShowWhatsNew(*rel, "CrabMods Manager");
                   });
        } else if (DecideUpdate(have, rel->version, g_opt.demo == "banner" ? std::optional<bool>(false) : g_cfg.autoManager,
                                g_cfg.skip["Manager"]) == UpdateAction::Banner) {
            Notice(col::Yellow, "CrabMods Manager " + rel->version + " is available (you have " + have + ").", [&] {
                if (Button("Update", ImVec2(0, 0), true, g_pending == 0 && !g_stageQueued)) StageManager(*rel, true);
                ImGui::SameLine();
                if (Button("Skip this version")) {
                    g_cfg.skip["Manager"] = rel->version;
                    SaveConfig();
                }
                ImGui::SameLine();
                if (Button("What's new")) ShowWhatsNew(*rel, "CrabMods Manager");
            });
        }
    }
    if (!g_waiting.empty()) {
        std::vector<std::string> titles;
        for (const Action& a : g_waiting) titles.push_back(a.plan.title);
        Notice(col::Yellow, "Waiting for Crab Champions to close: " + Join(titles, "; ") + ".", [] {
            if (Button("Cancel")) g_waiting.clear();
        });
    }
    if (LinuxPlayer() && !g_cfg.linuxHintDone && (ModsInstalled() || SyncInstalled()) && LaunchOptionSet() != true)
        Notice(col::Yellow,
               "Linux / Steam Deck: add this to Crab Champions' launch options in Steam (Properties > General), or the mods "
               "won't load:",
               [] {
                   TextColored(col::Yellow, kLaunchOption);
                   if (Button("Copy")) Copy(kLaunchOption);
                   ImGui::SameLine();
                   if (Button("Done")) {
                       g_cfg.linuxHintDone = true;
                       SaveConfig();
                   }
               });
}

void ModsPage() {
    Notices();
    std::vector<Entry> all = Entries();
    int installed = 0;
    for (auto& e : all) installed += e.installed;

    // Toolbar.
    ImGui::SetNextItemWidth(S(220));
    ImGui::InputTextWithHint("##search", "Search", g_search, sizeof g_search);
    ImGui::SameLine();
    const char* shows[] = {"All", "Installed", "Not installed"};
    ImGui::SetNextItemWidth(S(124));
    ImGui::Combo("##show", &g_show, shows, 3);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(S(134));
    std::string kindLabel = g_kind < 0 ? "All types" : std::string(kKinds[g_kind]) + "s";
    if (ImGui::BeginCombo("##kind", kindLabel.c_str())) {
        if (ImGui::Selectable("All types", g_kind < 0)) g_kind = -1;
        for (int i = 0; i < int(std::size(kKinds)); i++)
            if (ImGui::Selectable((std::string(kKinds[i]) + "s").c_str(), g_kind == i)) g_kind = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(S(150));
    std::string sortLabel = std::string("Sort: ") + kSortNames[int(g_sort)];
    if (ImGui::BeginCombo("##sort", sortLabel.c_str())) {
        for (int i = 0; i < int(std::size(kSortNames)); i++)
            if (ImGui::Selectable(kSortNames[i], int(g_sort) == i)) g_sort = SortBy(i), g_sortDesc = false;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##dir", g_sortDesc ? ImGuiDir_Up : ImGuiDir_Down)) g_sortDesc = !g_sortDesc;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(g_sortDesc ? "Reversed" : "Reverse the order");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    TextColored(col::Faint, std::to_string(installed) + " of " + std::to_string(all.size()) + " installed");

    std::string q = Lower(std::string(g_search));
    std::vector<const Entry*> shown;
    for (const Entry& e : all) {
        if ((g_show == 1 && !e.installed) || (g_show == 2 && e.installed)) continue;
        if (g_kind >= 0 && e.item && e.kind != kKinds[g_kind]) continue;
        if (g_kind >= 0 && !e.item) continue;
        if (!q.empty() && Lower(e.name + " " + e.kind + " " + e.rarity + " " + e.about + " " + e.id).find(q) == std::string::npos)
            continue;
        shown.push_back(&e);
    }

    float h = ImGui::GetContentRegionAvail().y;
    float detailsW = std::max(S(360), ImGui::GetContentRegionAvail().x * 0.38f);
    float listW = ImGui::GetContentRegionAvail().x - detailsW - S(10);
    ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter |
                         ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable |
                         ImGuiTableFlags_SortTristate;
    if (ImGui::BeginTable("mods", 4, tf, ImVec2(listW, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0, ImGuiID(SortBy::Name));
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, S(100), ImGuiID(SortBy::Type));
        ImGui::TableSetupColumn("Rarity", ImGuiTableColumnFlags_WidthFixed, S(76), ImGuiID(SortBy::Rarity));
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, S(86), ImGuiID(SortBy::Installed));
        ImGui::TableHeadersRow();
        // Clicking a header sorts by it; the Sort menu shows the same choice.
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty) {
            if (specs->SpecsCount > 0) {
                g_sort = SortBy(specs->Specs[0].ColumnUserID);
                g_sortDesc = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            } else {
                g_sort = SortBy::Type, g_sortDesc = false;
            }
            specs->SpecsDirty = false;
        }
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
            if (e->isNew) {
                ImVec2 a = ImGui::GetItemRectMin();
                float x = a.x + ImGui::CalcTextSize(e->name.c_str()).x + S(8);
                ImVec2 ts = g_fSmall->CalcTextSizeA(g_fSmall->FontSize, FLT_MAX, 0, "NEW");
                ImDrawList* dl = ImGui::GetWindowDrawList();
                float top = a.y + (S(22) - ts.y) * 0.5f - S(1);
                dl->AddRectFilled(ImVec2(x, top), ImVec2(x + ts.x + S(10), top + ts.y + S(2)), Alpha(col::Accent, 0.9f), S(3));
                dl->AddText(g_fSmall, g_fSmall->FontSize, ImVec2(x + S(5), top + S(1)), IM_COL32(255, 255, 255, 255), "NEW");
            }
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
            if (e->installed ? Button("Uninstall##row", ImVec2(-FLT_MIN, 0), false, CanChange(*e))
                             : Button("Install##row", ImVec2(-FLT_MIN, 0), true, CanChange(*e)))
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
    ImGui::BeginChild("details", ImVec2(detailsW, h), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    for (const Entry& e : all)
        if (e.id == g_select) Details(e);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// --- First run -------------------------------------------------------------------------------------------------------
// Nothing in the game folder changes, and nothing is saved, until Install. Only the mods' small catalog files are
// downloaded before that, to show what is in them.

bool g_wantMods = true, g_wantSync = true;
std::optional<bool> g_askMods, g_askManager;

Action WizardAction(std::string& error) {
    Action a;
    a.plan.title = "Install the mods";
    a.setupDone = true;
    std::vector<std::string> names;
    for (const ModInfo* m : {&ModsInfo(), &SyncInfo()}) {
        if (!(m == &ModsInfo() ? g_wantMods : g_wantSync)) continue;
        const Release* rel = Latest(*m);
        if (!rel) {
            error = std::string("no release of ") + m->name + " found";
            continue;
        }
        if (Installed(*m) && DecideUpdate(FilesOf(*m).version, rel->version, true, "") == UpdateAction::None) continue;
        if (!PlanMod(a.plan, *m, *rel, error)) continue;
        names.push_back(std::string(m->name) + " " + rel->version);
    }
    a.done = names.empty() ? "Ready." : Join(names, " and ") + " installed. Uninstall any item you don't want.";
    return a;
}

void WizardPage() {
    float w = std::min(ImGui::GetContentRegionAvail().x, S(780));
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - w) * 0.5f + ImGui::GetCursorPosX());
    ImGui::BeginChild("wizard", ImVec2(w, 0));
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted("Welcome to CrabMods");
    ImGui::PopFont();
    Wrapped(col::Dim, "Choose what to install. Nothing in your game folder changes until you press Install, and you can "
                      "remove everything again from Settings.");

    Heading("1. Game folder");
    Wrapped(g_game.empty() ? col::Red : col::Text, g_game.empty() ? "Crab Champions wasn't found." : U8(g_game));
    if (Button(g_game.empty() ? "Choose game folder..." : "Change...", ImVec2(0, 0), g_game.empty())) BrowseForGame();

    Heading("2. Mods");
    std::string items = g_catalog ? std::to_string(g_catalog->items.size()) + " perks, relics, weapons and tweaks" : "perks, relics and tweaks";
    ImGui::Checkbox("CrabMods", &g_wantMods);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    TextColored(col::Dim, items + ". Every player needs it. Turn single items off afterwards.");
    ImGui::Checkbox("CrabSync", &g_wantSync);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    TextColored(col::Dim, "Shared team inventory and health in multiplayer. Only the host needs it.");

    Heading("3. Updates");
    Small(col::Dim, "Either way the manager checks GitHub every few minutes and tells you when something new is out.");
    UpdateQuestion("Install mod updates automatically?", g_askMods);
    UpdateQuestion("Update the manager automatically?", g_askManager);

    Heading("4. What happens");
    std::string error;
    static std::string key;
    static Action plan;
    static std::string planError;
    std::string k = U8(g_game) + (g_wantMods ? "M" : "") + (g_wantSync ? "S" : "") + std::to_string(g_releases.all.size()) +
                    (g_releasesLoaded ? "L" : "") + std::to_string(g_rec.files.size());
    if (k != key && g_releasesLoaded && !g_game.empty()) {
        key = k;
        planError.clear();
        plan = WizardAction(planError);
    }
    bool ready = g_releasesLoaded && !g_game.empty() && planError.empty();
    if (!g_releasesLoaded) {
        Wrapped(g_releaseError.empty() ? col::Dim : col::Red,
                g_releaseError.empty() ? "Checking GitHub for the latest versions..." : "Can't reach GitHub: " + g_releaseError);
        if (!g_releaseError.empty() && Button("Try again")) CheckForUpdates();
    } else if (g_game.empty()) {
        Wrapped(col::Dim, "Choose the game folder first.");
    } else if (!planError.empty()) {
        Wrapped(col::Red, Capital(planError));
    } else {
        auto lines = plan.plan.Lines();
        if (lines.empty()) Wrapped(col::Dim, "Nothing to install: what you chose is installed already.");
        for (const std::string& l : lines) Wrapped(col::Text, "\xE2\x80\xA2 " + l);
        Small(col::Faint, "Into " + U8(g_game) + ". Each file is checked against the SHA-256 checksum GitHub publishes for "
                          "it. A file that is replaced is kept in the manager's backups.");
    }
    ImGui::Dummy(ImVec2(0, S(8)));
    bool answered = g_askMods && g_askManager;
    if (Button("Install", ImVec2(S(140), 0), true, ready && answered && g_pending == 0)) {
        g_cfg.autoMods = g_askMods;
        g_cfg.autoManager = g_askManager;
        g_cfg.setupDone = true;
        SaveConfig();
        Run(plan);
        key.clear();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !answered) ImGui::SetTooltip("Answer the two update questions first.");
    ImGui::SameLine();
    if (Button("Just browse", ImVec2(S(140), 0))) {
        g_cfg.autoMods = g_askMods;
        g_cfg.autoManager = g_askManager;
        g_cfg.setupDone = true;
        SaveConfig();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    TextColored(col::Faint, "Look around first; install from the Mods tab whenever you like.");
    ImGui::EndChild();
}

// --- Share page ------------------------------------------------------------------------------------------------------

char g_codeInput[4096] = "";

std::string Summary(const Setup& s) {
    std::vector<std::string> parts;
    if (s.crabMods) {
        int on = 0;
        for (auto& [k, v] : s.on) on += v;
        parts.push_back("CrabMods (" + std::to_string(on) + "/" + std::to_string(s.on.size()) + " items)");
    }
    if (s.crabSync) parts.push_back("CrabSync");
    return parts.empty() ? "no mods" : Join(parts, " + ");
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
    if (Button("Copy", ImVec2(-FLT_MIN, 0), true)) Copy(code);
    TextColored(col::Faint, Capital(Describe(CurrentSetup())));

    Heading("Load a code");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - S(90));
    ImGui::InputTextWithHint("##in", "CRAB2-...", g_codeInput, sizeof g_codeInput);
    ImGui::SameLine();
    if (Button("Paste", ImVec2(-FLT_MIN, 0)))
        if (const char* clip = ImGui::GetClipboardText()) std::snprintf(g_codeInput, sizeof g_codeInput, "%s", Trim(clip).c_str());
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
            TextColored(col::Text, Capital(Describe(*setup)));
            if (!add.empty()) Wrapped(col::Green, "Installs " + Join(add));
            if (!remove.empty()) Wrapped(col::Red, "Uninstalls " + Join(remove));
            if (setup->unknown) Wrapped(col::Yellow, std::to_string(setup->unknown) + " part(s) need a newer CrabMods and are skipped.");
            if (Button("Review and load...", ImVec2(0, 0), true, g_pending == 0 && !g_game.empty())) {
                ApplySetup(*setup, "Load a setup code");
                g_codeInput[0] = 0;
            }
        } else if (old) {
            TextColored(col::Text, "Old-style code with settings for " + old->iniName + ".");
            if (Button("Review and load...", ImVec2(0, 0), true, !g_game.empty())) {
                if (const ModInfo* m = ModNamed(old->iniName)) {
                    Action a;
                    a.plan.title = std::string("Load settings for ") + m->name;
                    a.plan.iniWrites.push_back({m->ini, old->settings});
                    a.plan.settings = DescribeSettings(FilesOf(*m).ini, old->settings, ItemNames());
                    a.done = "Settings loaded.";
                    Review(std::move(a));
                }
                g_codeInput[0] = 0;
            }
        } else {
            TextColored(col::Red, "Not a valid code (it may be cut off).");
        }
    }

    Heading("Reset");
    Small(col::Dim, "Install everything with default settings. You see every change before it is made.");
    if (Button("Reset to defaults...", ImVec2(0, 0), false, g_pending == 0 && !g_game.empty())) {
        Setup s;
        s.crabMods = s.crabSync = true;
        if (g_catalog)
            for (const CatalogItem& it : g_catalog->items) s.on[it.section] = true;
        ApplySetup(s, "Reset to defaults");
    }
    ImGui::EndChild();
}

// --- Settings page ---------------------------------------------------------------------------------------------------

std::string Ago(int64_t t) {
    if (!t) return "not yet";
    int64_t d = UnixNow() - t;
    if (d < 60) return "just now";
    if (d < 3600) return std::to_string(d / 60) + " min ago";
    return std::to_string(d / 3600) + " h ago";
}

void SettingsPage() {
    float w = std::min(ImGui::GetContentRegionAvail().x, S(780));
    ImGui::BeginChild("settings", ImVec2(w, 0));
    Heading("Game folder");
    Wrapped(g_game.empty() ? col::Red : col::Text, g_game.empty() ? "Not found" : U8(g_game));
    if (Button("Change...")) BrowseForGame();
    ImGui::SameLine();
    if (Button("Open", ImVec2(0, 0), false, !g_game.empty())) plat::OpenExternal(U8(g_game));

    Heading("Updates");
    bool mods = g_cfg.autoMods.value_or(false), manager = g_cfg.autoManager.value_or(false);
    if (ImGui::Checkbox("Install mod updates automatically", &mods) || (Pressed("Install mod updates automatically", true) && (mods = !mods, true)))
        g_cfg.autoMods = mods, SaveConfig();
    Small(col::Faint, "Off: a notice offers each update, and nothing changes until you press Update. Either way you're told "
                      "when an update went in.");
    if (ImGui::Checkbox("Update the manager automatically", &manager) || (Pressed("Update the manager automatically", true) && (manager = !manager, true)))
        g_cfg.autoManager = manager, SaveConfig();
    Small(col::Faint, "On: a new manager is downloaded and checked in the background and goes in when you close it. It never "
                      "restarts on its own.");
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Check GitHub");
    ImGui::SameLine();
    const int choices[] = {0, 1, 5, 15, 30, 60};
    const char* names[] = {"only when I press Check now", "every minute", "every 5 minutes", "every 15 minutes",
                           "every 30 minutes", "every hour"};
    int at = 2;
    for (int i = 0; i < 6; i++)
        if (choices[i] == g_cfg.checkMinutes) at = i;
    ImGui::SetNextItemWidth(S(240));
    if (ImGui::Combo("##every", &at, names, 6)) {
        g_cfg.checkMinutes = choices[at];
        g_poller.next = UnixNow() + int64_t(std::max(1, g_cfg.checkMinutes)) * 60;
        SaveConfig();
    }
    if (ImGui::Checkbox("Offline: never go online on my own", &g_cfg.offline)) SaveConfig();
    Small(col::Faint, "No update checks and no downloads unless you press Check now or install something.");
    ImGui::Dummy(ImVec2(0, S(2)));
    if (ImGui::BeginTable("versions", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("", 0, S(140));
        ImGui::TableSetupColumn("Installed", 0, S(100));
        ImGui::TableSetupColumn("Latest", 0, S(100));
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        auto line = [](const std::string& name, const std::string& have, const Release* latest) {
            ImGui::PushID(name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(name.c_str());
            ImGui::TableNextColumn();
            TextColored(have.empty() ? col::Faint : col::Text, have.empty() ? "-" : have);
            ImGui::TableNextColumn();
            std::string v = latest ? latest->version : "";
            bool newer = !have.empty() && !v.empty() && (have == "?" || CompareVersions(have, v) < 0);
            TextColored(newer ? col::Yellow : col::Dim, v.empty() ? (g_releasesLoaded ? "-" : "...") : v);
            ImGui::TableNextColumn();
            auto s = g_cfg.skip.find(name == "Manager" ? "Manager" : name);
            if (s != g_cfg.skip.end()) {
                TextColored(col::Faint, "skipping " + s->second);
                ImGui::SameLine();
                if (ImGui::SmallButton("Don't skip")) {
                    g_cfg.skip.erase(s);
                    SaveConfig();
                }
            } else if (latest && ImGui::SmallButton("What's new")) {
                ShowWhatsNew(*latest, name);
            }
            ImGui::PopID();
        };
        line("CrabMods", g_mods.version, Latest(ModsInfo()));
        line("CrabSync", g_sync.version, Latest(SyncInfo()));
        line("Manager", kVersion, LatestManager());
        ImGui::EndTable();
    }
    if (Button("Check now", ImVec2(0, 0), false, !g_fetching)) CheckForUpdates();
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    if (!g_releaseError.empty()) TextColored(col::Red, "GitHub: " + g_releaseError);
    else TextColored(col::Faint, g_lastCheck ? "Last checked " + Ago(g_lastCheck) : "Not checked yet");

    if (LinuxPlayer()) {
        Heading("Linux / Steam Deck");
        Small(col::Dim, "Steam launch options for Crab Champions (the mods don't load without it). The manager never "
                        "changes Steam's settings itself:");
        TextColored(col::Yellow, kLaunchOption);
        if (Button("Copy")) Copy(kLaunchOption);
        if (auto set = LaunchOptionSet()) {
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
        std::error_code ec;
        fs::create_directories(ReportsDir(), ec);
        plat::OpenExternal(U8(ReportsDir()));
    }

    Heading("Remove all mods");
    Small(col::Dim, "Takes out every file the manager put into the game folder and puts back any file it replaced, so the "
                    "game is as it was before. You see the list first.");
    ImGui::Checkbox("Also delete the mods' settings and logs (CrabMods.ini, CrabSync.ini, the .log files)", &g_removeSettingsToo);
    if (Pressed("Also delete the mods' settings and logs", true)) g_removeSettingsToo = !g_removeSettingsToo;
    if (Button("Remove all mods...", ImVec2(0, 0), false, !g_game.empty() && g_pending == 0)) RemoveAll();

    Heading("The manager's files");
    Small(col::Dim, "Settings (CrabModsManager.ini), the install record and backups (state/), its log, and crash reports: " +
                        U8(plat::DataDir()) + ". Downloads: " + U8(CacheRoot()) + ".");
    if (Button("Open its folder")) plat::OpenExternal(U8(plat::DataDir()));
    ImGui::SameLine();
    if (Button("Open its log", ImVec2(0, 0), false, Exists(LogPath()))) plat::OpenExternal(U8(LogPath()));

    Heading("About");
    TextColored(col::Dim, std::string("CrabMods Manager ") + kVersion + " (" + plat::PlatformName() +
                              "). Fan-made, not affiliated with Noisestorm.");
    if (Button("GitHub")) plat::OpenExternal(RepoUrl());
    ImGui::SameLine();
    if (Button("Report a bug")) plat::OpenExternal(RepoUrl("/issues/new"));
    ImGui::EndChild();
}

// --- Help page -------------------------------------------------------------------------------------------------------

struct FileCheck {
    std::string name, version, sha, verdict;
    ImU32 color;
};

// The installed files and whether each is exactly a published release file. Hashed again only when they change.
std::vector<FileCheck> CheckFiles() {
    static std::string key;
    static std::vector<FileCheck> rows;
    std::string k = U8(g_game) + std::to_string(g_releases.all.size());
    std::vector<std::string> names = {ModsInfo().dll, SyncInfo().dll, kLoaderDll};
    std::error_code ec;
    for (const std::string& n : names)
        if (!g_game.empty() && Exists(g_game / n))
            k += n + std::to_string(fs::file_size(g_game / n, ec)) +
                 std::to_string(int64_t(fs::last_write_time(g_game / n, ec).time_since_epoch().count()));
    if (k == key) return rows;
    key = k;
    rows.clear();
    for (const std::string& n : names) {
        if (g_game.empty() || !Exists(g_game / n)) continue;
        FileCheck f{n, FileVersion(g_game / n), FileSha(g_game / n), "", col::Yellow};
        if (f.version == "?") f.version = "-";
        for (auto& [mod, releases] : g_releases.all)
            for (const Release& r : releases)
                if (const Asset* a = r.Get(n); a && !a->sha256.empty() && a->sha256 == f.sha && f.verdict.empty())
                    f.verdict = "Same as the file in " + r.tag, f.color = col::Green;
        if (f.verdict.empty()) f.verdict = g_releasesLoaded ? "Not a file from any release" : "Not checked yet (offline)";
        rows.push_back(f);
    }
    return rows;
}

void HelpPage() {
    float w = std::min(ImGui::GetContentRegionAvail().x, S(820));
    ImGui::BeginChild("help", ImVec2(w, 0));
    Heading("Why does my antivirus warn about this?");
    Wrapped(col::Text,
            "Crab Champions loads the mods as DLL files from its own folder. xinput1_3.dll is a small loader: the game picks it "
            "up instead of Windows' controller library (it passes everything on to the real one) and it loads CrabMods.dll and "
            "CrabSync.dll beside it. Programs that put DLLs into another program's folder, download them from the internet and "
            "aren't signed with a paid certificate look like malware to some antivirus programs, even when they do what they "
            "say.");
    ImGui::Dummy(ImVec2(0, S(4)));
    Wrapped(col::Text,
            "What the manager does to stay honest: it asks before it changes anything in the game folder and shows you every "
            "file first. It checks every download against the SHA-256 checksum GitHub publishes for it, and refuses one that "
            "doesn't match. It keeps a copy of any file it replaces, and Remove all mods puts them back. It never loads or "
            "runs the mods itself. Its source code, and the mods', is on GitHub.");
    ImGui::Dummy(ImVec2(0, S(4)));
    Wrapped(col::Dim, "If your antivirus removed a file, restore it from its quarantine or add an exception for the game folder, "
                      "then compare the checksums below with SHA256SUMS.txt on the release page.");

    Heading("Installed files");
    auto rows = CheckFiles();
    if (rows.empty()) Wrapped(col::Dim, g_game.empty() ? "The game folder isn't known yet." : "No mod files are installed.");
    else if (ImGui::BeginTable("files", 4, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("File", 0, S(120));
        ImGui::TableSetupColumn("Version", 0, S(70));
        ImGui::TableSetupColumn("SHA-256", 0, S(300));
        ImGui::TableSetupColumn("Checked against GitHub", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const FileCheck& f : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(f.name.c_str());
            ImGui::TableNextColumn();
            TextColored(col::Dim, f.version);
            ImGui::TableNextColumn();
            ImGui::PushFont(g_fSmall);
            TextColored(col::Dim, f.sha.substr(0, 32) + "\n" + f.sha.substr(32));
            ImGui::PopFont();
            ImGui::TableNextColumn();
            Wrapped(f.color, f.verdict);
        }
        ImGui::EndTable();
    }
    if (Button("Copy checksums", ImVec2(0, 0), false, !rows.empty())) {
        std::string t;
        for (const FileCheck& f : rows) t += f.sha + "  " + f.name + "\n";
        Copy(t);
    }
    ImGui::SameLine();
    if (Button("Open the releases page")) plat::OpenExternal(RepoUrl("/releases"));
    ImGui::SameLine();
    if (Button("Show the backups", ImVec2(0, 0), false, Exists(StateDir() / "backups"))) plat::OpenExternal(U8(StateDir() / "backups"));

    Heading("What it keeps track of");
    int adopted = 0;
    for (const RecordEntry& e : g_rec.files) adopted += e.adopted;
    Wrapped(col::Dim, std::to_string(g_rec.files.size()) + " file(s) in the install record" +
                          (adopted ? " (" + std::to_string(adopted) + " installed before manager 0.4, taken over as they were)" : "") +
                          ". Everything it did is in its log.");
    if (Button("Open the log", ImVec2(0, 0), false, Exists(LogPath()))) plat::OpenExternal(U8(LogPath()));
    ImGui::EndChild();
}

// --- Dialogs -----------------------------------------------------------------------------------------------------------

void CenterNext(float w) {
    ImVec2 d = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(d.x * 0.5f, d.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(w, d.x - S(40)), 0));
}

void ReviewDialog() {
    if (g_openReview) ImGui::OpenPopup("Review changes"), g_openReview = false;
    CenterNext(S(640));
    if (!ImGui::BeginPopupModal("Review changes", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) return;
    if (!g_review) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const Action& a = *g_review;
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted(a.plan.title.c_str());
    ImGui::PopFont();
    auto lines = a.plan.Lines();
    uint64_t download = 0;
    for (const FileChange& f : a.plan.files)
        if ((f.op == FileChange::Add || f.op == FileChange::Replace) && !Exists(f.source)) download += f.size;
    Small(col::Dim, "In " + U8(g_game) + ":");
    ImGui::BeginChild("lines", ImVec2(0, std::min(S(300), (lines.size() + 1) * ImGui::GetTextLineHeightWithSpacing() + S(8))),
                      ImGuiChildFlags_Borders);
    for (const std::string& l : lines) Wrapped(l.rfind("Setting", 0) == 0 ? col::Dim : col::Text, "\xE2\x80\xA2 " + l);
    if (lines.empty()) Wrapped(col::Dim, "Only settings change.");
    ImGui::EndChild();
    if (download) Small(col::Faint, "Downloads " + HumanSize(download) + " from GitHub, checked against GitHub's published checksums.");
    if (plat::GameRunning()) Small(col::Yellow, "Crab Champions is running: this waits until it closes.");
    ImGui::Dummy(ImVec2(0, S(4)));
    if (Button("Apply", ImVec2(S(120), 0), true)) {
        Run(std::move(*g_review));
        g_review.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (Button("Cancel", ImVec2(S(120), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        g_review.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void WhatsNewDialog() {
    if (g_whatsNew && !ImGui::IsPopupOpen("What's new")) ImGui::OpenPopup("What's new");
    CenterNext(S(680));
    if (!ImGui::BeginPopupModal("What's new", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) return;
    if (!g_whatsNew) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted(g_whatsNew->title.c_str());
    ImGui::PopFont();
    ImGui::BeginChild("notes", ImVec2(0, S(340)), ImGuiChildFlags_Borders);
    Wrapped(col::Text, g_whatsNew->notes.empty() ? "No notes for this release." : g_whatsNew->notes);
    ImGui::EndChild();
    bool close = false;
    if (!g_whatsNew->page.empty() && Button("Open on GitHub")) plat::OpenExternal(g_whatsNew->page);
    ImGui::SameLine();
    close = Button("Close", ImVec2(S(100), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (close) {
        g_whatsNew.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void CrashDialog() {
    if (g_openCrash) ImGui::OpenPopup("Crash report"), g_openCrash = false;
    CenterNext(S(640));
    if (!ImGui::BeginPopupModal("Crash report", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) return;
    if (!g_crashOffer) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    const BuiltReport& b = *g_crashOffer;
    const CrashReport& r = b.report;
    ImGui::PushFont(g_fTitle);
    ImGui::TextUnformatted(r.crashed ? "Crab Champions crashed" : "Logs collected");
    ImGui::PopFont();
    if (r.crashed) {
        TextColored(col::Dim, r.when);
        if (!r.error.empty())
            Wrapped(col::Red, r.error + (r.callStack.empty() ? "" : " at " + r.callStack.substr(0, r.callStack.find('\n'))));
    }
    ImGui::Dummy(ImVec2(0, S(2)));
    Wrapped(col::Text, std::string("The mod logs and settings") + (r.crashed ? " and the game's crash report" : "") +
                           " are saved, with player names, user folders and machine ids taken out, in:");
    Small(col::Dim, U8(b.zip));
    Small(col::Faint, "Report it on GitHub: opens a filled-in issue in your browser (you need a GitHub account) and the folder "
                      "with the zip, to drag into the issue. report.md in the folder has the same text, to post anywhere else.");
    ImGui::Dummy(ImVec2(0, S(4)));
    bool close = false;
    if (Button("Report on GitHub", ImVec2(0, 0), true)) {
        plat::OpenExternal(IssueUrl(r));
        plat::OpenExternal(U8(b.folder));
        close = true;
    }
    ImGui::SameLine();
    if (Button("Open the folder")) plat::OpenExternal(U8(b.folder)), close = true;
    ImGui::SameLine();
    if (Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) close = true;
    if (close) {
        g_crashOffer.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// Where there is no folder picker (a Linux without zenity or kdialog): the path, typed or pasted.
void PathDialog() {
    if (g_openPath) ImGui::OpenPopup("Game folder"), g_openPath = false;
    CenterNext(S(640));
    if (!ImGui::BeginPopupModal("Game folder", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) return;
    Wrapped(col::Text, "Where is Crab Champions? The game folder, its CrabChampions/Binaries/Win64 folder, or the Steam library "
                       "it is in.");
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool enter = ImGui::InputText("##path", g_pathInput, sizeof g_pathInput, ImGuiInputTextFlags_EnterReturnsTrue);
    if (Button("Use this folder", ImVec2(0, 0), true) || enter) {
        UseGameDir(PathU8(Trim(g_pathInput)));
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// --- The frame ---------------------------------------------------------------------------------------------------------

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
        if (ImGui::InvisibleButton(label, ImVec2(tw, h)) || Pressed(label, true)) g_page = page;
        bool hover = ImGui::IsItemHovered();
        dl->AddText(g_fBold, g_fBold->FontSize, ImVec2(x + S(14), (h - g_fBold->FontSize) * 0.5f),
                    on ? col::Text : hover ? IM_COL32(200, 203, 208, 255) : col::Dim, label);
        if (on) dl->AddRectFilled(ImVec2(x + S(8), h - S(3)), ImVec2(x + tw - S(8), h), col::Accent);
        x += tw;
    };
    if (g_cfg.setupDone) {
        tab("Mods", Page::Mods);
        tab("Share", Page::Share);
        tab("Settings", Page::Settings);
        tab("Help", Page::Help);
    }
    float pw = S(120), ph = h - S(16);
    ImGui::SetCursorPos(ImVec2(w - pw - S(12), S(8)));
    ImGui::PushFont(g_fBold);
    if (Button(g_launchAfterJobs ? "Starting..." : "Play", ImVec2(pw, ph), true, !g_game.empty() && !g_launchAfterJobs)) Play();
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
    } else if (!g_waiting.empty()) {
        left = "Changes will be made when the game closes", c = col::Yellow;
    } else if (g_cfg.offline) {
        left = "Offline: no update checks";
    } else if (!g_releaseError.empty()) {
        left = "Couldn't check for updates: " + g_releaseError, c = col::Yellow;
    } else if (!g_releasesLoaded) {
        left = "Checking for updates...";
    } else {
        left = g_lastCheck ? "Checked for updates " + Ago(g_lastCheck) : "Not checked for updates yet";
    }
    dl->AddText(g_fSmall, g_fSmall->FontSize, ImVec2(x, ty), c, left.c_str());
    std::string right = std::string("Manager ") + kVersion;
    float rw = g_fSmall->CalcTextSizeA(g_fSmall->FontSize, FLT_MAX, 0, right.c_str()).x;
    dl->AddText(g_fSmall, g_fSmall->FontSize, ImVec2(w - rw - S(12), ty), col::Faint, right.c_str());
    ImGui::PopFont();
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    // Latin, plus the dashes, bullet and arrows the pages use.
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x2190, 0x2193, 0};
    auto font = [&](float px) {
        return io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kFontData), int(sizeof kFontData), S(px), &cfg, ranges);
    };
    g_fBody = font(16);
    g_fSmall = font(13.5f);
    g_fBold = font(16);
    g_fTitle = font(20);
    io.FontDefault = g_fBody;
}

// Screenshots of states that are hard to reach by hand (--demo), drawn from what is on this machine.
void Demo(const std::string& d) {
    if (d == "wizard") {
        g_cfg.setupDone = false;
    } else if (d == "review") {
        if (ModsInstalled()) UninstallMod(ModsInfo());
        else InstallMod(ModsInfo());
        if (!g_review && Latest(SyncInfo())) {
            Action a;
            std::string e;
            a.plan.title = "Update CrabSync";
            PlanMod(a.plan, SyncInfo(), *Latest(SyncInfo()), e);
            Review(a);
        }
    } else if (d == "updated") {
        if (const Release* r = Latest(ModsInfo()))
            g_notes.push_back({"CrabMods updated 0.13.4 \xE2\x86\x92 " + r->version + ".", "CrabMods " + r->version, r->notes, r->page});
        g_notes.push_back({std::string("CrabMods Manager updated to ") + kVersion + "."});
    } else if (d == "crash") {
        BuiltReport b;
        b.report.crashed = true;
        b.report.when = LocalTime(FileNow(), false);
        b.report.error = "EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000010";
        b.report.callStack = "CrabChampions_Win64_Shipping!UObject::ProcessEvent()";
        b.folder = ReportsDir() / "CrabCrash-demo";
        b.zip = b.folder / "CrabCrash-demo.zip";
        g_crashOffer = b;
        g_openCrash = true;
    }
}

}  // namespace

// =================================================================================================================

void Init(const Options& o, float scale) {
    g_opt = o;
    g_readOnly = !o.screenshot.empty();
    g_s = scale;
    Style();
    LoadFonts();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    std::error_code ec;
    if (!g_readOnly && fs::file_size(LogPath(), ec) > 1'000'000) {
        fs::path old = LogPath();
        old += ".old";
        fs::rename(LogPath(), old, ec);
    }
    if (auto t = ReadFile(ConfigPath())) g_cfg = ParseConfig(*t);
    // A manager update from elsewhere (or this one, swapped in at close) says so once.
    if (!g_cfg.lastRun.empty() && g_cfg.lastRun != kVersion)
        g_notes.push_back({std::string("CrabMods Manager updated to ") + kVersion + ".", std::string("CrabMods Manager ") + kVersion, "",
                           RepoUrl("/releases/tag/") + kManagerTag + kVersion});
    else if (o.updated && g_cfg.lastRun.empty())
        g_notes.push_back({std::string("CrabMods Manager updated to ") + kVersion + "."});
    bool firstOfVersion = g_cfg.lastRun != kVersion;
    g_cfg.lastRun = kVersion;
    Log(std::string("CrabMods Manager ") + kVersion + " started (" + plat::PlatformName() + ")");

    // A staged update older than this one (or this one) is done with.
    fs::path staged = plat::StagedUpdate(), stagedVersion = staged;
    stagedVersion += ".version";
    if (auto v = ReadFile(stagedVersion); v && Exists(staged) && CompareVersions(Trim(*v), kVersion) > 0) g_staged = Trim(*v);
    else if (!g_readOnly && (Exists(staged) || Exists(stagedVersion))) fs::remove(staged, ec), fs::remove(stagedVersion, ec);

    fs::path dir = NormalizeGameDir(PathU8(g_cfg.game));
    if (dir.empty()) dir = plat::FindGame();
    LoadCachedReleases();
    SetGameDir(dir);
    if (firstOfVersion) SaveConfig();
    if (o.page == "share") g_page = Page::Share;
    if (o.page == "settings") g_page = Page::Settings;
    if (o.page == "help") g_page = Page::Help;
    if (!o.select.empty()) g_select = o.select;
    if (!o.demo.empty()) Demo(o.demo);

    std::thread(Worker).detach();
    if (!g_readOnly) {
        if (g_releasesLoaded) AfterReleases();
        if (!g_cfg.offline || !g_cfg.setupDone) CheckForUpdates();
        else g_poller.next = INT64_MAX;
    }
}

void Tick() {
    for (;;) {
        JobResult r;
        {
            std::lock_guard lock(g_jobMutex);
            if (g_results.empty()) break;
            r = std::move(g_results.front());
            g_results.pop_front();
        }
        if (!r.status.empty()) SetStatus(r.status, r.error);
        Rescan();
        if (r.then) r.then();
    }
    if (g_pending == 0 && g_launchAfterJobs) {
        g_launchAfterJobs = false;
        SetStatus(plat::LaunchGame(g_game));
    }
    if (g_readOnly) return;
    static double lastSecond = -100;
    double now = ImGui::GetTime();
    if (now - lastSecond < 3) return;
    lastSecond = now;
    CheckForCrash();
    if (!g_cfg.offline && g_poller.Due(UnixNow(), g_cfg.checkMinutes)) CheckForUpdates();
    if (!g_waiting.empty() && !plat::GameRunning()) {
        std::deque<Action> go;
        go.swap(g_waiting);
        Log("the game closed: carrying out what waited for it");
        for (Action& a : go) Run(std::move(a));
    }
}

void Frame() {
    ImGuiIO& io = ImGui::GetIO();
    g_pressing.swap(g_press);
    g_press.clear();
    g_drawing.clear();
    g_noticing.clear();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::Begin("main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    float top = S(48), bottom = S(26);
    TopBar(io.DisplaySize.x, top);
    ImGui::SetCursorPos(ImVec2(0, top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(14), S(12)));
    bool mods = g_cfg.setupDone && g_page == Page::Mods;
    ImGui::BeginChild("page", ImVec2(io.DisplaySize.x, io.DisplaySize.y - top - bottom), ImGuiChildFlags_AlwaysUseWindowPadding,
                      mods ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0);
    ImGui::PopStyleVar();
    if (!g_cfg.setupDone) WizardPage();
    else switch (g_page) {
        case Page::Mods: ModsPage(); break;
        case Page::Share: SharePage(); break;
        case Page::Settings: SettingsPage(); break;
        case Page::Help: HelpPage(); break;
        }
    ImGui::EndChild();
    StatusBar(io.DisplaySize.x, io.DisplaySize.y - bottom, bottom);
    ReviewDialog();
    WhatsNewDialog();
    CrashDialog();
    PathDialog();
    ImGui::End();
    g_pressing.clear();
    g_drawn = g_drawing;
    g_noticeTexts = g_noticing;
}

void Press(const std::string& label) { g_press.insert(label); }
bool Shown(const std::string& label) {
    auto it = g_drawn.find(label);
    return it != g_drawn.end() && it->second;
}
std::vector<std::string> NoticeTexts() { return g_noticeTexts; }
std::string StatusLine() { return g_status; }
bool Busy() { return g_pending > 0; }

bool Animating() { return g_pending > 0 || ImGui::GetTime() - g_statusTime < 13; }
bool WantsQuit() { return g_quit; }

void Shutdown() {
    if (g_readOnly || g_staged.empty() || g_cfg.autoManager != true) return;
    std::string error;
    if (plat::SwapInUpdate(error)) Log("manager " + g_staged + " goes in now that the manager closes");
    else Log("could not swap in manager " + g_staged + ": " + error);
}

// --- --selftest ----------------------------------------------------------------------------------------------------

int SelfTest(bool report) {
    std::string r = std::string("CrabMods Manager ") + kVersion + " (" + plat::PlatformName() + ")\n";
    if (auto t = ReadFile(ConfigPath())) g_cfg = ParseConfig(*t);
    r += "data: " + U8(plat::DataDir()) + "\ncache: " + U8(CacheRoot()) + "\n";
    fs::path dir = NormalizeGameDir(PathU8(g_cfg.game));
    if (dir.empty()) dir = plat::FindGame();
    g_game = dir;
    g_rec = LoadRecord(StateDir());
    r += "game: " + (g_game.empty() ? std::string("not found") : U8(g_game)) + "\n";
    HttpResponse resp = plat::Http(kReleasesUrl);
    bool ok = resp.status == 200;
    r += "releases: " + (ok ? std::to_string(resp.body.size()) + " bytes" : "failed, " + std::to_string(resp.status) + " " + resp.error) +
         (resp.rateRemaining >= 0 ? ", " + std::to_string(resp.rateRemaining) + " requests left this hour" : "") + "\n";
    if (ok) {
        WriteFile(CacheRoot() / "releases.json", resp.body);
        WriteFile(CacheRoot() / "releases.etag", resp.etag);
        g_releases = ParseReleases(resp.body);
        g_releasesLoaded = g_releases.ok;
        // An unchanged list answers 304.
        if (!resp.etag.empty()) r += "etag check: " + std::to_string(plat::Http(kReleasesUrl, resp.etag).status) + "\n";
    }
    for (const ModInfo& m : Mods())
        if (const Release* rel = Latest(m)) {
            r += std::string(m.name) + ": latest " + rel->version;
            for (const char* f : {m.dll, kLoaderDll})
                r += std::string(", ") + f + (rel->Get(f) && !rel->Get(f)->sha256.empty() ? " checksum ok" : " NO CHECKSUM");
            r += ", " + CatalogAsset(m) + (rel->Has(CatalogAsset(m)) ? " published" : " MISSING") + "\n";
        }
    if (const Release* m = LatestManager()) r += "Manager: latest " + m->version + " (" + plat::ManagerAsset() + ")\n";
    Rescan();
    // The catalogs, downloaded and checked like any file, then read (never run).
    for (const ModInfo& m : Mods())
        if (const Release* rel = Latest(m); rel && rel->Has(CatalogAsset(m))) {
            std::string error;
            if (!DownloadChecked(Get, *rel->Get(CatalogAsset(m)), CatalogFile(m, rel->version), error)) r += "catalog: " + error + "\n";
        }
    g_catalogKey.clear(), g_syncKey.clear();
    LoadCatalogs();
    r += "installed: CrabMods " + (g_mods.version.empty() ? "no" : g_mods.version) + ", CrabSync " +
         (g_sync.version.empty() ? "no" : g_sync.version) + "\n";
    r += "install record: " + std::to_string(g_rec.files.size()) + " file(s)" + (g_rec.exists ? "" : " (none yet)") + "\n";
    if (g_catalog) {
        r += "catalog: " + std::to_string(g_catalog->items.size()) + " items (" + g_catalogVersion + ")\n";
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
    if (g_syncItem) r += "crabsync settings: " + std::to_string(g_syncItem->settings.size()) + "\n";
    r += "setup code: " + CurrentCode() + "\n";
    for (const SavedDir& d : plat::SavedDirs(g_game)) r += "saved: " + U8(d.path) + (d.proton ? " (proton)" : "") + "\n";
    auto crash = NewestCrash(plat::SavedDirs(g_game));
    r += "newest crash: " + (crash ? crash->name + " " + LocalTime(crash->time, false) : std::string("none")) + "\n";
    auto lo = LaunchOptionSetIn(plat::SteamRoots());
    r += std::string("launch option: ") + (lo ? (*lo ? "set" : "not set") : "unknown") + "\n";
    r += std::string("game running: ") + (plat::GameRunning() ? "yes" : "no") + "\n";
    if (report) {
        BuiltReport b = MakeReport(crash);
        r += "report: " + (b.error.empty() ? U8(b.zip) : b.error) + "\nissue: " + IssueUrl(b.report) + "\n";
    }
    WriteFile(plat::DataDir() / "CrabModsManager-selftest.txt", r);
    std::fputs(r.c_str(), stdout);
    return ok && g_catalog ? 0 : 1;
}

}  // namespace app
