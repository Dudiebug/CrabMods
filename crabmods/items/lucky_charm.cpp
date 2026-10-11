// Lucky Charm, a Legendary relic: the items in the chests you open have a chance to be level 2.
//
// Host: an opened chest rolls its loot for the opener into its PickupsToSpawn and spawns them right after
// (main.cpp hands the list to OnChestLoot in between). Each level-1 weapon, ability or melee mod or perk in it
// gets the chance; relics and weapons stay level 1.
#include <random>

#include "../item.hpp"

namespace crabmods {
namespace {

constexpr int kPickupInfoSize = 0x50;  // FCrabPickupInfo
constexpr int kPickupLevel = 0x08;     // its FCrabInventoryInfo.Level (uint8)

class LuckyCharm : public AssetItem {
public:
    LuckyCharm()
        : AssetItem("luckyCharm", 182,
                    {Kind::Relic, L"DA_Relic_LuckyCharm", L"Lucky Charm", L"Items from chests can be level 2",
                     L"Items from the chests you open have a PrimaryBuff% chance to be level 2", 212, Relic_FireRing,
                     pool::Luck},
                    rarity::Legendary),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Lucky Charm, a relic: the mods and perks in the chests you open have a chance to be level 2.
enabled = true

# Chance for each item in a chest to be level 2, in percent.
chancePercent = 25

# Which chests it drops from: common, epic, legendary or greed.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chancePercent") chance_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% for chest items to be level 2, " + std::string(RarityName(rarity_));
    }

    void OnChestLoot(void* chest, void* ps, TArray<uint8_t>& pickups) override {
        if (Level(ps) == 0 || chance_ <= 0.f || !pickups.Data) return;
        int upgraded = 0;
        for (int i = 0; i < pickups.Num; i++) {
            uint8_t* info = pickups.Data + i * kPickupInfoSize;
            void* da = At<void*>(info, 0);
            uint8_t& level = At<uint8_t>(info, kPickupLevel);
            if (!da || level != 1 || !Levels(da)) continue;
            if (std::uniform_real_distribution<float>(0.f, 100.f)(rng_) >= chance_) continue;
            level = 2;
            upgraded++;
        }
        if (upgraded) CRAB_LOG("Lucky Charm: %d of %d chest items are level 2", upgraded, pickups.Num);
    }

protected:
    float PrimaryBuff() const override { return chance_; }

private:
    // Kinds of item that stack in levels.
    static bool Levels(void* da) {
        if (At<bool>(da, off::DA_bLimited)) return false;
        for (Kind k : {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk})
            if (IsA(da, StaticClass(Info(k).staticClass))) return true;
        return false;
    }

    float chance_ = 25.f;
    std::minstd_rand rng_;
};

LuckyCharm g_item;

}  // namespace
}  // namespace crabmods
