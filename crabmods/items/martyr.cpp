// Martyr, a relic: while you are down, your teammates deal more damage.
#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class Martyr : public AssetItem {
public:
    Martyr()
        : AssetItem("martyr", 100,
                    {Kind::Relic, L"DA_Relic_Martyr", L"Martyr",
                     L"While you are down, your teammates deal more damage",
                     L"While you are down, your teammates deal PrimaryBuff% more damage", 208,
                     Relic_RingOfDefense, pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Martyr, a relic: while you are down, your teammates deal more damage.
enabled = true

# Damage bonus for each teammate while you are down, in percent. Each extra Martyr stacks it again, and so do
# several downed Martyr holders.
damagePercent = 15

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% for teammates while down, " + RarityName(rarity_);
    }

    // Martyr level of every other downed player x damagePercent, for a player who is still up.
    float DamageBonus(void* ps) override {
        if (IsDown(ps)) return 0.f;
        int sum = 0;
        ForEachPlayer(ps, [&](void* q) {
            if (q == ps || !IsDown(q)) return;
            sum += Level(q);
        });
        return sum * pct_ * 0.01f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 15.f;
};

Martyr g_item;

}  // namespace
}  // namespace crabmods
