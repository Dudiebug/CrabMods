// CrabMods Manager: everything that is not the window. Release lists, versions, share codes, ini merging,
// crash reports.
// No Windows calls here, so tests/manager_test.cpp can check it all.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace mgr {

// ---------------------------------------------------------------------------------------------------------
// The mods the manager knows

struct ModInfo {
    const char* name;         // "CrabMods"
    const char* dll;          // "CrabMods.dll"
    const char* tagPrefix;    // release tags: "crabmods-v0.3.0"
    const char* ini;          // its settings file, next to the DLL
    const char* description;
};

inline const std::vector<ModInfo>& Mods() {
    static const std::vector<ModInfo> mods = {
        {"CrabMods", "CrabMods.dll", "crabmods-v", "CrabMods.ini",
         "New perks, relics and mods. Every player needs it; the host's settings are used by everyone."},
        {"CrabSync", "CrabSync.dll", "crabsync-v", "CrabSync.ini",
         "Shared team inventory and health. Only the host needs it."},
    };
    return mods;
}

// The mod loader every mod needs: the game loads xinput1_3.dll, and it loads the other DLLs beside it.
inline constexpr const char kLoaderDll[] = "xinput1_3.dll";
inline constexpr const char kRepo[] = "Dudiebug/CrabMods";

// ---------------------------------------------------------------------------------------------------------
// Versions: "0.3.0" < "0.10.0"

inline std::vector<int> ParseVersion(const std::string& v) {
    std::vector<int> parts;
    int n = 0;
    bool digit = false;
    for (char c : v) {
        if (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            digit = true;
        } else if (c == '.') {
            parts.push_back(n);
            n = 0;
            digit = false;
        } else {
            break;  // "1.0.1-beta": the rest is not compared
        }
    }
    if (digit) parts.push_back(n);
    return parts;
}

// <0, 0, >0 like strcmp. Missing parts count as 0: "1.0" == "1.0.0".
inline int CompareVersions(const std::string& a, const std::string& b) {
    auto x = ParseVersion(a), y = ParseVersion(b);
    for (size_t i = 0; i < std::max(x.size(), y.size()); i++) {
        int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------
// JSON, just enough for the GitHub releases API

struct Json {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> items;                           // Array
    std::vector<std::pair<std::string, Json>> fields;  // Object, in order

    const Json* Get(const std::string& key) const {
        for (auto& [k, v] : fields)
            if (k == key) return &v;
        return nullptr;
    }
    std::string Str(const std::string& key) const {
        const Json* v = Get(key);
        return v && v->type == String ? v->str : std::string();
    }
    bool Flag(const std::string& key) const {
        const Json* v = Get(key);
        return v && v->type == Bool && v->b;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& s) : s_(s) {}
    std::optional<Json> Parse() {
        Json v;
        if (!Value(v)) return std::nullopt;
        Ws();
        if (i_ != s_.size()) return std::nullopt;
        return v;
    }

private:
    void Ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) i_++;
    }
    bool Lit(const char* w) {
        size_t n = strlen(w);
        if (s_.compare(i_, n, w) != 0) return false;
        i_ += n;
        return true;
    }
    static void Utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out += char(cp);
        } else if (cp < 0x800) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }
    bool Hex4(uint32_t& cp) {
        if (i_ + 4 > s_.size()) return false;
        cp = 0;
        for (int k = 0; k < 4; k++) {
            char c = s_[i_++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= c - '0';
            else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
            else return false;
        }
        return true;
    }
    bool String(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        i_++;
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i_ >= s_.size()) return false;
            char e = s_[i_++];
            switch (e) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                uint32_t cp;
                if (!Hex4(cp)) return false;
                if (cp >= 0xD800 && cp < 0xDC00 && s_.compare(i_, 2, "\\u") == 0) {
                    i_ += 2;
                    uint32_t lo;
                    if (!Hex4(lo)) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                Utf8(out, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    bool Value(Json& v) {
        if (++depth_ > 64) return false;
        Ws();
        bool ok = ValueInner(v);
        depth_--;
        return ok;
    }
    bool ValueInner(Json& v) {
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '{') {
            v.type = Json::Object;
            i_++;
            Ws();
            if (i_ < s_.size() && s_[i_] == '}') return ++i_, true;
            while (true) {
                Ws();
                std::string key;
                if (!String(key)) return false;
                Ws();
                if (i_ >= s_.size() || s_[i_++] != ':') return false;
                Json item;
                if (!Value(item)) return false;
                v.fields.emplace_back(std::move(key), std::move(item));
                Ws();
                if (i_ >= s_.size()) return false;
                if (s_[i_] == ',') { i_++; continue; }
                if (s_[i_] == '}') return ++i_, true;
                return false;
            }
        }
        if (c == '[') {
            v.type = Json::Array;
            i_++;
            Ws();
            if (i_ < s_.size() && s_[i_] == ']') return ++i_, true;
            while (true) {
                Json item;
                if (!Value(item)) return false;
                v.items.push_back(std::move(item));
                Ws();
                if (i_ >= s_.size()) return false;
                if (s_[i_] == ',') { i_++; continue; }
                if (s_[i_] == ']') return ++i_, true;
                return false;
            }
        }
        if (c == '"') {
            v.type = Json::String;
            return String(v.str);
        }
        if (Lit("true")) { v.type = Json::Bool; v.b = true; return true; }
        if (Lit("false")) { v.type = Json::Bool; return true; }
        if (Lit("null")) return true;
        size_t start = i_;
        while (i_ < s_.size() && strchr("+-0123456789.eE", s_[i_])) i_++;
        if (i_ == start) return false;
        v.type = Json::Number;
        v.num = strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return true;
    }

    const std::string& s_;
    size_t i_ = 0;
    int depth_ = 0;
};

// ---------------------------------------------------------------------------------------------------------
// Releases

struct Asset {
    std::string name, url;
    std::string sha256;  // from GitHub's "digest" ("sha256:..."), lowercase hex; "" when GitHub has none
    uint64_t size = 0;
};

struct Release {
    std::string tag, version;  // "crabmods-v0.3.0", "0.3.0"
    std::string page;          // the release's page on GitHub ("What's new")
    std::string notes;         // its text (Markdown)
    std::map<std::string, Asset> assets;  // by file name
    bool Has(const std::string& file) const { return assets.count(file) > 0; }
    const Asset* Get(const std::string& file) const {
        auto it = assets.find(file);
        return it == assets.end() ? nullptr : &it->second;
    }
};

// Every published release whose tag starts with `prefix`, newest version first. Drafts and pre-releases are
// skipped.
inline std::vector<Release> ReleasesTagged(const Json& root, const std::string& prefix) {
    std::vector<Release> out;
    if (root.type != Json::Array) return out;
    for (const Json& r : root.items) {
        if (r.type != Json::Object || r.Flag("draft") || r.Flag("prerelease")) continue;
        std::string tag = r.Str("tag_name");
        if (tag.rfind(prefix, 0) != 0) continue;
        Release rel{tag, tag.substr(prefix.size()), r.Str("html_url"), r.Str("body"), {}};
        if (const Json* assets = r.Get("assets"); assets && assets->type == Json::Array)
            for (const Json& a : assets->items) {
                if (a.type != Json::Object) continue;
                Asset as{a.Str("name"), a.Str("browser_download_url"), "", 0};
                std::string d = a.Str("digest");
                if (d.rfind("sha256:", 0) == 0) as.sha256 = d.substr(7);
                if (const Json* s = a.Get("size"); s && s->type == Json::Number) as.size = uint64_t(s->num);
                rel.assets[as.name] = as;
            }
        out.push_back(std::move(rel));
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const Release& a, const Release& b) { return CompareVersions(a.version, b.version) > 0; });
    return out;
}

