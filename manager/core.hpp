// CrabMods Manager: everything that is not the window. Release lists, versions, share codes, ini merging.
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

}  // namespace mgr
