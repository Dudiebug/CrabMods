// Double or Nothing, a Greed relic: at the end of each island you get twice as many reward chests, or none.
// The host builds each player's reward chests (bonus chests, Care Package and totems included) and then rolls
// High Roller; this rolls just before that, so High Roller still works on what is left.
#include <random>

#include "../item.hpp"

namespace crabmods {
namespace {

class DoubleOrNothing : public AssetItem {
public:
    DoubleOrNothing()
        : AssetItem("doubleOrNothing", 120,
                    {Kind::Relic, L"DA_Relic_DoubleOrNothing", L"Double or Nothing",
                     L"Twice as many island reward chests, or none at all",
                     L"At the end of each island, a 50% chance of twice as many reward chests, and otherwise none",
                     210, 63 /* HighRoller */, pool::Greed},
                    rarity::Greed),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Double or Nothing, a Greed relic: at the end of each island you get twice as many reward chests, or
# none at all. It counts every chest the island gives you (bonus chests, Care Package, totems) and rolls before
# High Roller.
enabled = true

# Chance for twice as many chests, in percent. Otherwise you get none.
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
        return Num(chance_) + "% double the island's chests, otherwise none, " + RarityName(rarity_);
    }

    void OnRewardChests(void* ps, TArray<void*>& chests) override {
        if (Level(ps) == 0 || chests.Num <= 0) return;
        int n = chests.Num;
        bool win = std::uniform_real_distribution<float>(0.f, 100.f)(rng_) < chance_;
        if (win)
            for (int i = 0; i < n; i++) Append(chests, chests.Data[i]);
        else
            chests.Num = 0;  // the game frees the array afterwards
        ShowMessage(ps, win ? L"Double or Nothing doubled your chests!" : L"Double or Nothing took all your chests!");
        CRAB_LOG("Double or Nothing: %d reward chests -> %d", n, chests.Num);
    }

protected:
    void Customize(void* da) override {
        std::string c = Num(chance_);
        std::wstring text = L"At the end of each island, a " + std::wstring(c.begin(), c.end()) +
                            L"% chance of twice as many reward chests, and otherwise none";
        SetFString(da, off::DA_LevelDescription, text.c_str());
    }

private:
    float chance_ = 50.f;
    std::minstd_rand rng_;
};

DoubleOrNothing g_item;

}  // namespace
}  // namespace crabmods
