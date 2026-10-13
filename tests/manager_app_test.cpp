// The manager from end to end, on Windows and Linux: the real pages (manager/app.cpp) driven without a screen,
// against a stand-in for GitHub and the system (the platform.hpp below) and a scratch game folder.
//   manager_app_test           a new player: first start, install, the game running, rollback, a tampered
//                              download, an automatic update, the ETag check, Remove all, a manager update
//   manager_app_test migrate   a player coming from manager 0.3 with mods installed
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <thread>

#include "../manager/app.hpp"
#include "../manager/platform.hpp"
#include "imgui.h"
#include "test_util.hpp"

using namespace mgr;
using namespace mgr::test;

static int g_failures = 0;
#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

// --- The stand-in system and GitHub ---------------------------------------------------------------------------

namespace fake {
fs::path root, data, game;
std::mutex m;
std::map<std::string, std::string> web;  // url -> body
std::string releasesEtag = "\"v1\"";
std::vector<std::string> requests;       // urls asked for, "url (etag)" with If-None-Match
std::atomic<bool> gameRunning{false};
std::vector<std::string> opened;
bool swapped = false;
std::vector<std::string> relaunched;
constexpr const char kReleases[] = "https://api.github.com/repos/Dudiebug/CrabMods/releases?per_page=100";

struct Rel {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> files;  // name, body
};

// The release list as GitHub sends it, with each asset's digest; the assets are served too.
void Publish(const std::vector<Rel>& rels, const std::string& etag) {
    std::lock_guard lock(m);
    std::string j = "[";
    for (const Rel& r : rels) {
        if (j.size() > 1) j += ",";
        j += "{\"tag_name\":\"" + r.tag + "\",\"html_url\":\"https://github.com/x/releases/tag/" + r.tag +
             "\",\"body\":\"Notes for " + r.tag + "\",\"assets\":[";
        bool first = true;
        for (const auto& [name, body] : r.files) {
            std::string url = "https://dl/" + r.tag + "/" + name;
            web[url] = body;
            j += std::string(first ? "" : ",") + "{\"name\":\"" + name + "\",\"browser_download_url\":\"" + url +
                 "\",\"size\":" + std::to_string(body.size()) + ",\"digest\":\"sha256:" + Sha256Hex(body) + "\"}";
            first = false;
        }
        j += "]}";
    }
    web[kReleases] = j + "]";
    releasesEtag = etag;
}

void Serve(const std::string& url, const std::string& body) {
    std::lock_guard lock(m);
    web[url] = body;
}

bool Asked(const std::string& what) {
    std::lock_guard lock(m);
    for (const std::string& r : requests)
        if (r.find(what) != std::string::npos) return true;
    return false;
}
}  // namespace fake

namespace mgr::plat {
std::string PlatformName() { return "Test"; }
bool RunningOnWine() { return false; }
bool NativeLinux() { return false; }
fs::path ExePath() { return fake::data / kManagerExe; }
fs::path DataDir() { return fake::data; }
fs::path CacheDir() { return fake::data / "cache"; }
std::vector<fs::path> SteamRoots() { return {}; }
fs::path LibraryPath(const std::string& p) { return PathU8(p); }
fs::path FindGame() { return {}; }
std::vector<SavedDir> SavedDirs(const fs::path&) { return {}; }
std::vector<std::string> Secrets() { return {}; }
bool GameRunning() { return fake::gameRunning; }

HttpResponse Http(const std::string& url, const std::string& etag, const std::string&) {
    std::lock_guard lock(fake::m);
    fake::requests.push_back(url + (etag.empty() ? "" : " (" + etag + ")"));
    HttpResponse r;
    auto it = fake::web.find(url);
    if (it == fake::web.end()) {
        r.status = 404;
        return r;
    }
    if (url == fake::kReleases) {
        r.etag = fake::releasesEtag;
        r.rateRemaining = 59;
        if (!etag.empty() && etag == fake::releasesEtag) {
            r.status = 304;
            return r;
        }
    }
    r.status = 200;
    r.body = it->second;
    return r;
}

void OpenExternal(const std::string& t) { fake::opened.push_back(t); }
std::string LaunchGame(const fs::path&) { return "Starting."; }
std::optional<fs::path> PickFolder(const std::string&) { return std::nullopt; }
void Wake() {}
void Attention() {}
const char* ManagerAsset() { return kManagerExe; }
fs::path StagedUpdate() { return fake::data / "update" / kManagerExe; }
bool SwapInUpdate(std::string&) {
    fake::swapped = true;
    return true;
}
bool Relaunch(const std::vector<std::string>& args) {
    fake::relaunched = args;
    return true;
}
}  // namespace mgr::plat

