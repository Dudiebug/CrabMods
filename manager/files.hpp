// CrabMods Manager: everything it does to files, on any system. Reading and writing, finding the game, the
// record of what it installed (and the backups of what it replaced), checked downloads, the download cache and
// crash reports.
// No calls into the operating system beyond std::filesystem, so tests/manager_core_test.cpp checks it on Windows
// and Linux alike.
#pragma once
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>

#include "core.hpp"
#include "pe_version.hpp"
#include "sha256.hpp"

namespace mgr {

namespace fs = std::filesystem;

// --- Paths and files ------------------------------------------------------------------------------------------

inline std::string U8(const fs::path& p) {
    auto s = p.u8string();
    return std::string(s.begin(), s.end());
}
inline fs::path PathU8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

inline std::string Lower(std::string s) {
    for (char& c : s) c = Lower(c);
    return s;
}

inline bool Exists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

inline std::optional<std::string> ReadFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad()) return std::nullopt;
    return data;
}

// Writes through a temporary file next to it, so a failure (a full disk, a crash) never leaves half a file.
inline bool WriteFile(const fs::path& p, const std::string& data) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << data;
        f.flush();
        if (!f) {
            f.close();
            fs::remove(tmp, ec);
            return false;
        }
    }
    fs::rename(tmp, p, ec);
    if (ec) fs::remove(tmp, ec);
    return !ec;
}

inline std::string FileSha(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    Sha256 s;
    char buf[65536];
    while (f.read(buf, sizeof buf) || f.gcount() > 0) s.Update(buf, size_t(f.gcount()));
    return s.Hex();
}

// "0.14.1" from a DLL's version resource; "" without the file, "?" if it has none (releases before the manager).
inline std::string FileVersion(const fs::path& p) {
    auto data = ReadFile(p);
    if (!data) return {};
    std::string v = PeFileVersion(*data);
    return v.empty() ? "?" : v;
}

inline std::string HumanSize(uint64_t bytes) {
    char b[32];
    if (bytes >= 1024 * 1024) std::snprintf(b, sizeof b, "%.1f MB", bytes / 1048576.0);
    else std::snprintf(b, sizeof b, "%llu KB", (unsigned long long)((bytes + 1023) / 1024));
    return b;
}

// --- Time -------------------------------------------------------------------------------------------------------

using FileTime = fs::file_time_type;

inline FileTime FileNow() { return FileTime::clock::now(); }
inline int64_t UnixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// "2026-10-11 14:03" (or "2026-10-11_14-03-05" for file names) in local time.
inline std::string LocalTime(FileTime t, bool forFile) {
    auto sys = std::chrono::system_clock::now() +
               std::chrono::duration_cast<std::chrono::system_clock::duration>(t - FileNow());
    std::time_t tt = std::chrono::system_clock::to_time_t(sys);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, forFile ? "%Y-%m-%d_%H-%M-%S" : "%Y-%m-%d %H:%M", &tm);
    return buf;
}

// --- The game folder ----------------------------------------------------------------------------------------------

inline constexpr const char kGameExe[] = "CrabChampions-Win64-Shipping.exe";

inline fs::path GameSub() { return fs::path("steamapps") / "common" / "Crab Champions" / "CrabChampions" / "Binaries" / "Win64"; }

inline bool IsGameDir(const fs::path& d) { return !d.empty() && Exists(d / kGameExe); }

// The Win64 folder for whatever the player picked: Win64 itself, the game folder, or a Steam library.
inline fs::path NormalizeGameDir(fs::path d) {
    if (d.empty()) return {};
    std::string s = U8(d);
    while (s.size() > 1 && (s.back() == '\\' || s.back() == '/')) s.pop_back();
    d = PathU8(s);
    const fs::path subs[] = {fs::path(), fs::path("CrabChampions") / "Binaries" / "Win64", fs::path("Binaries") / "Win64", GameSub(),
                             fs::path("common") / "Crab Champions" / "CrabChampions" / "Binaries" / "Win64"};
    for (const fs::path& sub : subs) {
        fs::path p = sub.empty() ? d : d / sub;
        if (IsGameDir(p)) return p;
    }
    return {};
}

