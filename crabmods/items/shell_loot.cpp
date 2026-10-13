// Shellbreaker's loot: 19 pickups (weapon mods, perks, relics) that only drop for players holding Shellbreaker,
// from chests of their own loot pool (and any-pool chests). Each is an ordinary new pickup with one number per
// level in CrabMods.ini; what it does is up to shellbreaker.cpp (shell_loot.hpp). The same pattern as the Soul
// Whip's (necro_loot.cpp). Type bytes 220-229, 235-236.
#include "shell_loot.hpp"

#include "../item.hpp"

namespace crabmods {
namespace {

using shell_loot::Id;

struct Def {
    Id id;
    const char* section;
    AssetSpec spec;
    uint8_t rarity;
    const char* key;      // the per-level number's ini key
    float amount;         // and its default
    const char* comment;  // the ini's first line, after "# <Name>, a <kind>: "
};

// clang-format off
const Def kDefs[] = {
    {shell_loot::LongChain, "longChain",
     {Kind::WeaponMod, L"DA_WeaponMod_LongChain", L"Long Chain", L"A longer chain: wider swings, a farther hook",
      L"Shellbreaker's chain gets PrimaryBuff more links: its swings, circle and hook reach farther", 220,
      WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Common, "links", 4.f, "Shellbreaker's chain gets more links; its swings, circle and hook reach\n# farther in proportion."},
    {shell_loot::WideArc, "wideArc",
     {Kind::WeaponMod, L"DA_WeaponMod_WideArc", L"Wide Arc", L"Swings hit a wider area",
      L"Shellbreaker's swings hit PrimaryBuff% wider", 221, WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Common, "radiusPercent", 15.f, "Shellbreaker's swings hit a wider area."},
    {shell_loot::Whirlwind, "whirlwind",
     {Kind::WeaponMod, L"DA_WeaponMod_Whirlwind", L"Whirlwind", L"The overhead circle drags enemies in",
      L"Shellbreaker's overhead circle drags nearby enemies toward you (PrimaryBuff strength)", 222,
      WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Epic, "pullSpeed", 500.f, "Shellbreaker's overhead circle drags nearby enemies (not bosses) toward\n# you, at this speed (cm/s) per level."},
    {shell_loot::Executioner, "executioner",
     {Kind::WeaponMod, L"DA_WeaponMod_Executioner", L"Executioner", L"The chop hits harder",
      L"Shellbreaker's chop and its rebound deal PrimaryBuff% more damage", 223, WeaponMod_StreakShot, pool::Damage,
      tag::None, 1.f},
     rarity::Epic, "damagePercent", 55.f, "Shellbreaker's chop and its rebound (swings 3 and 4) deal more damage."},
    {shell_loot::SearingEdge, "searingEdge",
     {Kind::WeaponMod, L"DA_WeaponMod_SearingEdge", L"Searing Edge", L"Swings set enemies on fire",
      L"Shellbreaker's swings apply PrimaryBuff fire", 224, WeaponMod_StreakShot, pool::Elemental, tag::Fire, 1.f},
     rarity::Epic, "fireStacks", 3.f, "every enemy a Shellbreaker swing hits gets fire stacks."},

    {shell_loot::Riptide, "riptide",
     {Kind::Perk, L"DA_Perk_Riptide", L"Riptide", L"More damage after the hook lands",
      L"For 4 seconds after your hook lands, PrimaryBuff% more damage", 220, Perk_Collector, pool::Damage, tag::None,
      1.f},
     rarity::Epic, "damagePercent", 35.f, "for 4 seconds after Shellbreaker's hook lands (enemy or wall), more damage."},
    {shell_loot::QuickDraw, "quickDraw",
     {Kind::Perk, L"DA_Perk_QuickDraw", L"Quick Draw", L"The hook recharges faster",
      L"Shellbreaker's hook recharges about PrimaryBuff% faster", 221, Perk_Collector, pool::Skill, tag::None, 1.f},
     rarity::Common, "cooldownPercent", 15.f,
     "Shellbreaker's hook cooldown is cut. Levels add up hyperbolically and never take it below a sixth."},
    {shell_loot::Undertow, "undertow",
     {Kind::Perk, L"DA_Perk_Undertow", L"Undertow", L"The hook hits harder",
      L"Shellbreaker's hook deals PrimaryBuff% more damage to what it catches", 222, Perk_Collector, pool::Damage,
      tag::None, 1.f},
     rarity::Common, "damagePercent", 300.f, "the hook's own hit on an enemy deals more damage."},
    {shell_loot::Rhythm, "rhythm",
     {Kind::Perk, L"DA_Perk_Rhythm", L"Rhythm", L"The string's finisher hits harder",
      L"Shellbreaker's overhead circle deals PrimaryBuff% more damage", 223, Perk_Collector, pool::Damage, tag::None,
      1.f},
     rarity::Common, "damagePercent", 30.f, "the overhead circle that ends Shellbreaker's string deals more damage."},

    {shell_loot::Grappler, "grappler",
     {Kind::Relic, L"DA_Relic_Grappler", L"Grappler", L"More hook charges",
      L"Shellbreaker's hook holds PrimaryBuff more charges", 220, Relic_FireRing, pool::Skill, tag::None, 1.f},
     rarity::Epic, "charges", 1.f, "Shellbreaker's hook holds more charges (they recharge one at a time)."},
    {shell_loot::KrakensGrasp, "krakensGrasp",
     {Kind::Relic, L"DA_Relic_KrakensGrasp", L"Kraken's Grasp", L"The hook yanks anything",
      L"Your hook yanks hard and elite enemies too, and drags the PrimaryBuff nearest along", 221, Relic_FireRing,
      pool::Skill, tag::None, 1.f},
     rarity::Legendary, "extraEnemies", 2.f,
     "the hook yanks hard and elite enemies too (never bosses), and drags this many of the nearest\n# others along."},
    {shell_loot::FrostChain, "frostChain",
     {Kind::Relic, L"DA_Relic_FrostChain", L"Frost Chain", L"The hook freezes",
      L"Shellbreaker's hook applies PrimaryBuff ice to what it catches", 222, Relic_FireRing, pool::Elemental, tag::Ice,
      1.f},
     rarity::Epic, "iceStacks", 10.f, "the hook's hit applies ice stacks to the enemy it catches."},

    {shell_loot::SerratedLinks, "serratedLinks",
     {Kind::WeaponMod, L"DA_WeaponMod_SerratedLinks", L"Serrated Links", L"Swings deal more damage",
      L"Shellbreaker's swings deal PrimaryBuff% more damage", 225, WeaponMod_StreakShot, pool::Damage, tag::None, 1.f},
     rarity::Common, "damagePercent", 10.f, "Shellbreaker's swings deal more damage."},
    {shell_loot::VenomBarbs, "venomBarbs",
     {Kind::WeaponMod, L"DA_WeaponMod_VenomBarbs", L"Venom Barbs", L"Swings poison enemies",
      L"Shellbreaker's swings apply PrimaryBuff poison", 226, WeaponMod_StreakShot, pool::Elemental, tag::Poison, 1.f},
     rarity::Common, "poisonStacks", 2.f, "every enemy a Shellbreaker swing hits gets poison stacks."},
    {shell_loot::StaticChain, "staticChain",
     {Kind::WeaponMod, L"DA_WeaponMod_StaticChain", L"Static Chain", L"Swings shock enemies",
      L"Shellbreaker's swings apply PrimaryBuff lightning", 227, WeaponMod_StreakShot, pool::Elemental,
      tag::Lightning, 1.f},
     rarity::Epic, "lightningStacks", 3.f, "every enemy a Shellbreaker swing hits gets lightning stacks."},
    {shell_loot::RimeEdge, "rimeEdge",
     {Kind::WeaponMod, L"DA_WeaponMod_RimeEdge", L"Rime Edge", L"Swings freeze enemies",
      L"Shellbreaker's swings apply PrimaryBuff ice", 228, WeaponMod_StreakShot, pool::Elemental, tag::Ice, 1.f},
     rarity::Epic, "iceStacks", 3.f, "every enemy a Shellbreaker swing hits gets ice stacks."},
    {shell_loot::HexLinks, "hexLinks",
     {Kind::WeaponMod, L"DA_WeaponMod_HexLinks", L"Hex Links", L"Swings curse enemies with arcane",
      L"Shellbreaker's swings apply PrimaryBuff arcane", 229, WeaponMod_StreakShot, pool::Elemental, tag::Arcane, 1.f},
     rarity::Legendary, "arcaneStacks", 4.f, "every enemy a Shellbreaker swing hits gets arcane stacks."},
    {shell_loot::Momentum, "momentum",
     {Kind::WeaponMod, L"DA_WeaponMod_Momentum", L"Momentum", L"Each swing of the string hits harder",
      L"Each Shellbreaker swing deals PrimaryBuff% more than the one before it in the string", 235,
      WeaponMod_StreakShot, pool::Damage, tag::Combo, 1.f},
     rarity::Epic, "damagePercent", 8.f,
     "each swing deeper into Shellbreaker's string deals more: the second +1x this, the circle +4x."},
    {shell_loot::Backlash, "backlash",
     {Kind::WeaponMod, L"DA_WeaponMod_Backlash", L"Backlash", L"Swings also strike behind you",
      L"Shellbreaker's swings also strike behind you for PrimaryBuff% of their damage", 236, WeaponMod_StreakShot,
      pool::Damage, tag::None, 1.f},
     rarity::Legendary, "damagePercent", 60.f,
     "each Shellbreaker sweep and chop also strikes as far behind you, for this share of its damage."},
};
// clang-format on

const char* PoolName(uint8_t p) {
    switch (p) {
        case pool::Damage: return "Damage";
        case pool::Elemental: return "Elemental";
        case pool::Luck: return "Luck";
        case pool::Health: return "Health";
        case pool::Economy: return "Economy";
        case pool::Skill: return "Skill";
        default: return "its";
    }
}

class ShellItem;
ShellItem* g_byId[shell_loot::kCount];

class ShellItem : public AssetItem {
public:
    explicit ShellItem(const Def& d)
        : AssetItem(d.section, 230 + int(d.id), d.spec, d.rarity), def_(d), amount_(d.amount) {
        g_byId[d.id] = this;
    }

    std::string DefaultIni() const override {
        static const char* const kinds[] = {"Shellbreaker weapon mod", "ability mod", "perk", "relic", "melee mod"};
        return "# " + Title() + ", a " + kinds[int(spec_.kind)] + ": " + def_.comment +
               "\n# Only drops for players holding Shellbreaker, from " + PoolName(spec_.lootPool) +
               " chests.\nenabled = true\n\n# Per level.\n" + def_.key + " = " + Num(def_.amount) +
               "\n\n# common, epic, legendary or greed.\nrarity = " + RarityName(def_.rarity) + "\n";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == def_.key) amount_ = std::max(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::string(def_.key) + " " + Num(amount_) + ", " + RarityName(rarity_) + ", Shellbreaker holders only";
    }

    // Shellbreaker holders only.
    float LootWeight(void* ps, void* da) override { return da == da_ && !HoldsShellbreaker(ps) ? 0.f : 1.f; }

    float Amount() const { return amount_; }

protected:
    float PrimaryBuff() const override { return amount_; }

private:
    const Def& def_;
    float amount_;
};

struct All {
    std::vector<ShellItem*> items;
    All() {
        for (const Def& d : kDefs) items.push_back(new ShellItem(d));
    }
} g_all;

}  // namespace

namespace shell_loot {
bool IsShellItem(void* da) {
    if (!da) return false;
    for (ShellItem* i : g_byId)
        if (i && i->Asset() == da) return true;
    return false;
}
int Level(void* ps, Id id) {
    ShellItem* i = id < kCount ? g_byId[id] : nullptr;
    return i && i->Enabled() ? i->Level(ps) : 0;
}
float Amount(Id id) {
    ShellItem* i = id < kCount ? g_byId[id] : nullptr;
    return i ? i->Amount() : 0.f;
}
}  // namespace shell_loot
}  // namespace crabmods
