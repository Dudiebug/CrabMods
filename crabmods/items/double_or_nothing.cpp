// Double or Nothing, a Greed relic: each chest you open offers twice as many loot choices, or nothing at all.
#include <random>

#include "../item.hpp"

namespace crabmods {
namespace {

class DoubleOrNothing : public AssetItem {
public:
    DoubleOrNothing()
        : AssetItem("doubleOrNothing", 120,
                    {Kind::Relic, L"DA_Relic_DoubleOrNothing", L"Double or Nothing",
                     L"Chests offer twice as many loot choices, or nothing at all",
                     L"Each chest you open has a 50% chance to offer twice as many loot choices, and otherwise "
                     L"offers nothing",
                     210, 63 /* HighRoller */, pool::Greed},
                    rarity::Greed),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Double or Nothing, a Greed relic: each chest you open offers twice as many loot choices, or nothing at
# all. It applies after every other perk and relic that changes how many choices a chest has.
enabled = true

# Chance for twice as many choices, in percent. Otherwise the chest is empty.
doubleChancePercent = 50

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "doubleChancePercent") chance_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% double, otherwise nothing, " + RarityName(rarity_);
    }

    int ChestChoices(void* ps, int n) override {
        if (Level(ps) == 0 || n <= 0) return n;
        float roll = std::uniform_real_distribution<float>(0.f, 100.f)(rng_);
        int result = roll < chance_ ? n * 2 : 0;
        CRAB_LOG("Double or Nothing: %d loot choices -> %d", n, result);
        return result;
    }

protected:
    void Customize(void* da) override {
        std::string c = Num(chance_);
        std::wstring text = L"Each chest you open has a " + std::wstring(c.begin(), c.end()) +
                            L"% chance to offer twice as many loot choices, and otherwise offers nothing";
        SetFString(da, off::DA_LevelDescription, text.c_str());
    }

private:
    float chance_ = 50.f;
    std::minstd_rand rng_;
};

DoubleOrNothing g_item;

}  // namespace
}  // namespace crabmods