inline constexpr const char kManagerTag[] = "manager-v";
inline constexpr const char kManagerExe[] = "CrabModsManager.exe";
inline constexpr const char kManagerAppImage[] = "CrabModsManager-x86_64.AppImage";

// The releases of each mod (keyed by mod name) and of the manager ("Manager"), newest first, from a GitHub
// /releases response.
struct ReleaseList {
    std::map<std::string, std::vector<Release>> all;
    bool ok = false;  // the response was a release list

    // The newest release of `name` that has the file `asset` (older releases only had a zip, and can't be
    // installed from).
    const Release* Latest(const std::string& name, const std::string& asset) const {
        auto it = all.find(name);
        if (it != all.end())
            for (const Release& r : it->second)
                if (r.Has(asset)) return &r;
        return nullptr;
    }
    const Release* Version(const std::string& name, const std::string& version) const {
        auto it = all.find(name);
        if (it != all.end())
            for (const Release& r : it->second)
                if (CompareVersions(r.version, version) == 0) return &r;
        return nullptr;
    }
    // The newest release before `version` that has `asset`: what "Roll back" installs.
    const Release* Before(const std::string& name, const std::string& version, const std::string& asset) const {
        auto it = all.find(name);
        if (it != all.end())
            for (const Release& r : it->second)
                if (CompareVersions(r.version, version) < 0 && r.Has(asset)) return &r;
        return nullptr;
    }
};

inline ReleaseList ParseReleases(const std::string& json) {
    ReleaseList list;
    auto root = JsonParser(json).Parse();
    if (!root || root->type != Json::Array) return list;
    list.ok = true;
    for (const ModInfo& m : Mods()) list.all[m.name] = ReleasesTagged(*root, m.tagPrefix);
    list.all["Manager"] = ReleasesTagged(*root, kManagerTag);
    return list;
}

// The newest release of each mod, keyed by mod name, and of the manager itself under "Manager" (the one with
// `managerAsset`, the exe on Windows and the AppImage on Linux).
inline std::map<std::string, Release> LatestReleases(const std::string& json, const std::string& managerAsset = kManagerExe) {
    std::map<std::string, Release> latest;
    ReleaseList list = ParseReleases(json);
    for (const ModInfo& m : Mods())
        if (const Release* r = list.Latest(m.name, m.dll)) latest[m.name] = *r;
    if (const Release* r = list.Latest("Manager", managerAsset)) latest["Manager"] = *r;
    return latest;
}

// The catalog file each mod release carries: the mod's items and default settings, as JSON (made from the DLL when
// it is built, so the manager never has to load a mod to read them).
inline std::string CatalogAsset(const ModInfo& m) { return std::string(m.name) + ".catalog.json"; }

// ---------------------------------------------------------------------------------------------------------
// Ini text

inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// The settings alone: "[section]" and "key = value" lines, no comments or blank lines.
inline std::string CompactIni(const std::string& text) {
    std::string out, line;
    for (std::istringstream f(text); std::getline(f, line);) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        line = Trim(line);
        if (!line.empty()) out += line + "\n";
    }
    return out;
}

// section -> key -> value, in a compact or full ini. Keys before any section go under "".
using IniValues = std::map<std::string, std::map<std::string, std::string>>;

inline IniValues ReadIni(const std::string& text) {
    IniValues v;
    std::string section, line;
    for (std::istringstream f(text); std::getline(f, line);) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        line = Trim(line);
        if (line.size() > 1 && line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            v[section];
            continue;
        }
        auto eq = line.find('=');
        if (eq != std::string::npos) v[section][Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return v;
}

// Puts the values from `incoming` into the ini `text`, keeping its comments and layout: existing keys get the
// new value in place, keys the file lacks are added at the end of their section, sections it lacks are
// appended. Keys only the file has are left as they are.
inline std::string MergeIni(const std::string& text, const std::string& incoming) {
    IniValues in = ReadIni(incoming);
    std::vector<std::string> lines;
    {
        std::string line;
        for (std::istringstream f(text); std::getline(f, line);) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
    }
    std::map<std::string, std::map<std::string, bool>> done;
    std::map<std::string, bool> sectionSeen;
    std::vector<std::string> out;
    std::string section;
    auto flush = [&](const std::string& s) {  // keys of section s the file did not have
        auto it = in.find(s);
        if (it == in.end()) return;
        // Insert before the blank lines that end the section.
        size_t at = out.size();
        while (at > 0 && Trim(out[at - 1]).empty()) at--;
        std::vector<std::string> add;
        for (auto& [k, v] : it->second)
            if (!done[s][k]) add.push_back(k + " = " + v);
        out.insert(out.begin() + at, add.begin(), add.end());
    };
    for (const std::string& raw : lines) {
        std::string body = raw;
        auto hash = body.find_first_of("#;");
        std::string comment = hash == std::string::npos ? "" : body.substr(hash);
        if (hash != std::string::npos) body.resize(hash);
        std::string t = Trim(body);
        if (t.size() > 1 && t.front() == '[' && t.back() == ']') {
            flush(section);
            section = Trim(t.substr(1, t.size() - 2));
            sectionSeen[section] = true;
            out.push_back(raw);
            continue;
        }
        auto eq = t.find('=');
        if (eq != std::string::npos) {
            std::string key = Trim(t.substr(0, eq));
            auto s = in.find(section);
            if (s != in.end()) {
                auto k = s->second.find(key);
                if (k != s->second.end()) {
                    done[section][key] = true;
                    std::string indent = raw.substr(0, raw.find_first_not_of(" \t"));
                    out.push_back(indent + key + " = " + k->second + (comment.empty() ? "" : "  " + comment));
                    continue;
                }
            }
        }
        out.push_back(raw);
    }
    flush(section);
    for (auto& [s, keys] : in) {
        if (sectionSeen[s] || keys.empty()) continue;
        if (!out.empty() && !Trim(out.back()).empty()) out.push_back("");
        if (!s.empty()) out.push_back("[" + s + "]");
        for (auto& [k, v] : keys) out.push_back(k + " = " + v);
    }
    std::string r;
    for (auto& l : out) r += l + "\n";
    return r;
}

// ---------------------------------------------------------------------------------------------------------
// Old share codes (manager 0.1 and 0.2), still read: "CRAB1-" + base64url(CRC32 + "<ini file name>\n" + compact settings)

inline uint32_t Crc32(const std::string& data) {
    uint32_t c = 0xFFFFFFFF;
    for (unsigned char b : data) {
        c ^= b;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1)));
    }
    return ~c;
}

inline constexpr const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