// --- Driving the window --------------------------------------------------------------------------------------

static void Frame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1240, 800);
    io.DeltaTime = 0.25f;  // a quarter second a frame: the app's 3-second timers come round quickly
    app::Tick();
    ImGui::NewFrame();
    app::Frame();
    ImGui::Render();
}

// Frames until the background jobs are done (and a few more, for what they do afterwards).
static void Settle() {
    for (int i = 0, idle = 0; i < 4000 && idle < 3; i++) {
        Frame();
        idle = app::Busy() ? 0 : idle + 1;
        if (app::Busy()) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

static void Press(const std::string& label) {
    app::Press(label);
    Frame();
    Settle();
}

static bool Notice(const std::string& part) {
    for (const std::string& n : app::NoticeTexts())
        if (n.find(part) != std::string::npos) return true;
    return false;
}

static std::string Read(const fs::path& p) { return ReadFile(p).value_or(""); }

static void Start(const std::string& ini, bool updated = false) {
    if (!ini.empty()) WriteFile(fake::data / "CrabModsManager.ini", ini);
    ImGui::CreateContext();
    app::Options o;
    o.updated = updated;
    app::Init(o, 1);
    ImGui::GetIO().Fonts->Build();
    Settle();
}

static void Scratch(const char* name) {
    fake::root = fs::temp_directory_path() / (std::string("crabmgr-app-test-") + name);
    std::error_code ec;
    fs::remove_all(fake::root, ec);
    fake::data = fake::root / "manager";
    fake::game = fake::root / GameSub();
    fs::create_directories(fake::data);
    fs::create_directories(fake::game);
    WriteFile(fake::game / kGameExe, "MZ the game");
}

// --- The releases -------------------------------------------------------------------------------------------

const std::string kLoader = FakePe(1, 0, 0, true), kMods0141 = FakePe(0, 14, 1, true), kMods0140 = FakePe(0, 14, 0, true),
                  kMods0142 = FakePe(0, 14, 2, true), kSync108 = FakePe(1, 0, 8, true), kSync109 = FakePe(1, 0, 9, true);
const std::string kCatalog =
    R"({"header":"# CrabMods\n","items":[)"
    R"({"section":"alpha","name":"Alpha","kind":"Perk","rarity":"epic","ini":"# Alpha, a perk: more damage.\nenabled = true\n\n# Damage bonus, in percent.\ndamagePercent = 10\n"},)"
    R"({"section":"beta","name":"Beta","kind":"Relic","rarity":"common","ini":"# Beta, a relic: a shield.\nenabled = true\n"}]})";
const std::string kSyncCatalog = R"({"ini":"# CrabSync: a shared team.\n\n# Share health.\nsyncHealth = true\n"})";

std::vector<fake::Rel> Releases(bool newer) {
    std::vector<fake::Rel> r = {
        {"crabmods-v0.14.1", {{"CrabMods.dll", kMods0141}, {"xinput1_3.dll", kLoader}, {"CrabMods.catalog.json", kCatalog}}},
        {"crabmods-v0.14.0", {{"CrabMods.dll", kMods0140}, {"xinput1_3.dll", kLoader}, {"CrabMods.catalog.json", kCatalog}}},
        {"crabsync-v1.0.8", {{"CrabSync.dll", kSync108}, {"xinput1_3.dll", kLoader}, {"CrabSync.catalog.json", kSyncCatalog}}},
        {"manager-v0.5.0", {{"CrabModsManager.exe", "manager 0.5.0"}}},
    };
    if (newer) {
        r.push_back({"crabsync-v1.0.9", {{"CrabSync.dll", kSync109}, {"xinput1_3.dll", kLoader}, {"CrabSync.catalog.json", kSyncCatalog}}});
        r.push_back({"manager-v0.6.0", {{"CrabModsManager.exe", "manager 0.6.0"}}});
    }
    return r;
}

// --- A new player --------------------------------------------------------------------------------------------

static void NewPlayer() {
    std::printf("new player\n");
    Scratch("new");
    WriteFile(fake::game / "xinput1_3.dll", "someone else's xinput");  // a controller fix, say
    auto before = Tree(fake::root / "steamapps");
    fake::Publish(Releases(false), "\"v1\"");
    Start("game=" + U8(fake::game) + "\nsetupDone=false\n");

    // The first-run screen: nothing changes, and nothing is saved, before Install.
    CHECK(app::Shown("Just browse"));
    CHECK(!app::Shown("Install"));  // the two update questions come first
    CHECK(Tree(fake::root / "steamapps") == before);
    CHECK(Read(fake::data / "CrabModsManager.ini") == "game=" + U8(fake::game) + "\nsetupDone=false\n");  // as it was
    CHECK(!Exists(fake::data / "state"));
    CHECK(fake::Asked("crabmods-v0.14.1/CrabMods.catalog.json"));  // the list of what's in it, only read
    CHECK(!fake::Asked("CrabMods.dll"));
    app::Press("No, ask me first");  // both questions
    Frame();
    CHECK(app::Shown("Install"));
    Press("Install");
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0141);
    CHECK(Read(fake::game / "CrabSync.dll") == kSync108);
    CHECK(Read(fake::game / "xinput1_3.dll") == kLoader);
    std::string ini = Read(fake::data / "CrabModsManager.ini");
    CHECK(ini.find("setupDone=true") != std::string::npos);
    CHECK(ini.find("autoUpdateMods=false") != std::string::npos && ini.find("autoUpdateManager=false") != std::string::npos);
    InstallRecord rec = LoadRecord(fake::data / "state");
    CHECK(rec.files.size() == 3);
    const RecordEntry* loader = rec.Find("xinput1_3.dll");
    CHECK(loader && !loader->backup.empty() && Read(fake::data / "state" / PathU8(loader->backup)) == "someone else's xinput");
    CHECK(Read(fake::data / "CrabModsManager.log").find("checked CrabMods.dll 0.14.1") != std::string::npos);
    CHECK(!app::Shown("Just browse"));
    CHECK(!Notice("How should updates work"));

    // Uninstalling CrabMods while the game runs waits for it to close.
    fake::gameRunning = true;
    Press("Uninstall");  // CrabMods, in the details pane
    CHECK(app::Shown("Apply"));
    Press("Apply");
    CHECK(Exists(fake::game / "CrabMods.dll"));
    CHECK(Notice("Waiting for Crab Champions to close: Uninstall CrabMods"));
    fake::gameRunning = false;
    for (int i = 0; i < 16; i++) Frame();
    Settle();
    CHECK(!Exists(fake::game / "CrabMods.dll"));
    CHECK(Read(fake::game / "xinput1_3.dll") == kLoader);  // CrabSync still needs it
    CHECK(!Notice("Waiting"));

    // Back again, then rolled back to 0.14.0: 0.14.1 is skipped, so it isn't offered on its own.
    Press("Install");
    Press("Apply");
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0141);
    CHECK(app::Shown("Roll back to 0.14.0"));
    Press("Roll back to 0.14.0");
    Press("Apply");
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0140);
    CHECK(Read(fake::data / "CrabModsManager.ini").find("skip.CrabMods=0.14.1") != std::string::npos);
    CHECK(!Notice("CrabMods 0.14.1 is available"));

    // A download that doesn't match GitHub's checksum is refused, and the game folder stays as it was.
    fs::remove(CachedFile(fake::data / "cache", "CrabMods", "0.14.1", "CrabMods.dll"));
    fake::Serve("https://dl/crabmods-v0.14.1/CrabMods.dll", kMods0141 + "tampered");
    CHECK(app::Shown("Update to 0.14.1"));
    Press("Update to 0.14.1");
    Press("Apply");
    CHECK(app::StatusLine().find("didn't match its published checksum") != std::string::npos);
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0140);
    CHECK(!Exists(CachedFile(fake::data / "cache", "CrabMods", "0.14.1", "CrabMods.dll")));
    fake::Serve("https://dl/crabmods-v0.14.1/CrabMods.dll", kMods0141);
    Press("Update to 0.14.1");
    Press("Apply");
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0141);
    CHECK(Notice("CrabMods updated 0.14.0"));

    // Automatic mod updates, once switched on: a new CrabSync goes in without asking, and says so. A new manager
    // isn't installed on its own (that is a separate choice): it is offered.
    Press("Settings");
    Press("Install mod updates automatically");
    CHECK(Read(fake::data / "CrabModsManager.ini").find("autoUpdateMods=true") != std::string::npos);
    fake::Publish(Releases(true), "\"v2\"");
    Press("Check now");
    CHECK(fake::Asked(std::string(fake::kReleases) + " (\"v1\")"));
    CHECK(Read(fake::game / "CrabSync.dll") == kSync109);
    Press("Mods");
    CHECK(Notice("CrabSync updated 1.0.8"));
    CHECK(Notice("CrabMods Manager 0.6.0 is available"));

    // An unchanged list comes back as 304 (it doesn't count against GitHub's limit).
    Press("Settings");
    Press("Check now");
    CHECK(fake::Asked(std::string(fake::kReleases) + " (\"v2\")"));
    CHECK(Read(fake::data / "CrabModsManager.log").find("releases: 304") != std::string::npos);

    // Remove all mods: the game folder is exactly as it was before the manager.
    Press("Remove all mods...");
    CHECK(app::Shown("Apply"));
    Press("Apply");
    CHECK(Tree(fake::root / "steamapps") == before);
    CHECK(LoadRecord(fake::data / "state").files.empty());

    // The manager's own update: downloaded, checked, then swapped in and restarted when asked.
    Press("Mods");
    Press("Update");
    CHECK(Read(fake::data / "update" / kManagerExe) == "manager 0.6.0");
    CHECK(fake::swapped && fake::relaunched == std::vector<std::string>{"--updated"});
    CHECK(app::WantsQuit());
}

