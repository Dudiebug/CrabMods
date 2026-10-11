// CrabMods.ini: [section] headers, key=value lines, '#' or ';' comments. Each item reads its own [section];
// missing keys keep their defaults. Sections for items added in an update are appended to an existing file.
#pragma once
#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "item.hpp"

namespace crabmods {

inline constexpr const char kIniHeader[] = R"(# CrabMods: new items for Crab Champions.
# Every player needs CrabMods installed. In multiplayer the host's settings are used by everyone.
)";

inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Items in ini order.
inline std::vector<Item*> SortedItems() {
    std::vector<Item*> v = Items();
    std::stable_sort(v.begin(), v.end(), [](Item* a, Item* b) { return a->Order() < b->Order(); });
    return v;
}

// Applies an ini text to the items and returns the sections it has. Keys before any section belong to
// [arsenal] (the old Arsenal.ini had no sections). Bad values are skipped, and so are the players' own settings
// (Item::LocalSetting) when keepLocal is set.
inline std::set<std::string> ApplyIni(const std::string& text, bool keepLocal = false) {
    std::set<std::string> sections;
    std::istringstream f(text);
    std::string line, section = "arsenal";
    Item* item = FindItem(section);
    while (std::getline(f, line)) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        line = Trim(line);
        if (line.size() > 1 && line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            sections.insert(section);
            item = FindItem(section);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos || !item) continue;
        std::string key = Trim(line.substr(0, eq));
        if (keepLocal && item->LocalSetting(key)) continue;
        try {
            item->Set(key, Trim(line.substr(eq + 1)));
        } catch (...) {
        }
    }
    return sections;
}

inline std::string SectionText(Item* item) { return std::string("[") + item->Section() + "]\n" + item->DefaultIni(); }

// The settings alone ("[section]" and "key = value" lines), as the host sends them to the other players.
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

// Replaces every setting with the host's: defaults first, then the given ini (so anything it leaves out is the
// default, not whatever was set before). Players' own settings (keybinds) stay as this machine's ini has them.
inline void ApplyAllSettings(const std::string& text) {
    std::string defaults;
    for (Item* item : SortedItems()) defaults += SectionText(item);
    ApplyIni(defaults, true);
    ApplyIni(text, true);
}

struct LoadResult {
    std::string text;           // the ini as written on disk
    bool created = false;       // there was no CrabMods.ini
    bool migrated = false;      // Arsenal settings were taken from an old Arsenal.ini
    std::string added;          // sections appended to an existing file, "minimalist, underdog"
};

// Reads the ini into the items, creating it with the defaults if it does not exist yet. A first launch next
// to an old Arsenal.ini carries its settings over into [arsenal].
inline LoadResult LoadConfig(const std::wstring& path, const std::wstring& oldArsenalIni) {
    LoadResult r;
    std::ifstream f(path.c_str(), std::ios::binary);
    if (f) {
        std::stringstream ss;
        ss << f.rdbuf();
        r.text = ss.str();
        std::set<std::string> have = ApplyIni(r.text);
        std::string append;
        for (Item* item : SortedItems()) {
            if (have.count(item->Section())) continue;
            append += "\n" + SectionText(item);
            r.added += (r.added.empty() ? "" : ", ") + std::string(item->Section());
        }
        if (!append.empty()) {
            if (!r.text.empty() && r.text.back() != '\n') r.text += "\n";
            r.text += append;
            ApplyIni(append);
            std::ofstream(path.c_str(), std::ios::binary) << r.text;
        }
        return r;
    }
    r.text = kIniHeader;
    for (Item* item : SortedItems()) {
        std::string body = item->DefaultIni();
        if (std::string(item->Section()) == "arsenal") {
            if (std::ifstream old(oldArsenalIni.c_str()); old) {
                // Keep Arsenal's heading comment, then the old file's lines minus its own header.
                std::stringstream ss;
                ss << old.rdbuf();
                std::string kept = body.substr(0, body.find('\n') + 1), line;
                for (std::istringstream in(ss.str()); std::getline(in, line);) {
                    if (line.rfind("# Arsenal:", 0) == 0 || line.rfind("# Every player", 0) == 0) continue;
                    kept += line + "\n";
                }
                body = Trim(kept) + "\n";
                r.migrated = true;
            }
        }
        r.text += "\n[" + std::string(item->Section()) + "]\n" + body;
    }
    ApplyIni(r.text);
    std::ofstream(path.c_str(), std::ios::binary) << r.text;
    r.created = true;
    return r;
}

// What CrabMods Manager shows: every item with its name, kind, default rarity and default settings, as JSON
// ({"header": "...", "items": [{"section", "name", "kind", "rarity", "ini"}, ...]}).
inline std::string CatalogJson() {
    auto str = [](const std::string& s) {
        std::string r = "\"";
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') r += '\\', r += char(c);
            else if (c == '\n') r += "\\n";
            else if (c < 0x20) r += ' ';
            else r += char(c);
        }
        return r + "\"";
    };
    std::string j = "{\"header\":" + str(kIniHeader) + ",\"items\":[";
    bool first = true;
    for (Item* i : SortedItems()) {
        j += first ? "{" : ",{";
        first = false;
        j += "\"section\":" + str(i->Section()) + ",\"name\":" + str(i->Title()) + ",\"kind\":" + str(i->CatalogKind()) +
             ",\"rarity\":" + str(i->CatalogRarity()) + ",\"ini\":" + str(i->DefaultIni()) + "}";
    }
    return j + "]}";
}

}  // namespace crabmods