inline std::string Base64Url(const std::string& in) {
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        uint32_t n = uint8_t(in[i]) << 16 | uint8_t(in[i + 1]) << 8 | uint8_t(in[i + 2]);
        for (int s = 18; s >= 0; s -= 6) out += kB64[(n >> s) & 63];
    }
    if (i + 1 == in.size()) {
        uint32_t n = uint8_t(in[i]) << 16;
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
    } else if (i + 2 == in.size()) {
        uint32_t n = uint8_t(in[i]) << 16 | uint8_t(in[i + 1]) << 8;
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += kB64[(n >> 6) & 63];
    }
    return out;
}

inline std::optional<std::string> FromBase64Url(const std::string& in) {
    std::string out;
    uint32_t n = 0;
    int bits = 0;
    for (char c : in) {
        const char* p = strchr(kB64, c);
        if (!p || !c) return std::nullopt;
        n = n << 6 | uint32_t(p - kB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += char((n >> bits) & 0xFF);
        }
    }
    return out;
}

inline constexpr const char kCodePrefix[] = "CRAB1-";

inline std::string MakeShareCode(const std::string& iniName, const std::string& iniText) {
    std::string payload = iniName + "\n" + CompactIni(iniText);
    uint32_t crc = Crc32(payload);
    std::string raw;
    for (int k = 0; k < 4; k++) raw += char(crc >> (8 * k));
    return kCodePrefix + Base64Url(raw + payload);
}

struct SharedSettings {
    std::string iniName;   // "CrabMods.ini"
    std::string settings;  // compact ini
};

// Reads a share code, ignoring spaces and line breaks pasted along with it. Nothing for a damaged code.
inline std::optional<SharedSettings> ReadShareCode(const std::string& code) {
    std::string c;
    for (char ch : code)
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') c += ch;
    size_t plen = strlen(kCodePrefix);
    if (c.compare(0, plen, kCodePrefix) != 0) return std::nullopt;
    auto raw = FromBase64Url(c.substr(plen));
    if (!raw || raw->size() < 5) return std::nullopt;
    uint32_t crc = 0;
    for (int k = 0; k < 4; k++) crc |= uint32_t(uint8_t((*raw)[k])) << (8 * k);
    std::string payload = raw->substr(4);
    if (Crc32(payload) != crc) return std::nullopt;
    auto nl = payload.find('\n');
    if (nl == std::string::npos) return std::nullopt;
    return SharedSettings{payload.substr(0, nl), payload.substr(nl + 1)};
}

// ---------------------------------------------------------------------------------------------------------
// The catalog: every mod the manager shows as something to install. CrabMods' items come from the DLL itself
// (its CrabModsCatalog export, JSON), CrabSync is one entry made from its default ini (CrabSyncDefaultIni).
// A setting's kind, choices and range are read from the comments the mods write above it.

struct Setting {
    enum Type { Bool, Int, Float, Choice, Text };
    std::string key, def, help;       // help: the comment above the key ("" when it shares the one above)
    Type type = Text;
    std::vector<std::string> choices;  // Choice
    bool range = false;                // "0 to 99" in the help
    double lo = 0, hi = 0;
};

struct CatalogItem {
    std::string mod;      // "CrabMods" or "CrabSync"
    std::string section;  // its [section] in CrabMods.ini; "" for CrabSync, whose ini has none
    std::string name, kind, rarity, about;
    std::string ini;      // the default settings, as in a new ini (no section header)
    bool toggle = false;  // it has "enabled", so it can be installed and uninstalled on its own
    std::vector<Setting> settings;  // all but "enabled"
};

inline bool IsNumber(const std::string& s, bool* integer = nullptr) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size()) return false;
    if (integer) *integer = s.find_first_of(".eE") == std::string::npos;
    return true;
}

// The words of a list like "common, epic, legendary or greed" in `text` that has `value` in it.
inline std::vector<std::string> ListAround(const std::string& text, const std::string& value) {
    // Split into phrases at sentence punctuation, then a phrase that is only "w, w, w or w".
    std::string phrase;
    auto check = [&](std::string p) -> std::vector<std::string> {
        std::vector<std::string> words;
        for (size_t i; (i = p.find(" or ")) != std::string::npos;) p.replace(i, 4, ",");
        std::istringstream in(p);
        for (std::string w; std::getline(in, w, ',');) {
            w = Trim(w);
            if (w.empty() || w.find(' ') != std::string::npos) {
                // "Which chests it drops from: common": only the last word of the first part counts.
                if (!words.empty() || w.empty()) return {};
                w = w.substr(w.rfind(' ') + 1);
            }
            words.push_back(w);
        }
        if (words.size() < 2 || std::find(words.begin(), words.end(), value) == words.end()) return {};
        return words;
    };
    for (char c : text + ".") {
        if (c == '.' || c == ':' || c == ';' || c == '(' || c == ')' || c == '\n') {
            if (auto w = check(phrase); !w.empty()) return w;
            phrase.clear();
        } else {
            phrase += c;
        }
    }
    return {};
}

// A setting's kind from its default value and its comment.
inline void ClassifySetting(Setting& s, const std::string& groupHelp) {
    const std::string& help = s.help.empty() ? groupHelp : s.help;
    bool integer = false;
    if (s.def == "true" || s.def == "false") {
        s.type = Setting::Bool;
    } else if (IsNumber(s.def, &integer)) {
        s.type = integer ? Setting::Int : Setting::Float;
        // "0 to 99", "(1 to 8)"
        for (size_t i = help.find(" to "); i != std::string::npos; i = help.find(" to ", i + 4)) {
            size_t a = help.rfind(' ', i - 1);
            a = a == std::string::npos ? 0 : a + 1;
            size_t b = help.find_first_of(" .,)", i + 4);
            std::string lo = help.substr(a, i - a), hi = help.substr(i + 4, b == std::string::npos ? b : b - i - 4);
            if (!lo.empty() && lo[0] == '(') lo.erase(0, 1);
            if (IsNumber(lo) && IsNumber(hi)) {
                s.range = true;
                s.lo = std::strtod(lo.c_str(), nullptr);
                s.hi = std::strtod(hi.c_str(), nullptr);
                break;
            }
        }
    } else {
        // "#   split = ..." lines, one per choice
        std::vector<std::string> choices;
        std::istringstream in(help);
        for (std::string line; std::getline(in, line);) {
            if (line.size() < 2 || line[0] != ' ') continue;
            auto eq = line.find(" = ");
            std::string w = Trim(line.substr(0, eq == std::string::npos ? 0 : eq));
            if (!w.empty() && w.find(' ') == std::string::npos) choices.push_back(w);
        }
        if (std::find(choices.begin(), choices.end(), s.def) == choices.end()) choices = ListAround(help, s.def);
        if (s.key == "rarity" && choices.empty()) choices = {"common", "epic", "legendary", "greed"};
        if (!choices.empty()) {
            s.type = Setting::Choice;
            s.choices = choices;
        }
    }
}

