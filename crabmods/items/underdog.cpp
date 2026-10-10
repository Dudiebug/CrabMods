// Underdog, a perk: more damage the more health you are missing.
#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class Underdog : public AssetItem {
public:
    Underdog()
        : AssetItem("underdog", 50,
                    {Kind::Perk, L"DA_Perk_Underdog", L"Underdog",
                     L"Damage increased the more health you are missing",
                     L"Damage increased by PrimaryBuff% for every 1% of health you are missing", 202,
                     74 /* HealthIsPower */},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Underdog, a perk: more damage the more health you are missing.
enabled = true

# Damage bonus for every 1% of health you are missing, in percent: 0.5 gives +25% at half health. Each
# extra Underdog stacks it again.
damagePercent = 0.5

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per 1% health missing, " + RarityName(rarity_);
    }

    // Underdog level x percent of health missing x damagePercent.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        float f = HealthFraction(ps);
        if (f < 0.f) return 0.f;
        if (f > 1.f) f = 1.f;
        float missing = (1.f - f) * 100.f;
        return level * missing * pct_ * 0.01f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 0.5f;
};

Underdog g_item;

}  // namespace
}  // namespace crabmods
