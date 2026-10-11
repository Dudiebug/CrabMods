// Thick Shell, a perk: more max health for each perk level you have (Collector, for health).
#include "../item.hpp"

namespace crabmods {
namespace {

class ThickShell : public AssetItem {
public:
    ThickShell()
        : AssetItem("thickShell", 150,
                    {Kind::Perk, L"DA_Perk_ThickShell", L"Thick Shell",
                     L"Max health multiplier increased for each perk level you have",
                     L"Max health multiplier increased by PrimaryBuff% for each perk level you have", 206, Perk_Banana,
                     pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Thick Shell, a perk: more max health for each perk level you have.
enabled = true

# Max health multiplier bonus for each perk level (this one included), in percent. Each extra Thick Shell
# stacks it again. It adds to the multiplier like Banana does.
healthPercent = 3

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "healthPercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% max health per perk level, " + RarityName(rarity_);
    }

    // Thick Shell level x perk levels, counting itself like Collector does. The game recomputes max health
    // whenever the player's items change, so a new perk level shows at once.
    float MaxHealthMultiplierBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        int perks = 0;
        ForEachItem(ps, off::PS_Perks, [&](void*, int l) { perks += l; });
        return level * perks * pct_ * 0.01f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
};

ThickShell g_item;

}  // namespace
}  // namespace crabmods