// Reads a default ini body into `about` (the first comment, without its "Name, a perk: " start), the
// "enabled" toggle and the settings.
inline void ParseDefaults(CatalogItem& it, const std::string& ini) {
    it.ini = ini;
    std::string comment, groupHelp;
    bool first = true;
    std::istringstream in(ini);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = Trim(line);
        if (t.empty()) {
            if (first && !comment.empty()) {
                it.about = comment;
                first = false;
            }
            comment.clear();
            continue;
        }
        if (t[0] == '#' || t[0] == ';') {
            std::string c = line.substr(line.find_first_of("#;") + 1);
            if (!c.empty() && c[0] == ' ') c.erase(0, 1);
            comment += (comment.empty() ? "" : "\n") + c;
            continue;
        }
        auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        Setting s;
        s.key = Trim(t.substr(0, eq));
        s.def = Trim(t.substr(eq + 1));
        if (auto h = s.def.find_first_of("#;"); h != std::string::npos) s.def = Trim(s.def.substr(0, h));
        if (first) {
            it.about = comment;
            first = false;
            comment.clear();
        }
        if (s.key == "enabled") {
            it.toggle = true;
            comment.clear();
            continue;
        }
        s.help = comment;
        if (!comment.empty()) groupHelp = comment;
        comment.clear();
        ClassifySetting(s, groupHelp);
        it.settings.push_back(s);
    }
    if (first) it.about = comment;
    // "Hoarder, a perk: more damage ..." -> "More damage ...". Line breaks in it were only for the ini.
    std::string& a = it.about;
    if (size_t c = a.find(": "); c != std::string::npos && c < 70 && a.find('.') > c) a.erase(0, c + 2);
    for (char& ch : a)
        if (ch == '\n') ch = ' ';
    if (!a.empty() && a[0] >= 'a' && a[0] <= 'z') a[0] = char(a[0] - 'a' + 'A');
}

struct Catalog {
    std::string header;  // the start of a new CrabMods.ini
    std::vector<CatalogItem> items;
};

inline std::optional<Catalog> ParseCatalog(const std::string& json) {
    auto root = JsonParser(json).Parse();
    if (!root || root->type != Json::Object) return std::nullopt;
    const Json* items = root->Get("items");
    if (!items || items->type != Json::Array) return std::nullopt;
    Catalog c;
    c.header = root->Str("header");
    for (const Json& j : items->items) {
        CatalogItem it;
        it.mod = "CrabMods";
        it.section = j.Str("section");
        it.name = j.Str("name");
        it.kind = j.Str("kind");
        it.rarity = j.Str("rarity");
        if (it.section.empty()) continue;
        ParseDefaults(it, j.Str("ini"));
        c.items.push_back(std::move(it));
    }
    return c;
}

// CrabSync's catalog file: {"ini": "<its default CrabSync.ini>"}.
inline std::optional<std::string> ParseSyncCatalog(const std::string& json) {
    auto root = JsonParser(json).Parse();
    if (!root || root->type != Json::Object || !root->Get("ini")) return std::nullopt;
    return root->Str("ini");
}

inline std::string JsonString(const std::string& s) {
    static const char hex[] = "0123456789abcdef";
    std::string r = "\"";
    for (unsigned char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 0x20) r += std::string("\\u00") + hex[c >> 4] + hex[c & 15];
        else r += char(c);
    }
    return r + "\"";
}

inline CatalogItem CrabSyncItem(const std::string& defaultIni) {
    CatalogItem it;
    it.mod = "CrabSync";
    it.name = "CrabSync";
    it.kind = "Multiplayer";
    ParseDefaults(it, defaultIni);
    it.toggle = false;  // installed or not: its "enabled" stays a setting of the DLL
    return it;
}

// A new CrabMods.ini with every default, as CrabMods itself writes one.
inline std::string DefaultCrabModsIni(const Catalog& c) {
    std::string t = c.header;
    for (const CatalogItem& it : c.items) t += "\n[" + it.section + "]\n" + it.ini;
    return t;
}

// The value a setting has in an ini file (its default when the file lacks it).
inline std::string ValueOf(const IniValues& v, const std::string& section, const Setting& s) {
    auto sec = v.find(section);
    if (sec == v.end()) return s.def;
    auto k = sec->second.find(s.key);
    return k == sec->second.end() ? s.def : k->second;
}

inline bool ItemOn(const IniValues& v, const CatalogItem& it) {
    if (!it.toggle) return true;
    auto sec = v.find(it.section);
    if (sec == v.end()) return true;
    auto k = sec->second.find("enabled");
    return k == sec->second.end() || k->second == "true" || k->second == "1" || k->second == "yes" || k->second == "on";
}

// ---------------------------------------------------------------------------------------------------------
// Setup codes: "CRAB2-" + what differs from a full default install + "-" + a 4-digit check.
//   S             CrabSync is installed
//   M             CrabMods is installed, with every item...
//   -martyr       ...but this one
//   0 / +hoarder  (instead of M, when most are off) only these items
//   hoarder.damagePercent=5    a setting that isn't the default (S.key for CrabSync; true/false as 1/0)
// Joined by ";". "CRAB2-S;M-xxxx" is everything installed with the default settings.

struct Setup {
    bool crabSync = false, crabMods = false;
    std::map<std::string, bool> on;  // CrabMods item section -> installed
    std::map<std::string, std::map<std::string, std::string>> values;  // "S" or section -> key -> value
    int unknown = 0;  // parts of the code this catalog doesn't have (a newer version's items or settings)
};

inline constexpr const char kSetupPrefix[] = "CRAB2-";

inline std::string CodeEscape(const std::string& v) {
    static const char hex[] = "0123456789ABCDEF";
    std::string r;
    for (unsigned char c : v) {
        if (c == ';' || c == '%' || c == '=' || c <= ' ' || c >= 0x7F) r += std::string("%") + hex[c >> 4] + hex[c & 15];
        else r += char(c);
    }
    return r;
}
inline std::string CodeUnescape(const std::string& v) {
    std::string r;
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == '%' && i + 2 < v.size()) {
            r += char(std::strtol(v.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            r += v[i];
        }
    }
    return r;
}

inline std::string CodeCheck(const std::string& payload) {
    char b[8];
    std::snprintf(b, sizeof b, "%04x", unsigned(Crc32(payload) & 0xFFFF));
    return b;
}

inline std::string MakeSetupCode(const Catalog& cat, const CatalogItem* sync, const Setup& s) {
    std::vector<std::string> parts;
    if (s.crabSync) parts.push_back("S");
    auto settingParts = [&](const CatalogItem& it, const std::string& name) {
        auto v = s.values.find(name);
        for (const Setting& st : it.settings) {
            std::string val = st.def;
            if (v != s.values.end())
                if (auto k = v->second.find(st.key); k != v->second.end()) val = k->second;
            if (val == st.def) continue;
            if (st.type == Setting::Bool) val = val == "true" ? "1" : "0";
            parts.push_back(name + "." + st.key + "=" + CodeEscape(val));
        }
    };
    if (s.crabSync && sync) settingParts(*sync, "S");
    if (s.crabMods) {
        std::vector<std::string> off, onList;
        for (const CatalogItem& it : cat.items) {
            if (!it.toggle) continue;
            auto o = s.on.find(it.section);
            (o == s.on.end() || o->second ? onList : off).push_back(it.section);
        }
        if (off.size() > onList.size()) {
            parts.push_back("0");
            for (auto& x : onList) parts.push_back("+" + x);
        } else {
            parts.push_back("M");
            for (auto& x : off) parts.push_back("-" + x);
        }
        for (const CatalogItem& it : cat.items) settingParts(it, it.section);
    }
    std::string payload;
    for (auto& p : parts) payload += (payload.empty() ? "" : ";") + p;
    return kSetupPrefix + payload + "-" + CodeCheck(payload);
}

