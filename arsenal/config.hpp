// Arsenal.ini: key=value lines, '#' or ';' comments. Missing keys keep their defaults.
#pragma once
#include <fstream>
#include <string>

namespace arsenal {

struct Config {
    bool enabled = true;
    float damagePerModPercent = 3.f;  // per other weapon mod, per Arsenal level
    uint8_t rarity = 2;               // ECrabRarity: 1 Common, 2 Epic, 3 Legendary
    bool countStacks = false;         // a x3 stack counts as three mods
};

// Written next to the DLL on first launch.
inline constexpr const char kDefaultIni[] = R"(# Arsenal: a new weapon mod for Crab Champions.
# Every player needs Arsenal installed, and everyone should use the same settings.
enabled = true

# Damage bonus for each other weapon mod you hold, in percent. Each extra Arsenal stacks it again.
damagePerModPercent = 3

# Which chests it drops from: common, epic or legendary.
rarity = epic

# true: a x3 stack of a mod counts as three mods. false: each different mod counts once.
countStacks = false
)";

inline bool ParseBool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

inline uint8_t ParseRarity(const std::string& v) {
    if (v == "common" || v == "1") return 1;
    if (v == "legendary" || v == "3") return 3;
    return 2;
}

// Reads the ini, creating it with the defaults if it does not exist yet.
inline Config LoadConfig(const std::wstring& path, bool* created = nullptr) {
    Config c;
    std::ifstream f(path.c_str());
    if (!f) {
        std::ofstream(path.c_str(), std::ios::binary) << kDefaultIni;
        if (created) *created = true;
        return c;
    }
    std::string line;
    while (std::getline(f, line)) {
        auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.resize(hash);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string s) {
            size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
        };
        std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        try {
            if (k == "enabled") c.enabled = ParseBool(v);
            else if (k == "damagePerModPercent") c.damagePerModPercent = std::stof(v);
            else if (k == "rarity") c.rarity = ParseRarity(v);
            else if (k == "countStacks") c.countStacks = ParseBool(v);
        } catch (...) {
        }
    }
    return c;
}

}  // namespace arsenal
