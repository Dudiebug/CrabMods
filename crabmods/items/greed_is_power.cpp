// Greed Is Power, a Greed perk: more damage for each Greed perk level you have, this one included.
#include "../item.hpp"

namespace crabmods {
namespace {

class GreedIsPower : public AssetItem {
public:
    GreedIsPower()
        : AssetItem("greedIsPower", 80,
                    {Kind::Perk, L"DA_Perk_GreedIsPower", L"Greed Is Power",
                     L"Damage increased for each Greed perk level you have",
                     L"Damage increased by PrimaryBuff% for each Greed perk level you have", 205,
                     109 /* GlassCannon */, pool::Greed},
                    rarity::Greed) {}

    std::string DefaultIni() const override {
        return R"(# Greed Is Power, a Greed perk: more damage for each Greed perk level you have, this one included.
enabled = true

# Damage bonus for each Greed perk level, in percent. Each extra Greed Is Power stacks it again.
damagePercent = 5

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per Greed perk level, " + RarityName(rarity_);
    }

    // Greed Is Power level x Greed perk levels, always counting itself even if the ini moves it out of Greed.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        int greedLevels = 0;
        ForEachItem(ps, off::PS_Perks, [&](void* da, int l) {
            if (At<uint8_t>(da, off::DA_Rarity) == rarity::Greed || da == da_) greedLevels += l;
        });
        return level * greedLevels * pct_ * 0.01f;
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        int greedLevels = 0;
        ForEachItem(ps, off::PS_Perks, [&](void* d, int l) {
            if (At<uint8_t>(d, off::DA_Rarity) == rarity::Greed || d == da_) greedLevels += l;
        });
        return CountNote(ps, da, "Greed perk levels counted", greedLevels);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 5.f;
};

GreedIsPower g_item;

}  // namespace
}  // namespace crabmods