// Reads a setup code; nothing when it is damaged or cut short. Spaces and line breaks pasted with it are
// ignored. Every item and setting the code doesn't mention gets its default.
inline std::optional<Setup> ReadSetupCode(const Catalog& cat, const CatalogItem* sync, const std::string& text) {
    std::string c;
    for (char ch : text)
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') c += ch;
    size_t plen = std::strlen(kSetupPrefix);
    if (c.size() < plen + 5 || c.compare(0, plen, kSetupPrefix) != 0) return std::nullopt;
    size_t dash = c.rfind('-');
    if (dash < plen || c.size() - dash != 5) return std::nullopt;
    std::string payload = c.substr(plen, dash - plen);
    if (CodeCheck(payload) != c.substr(dash + 1)) return std::nullopt;
    Setup s;
    bool allOn = true;
    std::vector<std::pair<std::string, bool>> listed;
    std::istringstream in(payload);
    for (std::string p; std::getline(in, p, ';');) {
        if (p == "S") s.crabSync = true;
        else if (p == "M") s.crabMods = true, allOn = true;
        else if (p == "0") s.crabMods = true, allOn = false;
        else if (p.size() > 1 && (p[0] == '-' || p[0] == '+')) listed.push_back({p.substr(1), p[0] == '+'});
        else if (auto eq = p.find('='), dot = p.find('.'); eq != std::string::npos && dot < eq) {
            std::string name = p.substr(0, dot), key = p.substr(dot + 1, eq - dot - 1), val = CodeUnescape(p.substr(eq + 1));
            const CatalogItem* it = name == "S" ? sync : nullptr;
            for (const CatalogItem& i : cat.items)
                if (i.section == name) it = &i;
            const Setting* st = nullptr;
            if (it)
                for (const Setting& x : it->settings)
                    if (x.key == key) st = &x;
            if (!st) {
                s.unknown++;
                continue;
            }
            if (st->type == Setting::Bool) val = val == "1" || val == "true" ? "true" : "false";
            s.values[name][key] = val;
        } else {
            s.unknown++;
        }
    }
    for (const CatalogItem& it : cat.items)
        if (it.toggle) s.on[it.section] = allOn;
    for (auto& [section, on] : listed) {
        if (s.on.count(section)) s.on[section] = on;
        else s.unknown++;
    }
    return s;
}

// The compact ini of a setup for one mod: every setting (defaults included) and, for CrabMods, every item's
// "enabled", to merge into the mod's own file.
inline std::string SetupIni(const Setup& s, const std::vector<const CatalogItem*>& items) {
    std::string out;
    for (const CatalogItem* it : items) {
        std::string name = it->mod == "CrabSync" ? "S" : it->section;
        if (!it->section.empty()) out += "[" + it->section + "]\n";
        if (it->toggle) {
            auto o = s.on.find(it->section);
            out += std::string("enabled = ") + (o == s.on.end() || o->second ? "true" : "false") + "\n";
        }
        auto v = s.values.find(name);
        for (const Setting& st : it->settings) {
            std::string val = st.def;
            if (v != s.values.end())
                if (auto k = v->second.find(st.key); k != v->second.end()) val = k->second;
            out += st.key + " = " + val + "\n";
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------
// Steam's libraryfolders.vdf: the "path" of every library

inline std::vector<std::string> SteamLibraries(const std::string& vdf) {
    std::vector<std::string> paths;
    size_t i = 0;
    while ((i = vdf.find("\"path\"", i)) != std::string::npos) {
        i += 6;
        size_t a = vdf.find('"', i);
        if (a == std::string::npos) break;
        std::string p;
        size_t j = a + 1;
        for (; j < vdf.size() && vdf[j] != '"'; j++) {
            if (vdf[j] == '\\' && j + 1 < vdf.size()) j++;  // "C:\\Program Files" is escaped in the file
            p += vdf[j];
        }
        paths.push_back(p);
        i = j + 1;
    }
    return paths;
}

// ---------------------------------------------------------------------------------------------------------
// Crash reports: the game's crash folder and the mod logs, with what could identify the player taken out,
// zipped, and summed up for a GitHub issue opened in the browser (prefilled through the address).

inline constexpr const char kAppId[] = "774801";  // Crab Champions on Steam

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }

// Every case-insensitive occurrence of `from` (at least 3 characters) replaced with `to`.
inline std::string ReplaceNoCase(const std::string& s, const std::string& from, const std::string& to) {
    if (from.size() < 3) return s;
    std::string r;
    size_t i = 0;
    while (i < s.size()) {
        bool hit = i + from.size() <= s.size();
        for (size_t k = 0; hit && k < from.size(); k++) hit = Lower(s[i + k]) == Lower(from[k]);
        if (hit) {
            r += to;
            i += from.size();
        } else {
            r += s[i++];
        }
    }
    return r;
}

// "C:\Users\Name\..." and "/home/name/..." with the name replaced by "<user>" (Proton's shared "steamuser",
// "Public" and "Default" are left alone).
inline std::string ScrubHomes(const std::string& s) {
    auto sep = [](char c) { return c == '\\' || c == '/'; };
    std::string r;
    size_t i = 0;
    while (i < s.size()) {
        r += s[i];
        if (!sep(s[i++])) continue;
        for (const char* dir : {"users", "home"}) {
            size_t n = std::strlen(dir);
            if (i + n >= s.size() || !sep(s[i + n])) continue;
            bool hit = true;
            for (size_t k = 0; hit && k < n; k++) hit = Lower(s[i + k]) == dir[k];
            if (!hit) continue;
            size_t a = i + n + 1, b = a;
            while (b < s.size() && !sep(s[b]) && s[b] != '"' && s[b] != '<' && s[b] != '\n' && s[b] != '\r') b++;
            std::string low;
            for (size_t k = a; k < b; k++) low += Lower(s[k]);
            if (low.empty() || low == "steamuser" || low == "public" || low == "default") break;
            r += s.substr(i, n + 1) + "<user>";
            i = b;
            break;
        }
    }
    return r;
}

// The player names in CrabSync.log: its lines about a player start with the name ("[time] Name: gained ...").
inline std::vector<std::string> PlayerNames(const std::string& log) {
    static const char* const notNames[] = {"hook ", "back in the lobby", "F9", "the host", "run started", "map loaded"};
    std::vector<std::string> names;
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        size_t a = line.find("] ");
        if (line.empty() || line[0] != '[' || a == std::string::npos) continue;
        a += 2;
        while (a < line.size() && line[a] == ' ') a++;
        size_t b = line.find(": ", a);
        if (b == std::string::npos || b == a) continue;
        std::string name = line.substr(a, b - a);
        bool skip = name.find(" @ ") != std::string::npos;
        for (const char* n : notNames) skip |= name.rfind(n, 0) == 0;
        if (!skip && std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    }
    return names;
}

// Takes out the user's folders, the given secrets (user and computer name) and the player names, which become
// "Player 1", "Player 2", ... in the order given, so every file of one report uses the same numbers.
inline std::string Scrub(std::string s, const std::vector<std::string>& secrets, const std::vector<std::string>& players) {
    s = ScrubHomes(s);
    std::vector<size_t> order(players.size());  // longest name first, so "Bob" doesn't break up "Bobby"
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return players[a].size() > players[b].size(); });
    for (size_t n : order) {
        const std::string& name = players[n];
        if (name.empty()) continue;
        std::string to = "Player " + std::to_string(n + 1), r;
        size_t i = 0;
        for (size_t j; (j = s.find(name, i)) != std::string::npos; i = j + name.size()) r += s.substr(i, j - i) + to;
        s = r + s.substr(i);
    }
    for (const auto& secret : secrets) s = ReplaceNoCase(s, secret, "<user>");
    return s;
}

