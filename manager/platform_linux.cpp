// CrabMods Manager, the Linux side of platform.hpp: XDG folders, libcurl, /proc, xdg-open, zenity or kdialog, and
// the AppImage's own updates. linux_main.cpp is the window.
#include <SDL.h>
#include <curl/curl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "platform.hpp"

namespace mgr::plat {

SDL_Window* g_window = nullptr;  // set by linux_main.cpp
uint32_t g_wakeEvent = 0;        // SDL_RegisterEvents, by linux_main.cpp

namespace {

std::string Env(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

fs::path Home() {
    if (std::string h = Env("HOME"); !h.empty()) return PathU8(h);
    if (passwd* pw = getpwuid(getuid())) return pw->pw_dir;
    return "/tmp";
}

// $XDG_<name>_HOME, or its default under the home folder.
fs::path Xdg(const char* var, const char* fallback) {
    std::string v = Env(var);
    return v.size() && v[0] == '/' ? PathU8(v) : Home() / fallback;
}

// Runs a program without a shell and returns what it printed (and its exit code), for the folder pickers.
int Run(const std::vector<std::string>& argv, std::string* out) {
    int fds[2];
    if (pipe(fds) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> args;
        for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    close(fds[1]);
    char buf[4096];
    for (ssize_t n; (n = read(fds[0], buf, sizeof buf)) > 0;)
        if (out) out->append(buf, size_t(n));
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Starts a program and doesn't wait for it (a link, a folder, Steam). The child is reaped by a short-lived middle
// process, so no zombie is left behind.
bool Spawn(const std::vector<std::string>& argv) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        if (fork() == 0) {
            setsid();
            std::vector<char*> args;
            for (const std::string& a : argv) args.push_back(const_cast<char*>(a.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

bool OnPath(const char* program) {
    std::string path = Env("PATH");
    for (size_t i = 0; i <= path.size();) {
        size_t j = path.find(':', i);
        if (j == std::string::npos) j = path.size();
        if (j > i && access((path.substr(i, j - i) + "/" + program).c_str(), X_OK) == 0) return true;
        i = j + 1;
    }
    return false;
}

size_t OnBody(char* p, size_t size, size_t n, void* user) {
    static_cast<HttpResponse*>(user)->body.append(p, size * n);
    return size * n;
}

size_t OnHeader(char* p, size_t size, size_t n, void* user) {
    std::string line(p, size * n);
    auto* out = static_cast<HttpResponse*>(user);
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
        std::string name = line.substr(0, colon), value = line.substr(colon + 1);
        for (char& c : name) c = Lower(c);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();
        if (name == "etag") out->etag = value;
        else if (name == "x-ratelimit-remaining") out->rateRemaining = std::atoll(value.c_str());
        else if (name == "x-ratelimit-reset") out->rateReset = std::atoll(value.c_str());
    } else if (line.rfind("HTTP/", 0) == 0) {
        // A redirect's headers come first; only the last response's count.
        out->etag.clear();
    }
    return size * n;
}

// The AppImage carries its own libcurl, built on Ubuntu, which looks for the certificates where Ubuntu keeps them.
// Other distributions keep them elsewhere.
const std::string& CaBundle() {
    static const std::string path = [] {
        if (std::string v = Env("SSL_CERT_FILE"); !v.empty() && access(v.c_str(), R_OK) == 0) return v;
        for (const char* p : {"/etc/ssl/certs/ca-certificates.crt",  // Debian, Ubuntu, Arch, SteamOS
                              "/etc/pki/tls/certs/ca-bundle.crt",    // Fedora, RHEL
                              "/etc/ssl/ca-bundle.pem",              // openSUSE
                              "/etc/pki/tls/cacert.pem", "/etc/ssl/cert.pem"})
            if (access(p, R_OK) == 0) return std::string(p);
        return std::string();
    }();
    return path;
}

}  // namespace

bool RunningOnWine() { return false; }
bool NativeLinux() { return true; }
std::string PlatformName() { return "Linux"; }

fs::path ExePath() {
    // Inside an AppImage the program runs from a temporary mount; $APPIMAGE is the file the player has.
    if (std::string a = Env("APPIMAGE"); !a.empty()) return PathU8(a);
    std::error_code ec;
    fs::path p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path("CrabModsManager") : p;
}
fs::path DataDir() { return Xdg("XDG_CONFIG_HOME", ".config") / "CrabModsManager"; }
fs::path CacheDir() { return Xdg("XDG_CACHE_HOME", ".cache") / "CrabModsManager"; }

std::vector<fs::path> SteamRoots() {
    fs::path home = Home();
    return {home / ".steam" / "steam", home / ".local" / "share" / "Steam", home / ".steam" / "root",
            home / ".var" / "app" / "com.valvesoftware.Steam" / ".local" / "share" / "Steam",
            home / "snap" / "steam" / "common" / ".local" / "share" / "Steam"};
}

fs::path LibraryPath(const std::string& steamPath) { return PathU8(steamPath); }

fs::path FindGame() { return FindGameIn(SteamRoots(), LibraryPath); }

// The game is a Windows program under Proton: its Saved folder is inside its own prefix.
std::vector<SavedDir> SavedDirs(const fs::path& game) {
    std::vector<SavedDir> dirs;
    if (fs::path p = ProtonSaved(game); !p.empty() && Exists(p)) dirs.push_back({p, true});
    return dirs;
}

std::vector<std::string> Secrets() {
    std::vector<std::string> secrets;
    for (const char* var : {"USER", "LOGNAME", "USERNAME", "HOSTNAME"})
        if (std::string v = Env(var); !v.empty()) secrets.push_back(v);
    char host[256] = {};
    if (gethostname(host, sizeof host - 1) == 0 && host[0]) secrets.push_back(host);
    if (passwd* pw = getpwuid(getuid())) secrets.push_back(pw->pw_name);
    std::erase_if(secrets, [](const std::string& s) { return s == "steamuser" || s == "localhost"; });
    return secrets;
}

// Proton runs the game as a process whose command line names its exe.
bool GameRunning() {
    std::error_code ec;
    for (const fs::directory_entry& e : fs::directory_iterator("/proc", ec)) {
        std::string pid = e.path().filename().string();
        if (pid.empty() || pid.find_first_not_of("0123456789") != std::string::npos) continue;
        std::optional<std::string> cmd = ReadFile(e.path() / "cmdline");
        if (cmd && cmd->find(kGameExe) != std::string::npos) return true;
    }
    return false;
}

HttpResponse Http(const std::string& url, const std::string& etag, const std::string& accept) {
    static bool init = (curl_global_init(CURL_GLOBAL_DEFAULT), true);
    (void)init;
    HttpResponse out;
    CURL* c = curl_easy_init();
    if (!c) {
        out.error = "libcurl didn't start";
        return out;
    }
    std::string ua = std::string("CrabModsManager/") + kVersion;
    curl_slist* headers = curl_slist_append(nullptr, ("Accept: " + accept).c_str());
    if (!etag.empty()) headers = curl_slist_append(headers, ("If-None-Match: " + etag).c_str());
    char err[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c, CURLOPT_USERAGENT, ua.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "https,http");
#else
    curl_easy_setopt(c, CURLOPT_PROTOCOLS, long(CURLPROTO_HTTPS | CURLPROTO_HTTP));
#endif
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);  // gives up on a download that stalls for a minute
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);  // runs on the worker thread
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, err);
    if (!CaBundle().empty()) curl_easy_setopt(c, CURLOPT_CAINFO, CaBundle().c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, OnBody);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, OnHeader);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, &out);
    CURLcode rc = curl_easy_perform(c);
    if (rc == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        out.status = int(status);
    } else {
        out.status = 0;
        out.body.clear();
        out.error = std::string("no connection (") + (err[0] ? err : curl_easy_strerror(rc)) + ")";
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(c);
    return out;
}

void OpenExternal(const std::string& target) { Spawn({"xdg-open", target}); }

std::string LaunchGame(const fs::path&) {
    std::string url = std::string("steam://rungameid/") + kAppId;
    if (OnPath("steam")) Spawn({"steam", url});
    else Spawn({"xdg-open", url});
    return "Starting Crab Champions through Steam. While this window is open, it watches for crashes.";
}

// zenity (GNOME and most others) or kdialog (KDE); without either the app asks for the path itself.
std::optional<fs::path> PickFolder(const std::string& title) {
    std::vector<std::string> argv;
    if (OnPath("zenity")) argv = {"zenity", "--file-selection", "--directory", "--title=" + title};
    else if (OnPath("kdialog")) argv = {"kdialog", "--getexistingdirectory", U8(Home()), "--title", title};
    else return std::nullopt;
    std::string out;
    int rc = Run(argv, &out);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    if (rc == 127) return std::nullopt;  // there after all, but wouldn't start
    if (rc != 0 || out.empty()) return fs::path();
    return PathU8(out);
}

void Wake() {
    if (!g_wakeEvent) return;
    SDL_Event e{};
    e.type = g_wakeEvent;
    SDL_PushEvent(&e);
}

void Attention() {
    if (g_window && !(SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS)) SDL_FlashWindow(g_window, SDL_FLASH_UNTIL_FOCUSED);
}

const char* ManagerAsset() { return kManagerAppImage; }

// Next to the AppImage, so the swap is a rename on the same drive. A manager not run as an AppImage (a developer's
// build) keeps it in the cache and never swaps.
fs::path StagedUpdate() {
    if (std::string a = Env("APPIMAGE"); !a.empty()) return PathU8(a + ".new");
    return CacheDir() / "update" / kManagerAppImage;
}

// Linux lets a running program's file be replaced: the new AppImage simply takes the old one's name.
bool SwapInUpdate(std::string& error) {
    std::string appimage = Env("APPIMAGE");
    if (appimage.empty()) {
        error = "this manager isn't running as an AppImage, so it can't replace itself";
        return false;
    }
    fs::path staged = StagedUpdate(), target = PathU8(appimage);
    if (!Exists(staged)) {
        error = "the downloaded update is gone";
        return false;
    }
    std::error_code ec;
    fs::permissions(staged, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec | fs::perms::others_read |
                                fs::perms::others_exec,
                    fs::perm_options::replace, ec);
    fs::rename(staged, target, ec);
    if (ec) {
        error = "could not put the new manager in place (" + ec.message() + ")";
        return false;
    }
    fs::path v = staged;
    v += ".version";
    fs::remove(v, ec);
    return true;
}

bool Relaunch(const std::vector<std::string>& args) {
    std::vector<std::string> argv = {U8(ExePath())};
    argv.insert(argv.end(), args.begin(), args.end());
    return Spawn(argv);
}

}  // namespace mgr::plat
