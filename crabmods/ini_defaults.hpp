// Moving a player's CrabMods.ini to new defaults after an update. A setting still at the default it had when the
// ini was last written takes the new default; a setting the player changed stays as it is.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace crabmods {

inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Every "key = value" in an ini text, by [section] and key. Keys before any section belong to [arsenal].
using IniValues = std::map<std::pair<std::string, std::string>, std::string>;

inline IniValues ParseValues(const std::string& text) {
    IniValues v;
    std::string section = "arsenal", line;
    for (std::istringstream f(text); std::getline(f, line);) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        line = Trim(line);
        if (line.size() > 1 && line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        auto eq = line.find('=');
        if (eq != std::string::npos) v[{section, Trim(line.substr(0, eq))}] = Trim(line.substr(eq + 1));
    }
    return v;
}

// The same setting written two ways: "10" and "10.0", "Legendary" and "legendary".
inline bool SameValue(const std::string& a, const std::string& b) {
    char *ea, *eb;
    double x = std::strtod(a.c_str(), &ea), y = std::strtod(b.c_str(), &eb);
    if (!a.empty() && !b.empty() && !*ea && !*eb) return std::fabs(x - y) <= 1e-6 * std::max(1.0, std::fabs(x));
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char p, char q) { return std::tolower((unsigned char)p) == std::tolower((unsigned char)q); });
}

struct DefaultsUpdate {
    std::string text;                  // the ini with the new defaults
    std::vector<std::string> changed;  // "serratedLinks.damagePercent 50 -> 10"
};

// Moves every setting at its old default (`was`) to its new one (`now`). A section where the player changed
// nothing is rewritten whole from `sectionDefault` (its comments describe the new numbers too); elsewhere only
// those lines change. `sectionDefault` gives a section's default text without its header, or "" if unknown.
inline DefaultsUpdate UpdateDefaults(const std::string& text, const IniValues& was, const IniValues& now,
                                     const std::function<std::string(const std::string&)>& sectionDefault) {
    std::vector<std::string> lines;
    for (size_t at = 0; at <= text.size();) {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos) nl = text.size();
        lines.push_back(text.substr(at, nl - at));
        at = nl + 1;
    }
    // [first, end) of each section; the lines before the first header are [arsenal] without a header.
    struct Block {
        std::string section;
        size_t first, end;
        bool header;
    };
    std::vector<Block> blocks{{"arsenal", 0, lines.size(), false}};
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = Trim(lines[i]);
        if (t.size() > 1 && t.front() == '[' && t.back() == ']') {
            blocks.back().end = i;
            blocks.push_back({Trim(t.substr(1, t.size() - 2)), i, lines.size(), true});
        }
    }

    DefaultsUpdate u;
    std::vector<std::string> out;
    auto find = [](const IniValues& m, const std::string& s, const std::string& k) -> const std::string* {
        auto it = m.find({s, k});
        return it == m.end() ? nullptr : &it->second;
    };
    for (const Block& b : blocks) {
        struct Setting {
            size_t line;
            std::string key, value;
        };
        std::vector<Setting> settings;
        for (size_t i = b.first + (b.header ? 1 : 0); i < b.end; i++) {
            std::string l = lines[i];
            auto hash = l.find_first_of("#;");
            if (hash != std::string::npos) l.resize(hash);
            auto eq = l.find('=');
            if (eq != std::string::npos) settings.push_back({i, Trim(l.substr(0, eq)), Trim(l.substr(eq + 1))});
        }
        std::map<size_t, std::string> move;  // line -> its new value
        bool custom = false;
        for (const Setting& s : settings) {
            const std::string *old = find(was, b.section, s.key), *nu = find(now, b.section, s.key);
            bool atOld = old && SameValue(s.value, *old);
            if (atOld && nu && !SameValue(*old, *nu)) {
                move[s.line] = *nu;
                u.changed.push_back(b.section + "." + s.key + " " + s.value + " -> " + *nu);
            } else if (!atOld && !(nu && SameValue(s.value, *nu))) {
                custom = true;
            }
        }
        std::string whole = b.header && !move.empty() && !custom ? sectionDefault(b.section) : "";
        if (!whole.empty()) {
            // The header, the new default text, then the blank lines that ended the section.
            out.push_back(lines[b.first]);
            size_t blank = b.end;
            while (blank > b.first + 1 && Trim(lines[blank - 1]).empty()) blank--;
            std::string l;
            for (std::istringstream f(whole); std::getline(f, l);) out.push_back(l);
            for (size_t i = blank; i < b.end; i++) out.push_back(lines[i]);
            continue;
        }
        for (size_t i = b.first; i < b.end; i++) {
            auto m = move.find(i);
            if (m == move.end()) {
                out.push_back(lines[i]);
                continue;
            }
            // Keep the key as written and any comment after the value.
            std::string l = lines[i], cr;
            if (!l.empty() && l.back() == '\r') cr = "\r", l.pop_back();
            size_t eq = l.find('='), comment = l.find_first_of("#;", eq);
            out.push_back(l.substr(0, eq + 1) + " " + m->second + (comment == std::string::npos ? "" : " " + l.substr(comment)) + cr);
        }
    }
    for (size_t i = 0; i < out.size(); i++) u.text += out[i] + (i + 1 < out.size() ? "\n" : "");
    return u;
}

}  // namespace crabmods