// The text of <tag>...</tag> in UE's CrashContext.runtime-xml, or "".
inline std::string XmlValue(const std::string& xml, const std::string& tag) {
    size_t a = xml.find("<" + tag + ">");
    if (a == std::string::npos) return {};
    a += tag.size() + 2;
    size_t b = xml.find("</" + tag + ">", a);
    return b == std::string::npos ? std::string() : Trim(xml.substr(a, b - a));
}

// The crash context with the ids that follow a machine or an account emptied.
inline std::string StripCrashIds(std::string xml) {
    for (const char* tag : {"MachineId", "LoginId", "EpicAccountId", "UserName", "UserDescription"}) {
        std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
        size_t a = xml.find(open);
        size_t b = a == std::string::npos ? a : xml.find(close, a);
        if (b != std::string::npos) xml.erase(a + open.size(), b - a - open.size());
    }
    return xml;
}

// "CrabSync                      0x00006ffffb540000 + 1b10" -> "CrabSync+0x1b10", a frame a line.
inline std::string CompactCallStack(const std::string& pcallstack, int maxFrames = 25) {
    std::istringstream in(pcallstack);
    std::string line, r;
    int frames = 0;
    while (std::getline(in, line) && frames < maxFrames) {
        std::istringstream w(line);
        std::string module, base, plus, offset;
        if (!(w >> module >> base >> plus >> offset) || plus != "+") continue;
        r += module + "+0x" + offset + "\n";
        frames++;
    }
    return r;
}

// The last `n` lines of `text`, without the final line break.
inline std::string LastLines(const std::string& text, int n) {
    if (n <= 0) return {};
    size_t end = text.size();
    while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) end--;
    size_t start = end;
    for (int k = 0; k < n && start > 0; k++) {
        size_t nl = text.rfind('\n', start - 1);
        start = nl == std::string::npos ? 0 : nl;
    }
    if (start < end && text[start] == '\n') start++;
    return text.substr(start, end - start);
}

inline std::string UrlEncode(const std::string& s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string r;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~')
            r += char(c);
        else
            r += std::string("%") + hex[c >> 4] + hex[c & 15];
    }
    return r;
}

struct CrashReport {
    bool crashed = false;  // false: logs collected by hand
    std::string when;      // "2026-10-11 14:03"
    std::string platform;  // "Windows" or "Linux (Proton)"
    std::string os;        // from the crash context
    std::string versions;  // "CrabMods 0.8.0, CrabSync 1.0.4, Manager 0.2.0"
    std::string error;     // "EXCEPTION_ACCESS_VIOLATION reading address 0x..."
    std::string callStack; // CompactCallStack
    int secondsIn = -1;    // how long the game had run
    std::string crabModsLog, crabSyncLog;  // scrubbed
    std::string zipName;
    std::string note;      // anything else to know
};

// Reads what the report shows from a crash context.
inline void ReadCrashContext(CrashReport& r, const std::string& xml) {
    r.crashed = true;
    r.error = XmlValue(xml, "ErrorMessage");
    if (r.error.rfind("Unhandled Exception: ", 0) == 0) r.error.erase(0, 21);
    r.callStack = CompactCallStack(XmlValue(xml, "PCallStack"));
    r.os = XmlValue(xml, "Misc.OSVersionMajor");
    std::string s = XmlValue(xml, "SecondsSinceStart");
    r.secondsIn = s.empty() ? -1 : std::atoi(s.c_str());
}

inline std::string IssueTitle(const CrashReport& r) {
    if (!r.crashed) return "Problem: ";
    std::string t = "Crash: " + (r.error.empty() ? std::string("the game closed") : r.error);
    if (t.size() > 90) t = t.substr(0, 87) + "...";
    std::string first = r.callStack.substr(0, r.callStack.find('\n'));
    if (!first.empty()) t += " at " + first;
    return t;
}

// The issue's text, with `logLines` lines from the end of each mod log (fewer make a shorter address).
inline std::string IssueBody(const CrashReport& r, int logLines) {
    auto code = [](std::string s) {  // a code block can't contain its own fence
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        for (size_t i; (i = s.find("```")) != std::string::npos;) s.replace(i, 3, "'''");
        return s;
    };
    std::string b = r.crashed ? "**What were you doing when it crashed?**\n\n\n\n" : "**What went wrong?**\n\n\n\n";
    if (r.crashed) {
        b += "**Crash**: `" + (r.error.empty() ? std::string("no error message") : r.error) + "`";
        if (r.secondsIn >= 0) b += " after " + std::to_string(r.secondsIn / 60) + " min";
        b += "\n";
        if (!r.callStack.empty()) b += "```\n" + code(r.callStack) + "```\n";
    }
    b += "**Versions**: " + r.versions + "\n";
    b += "**System**: " + r.platform + (r.os.empty() ? "" : ", " + r.os) + "\n";
    if (!r.when.empty()) b += "**When**: " + r.when + "\n";
    if (!r.note.empty()) b += "**Note**: " + r.note + "\n";
    for (auto [name, text] : {std::pair{"CrabMods.log", &r.crabModsLog}, std::pair{"CrabSync.log", &r.crabSyncLog}}) {
        std::string tail = LastLines(*text, logLines);
        if (tail.empty()) continue;
        b += "\n<details><summary>" + std::string(name) + " (last lines)</summary>\n\n```\n" + code(tail) +
             "\n```\n</details>\n";
    }
    if (!r.zipName.empty())
        b += "\n**Files**: drag `" + r.zipName + "` here (CrabMods Manager opened the folder it is in).\n";
    b += "\n_Made by CrabMods Manager. Player names, user folders and machine ids were taken out._\n";
    return b;
}

// The new-issue address, with as many log lines as fit (GitHub refuses addresses much over 8 KB).
inline std::string IssueUrl(const CrashReport& r, size_t maxLength = 7500) {
    std::string base = "https://github.com/" + std::string(kRepo) + "/issues/new?title=" + UrlEncode(IssueTitle(r));
    for (int lines : {40, 25, 15, 8, 3, 0}) {
        std::string url = base + "&body=" + UrlEncode(IssueBody(r, lines));
        if (url.size() <= maxLength) return url;
    }
    return base;
}