// The game in any Steam library of the given Steam folders. `libPath` turns a library path from Steam's files into
// one this program can open (under wine, "/home/me/..." is "Z:\home\me\...").
inline fs::path FindGameIn(const std::vector<fs::path>& steamRoots, const std::function<fs::path(const std::string&)>& libPath) {
    for (const fs::path& root : steamRoots) {
        std::vector<fs::path> libs{root};
        if (auto vdf = ReadFile(root / "steamapps" / "libraryfolders.vdf"))
            for (const std::string& lib : SteamLibraries(*vdf)) libs.push_back(libPath(lib));
        for (const fs::path& lib : libs)
            if (IsGameDir(lib / GameSub())) return lib / GameSub();
    }
    return {};
}

// The Steam library a game folder is in (".../steamapps/common/Crab Champions/..." -> "..."), or empty.
inline fs::path LibraryOf(const fs::path& game) {
    fs::path lib;
    for (fs::path p = game; p.has_parent_path() && p != p.parent_path(); p = p.parent_path())
        if (p.filename() == "common" && p.parent_path().filename() == "steamapps") return p.parent_path().parent_path();
    return lib;
}

// Proton's copy of the game's Saved folder (crash reports and log), in the game's own prefix.
inline fs::path ProtonSaved(const fs::path& game) {
    fs::path lib = LibraryOf(game);
    if (lib.empty()) return {};
    return lib / "steamapps" / "compatdata" / kAppId / "pfx" / "drive_c" / "users" / "steamuser" / "AppData" / "Local" /
           "CrabChampions" / "Saved";
}

