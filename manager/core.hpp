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

struct Release {
    std::string tag, version;                   // "crabmods-v0.3.0", "0.3.0"
    std::map<std::string, std::string> assets;  // file name -> download URL
};

// The newest published release of each mod in a GitHub /releases response, keyed by mod name. Drafts and
// pre-releases are skipped.
inline std::map<std::string, Release> LatestReleases(const std::string& json) {
    std::map<std::string, Release> latest;
    auto root = JsonParser(json).Parse();
    if (!root || root->type != Json::Array) return latest;
    for (const Json& r : root->items) {
        if (r.type != Json::Object || r.Flag("draft") || r.Flag("prerelease")) continue;
        std::string tag = r.Str("tag_name");
        for (const ModInfo& m : Mods()) {
            std::string prefix = m.tagPrefix;
            if (tag.rfind(prefix, 0) != 0) continue;
            Release rel{tag, tag.substr(prefix.size()), {}};
            if (const Json* assets = r.Get("assets"); assets && assets->type == Json::Array)
                for (const Json& a : assets->items)
                    if (a.type == Json::Object) rel.assets[a.Str("name")] = a.Str("browser_download_url");
            // A release without the raw DLL (older ones only had a zip) cannot be installed from.
            if (!rel.assets.count(m.dll)) continue;
            auto it = latest.find(m.name);
            if (it == latest.end() || CompareVersions(rel.version, it->second.version) > 0) latest[m.name] = rel;
        }
    }
    return latest;
}

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
// Share codes: "CRAB1-" + base64url(CRC32 + "<ini file name>\n" + compact settings)

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

}  // namespace mgr
