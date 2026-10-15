// Connoisseur, an Epic relic: items in your chests that you already hold come with an extra level, so taking one
// again gives 2 levels instead of 1.
//
// Host: an opened chest's loot (OnChestLoot) is raised one level for every mod or perk in it that the opener already
// holds. Relics, weapons and Limited items don't stack in levels and are left alone. Lucky Charm's card hook shows the
// level ("Vitality (Lv 2)").
#include <algorithm>

#include "../item.hpp"

namespace crabmods {
namespace {

constexpr int kPickupInfoSize = 0x50;
constexpr int kPickupLevel = 0x08;

class Connoisseur : public AssetItem {
public:
    Connoisseur()
        : AssetItem("connoisseur", 189,
                    {Kind::Relic, L"DA_Relic_Connoisseur", L"Connoisseur", L"Items you already hold come with more levels",
                     L"Items in your chests that you already hold are PrimaryBuff level(s) higher", 250, Relic_FireRing,
                     pool::Luck},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Connoisseur, a relic: items in the chests you open that you already hold come with extra levels, so
# taking one again gives 2 levels instead of 1.
enabled = true

# Extra levels on each item you already hold.
extraLevels = 1

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "extraLevels") extra_ = std::clamp(int(ParseFloat(v)), 0, 100);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + std::to_string(extra_) + " level(s) on chest items you hold, " + RarityName(rarity_);
    }

    void OnChestLoot(void* chest, void* ps, TArray<uint8_t>& pickups) override {
        if (!extra_ || !pickups.Data || Level(ps) == 0) return;
        int raised = 0;
        for (int i = 0; i < pickups.Num; i++) {
            uint8_t* info = pickups.Data + i * kPickupInfoSize;
            void* da = At<void*>(info, 0);
            if (!da || At<bool>(da, off::DA_bLimited) || !Held(ps, da)) continue;
            uint8_t& level = At<uint8_t>(info, kPickupLevel);
            level = uint8_t(std::min(int(level) + extra_, 255));
            raised++;
        }
        if (raised) CRAB_LOG("Connoisseur: %d of %d chest items are ones the player holds", raised, pickups.Num);
    }

protected:
    float PrimaryBuff() const override { return float(extra_); }

private:
    // A mod or perk the player holds (those are the kinds that stack in levels).
    static bool Held(void* ps, void* da) {
        for (Kind k : {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk})
            if (IsA(da, StaticClass(Info(k).staticClass))) return LevelOf(ps, k, da) > 0;
        return false;
    }

    int extra_ = 1;
};

Connoisseur g_item;

}  // namespace
}  // namespace crabmods
