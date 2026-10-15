// Fixation, a Greed relic: every chest you open offers 1 choice, but it rolls its rarity at x8 luck (Epic 30%,
// Legendary 2% instead of 3.75% / 0.25%; luck from other items is multiplied too, up to 50 in all).
//
// Host: the outermost loot roll of a chest the holder opens gets 1 choice (BeforeLootRoll), and its luck is
// multiplied (LootLuck, after the items that add luck).
#include <algorithm>
#include <cmath>

#include "../item.hpp"
#include "luck_shared.hpp"

namespace crabmods {
namespace {

class Fixation : public AssetItem {
public:
    Fixation()
        : AssetItem("fixation", 194,
                    {Kind::Relic, L"DA_Relic_Fixation", L"Fixation", L"Chests offer one choice, much more likely rare",
                     L"Chests you open offer 1 choice, but it is PrimaryBuff times as likely to be Epic or Legendary",
                     251, 63 /* HighRoller */, pool::Greed, tag::None, 0.3f},
                    rarity::Greed) {}

    std::string DefaultIni() const override {
        return R"(# Fixation, a Greed relic: every chest you open offers 1 choice, but its rarity rolls at much higher luck.
enabled = true

# Luck multiplier (the game's luck is 1 normally: Epic 3.75% x luck, Legendary 0.25% x luck).
luckMultiplier = 8
# The most luck in all, with other items' luck.
maxLuck = 50

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "luckMultiplier") mult_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxLuck") max_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "1 choice per chest at x" + Num(mult_) + " luck (max " + Num(max_) + "), " + RarityName(rarity_);
    }

    void BeforeLootRoll(uint8_t& lootPool, uint8_t& rarity, int& choices) override {
        if (choices > 1 && Level(g_lootPlayer) > 0 && g_lootSource &&
            IsA(g_lootSource, StaticClass(va::ACrabChest_StaticClass)))
            choices = 1;
    }

    void LootLuck(void* ps, void* source, float& m) override {
        if (!Level(ps) || !LuckyChest(source)) return;
        m = std::fmax(m, std::fmin(m * mult_, max_));
    }

protected:
    float PrimaryBuff() const override { return mult_; }

private:
    float mult_ = 8.f, max_ = 50.f;
};

Fixation g_item;

}  // namespace
}  // namespace crabmods