// --- From manager 0.3 ------------------------------------------------------------------------------------------

static void Migrate() {
    std::printf("from manager 0.3\n");
    Scratch("migrate");
    WriteFile(fake::game / "CrabMods.dll", kMods0141);
    WriteFile(fake::game / "CrabSync.dll", kSync108);
    WriteFile(fake::game / "xinput1_3.dll", kLoader);
    WriteFile(fake::game / "CrabMods.ini", "[alpha]\nenabled = false\n");
    auto before = Tree(fake::root / "steamapps");
    auto releases = Releases(false);
    releases.push_back({"crabmods-v0.14.2", {{"CrabMods.dll", kMods0142}, {"xinput1_3.dll", kLoader}, {"CrabMods.catalog.json", kCatalog}}});
    fake::Publish(releases, "\"v1\"");
    // What manager 0.3 wrote (it updated everything on its own), and it restarts into the new one with --updated.
    Start("lastCrash=\ngame=" + U8(fake::game) + "\nautoUpdateMods=true\nautoUpdateManager=true\n", true);

    CHECK(!app::Shown("Just browse"));  // no first-run screen
    CHECK(Notice("How should updates work"));
    CHECK(Notice("CrabMods Manager updated to 0.5.0"));
    CHECK(Notice("CrabMods 0.14.2 is available (you have 0.14.1)"));  // offered, not installed: not asked yet
    CHECK(Tree(fake::root / "steamapps") == before);
    InstallRecord rec = LoadRecord(fake::data / "state");
    CHECK(rec.files.size() == 3);
    for (const RecordEntry& e : rec.files) CHECK(e.adopted && e.backup.empty());
    CHECK(Read(fake::data / "CrabModsManager.log").find("adopted CrabMods.dll 0.14.1") != std::string::npos);

    // Answering the question: automatic mod updates, and the waiting update goes in.
    app::Press("Yes, automatically");  // both questions
    Frame();
    Settle();
    CHECK(!Notice("How should updates work"));
    CHECK(Read(fake::game / "CrabMods.dll") == kMods0142);
    CHECK(Notice("CrabMods updated 0.14.1"));
    CHECK(Read(fake::game / "CrabMods.ini") == "[alpha]\nenabled = false\n");  // the player's settings stay
    std::string ini = Read(fake::data / "CrabModsManager.ini");
    CHECK(ini.find("askedUpdates=true") != std::string::npos && ini.find("autoUpdateMods=true") != std::string::npos);
}

int main(int argc, char** argv) {
    bool migrate = argc > 1 && std::string(argv[1]) == "migrate";
    if (migrate) Migrate();
    else NewPlayer();
    // Closing stops the worker thread (on Linux, exiting with it still waiting used to hang).
    app::Shutdown();
    ImGui::DestroyContext();
    if (g_failures) {
        std::printf("manager app tests: %d failed\n", g_failures);
        return 1;
    }
    std::printf("manager app tests: all passed\n");
    std::error_code ec;
    fs::remove_all(fake::root, ec);
    return 0;
}
