// CrabSync.ini: key=value lines, '#' or ';' comments. Missing keys keep their defaults.
#pragma once
#include <fstream>
#include <string>

#include "ledger.hpp"

namespace crabsync {

struct Config {
    bool enabled = true;
    Settings sync;
    bool announce = true;        // teammate pickups show the pickup popup and run perk side effects
    float healthPerPlayer = 250.f;  // added to the shared health pool for each player after the first
    float settleSeconds = 3.f;   // wait after a map load before syncing
    float pollSeconds = 0.1f;    // how often the host compares inventories
};

// Written next to the DLL on first launch.
inline constexpr const char kDefaultIni[] = R"(# CrabSync: shared team inventory. Only the host's settings matter; clients need nothing.
enabled = true

# Equipment: the most recent change by anyone is applied to everyone.
syncWeapon = true
syncAbility = true
syncMelee = true

# Items: one shared list; a pickup by anyone adds to everyone's stack.
syncWeaponMods = true
syncAbilityMods = true
syncMeleeMods = true
syncPerks = true
syncRelics = true
syncEnhancements = true
syncAccumulatedBuffs = true

# Resources: one shared wallet and shared slot counts.
syncSlots = true
syncCrystals = true

# One shared health pool: damage to anyone hurts everyone, healing heals everyone, and when it
# reaches 0 (or anyone dies) everyone dies. Armor plates stay per-player.
# Max health is shared too: losing or gaining max health (Eternal Punishment, totems, perks) changes
# everyone's.
syncHealth = true
# Max health added to the pool for each player after the first (2 players = your max + 250).
healthPerPlayer = 250
# Chests that cost health or max health (spiked and greed chests):
#   split = the team pays the cost divided by the number of players (4 players opening one each
#           costs what one chest costs alone)
#   full  = the team pays every chest's whole cost
#   free  = they cost the team nothing
chestHealthCost = split

# Teammate pickups show the pickup popup and trigger perk side effects, like a real pickup.
announcePickups = true

# Seconds to wait after a map loads before syncing, and how often the host checks for changes.
settleSeconds = 3
pollSeconds = 0.1
)";

inline bool ParseBool(const std::string& v) { return v == "1" || v == "true" || v == "yes" || v == "on"; }

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
        if (k == "enabled") c.enabled = ParseBool(v);
        else if (k == "syncWeapon") c.sync.equip[Weapon] = ParseBool(v);
        else if (k == "syncAbility") c.sync.equip[Ability] = ParseBool(v);
        else if (k == "syncMelee") c.sync.equip[Melee] = ParseBool(v);
        else if (k == "syncWeaponMods") c.sync.cats[WeaponMods] = ParseBool(v);
        else if (k == "syncAbilityMods") c.sync.cats[AbilityMods] = ParseBool(v);
        else if (k == "syncMeleeMods") c.sync.cats[MeleeMods] = ParseBool(v);
        else if (k == "syncPerks") c.sync.cats[Perks] = ParseBool(v);
        else if (k == "syncRelics") c.sync.cats[Relics] = ParseBool(v);
        else if (k == "syncSlots") c.sync.slots = ParseBool(v);
        else if (k == "syncCrystals") c.sync.crystals = ParseBool(v);
        else if (k == "syncEnhancements") c.sync.enhancements = ParseBool(v);
        else if (k == "syncAccumulatedBuffs") c.sync.buffs = ParseBool(v);
        else if (k == "syncHealth") c.sync.health = ParseBool(v);
        else if (k == "healthPerPlayer") c.healthPerPlayer = std::stof(v);
        else if (k == "chestHealthCost")
            c.sync.chestCost = v == "full" ? ChestCost::Full : v == "free" ? ChestCost::Free : ChestCost::Split;

        else if (k == "announcePickups") c.announce = ParseBool(v);
        else if (k == "settleSeconds") c.settleSeconds = std::stof(v);
        else if (k == "pollSeconds") c.pollSeconds = std::stof(v);
    }
    return c;
}

}  // namespace crabsync
