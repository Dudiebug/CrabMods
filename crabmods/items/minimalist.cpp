// Minimalist, a perk: a big damage bonus that shrinks for each different pickup tag you hold (Jack of All Trades
// the other way round): it pays for staying focused on a few kinds of pickup.
#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Minimalist : public AssetItem {
public:
    Minimalist()
        : AssetItem("minimalist", 40,
                    {Kind::Perk, L"DA_Perk_Minimalist", L"Minimalist",
                     L"More damage the fewer different kinds of pickup you have",
                     L"Damage increased by PrimaryBuff%, minus a share for each different pickup tag you have (Fire, "
                     L"Critical, Speed, ...)",
                     201, Perk_Collector},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Minimalist, a perk: a damage bonus that shrinks for each different pickup tag you have (Healing, Critical,
# Speed, Bounce, Fire, Ice, Lightning, Poison, Arcane, Turret, Combo, ...). Jack of All Trades the other way round.
enabled = true

# Damage bonus with no tags at all, in percent.
maxPercent = 40

# Taken off it for each different tag, in percent (never below 0). With 40 and 4, five tags leave +20% and ten
# leave nothing. Each extra Minimalist stacks what is left again.
perTagPercent = 4

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "maxPercent") max_ = std::max(ParseFloat(v), 0.f);
        else if (k == "perTagPercent") perTag_ = std::max(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(max_) + "% minus " + Num(perTag_) + "% per different pickup tag, " + RarityName(rarity_);
    }

    // Minimalist level x what is left of the bonus after the tags.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        return level ? level * Left(ps) * 0.01f : 0.f;
    }
    std::string InventoryNote(void* ps, void* da) override {
        return CountNote(ps, da, "Different pickup tags held", DistinctTags(ps));
    }

protected:
    float PrimaryBuff() const override { return max_; }

private:
    float Left(void* ps) const { return std::max(max_ - perTag_ * float(DistinctTags(ps)), 0.f); }
    float max_ = 40.f, perTag_ = 4.f;
};

Minimalist g_item;

}  // namespace
}  // namespace crabmods
