// The Specialists, eight relics: each makes one kind of pickup more likely to be offered to you.
//   Fire, Ice, Lightning, Poison and Critical Specialist: pickups with that tag. The relic carries the tag
//   itself, so it also counts for pickups that need one of that tag to drop (Fire Ring, Mega Crit, ...).
//   Damage and Luck Specialist: pickups from that loot pool, and islands rewarding it (for the whole team).
//   Summoner Specialist: the Soul Whip's 22 pickups (necro_loot); like them, it only drops for whip holders.
// The weight goes on top of the game's own SpawnWeight for every loot roll made for the holder. Those pickups
// can also come out of the holder's chests of every other loot pool (Damage, Critical, Elemental, Speed, Luck,
// Health, Economy, Skill), where the game would never offer them.
#include <string>

#include "../item.hpp"
#include "necro_loot.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

struct SpecialistDef {
    const char* section;
    const wchar_t* objectName;
    const wchar_t* name;
    const wchar_t* description;
    const wchar_t* levelDescription;
    const char* what;  // for the ini and the log: "Fire pickups"
    uint8_t type;
    uint8_t tag;   // tag:: the boosted pickups have, or tag::None
    uint8_t pool;  // pool:: the boosted pickups and island rewards come from, when tag is None
    bool necro = false;  // boosts the Soul Whip's pickups instead (pool is only the relic's own)
};

const SpecialistDef kDefs[] = {
    {"fireSpecialist", L"DA_Relic_FireSpecialist", L"Fire Specialist", L"Fire pickups are offered more often",
     L"Fire pickups are PrimaryBuff times as likely to be offered", "Fire pickups", 200, tag::Fire,
     pool::Elemental},
    {"iceSpecialist", L"DA_Relic_IceSpecialist", L"Ice Specialist", L"Ice pickups are offered more often",
     L"Ice pickups are PrimaryBuff times as likely to be offered", "Ice pickups", 201, tag::Ice, pool::Elemental},
    {"lightningSpecialist", L"DA_Relic_LightningSpecialist", L"Lightning Specialist",
     L"Lightning pickups are offered more often", L"Lightning pickups are PrimaryBuff times as likely to be offered",
     "Lightning pickups", 202, tag::Lightning, pool::Elemental},
    {"poisonSpecialist", L"DA_Relic_PoisonSpecialist", L"Poison Specialist",
     L"Poison pickups are offered more often", L"Poison pickups are PrimaryBuff times as likely to be offered",
     "Poison pickups", 203, tag::Poison, pool::Elemental},
    {"criticalSpecialist", L"DA_Relic_CriticalSpecialist", L"Critical Specialist",
     L"Critical pickups are offered more often", L"Critical pickups are PrimaryBuff times as likely to be offered",
     "Critical pickups", 204, tag::Critical, pool::Critical},
    {"damageSpecialist", L"DA_Relic_DamageSpecialist", L"Damage Specialist",
     L"Damage pickups and Damage islands are offered more often",
     L"Damage pickups and Damage island rewards are PrimaryBuff times as likely", "Damage pickups and Damage islands",
     205, tag::None, pool::Damage},
    {"luckSpecialist", L"DA_Relic_LuckSpecialist", L"Luck Specialist",
     L"Luck pickups and Luck islands are offered more often",
     L"Luck pickups and Luck island rewards are PrimaryBuff times as likely", "Luck pickups and Luck islands", 206,
     tag::None, pool::Luck},
    {"summonerSpecialist", L"DA_Relic_SummonerSpecialist", L"Summoner Specialist",
     L"Soul Whip pickups are offered more often", L"Soul Whip pickups are PrimaryBuff times as likely to be offered",
     "Soul Whip pickups", 215, tag::None, pool::Skill, true},
};

void* g_specialists[std::size(kDefs)];  // every Specialist's asset, so none of them boosts another

bool IsSpecialist(void* da) {
    for (void* s : g_specialists)
        if (s && s == da) return true;
    return false;
}

class Specialist : public AssetItem {
public:
    explicit Specialist(int i)
        : AssetItem(kDefs[i].section, 200 + i,
                    {Kind::Relic, kDefs[i].objectName, kDefs[i].name, kDefs[i].description,
                     kDefs[i].levelDescription, kDefs[i].type, Relic_FireRing, kDefs[i].pool, kDefs[i].tag},
                    rarity::Epic),
          def_(kDefs[i]),
          index_(i) {}

    std::string DefaultIni() const override {
        std::string s = std::string("# ") + Title() + ", a relic: " + def_.what + " are offered 3 times as often";
        s += def_.necro ? " (it only drops while you hold the Soul Whip).\n"
             : def_.tag == tag::None ? " (islands for the whole team).\n" : ".\n";
        return s + R"(enabled = true

# How many times as likely.
multiplier = 3

# They can also come out of your chests of every other loot pool (a Speed chest, a Health chest, ...), this
# many times as likely as the chest's own pickups there. 0 keeps them in their own chests.
otherChests = 1

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "multiplier") mult_ = ParseFloat(v);
        else if (k == "otherChests") other_ = ParseFloat(v) > 0.f ? ParseFloat(v) : 0.f;
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::string(def_.what) + " x" + Num(mult_) + (other_ > 0.f ? ", in every chest" : "") + ", " +
               RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        g_specialists[index_] = da_;
    }

    float LootWeight(void* ps, void* da) override {
        if (def_.necro && da && da == da_ && !HoldsSoulWhip(ps)) return 0.f;
        if (!Boosts(da) || !Level(ps)) return 1.f;
        return mult_;
    }

    float OtherChestWeight(void* ps, void* da) override {
        if (other_ <= 0.f || !Boosts(da) || !Level(ps)) return 0.f;
        return other_;
    }

    // Damage and Luck: island rewards, if anyone in the game holds it.
    float PortalRewardWeight(void* lm, uint8_t lootPool) override {
        if (def_.necro || def_.tag != tag::None || lootPool != def_.pool || !da_) return 1.f;
        bool held = false;
        ForEachPlayer(lm, [&](void* ps) { held |= Level(ps) > 0; });
        return held ? mult_ : 1.f;
    }

protected:
    float PrimaryBuff() const override { return mult_; }

private:
    std::string Title() const override {
        std::string s;
        for (const wchar_t* p = def_.name; *p; p++) s.push_back(char(*p));
        return s;
    }
    bool Boosts(void* da) const {
        if (!da || IsSpecialist(da)) return false;
        if (def_.necro) return necro_loot::IsNecroItem(da);
        if (def_.tag != tag::None) return At<uint8_t>(da, off::DA_PickupTag) == def_.tag;
        return At<uint8_t>(da, off::DA_LootPool) == def_.pool;
    }

    const SpecialistDef& def_;
    int index_;
    float mult_ = 3.f;
    float other_ = 1.f;
};

Specialist g_fire(0), g_ice(1), g_lightning(2), g_poison(3), g_critical(4), g_damage(5), g_luck(6), g_summoner(7);

}  // namespace
}  // namespace crabmods