// A .zip with the files stored as they are (no compression), which every system can open.
inline std::string StoreZip(const std::vector<std::pair<std::string, std::string>>& files, uint16_t dosTime = 0,
                            uint16_t dosDate = (46 << 9) | (1 << 5) | 1) {
    auto u16 = [](std::string& o, uint32_t v) { o += char(v & 0xFF), o += char((v >> 8) & 0xFF); };
    auto u32 = [&](std::string& o, uint32_t v) { u16(o, v & 0xFFFF), u16(o, v >> 16); };
    std::string out, central;
    for (const auto& [name, data] : files) {
        uint32_t crc = Crc32(data), offset = uint32_t(out.size()), size = uint32_t(data.size());
        auto header = [&](std::string& o, bool isCentral) {
            u32(o, isCentral ? 0x02014b50 : 0x04034b50);
            if (isCentral) u16(o, 20);  // made by
            u16(o, 20);                 // version needed
            u16(o, 0x0800);             // UTF-8 names
            u16(o, 0);                  // stored
            u16(o, dosTime), u16(o, dosDate);
            u32(o, crc), u32(o, size), u32(o, size);
            u16(o, uint32_t(name.size())), u16(o, 0);
            if (isCentral) u16(o, 0), u16(o, 0), u16(o, 0), u32(o, 0), u32(o, offset);
            o += name;
        };
        header(out, false);
        out += data;
        header(central, true);
    }
    uint32_t at = uint32_t(out.size());
    out += central;
    u32(out, 0x06054b50), u16(out, 0), u16(out, 0);
    u16(out, uint32_t(files.size())), u16(out, uint32_t(files.size()));
    u32(out, uint32_t(central.size())), u32(out, at), u16(out, 0);
    return out;
}