// Whether the launch option is set: nothing when Steam's files aren't found.
inline std::optional<bool> LaunchOptionSetIn(const std::vector<fs::path>& steamRoots) {
    std::optional<bool> result;
    for (const fs::path& root : steamRoots) {
        std::error_code ec;
        for (auto it = fs::directory_iterator(root / "userdata", ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            auto vdf = ReadFile(it->path() / "config" / "localconfig.vdf");
            if (!vdf) continue;
            if (!result) result = false;
            if (LaunchOptionInVdf(*vdf)) return true;
        }
    }
    return result;
}

// --- The install record ---------------------------------------------------------------------------------------------
// state/installed.json: every file the manager put into the game folder, with its hash, and the backup of what
// was there before it (when that wasn't the manager's own). "Remove all mods" uses it to put the folder back.

struct RecordEntry {
    std::string name;     // in the game folder: "CrabMods.dll"
    std::string mod;      // "CrabMods", "CrabSync" or "loader"
    std::string version;
    std::string sha256;   // what the manager wrote
    std::string backup;   // the file that was there before, under the state folder ("backups/<time>/x.dll"), or ""
    bool adopted = false; // found installed by an older manager or by hand, taken over as it was
};

struct InstallRecord {
    std::vector<RecordEntry> files;
    bool exists = false;  // read from a file (a manager before 0.4 had none)

    RecordEntry* Find(const std::string& name) {
        for (auto& f : files)
            if (f.name == name) return &f;
        return nullptr;
    }
    const RecordEntry* Find(const std::string& name) const { return const_cast<InstallRecord*>(this)->Find(name); }
    // The file there now is the one the manager wrote.
    bool Owns(const std::string& name, const std::string& sha) const {
        const RecordEntry* e = Find(name);
        return e && e->sha256 == sha;
    }
    void Erase(const std::string& name) {
        std::erase_if(files, [&](const RecordEntry& e) { return e.name == name; });
    }
};

inline InstallRecord ParseRecord(const std::string& json) {
    InstallRecord r;
    auto root = JsonParser(json).Parse();
    if (!root || root->type != Json::Object) return r;
    r.exists = true;
    if (const Json* files = root->Get("files"); files && files->type == Json::Array)
        for (const Json& f : files->items)
            if (f.type == Json::Object && !f.Str("name").empty())
                r.files.push_back({f.Str("name"), f.Str("mod"), f.Str("version"), f.Str("sha256"), f.Str("backup"), f.Flag("adopted")});
    return r;
}

inline std::string RecordJson(const InstallRecord& r) {
    std::string j = "{\n  \"files\": [";
    for (size_t i = 0; i < r.files.size(); i++) {
        const RecordEntry& f = r.files[i];
        j += std::string(i ? "," : "") + "\n    {\"name\": " + JsonString(f.name) + ", \"mod\": " + JsonString(f.mod) +
             ", \"version\": " + JsonString(f.version) + ", \"sha256\": " + JsonString(f.sha256) +
             ", \"backup\": " + JsonString(f.backup) + ", \"adopted\": " + (f.adopted ? "true" : "false") + "}";
    }
    return j + "\n  ]\n}\n";
}

inline InstallRecord LoadRecord(const fs::path& state) {
    auto t = ReadFile(state / "installed.json");
    return t ? ParseRecord(*t) : InstallRecord{};
}
inline bool SaveRecord(const fs::path& state, InstallRecord& r) {
    r.exists = true;
    return WriteFile(state / "installed.json", RecordJson(r));
}

// A manager before 0.4 kept no record: the mod files it (or the player) put there are taken over as they are,
// with nothing backed up. The loader only when a mod is there too (another program may use an xinput1_3.dll).
inline bool AdoptInstalled(const fs::path& game, InstallRecord& r) {
    if (r.exists || game.empty()) return false;
    bool anyMod = false;
    for (const ModInfo& m : Mods())
        if (Exists(game / m.dll)) {
            anyMod = true;
            r.files.push_back({m.dll, m.name, FileVersion(game / m.dll), FileSha(game / m.dll), "", true});
        }
    if (anyMod && Exists(game / kLoaderDll))
        r.files.push_back({kLoaderDll, "loader", FileVersion(game / kLoaderDll), FileSha(game / kLoaderDll), "", true});
    return true;
}

// --- Change plans -------------------------------------------------------------------------------------------------
// What an install, update, rollback or removal will do to the game folder, worked out before anything is
// touched, shown to the player, then applied.

struct FileChange {
    enum Op { Add, Replace, Remove, Keep } op = Add;  // Keep: already there as wanted; only recorded
    std::string name;
    std::string mod, version;
    std::string sha256;     // Add/Replace/Keep: the content wanted
    fs::path source;        // Add/Replace: the checked file in the download cache
    std::string url;        // Add/Replace: where it comes from, when it isn't downloaded yet
    uint64_t size = 0;
    bool backup = false;    // Replace/Remove: what is there now isn't the manager's: keep a copy first
    std::string restore;    // Remove: a backup to put back
};

struct ChangePlan {
    std::string title;                   // "Install CrabMods 0.14.2"
    std::vector<FileChange> files;
    std::vector<std::string> settings;   // settings changes, in words
    std::vector<std::pair<std::string, std::string>> iniWrites;  // mod ini name -> compact ini to merge
    std::vector<std::string> deletes;    // settings and log files to delete (Remove all, if asked)

    bool Empty() const {
        for (const FileChange& f : files)
            if (f.op != FileChange::Keep) return false;
        return settings.empty() && iniWrites.empty() && deletes.empty();
    }
    std::vector<std::string> Lines() const {
        std::vector<std::string> l;
        for (const FileChange& f : files) {
            std::string v = f.version.empty() || f.version == "?" ? "" : " " + f.version;
            switch (f.op) {
            case FileChange::Add: l.push_back("Add " + f.name + v + (f.size ? " (" + HumanSize(f.size) + ")" : "")); break;
            case FileChange::Replace:
                l.push_back("Replace " + f.name + " with" + (v.empty() ? " the new one" : v) +
                            (f.backup ? " (the one there now is backed up)" : ""));
                break;
            case FileChange::Remove:
                l.push_back("Remove " + f.name + (f.restore.empty() ? "" : " and put back the " + f.name + " that was there before") +
                            (f.backup ? " (backed up first)" : ""));
                break;
            case FileChange::Keep: break;
            }
        }
        for (const std::string& d : deletes) l.push_back("Delete " + d);
        for (const std::string& s : settings) l.push_back("Setting: " + s);
        return l;
    }
};

struct WantFile {
    std::string name, mod, version, sha256, url;
    fs::path source;
    uint64_t size = 0;
};

// Puts `w` into the plan: nothing when the same file is there already (recorded as the manager's), a replacement
// (backing up a file that isn't the manager's), or an addition.
inline void PlanAdd(ChangePlan& plan, const fs::path& game, const InstallRecord& rec, const WantFile& w) {
    FileChange c{FileChange::Add, w.name, w.mod, w.version, w.sha256, w.source, w.url, w.size};
    fs::path at = game / w.name;
    if (Exists(at)) {
        std::string cur = FileSha(at);
        if (!w.sha256.empty() && cur == w.sha256) c.op = FileChange::Keep;
        else c.op = FileChange::Replace, c.backup = !rec.Owns(w.name, cur);
    }
    plan.files.push_back(c);
}

// Takes a file out, putting back what it replaced. A file that isn't the manager's is backed up, not lost.
inline void PlanRemove(ChangePlan& plan, const fs::path& game, const InstallRecord& rec, const std::string& name) {
    const RecordEntry* e = rec.Find(name);
    fs::path at = game / name;
    if (!Exists(at) && !e) return;
    FileChange c{FileChange::Remove, name};
    if (e) c.mod = e->mod, c.version = e->version, c.restore = e->backup;
    if (Exists(at)) c.backup = !rec.Owns(name, FileSha(at));
    plan.files.push_back(c);
}

// Carries out the file changes (not the settings, which the app writes). `stamp` names this run's backup folder.
// Stops at the first failure (a file the running game holds, a folder that can't be written), with the record
// saying exactly what was done.
inline bool ApplyFiles(const fs::path& game, const fs::path& state, const ChangePlan& plan, InstallRecord& rec,
                       const std::string& stamp, std::string& error) {
    std::error_code ec;
    auto backupOf = [&](const std::string& name) -> std::string {
        std::string rel = "backups/" + stamp + "/" + name;
        fs::create_directories(state / "backups" / stamp, ec);
        fs::copy_file(game / name, state / PathU8(rel), fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error = "could not back up " + name + ": " + ec.message();
            return {};
        }
        return rel;
    };
    bool ok = true;
    for (const FileChange& c : plan.files) {
        fs::path at = game / PathU8(c.name);
        if (c.op == FileChange::Keep) {
            if (!rec.Find(c.name)) rec.files.push_back({c.name, c.mod, c.version, c.sha256, "", true});
            else rec.Find(c.name)->version = c.version, rec.Find(c.name)->mod = c.mod;
            continue;
        }
        if (c.op == FileChange::Add || c.op == FileChange::Replace) {
            if (FileSha(c.source) != c.sha256) {
                error = c.name + " in the download cache doesn't match its checksum";
                ok = false;
                break;
            }
            std::string backup;
            if (const RecordEntry* e = rec.Find(c.name)) backup = e->backup;  // the player's original stays the one kept
            if (c.op == FileChange::Replace && c.backup && Exists(at)) {
                std::string b = backupOf(c.name);
                if (b.empty()) {
                    ok = false;
                    break;
                }
                if (backup.empty()) backup = b;
            }
            fs::path tmp = at;
            tmp += ".download";
            fs::copy_file(c.source, tmp, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                error = "could not write to the game folder (" + ec.message() + ")";
                ok = false;
                break;
            }
            fs::rename(tmp, at, ec);
            if (ec) {
                fs::remove(tmp, ec);
                error = "could not replace " + c.name + " while the game is running";
                ok = false;
                break;
            }
            rec.Erase(c.name);
            rec.files.push_back({c.name, c.mod, c.version, c.sha256, backup, false});
        } else if (c.op == FileChange::Remove) {
            if (Exists(at)) {
                if (c.backup && backupOf(c.name).empty()) {
                    ok = false;
                    break;
                }
                fs::remove(at, ec);
                if (ec) {
                    error = "could not remove " + c.name + " while the game is running";
                    ok = false;
                    break;
                }
            }
            if (!c.restore.empty()) {
                fs::path b = state / PathU8(c.restore);
                fs::copy_file(b, at, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    error = "could not put back the old " + c.name + " (it is still in " + U8(b) + ")";
                    ok = false;
                    break;
                }
                fs::remove(b, ec);
            }
            rec.Erase(c.name);
        }
    }
    if (ok)
        for (const std::string& d : plan.deletes) fs::remove(game / PathU8(d), ec);
    // Empty backup folders go.
    for (auto it = fs::directory_iterator(state / "backups", ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        if (it->is_directory() && fs::is_empty(it->path())) fs::remove(it->path(), ec);
    ec.clear();
    SaveRecord(state, rec);
    return ok;
}

// --- Downloads and the cache ---------------------------------------------------------------------------------------
// cache/<mod>/<version>/<file>: each release's files, checked against GitHub's hashes. The last two versions of
// each mod stay, so a rollback needs no download.

using Fetch = std::function<HttpResponse(const std::string& url)>;

inline fs::path CachedFile(const fs::path& cache, const std::string& mod, const std::string& version, const std::string& file) {
    return cache / PathU8(mod) / PathU8(version) / PathU8(file);
}

// Downloads `a` to `target`, refusing anything that doesn't match the hash GitHub published for it. A file already
// there with the right hash isn't downloaded again.
inline bool DownloadChecked(const Fetch& fetch, const Asset& a, const fs::path& target, std::string& error) {
    if (a.sha256.empty()) {
        error = "GitHub lists no checksum for " + a.name + ", so it isn't downloaded";
        return false;
    }
    if (Exists(target) && FileSha(target) == a.sha256) return true;
    HttpResponse r = fetch(a.url);
    if (r.status != 200) {
        error = "could not download " + a.name + " (" + (r.status ? "the server answered " + std::to_string(r.status) : r.error) + ")";
        return false;
    }
    if (Sha256Hex(r.body) != a.sha256) {
        error = a.name + " didn't match its published checksum, so it was thrown away";
        return false;
    }
    if (!WriteFile(target, r.body)) {
        error = "could not save " + a.name + " in " + U8(target.parent_path());
        return false;
    }
    return true;
}

// Keeps the newest `keep` versions of a mod in the cache, and `alsoKeep` (the installed one).
inline void PruneCache(const fs::path& modCache, size_t keep, const std::set<std::string>& alsoKeep) {
    std::error_code ec;
    std::vector<std::string> versions;
    for (auto it = fs::directory_iterator(modCache, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
        if (it->is_directory()) versions.push_back(U8(it->path().filename()));
    std::sort(versions.begin(), versions.end(), [](auto& a, auto& b) { return CompareVersions(a, b) > 0; });
    for (size_t i = keep; i < versions.size(); i++)
        if (!alsoKeep.count(versions[i])) fs::remove_all(modCache / PathU8(versions[i]), ec);
}

// --- Crash reports ------------------------------------------------------------------------------------------------

struct Crash {
    fs::path dir;
    std::string name;
    FileTime time{};      // the newest file in it
    uint64_t size = 0;    // all of them together (to see it's finished)
    bool proton = false;
};

struct SavedDir {
    fs::path path;
    bool proton;
};

inline std::optional<Crash> NewestCrash(const std::vector<SavedDir>& saved) {
    std::optional<Crash> newest;
    std::error_code ec;
    for (const SavedDir& s : saved) {
        for (auto it = fs::directory_iterator(s.path / "Crashes", ec); !ec && it != fs::directory_iterator(); it.increment(ec)) {
            if (!it->is_directory()) continue;
            Crash c{it->path(), U8(it->path().filename()), FileTime::min(), 0, s.proton};
            bool context = false;
            std::error_code e2;
            for (auto f = fs::directory_iterator(c.dir, e2); !e2 && f != fs::directory_iterator(); f.increment(e2)) {
                if (!f->is_regular_file()) continue;
                c.time = std::max(c.time, f->last_write_time());
                c.size += f->file_size();
                context |= Lower(U8(f->path().filename())) == "crashcontext.runtime-xml";
            }
            if (context && (!newest || c.time > newest->time)) newest = c;
        }
        ec.clear();
    }
    return newest;
}

struct BuiltReport {
    CrashReport report;
    fs::path folder, zip;
    std::string error;
};

struct ReportInput {
    fs::path game;
    std::vector<SavedDir> saved;
    std::optional<Crash> crash;
    fs::path reportsDir;               // CrashReports
    std::vector<std::string> secrets;  // user and computer names
    std::string versions, platform;
};

// Copies the mod logs and settings, the game's log and the crash folder (if any) into
// CrashReports/CrabCrash-<time>, with names and ids taken out, and zips them there.
inline BuiltReport BuildReport(const ReportInput& in) {
    BuiltReport b;
    CrashReport& r = b.report;
    const auto& crash = in.crash;
    std::vector<std::pair<std::string, std::string>> files;  // name in the report, contents
    if (!in.game.empty())
        for (const char* name : {"CrabMods.log", "CrabSync.log", "CrabMods.ini", "CrabSync.ini"})
            if (auto t = ReadFile(in.game / name)) files.push_back({name, *t});
    for (const SavedDir& saved : in.saved)
        if (auto t = ReadFile(saved.path / "Logs" / "CrabChampions.log")) {
            files.push_back({"CrabChampions.log", *t});
            break;
        }
    std::error_code ec;
    if (crash)
        for (auto f = fs::directory_iterator(crash->dir, ec); !ec && f != fs::directory_iterator(); f.increment(ec)) {
            if (!f->is_regular_file()) continue;
            std::string fname = U8(f->path().filename());
            bool dump = Lower(fname).ends_with(".dmp");
            if (dump && (f->file_size() == 0 || f->file_size() > 20'000'000)) continue;  // wine writes empty ones; GitHub takes 25 MB
            if (auto t = ReadFile(f->path())) files.push_back({"crash/" + fname, *t});
        }

    std::vector<std::string> players;
    for (const auto& [name, text] : files)
        if (name == "CrabSync.log") players = PlayerNames(text);
    for (auto& [name, text] : files) {
        if (name.ends_with(".dmp")) continue;
        if (name.ends_with(".runtime-xml")) text = StripCrashIds(text);
        text = Scrub(text, in.secrets, players);
        if (name == "CrabMods.log") r.crabModsLog = text;
        if (name == "CrabSync.log") r.crabSyncLog = text;
        if (name == "crash/CrashContext.runtime-xml") ReadCrashContext(r, text);
    }
    FileTime when = crash ? crash->time : FileNow();
    r.when = LocalTime(when, false);
    r.platform = in.platform;
    if (crash && crash->proton && r.platform == "Windows") r.platform = "Linux (Proton)";
    r.versions = in.versions;
    if (crash && !in.game.empty() && Exists(in.game / "CrabMods.log") &&
        fs::last_write_time(in.game / "CrabMods.log", ec) > crash->time + std::chrono::seconds(60))
        r.note = "the game was started again after the crash, so the mod logs are from that later session";

    std::string stamp = (crash ? "CrabCrash-" : "CrabLogs-") + LocalTime(when, true);
    b.folder = in.reportsDir / stamp;
    fs::create_directories(b.folder, ec);
    if (ec) {
        b.error = "could not make the folder " + U8(b.folder);
        return b;
    }
    r.zipName = stamp + ".zip";
    files.push_back({"report.md", "# " + IssueTitle(r) + "\n\n" + IssueBody(r, 200)});
    for (const auto& [name, text] : files) WriteFile(b.folder / PathU8(name), text);
    std::time_t tt = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    localtime_s(&t, &tt);
#else
    localtime_r(&tt, &t);
#endif
    uint16_t dosTime = uint16_t((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec / 2));
    uint16_t dosDate = uint16_t(((t.tm_year + 1900 - 1980) << 9) | ((t.tm_mon + 1) << 5) | t.tm_mday);
    b.zip = b.folder / PathU8(r.zipName);
    if (!WriteFile(b.zip, StoreZip(files, dosTime, dosDate))) b.error = "could not write " + r.zipName;
    return b;
}

}  // namespace mgr
