// The Linux side of the manager (manager/platform_linux.cpp) on a real system, in a scratch home folder: where it
// keeps its files, finding the game through Steam, seeing the game run, the folder picker without a program for
// it, and the AppImage replacing itself.
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "../manager/platform.hpp"

using namespace mgr;

static int g_failures = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

int main() {
    char tmpl[] = "/tmp/crabmgr-plat-XXXXXX";
    fs::path root = mkdtemp(tmpl);
    fs::path home = root / "home";
    fs::create_directories(home);
    setenv("HOME", home.c_str(), 1);
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_CACHE_HOME");
    unsetenv("APPIMAGE");

    // The XDG folders, and $XDG_*_HOME when set.
    CHECK(plat::DataDir() == home / ".config" / "CrabModsManager");
    CHECK(plat::CacheDir() == home / ".cache" / "CrabModsManager");
    setenv("XDG_CONFIG_HOME", (root / "cfg").c_str(), 1);
    setenv("XDG_CACHE_HOME", "relative/is/ignored", 1);
    CHECK(plat::DataDir() == root / "cfg" / "CrabModsManager");
    CHECK(plat::CacheDir() == home / ".cache" / "CrabModsManager");
    CHECK(plat::NativeLinux() && !plat::RunningOnWine() && plat::ManagerAsset() == std::string(kManagerAppImage));

    // The game, in a second Steam library listed in the Flatpak Steam's libraryfolders.vdf.
    CHECK(plat::FindGame().empty());
    fs::path steam = home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam";
    fs::path lib = root / "games";
    fs::create_directories(steam / "steamapps");
    WriteFile(steam / "steamapps" / "libraryfolders.vdf",
              "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" + U8(steam) + "\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + U8(lib) +
                  "\"\n\t}\n}\n");
    fs::create_directories(lib / GameSub());
    WriteFile(lib / GameSub() / kGameExe, "MZ");
    CHECK(plat::FindGame() == lib / GameSub());
    // Its crash reports are in the game's Proton prefix.
    fs::path saved = lib / "steamapps" / "compatdata" / kAppId / "pfx" / "drive_c" / "users" / "steamuser" / "AppData" / "Local" /
                     "CrabChampions" / "Saved";
    CHECK(plat::SavedDirs(lib / GameSub()).empty());
    fs::create_directories(saved);
    auto dirs = plat::SavedDirs(lib / GameSub());
    CHECK(dirs.size() == 1 && dirs[0].path == saved && dirs[0].proton);

    // The game running: a process whose command line names its exe, as Proton's does.
    CHECK(!plat::GameRunning());
    pid_t game = fork();
    if (game == 0) {
        execlp("sleep", (std::string("Z:\\games\\") + kGameExe).c_str(), "30", nullptr);
        _exit(127);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(plat::GameRunning());
    kill(game, SIGKILL);
    waitpid(game, nullptr, 0);
    CHECK(!plat::GameRunning());

    // No zenity or kdialog: the app asks for the path itself.
    std::string path = std::getenv("PATH") ? std::getenv("PATH") : "";
    setenv("PATH", (root / "empty").c_str(), 1);
    CHECK(!plat::PickFolder("Game folder").has_value());
    setenv("PATH", path.c_str(), 1);

    // Names to take out of crash reports.
    setenv("USER", "crabtester", 1);
    auto secrets = plat::Secrets();
    CHECK(std::find(secrets.begin(), secrets.end(), "crabtester") != secrets.end());

    // The AppImage's own update: the staged file sits next to it and takes its name, runnable.
    std::string error;
    CHECK(!plat::SwapInUpdate(error) && error.find("AppImage") != std::string::npos);  // not run as one
    fs::path app = root / "apps" / kManagerAppImage;
    fs::create_directories(app.parent_path());
    WriteFile(app, "old manager");
    setenv("APPIMAGE", app.c_str(), 1);
    CHECK(plat::ExePath() == app);
    fs::path staged = plat::StagedUpdate();
    CHECK(staged == root / "apps" / (std::string(kManagerAppImage) + ".new"));
    error.clear();
    CHECK(!plat::SwapInUpdate(error) && !error.empty());  // nothing staged
    WriteFile(staged, "new manager");
    fs::path version = staged;
    version += ".version";
    WriteFile(version, "0.5.0");
    CHECK(plat::SwapInUpdate(error));
    CHECK(ReadFile(app).value_or("") == "new manager");
    CHECK(!Exists(staged) && !Exists(version));
    CHECK(access(app.c_str(), X_OK) == 0);

    // Relaunch starts the (new) program with the arguments and doesn't wait for it.
    fs::path script = root / "apps" / "relaunch.sh";
    WriteFile(script, "#!/bin/sh\necho \"$@\" > \"" + U8(root / "relaunched.txt") + "\"\n");
    fs::permissions(script, fs::perms::owner_all);
    setenv("APPIMAGE", script.c_str(), 1);
    CHECK(plat::Relaunch({"--updated"}));
    for (int i = 0; i < 50 && !Exists(root / "relaunched.txt"); i++) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(ReadFile(root / "relaunched.txt").value_or("") == "--updated\n");

    // No connection: a status of 0 and a reason, not a crash or a hang.
    HttpResponse r = plat::Http("https://127.0.0.1:9/nothing-here");
    CHECK(r.status == 0 && !r.error.empty());

    std::error_code ec;
    fs::remove_all(root, ec);
    if (g_failures) {
        std::printf("linux platform tests: %d failed\n", g_failures);
        return 1;
    }
    std::printf("linux platform tests: all passed\n");
    return 0;
}
