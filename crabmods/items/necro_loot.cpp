// The Soul Whip's loot: 20 pickups (whip and raise mods, claw mods, perks, relics) that only drop for players
// holding the whip, from chests of their own loot pool (and any-pool chests: Random, Epic, Legendary, Spiked;
// relics also Relic chests). The Summoner Specialist relic brings them to every chest. Each is an ordinary new pickup with one number per level in CrabMods.ini;
// what it does is up to necromancer.cpp (necro_loot.hpp).
#include "necro_loot.hpp"

#include "../item.hpp"

namespace crabmods {
namespace {

using necro_loot::Id;

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
    {necro_loot::LongLash, "longLash",
     {Kind::WeaponMod, L"DA_WeaponMod_LongLash", L"Long Lash", L"Soul bolts fly farther",
      L"Your soul bolts fly PrimaryBuff% farther", 230, WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Common, "rangePercent", 15.f, "your soul bolts fly farther."},
    {necro_loot::SoulSiphon, "soulSiphon",
     {Kind::WeaponMod, L"DA_WeaponMod_SoulSiphon", L"Soul Siphon", L"Soul bolt hits give souls",
      L"Each enemy your soul bolt hits gives PrimaryBuff souls", 231, WeaponMod_StreakShot, pool::Economy, tag::None, 1.f},
     rarity::Epic, "souls", 1.f, "each enemy a soul bolt hits gives souls."},
    {necro_loot::BrandingIron, "brandingIron",
     {Kind::WeaponMod, L"DA_WeaponMod_BrandingIron", L"Branding Iron", L"Bigger soul bursts and tags",
      L"Your soul bolt's burst and tags reach PrimaryBuff% farther", 232, WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Common, "radiusPercent", 25.f, "the soul bolt's burst and its tags reach farther."},
    {necro_loot::DeathMark, "deathMark",
     {Kind::WeaponMod, L"DA_WeaponMod_DeathMark", L"Death Mark", L"Tagged enemies take more damage from you",
      L"Tagged enemies take PrimaryBuff% more damage from you", 233, WeaponMod_StreakShot, pool::Damage, tag::None, 1.f},
     rarity::Epic, "damagePercent", 10.f, "tagged enemies take more damage from you, not only from your undead."},
    {necro_loot::SoulBrand, "soulBrand",
     {Kind::WeaponMod, L"DA_WeaponMod_SoulBrand", L"Soul Brand", L"Tagged kills spread the tag",
      L"Killing a tagged enemy tags the PrimaryBuff nearest enemies", 234, WeaponMod_StreakShot, pool::Skill, tag::None, 1.f},
     rarity::Epic, "enemies", 2.f, "killing a tagged enemy tags the enemies nearest to it."},

    {necro_loot::SturdyBones, "sturdyBones",
     {Kind::AbilityMod, L"DA_AbilityMod_SturdyBones", L"Sturdy Bones", L"Your undead have more health",
      L"Undead you raise have PrimaryBuff% more health", 230, AbilityMod_HeatSink, pool::Health, tag::None, 1.f},
     rarity::Common, "healthPercent", 15.f, "undead you raise have more health."},
    {necro_loot::SoulDiscount, "soulDiscount",
     {Kind::AbilityMod, L"DA_AbilityMod_SoulDiscount", L"Soul Discount", L"Raising costs fewer souls",
      L"Raising costs about PrimaryBuff% fewer souls (never below half)", 231, AbilityMod_HeatSink, pool::Economy, tag::None, 1.f},
     rarity::Common, "discountPercent", 8.f,
     "raising costs fewer souls. Levels add up hyperbolically and never take a cost below half."},
    {necro_loot::MassGrave, "massGrave",
     {Kind::AbilityMod, L"DA_AbilityMod_MassGrave", L"Mass Grave", L"Fodder raises bring more",
      L"Fodder raises bring PrimaryBuff more", 232, AbilityMod_HeatSink, pool::Skill, tag::None, 1.f},
     rarity::Epic, "extraFodder", 1.f, "each fodder raise brings more of them, for the same souls."},
    {necro_loot::TwinRising, "twinRising",
     {Kind::AbilityMod, L"DA_AbilityMod_TwinRising", L"Twin Rising", L"Raises can bring twice as many",
      L"PrimaryBuff% chance that a raise brings twice as many for the same souls", 233, AbilityMod_HeatSink, pool::Luck, tag::None, 1.f},
     rarity::Epic, "chancePercent", 15.f,
     "a raise can bring twice as many undead for the same souls. Each level rolls again."},

    {necro_loot::GraveClaws, "graveClaws",
     {Kind::MeleeMod, L"DA_MeleeMod_GraveClaws", L"Grave Claws", L"Melee kills give more souls",
      L"Melee kills give PrimaryBuff% more souls", 230, MeleeMod_BigClaws, pool::Economy, tag::None, 1.f},
     rarity::Common, "soulsPercent", 100.f, "melee kills give more souls."},
    {necro_loot::Harvester, "harvester",
     {Kind::MeleeMod, L"DA_MeleeMod_Harvester", L"Harvester", L"Melee kills can rise for free",
      L"Melee kills on fodder and medium enemies have a PrimaryBuff% chance to rise as your undead, free",
      231, MeleeMod_BigClaws, pool::Luck, tag::None, 1.f},
     rarity::Legendary, "chancePercent", 25.f,
     "a melee kill on a fodder or medium enemy can raise it as your undead, for no souls. Each level rolls again."},

    {necro_loot::SoulJar, "soulJar",
     {Kind::Perk, L"DA_Perk_SoulJar", L"Soul Jar", L"More max souls", L"+PrimaryBuff max souls", 230,
      Perk_Collector, pool::Economy, tag::None, 1.f},
     rarity::Common, "souls", 10.f, "your soul bar holds more."},
    {necro_loot::HungryGhosts, "hungryGhosts",
     {Kind::Perk, L"DA_Perk_HungryGhosts", L"Hungry Ghosts", L"Kills give more souls",
      L"Kills give PrimaryBuff% more souls", 231, Perk_Collector, pool::Economy, tag::None, 1.f},
     rarity::Common, "soulsPercent", 10.f, "kills give more souls."},
    {necro_loot::LastRites, "lastRites",
     {Kind::Perk, L"DA_Perk_LastRites", L"Last Rites", L"Fallen undead give souls back",
      L"When one of your undead falls in a fight, you get back PrimaryBuff% of its souls", 232, Perk_Collector, pool::Economy, tag::None, 1.f},
     rarity::Common, "refundPercent", 15.f, "when one of your undead falls in a fight, part of its raise cost comes back."},
    {necro_loot::ArmyOfTheDead, "armyOfTheDead",
     {Kind::Perk, L"DA_Perk_ArmyOfTheDead", L"Army of the Dead", L"More damage for each undead",
      L"Damage increased by PrimaryBuff% for each undead you have", 233, Perk_Collector, pool::Damage, tag::None, 1.f},
     rarity::Legendary, "damagePercent", 4.f, "more damage for each of your undead."},
    {necro_loot::PlagueBearers, "plagueBearers",
     {Kind::Perk, L"DA_Perk_PlagueBearers", L"Plague Bearers", L"Your undead poison",
      L"Your undead's attacks apply PrimaryBuff poison", 234, Perk_Collector, pool::Elemental, tag::Poison, 1.f},
     rarity::Epic, "poisonStacks", 3.f, "your undead's attacks apply poison stacks."},
    {necro_loot::GrimHarvest, "grimHarvest",
     {Kind::Perk, L"DA_Perk_GrimHarvest", L"Grim Harvest", L"Tagged kills give more souls",
      L"Tagged kills give PrimaryBuff% more souls, on top of the tag's own", 235, Perk_Collector, pool::Economy, tag::None, 1.f},
     rarity::Epic, "soulsPercent", 50.f, "tagged kills give even more souls (on top of taggedSoulsPercent)."},

    {necro_loot::RingOfSouls, "ringOfSouls",
     {Kind::Relic, L"DA_Relic_RingOfSouls", L"Ring of Souls", L"More souls from everything",
      L"PrimaryBuff% more souls from everything", 230, Relic_FireRing, pool::Economy, tag::None, 1.f},
     rarity::Epic, "soulsPercent", 25.f, "more souls from everything."},
    {necro_loot::RingOfTheGrave, "ringOfTheGrave",
     {Kind::Relic, L"DA_Relic_RingOfTheGrave", L"Ring of the Grave", L"Stronger undead",
      L"Your undead have PrimaryBuff% more health and damage", 231, Relic_FireRing, pool::Damage, tag::None, 1.f},
     rarity::Epic, "percent", 25.f, "your undead have more health and damage."},
    {necro_loot::Phylactery, "phylactery",
     {Kind::Relic, L"DA_Relic_Phylactery", L"Phylactery", L"Your souls can save you",
      L"Once per island, a hit that would down you is cancelled. It spends all your souls (at least PrimaryBuff)",
      232, Relic_FireRing, pool::Health, tag::None, 1.f},
     rarity::Legendary, "minSouls", 50.f,
     "once per island, a hit that would down you is cancelled instead, for all your souls. Needs at least\n# this many souls."},
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

class NecroItem;
NecroItem* g_byId[necro_loot::kCount];

class NecroItem : public AssetItem {
public:
    explicit NecroItem(const Def& d)
        : AssetItem(d.section, 201 + int(d.id), d.spec, d.rarity), def_(d), amount_(d.amount) {
        g_byId[d.id] = this;
    }

    std::string DefaultIni() const override {
        static const char* const kinds[] = {"Soul Whip weapon mod", "Raise Dead ability mod", "perk", "relic",
                                            "melee mod"};
        return "# " + Title() + ", a " + kinds[int(spec_.kind)] + ": " + def_.comment +
               "\n# Only drops for players holding the Soul Whip, from " + PoolName(spec_.lootPool) + " chests.\nenabled = true\n\n# Per level.\n" +
               def_.key + " = " + Num(def_.amount) + "\n\n# common, epic, legendary or greed.\nrarity = " +
               RarityName(def_.rarity) + "\n";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == def_.key) amount_ = std::max(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::string(def_.key) + " " + Num(amount_) + ", " + RarityName(rarity_) + ", whip holders only";
    }

    // Whip holders only.
    float LootWeight(void* ps, void* da) override { return da == da_ && !HoldsSoulWhip(ps) ? 0.f : 1.f; }

    float Amount() const { return amount_; }

protected:
    float PrimaryBuff() const override { return amount_; }

private:
    const Def& def_;
    float amount_;
};

struct All {
    std::vector<NecroItem*> items;
    All() {
        for (const Def& d : kDefs) items.push_back(new NecroItem(d));
    }
} g_all;

}  // namespace

namespace necro_loot {
bool IsNecroItem(void* da) {
    if (!da) return false;
    for (NecroItem* i : g_byId)
        if (i && i->Asset() == da) return true;
    return false;
}
int Level(void* ps, Id id) {
    NecroItem* i = id < kCount ? g_byId[id] : nullptr;
    return i && i->Enabled() ? i->Level(ps) : 0;
}
float Amount(Id id) {
    NecroItem* i = id < kCount ? g_byId[id] : nullptr;
    return i ? i->Amount() : 0.f;
}
}  // namespace necro_loot
}  // namespace crabmods
