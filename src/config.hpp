// CrabMods.ini: [section] headers, key=value lines, '#' or ';' comments. Missing keys keep their defaults.
#pragma once
#include <fstream>
#include <sstream>
#include <string>

namespace crabmods {

struct Config {
    struct {
        bool enabled = true;
        float damagePerModPercent = 3.f;  // per other weapon mod, per Arsenal level
        uint8_t rarity = 2;               // ECrabRarity: 1 Common, 2 Epic, 3 Legendary
        bool countStacks = false;         // a x3 stack counts as three mods (Hoarder too)
    } arsenal;
    struct {
        bool enabled = true;
        float damagePercent = 3.f;  // per weapon mod and per perk level, per Hoarder level
        uint8_t rarity = 3;
    } hoarder;
    struct {
        bool anyWeapon = true;  // Heavy Shot drops for every weapon, not just Bounce weapons
    } heavyShot;
};

// Written next to the DLL on first launch. {arsenal} is replaced with the [arsenal] keys.
inline constexpr const char kDefaultIni[] = R"(# CrabMods: new items for Crab Champions.
# Every player needs CrabMods installed, and everyone should use the same settings.

[arsenal]
# Arsenal, a weapon mod: more damage for each other weapon mod you have.
{arsenal}
[hoarder]
# Hoarder, a perk: more damage for each weapon mod and each perk level you have.
enabled = true

# Damage bonus for each weapon mod and each perk level, in percent. Each extra Hoarder stacks it again.
damagePercent = 3

# Which chests it drops from: common, epic or legendary.
rarity = legendary

[heavyShot]
# The game's own Heavy Shot only drops if your weapon bounces. true lets it drop for every weapon.
# Only the host's setting matters.
anyWeapon = true
)";

inline constexpr const char kDefaultArsenal[] = R"(enabled = true

# Damage bonus for each other weapon mod you hold, in percent. Each extra Arsenal stacks it again.
damagePerModPercent = 3

# Which chests it drops from: common, epic or legendary.
rarity = epic

# true: a x3 stack of a mod counts as three mods (for Arsenal and Hoarder). false: each different mod
# counts once.
countStacks = false
)";

inline bool ParseBool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

inline uint8_t ParseRarity(const std::string& v, uint8_t def) {
    if (v == "common" || v == "1") return 1;
    if (v == "epic" || v == "2") return 2;
    if (v == "legendary" || v == "3") return 3;
    return def;
}

inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// Applies one ini text to c. Keys outside any section belong to [arsenal] (the old Arsenal.ini had none).
inline void ParseIni(std::istream& f, Config& c) {
    std::string line, section = "arsenal";
    while (std::getline(f, line)) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        line = Trim(line);
        if (line.size() > 1 && line.front() == '[' && line.back() == ']') {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        try {
            if (section == "arsenal") {
                if (k == "enabled") c.arsenal.enabled = ParseBool(v);
                else if (k == "damagePerModPercent") c.arsenal.damagePerModPercent = std::stof(v);
                else if (k == "rarity") c.arsenal.rarity = ParseRarity(v, c.arsenal.rarity);
                else if (k == "countStacks") c.arsenal.countStacks = ParseBool(v);
            } else if (section == "hoarder") {
                if (k == "enabled") c.hoarder.enabled = ParseBool(v);
                else if (k == "damagePercent") c.hoarder.damagePercent = std::stof(v);
                else if (k == "rarity") c.hoarder.rarity = ParseRarity(v, c.hoarder.rarity);
            } else if (section == "heavyShot") {
                if (k == "anyWeapon") c.heavyShot.anyWeapon = ParseBool(v);
            }
        } catch (...) {
        }
    }
}

// Reads the ini, creating it with the defaults if it does not exist yet. A first launch next to an old
// Arsenal.ini carries its settings over into [arsenal].
inline Config LoadConfig(const std::wstring& path, const std::wstring& oldArsenalIni, bool* created = nullptr,
                         bool* migrated = nullptr) {
    Config c;
    std::ifstream f(path.c_str());
    if (f) {
        ParseIni(f, c);
        return c;
    }
    std::string arsenal = kDefaultArsenal;
    if (std::ifstream old(oldArsenalIni.c_str()); old) {
        std::stringstream ss;
        ss << old.rdbuf();
        arsenal.clear();
        std::string line;
        for (std::istringstream in(ss.str()); std::getline(in, line);) {
            if (line.rfind("# Arsenal:", 0) == 0 || line.rfind("# Every player", 0) == 0) continue;
            arsenal += line + "\n";
        }
        std::istringstream in(arsenal);
        ParseIni(in, c);
        if (migrated) *migrated = true;
    }
    std::string ini = kDefaultIni;
    ini.replace(ini.find("{arsenal}\n"), 10, Trim(arsenal) + "\n\n");
    std::ofstream(path.c_str(), std::ios::binary) << ini;
    if (created) *created = true;
    return c;
}

}  // namespace crabmods