// An RGB .png of RGBA pixels (stored, not compressed), for --screenshot.
inline std::string Png(int w, int h, const std::vector<uint8_t>& rgba) {
    auto be32 = [](std::string& o, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) o += char((v >> s) & 0xFF);
    };
    std::string raw;
    for (int y = 0; y < h; y++) {
        raw += '\0';
        for (int x = 0; x < w; x++) raw.append(reinterpret_cast<const char*>(&rgba[(size_t(y) * w + x) * 4]), 3);
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

// ---------------------------------------------------------------------------------------------------------
// Steam's launch options. On Linux the game needs WINEDLLOVERRIDES in them to load the mod loader; they are in
// Steam's userdata/<id>/config/localconfig.vdf, in the game's own "774801" { ... "LaunchOptions" "..." } block.

inline bool LaunchOptionInVdf(const std::string& vdf) {
    std::string id = std::string("\"") + kAppId + "\"";
    for (size_t i = vdf.find(id); i != std::string::npos; i = vdf.find(id, i + 1)) {
        size_t open = vdf.find('{', i), next = vdf.find('"', i + id.size());
        if (open == std::string::npos || (next != std::string::npos && next < open)) continue;
        int depth = 0;
        size_t end = open;
        for (; end < vdf.size(); end++) {
            if (vdf[end] == '{') depth++;
            if (vdf[end] == '}' && --depth == 0) break;
        }
        std::string block = vdf.substr(open, end - open);
        size_t lo = block.find("\"LaunchOptions\"");
        if (lo != std::string::npos && block.find("xinput1_3", lo) != std::string::npos) return true;
    }
    return false;
}

inline constexpr const char kLaunchOption[] = "WINEDLLOVERRIDES=\"xinput1_3=n,b\" %command%";

// ---------------------------------------------------------------------------------------------------------
// The manager's own settings, CrabModsManager.ini

struct Config {
    std::string game;             // the game's Win64 folder (UTF-8)
    std::string lastCrash;        // the crash folder last reported (or seen), so each crash is offered once
    bool setupDone = false;       // the first-run screen is done (or a manager before 0.4 was already used)
    std::optional<bool> autoMods;     // install mod updates without asking; unset until the player answers
    std::optional<bool> autoManager;  // update the manager itself without asking
    bool linuxHintDone = false;   // "I've set the launch option"
    int checkMinutes = 5;         // how often to look for updates; 0: never on its own
    bool offline = false;         // no network at all, except "Check now"
    std::map<std::string, std::string> skip;  // "CrabMods" -> a version the player skipped
    std::string lastRun;          // the manager version that ran last, to say "updated to ..." once
};

inline std::string ConfigText(const Config& c) {
    auto b = [](bool v) { return v ? "true" : "false"; };
    auto ob = [&](const std::optional<bool>& v) { return v ? b(*v) : "ask"; };
    std::string t = "# CrabMods Manager settings\n";
    t += "game=" + c.game + "\n";
    t += "lastCrash=" + c.lastCrash + "\n";
    t += std::string("setupDone=") + b(c.setupDone) + "\n";
    t += std::string("autoUpdateMods=") + ob(c.autoMods) + "\n";
    t += std::string("autoUpdateManager=") + ob(c.autoManager) + "\n";
    t += std::string("askedUpdates=") + b(c.autoMods.has_value() && c.autoManager.has_value()) + "\n";
    t += std::string("linuxHintDone=") + b(c.linuxHintDone) + "\n";
    t += "# minutes between update checks (0: only when you press Check now)\n";
    t += "checkMinutes=" + std::to_string(c.checkMinutes) + "\n";
    t += std::string("offline=") + b(c.offline) + "\n";
    for (auto& [mod, v] : c.skip)
        if (!v.empty()) t += "skip." + mod + "=" + v + "\n";
    t += "lastRun=" + c.lastRun + "\n";
    return t;
}

// Reads the ini, including one written by manager 0.3 (which always wrote both auto-update keys, so it has no
// "askedUpdates": its player is asked once).
inline Config ParseConfig(const std::string& text) {
    Config c;
    IniValues all = ReadIni(text);
    auto& v = all[""];
    auto flag = [&](const char* k, bool def) { return v.count(k) ? v[k] == "true" : def; };
    c.game = v["game"];
    c.lastCrash = v["lastCrash"];
    c.setupDone = flag("setupDone", !c.game.empty());  // a manager from before 0.3 was already used
    if (flag("askedUpdates", false)) {
        for (auto [key, out] : {std::pair{"autoUpdateMods", &c.autoMods}, std::pair{"autoUpdateManager", &c.autoManager}})
            if (v.count(key) && v[key] != "ask") *out = v[key] == "true";
    }
    c.linuxHintDone = flag("linuxHintDone", false);
    if (v.count("checkMinutes") && IsNumber(v["checkMinutes"])) c.checkMinutes = std::max(0, std::atoi(v["checkMinutes"].c_str()));
    c.offline = flag("offline", false);
    for (auto& [k, val] : v)
        if (k.rfind("skip.", 0) == 0 && !val.empty()) c.skip[k.substr(5)] = val;
    c.lastRun = v["lastRun"];
    return c;
}

// ---------------------------------------------------------------------------------------------------------
// Updates: what to do about a newer release

enum class UpdateAction {
    None,    // up to date, not installed, or this version was skipped
    Banner,  // tell the player and let them choose
    Auto,    // the player said to install updates without asking
};

inline UpdateAction DecideUpdate(const std::string& installed, const std::string& latest, const std::optional<bool>& autoOn,
                                 const std::string& skipped) {
    if (installed.empty() || latest.empty()) return UpdateAction::None;
    if (installed != "?" && CompareVersions(installed, latest) >= 0) return UpdateAction::None;
    if (!skipped.empty() && CompareVersions(skipped, latest) == 0) return UpdateAction::None;
    return autoOn.value_or(false) ? UpdateAction::Auto : UpdateAction::Banner;
}

// An HTTP answer, as the platform layer returns it.
struct HttpResponse {
    int status = 0;        // 0: no connection
    std::string body, etag, error;
    int64_t rateRemaining = -1;  // X-RateLimit-Remaining, -1 when not sent
    int64_t rateReset = 0;       // X-RateLimit-Reset, unix seconds
};

// When to ask GitHub for the release list again. Unchanged lists come back as 304 (they don't count against
// GitHub's 60 requests an hour); a used-up limit waits for its reset, failures back off up to an hour.
struct Poller {
    int64_t next = 0;  // unix seconds
    int failures = 0;

    bool Due(int64_t now, int minutes) const { return minutes > 0 && now >= next; }

    void Done(int64_t now, int minutes, const HttpResponse& r) {
        int64_t every = int64_t(std::max(1, minutes)) * 60;
        if (r.status == 200 || r.status == 304) {
            failures = 0;
            next = now + every;
        } else if ((r.status == 403 || r.status == 429) && r.rateRemaining == 0 && r.rateReset > now) {
            failures = 0;
            next = std::max(now + every, r.rateReset + 5);
        } else {
            failures = std::min(failures + 1, 8);
            next = now + std::min<int64_t>(3600, every << (failures - 1));
        }
    }
};

// What a check found, from a response and the cached list.
struct CheckResult {
    bool changed = false;   // a new list to act on
    bool failed = false;
    std::string body, etag;  // the list to keep (the cached one on 304)
    std::string error;
};

inline CheckResult ReadCheck(const HttpResponse& r, const std::string& cachedBody, const std::string& cachedEtag) {
    CheckResult c;
    if (r.status == 304 && !cachedBody.empty()) {
        c.body = cachedBody;
        c.etag = cachedEtag;
        return c;
    }
    if (r.status == 200 && ParseReleases(r.body).ok) {
        c.changed = r.body != cachedBody;
        c.body = r.body;
        c.etag = r.etag;
        return c;
    }
    c.failed = true;
    c.body = cachedBody;
    c.etag = cachedEtag;
    if (r.status == 0) c.error = r.error.empty() ? "no connection" : r.error;
    else if ((r.status == 403 || r.status == 429) && r.rateRemaining == 0) c.error = "GitHub's hourly limit is used up; trying again later";
    else if (r.status == 200) c.error = "GitHub sent something that isn't a release list";
    else c.error = "GitHub answered " + std::to_string(r.status);
    return c;
}

// ---------------------------------------------------------------------------------------------------------
// The mods list: "New" items, and sorting

// state/seen.json: item section -> the CrabMods version it first appeared in ("" for items there when the
// manager first ran, which are not new).
using Seen = std::map<std::string, std::string>;

inline Seen ParseSeen(const std::string& json) {
    Seen s;
    auto root = JsonParser(json).Parse();
    if (root && root->type == Json::Object)
        for (auto& [k, v] : root->fields)
            if (v.type == Json::String) s[k] = v.str;
    return s;
}

inline std::string SeenJson(const Seen& s) {
    std::string r = "{";
    for (auto& [k, v] : s) r += (r.size() > 1 ? ",\n " : "\n ") + JsonString(k) + ": " + JsonString(v);
    return r + "\n}\n";
}

// Notes the items of a catalog of `version`. True when something was added.
inline bool NoteSeen(Seen& s, const Catalog& cat, const std::string& version, bool firstRun) {
    bool added = false;
    for (const CatalogItem& it : cat.items)
        if (!s.count(it.section)) {
            s[it.section] = firstRun ? "" : version;
            added = true;
        }
    return added;
}

inline bool IsNew(const Seen& s, const std::string& section, const std::string& version) {
    auto it = s.find(section);
    return it != s.end() && !it->second.empty() && !version.empty() && CompareVersions(it->second, version) == 0;
}

struct Entry {
    std::string id, name, kind, rarity, about;
    bool installed = false;
    const CatalogItem* item = nullptr;  // nullptr for CrabMods and CrabSync themselves
    std::string since;                  // the version it first appeared in ("" if always there)
    bool isNew = false;
};

enum class SortBy { Type, Name, Rarity, Installed, Newest };
inline const char* const kSortNames[] = {"Type", "Name", "Rarity", "Installed first", "Newest"};
inline const char* const kKinds[] = {"Perk",      "Relic",  "Weapon", "Weapon mod", "Ability mod", "Melee mod",
                                     "Game mode", "Island", "Tweak"};

inline int KindRank(const std::string& k) {
    return int(std::find(std::begin(kKinds), std::end(kKinds), k) - std::begin(kKinds));
}
inline int RarityRank(const std::string& r) {
    static const char* const order[] = {"common", "epic", "legendary", "greed"};
    return int(std::find(std::begin(order), std::end(order), r) - std::begin(order));
}

// Sorts everything after the first `pinned` entries (CrabMods and CrabSync stay on top). Ties fall back to type,
// then name.
inline void SortEntries(std::vector<Entry>& e, SortBy by, bool descending, size_t pinned = 2) {
    if (e.size() <= pinned) return;
    auto key = [&](const Entry& a, const Entry& b) -> int {  // <0: a first
        auto cmp = [](auto x, auto y) { return x < y ? -1 : x > y ? 1 : 0; };
        switch (by) {
        case SortBy::Type: return cmp(KindRank(a.kind), KindRank(b.kind));
        case SortBy::Name: return cmp(a.name, b.name);
        case SortBy::Rarity: return cmp(RarityRank(a.rarity), RarityRank(b.rarity));
        case SortBy::Installed: return cmp(!a.installed, !b.installed);
        case SortBy::Newest: {
            int c = CompareVersions(a.since, b.since);
            return c ? -c : 0;  // newest first
        }
        }
        return 0;
    };
    std::stable_sort(e.begin() + pinned, e.end(), [&](const Entry& a, const Entry& b) {
        int k = key(a, b);
        if (k) return descending ? k > 0 : k < 0;
        if (KindRank(a.kind) != KindRank(b.kind)) return KindRank(a.kind) < KindRank(b.kind);
        return a.name < b.name;
    });
}

// ---------------------------------------------------------------------------------------------------------
// What a settings change does, in words: "Hoarder: damage percent 5 → 7", "Martyr: off". `compact` is merged into
// the file whose values are `now`; `names` turns a section into an item name.
inline std::vector<std::string> DescribeSettings(const IniValues& now, const std::string& compact,
                                                 const std::map<std::string, std::string>& names,
                                                 const std::map<std::string, std::map<std::string, std::string>>& defaults = {}) {
    std::vector<std::string> out;
    for (auto& [section, keys] : ReadIni(compact)) {
        auto n = names.find(section);
        std::string who = n != names.end() ? n->second : section.empty() ? "" : section;
        for (auto& [k, v] : keys) {
            std::string old;
            auto s = now.find(section);
            if (s != now.end())
                if (auto kv = s->second.find(k); kv != s->second.end()) old = kv->second;
            if (old.empty())
                if (auto d = defaults.find(section); d != defaults.end())
                    if (auto kv = d->second.find(k); kv != d->second.end()) old = kv->second;
            if (k == "enabled" && old.empty()) old = "true";
            if (old == v) continue;
            std::string prefix = who.empty() ? "" : who + ": ";
            if (k == "enabled") out.push_back(prefix + (v == "true" ? "turned on" : "turned off"));
            else out.push_back(prefix + k + " " + (old.empty() ? "(default)" : old) + " \xE2\x86\x92 " + v);
        }
    }
    return out;
}

}  // namespace mgr
