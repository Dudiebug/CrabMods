// Last Crab Standing, a relic: more damage for every downed teammate, and more again when you are the last up.
#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class LastCrabStanding : public AssetItem {
public:
    LastCrabStanding()
        : AssetItem("lastCrabStanding", 110,
                    {Kind::Relic, L"DA_Relic_LastCrabStanding", L"Last Crab Standing",
                     L"Deal more damage for every downed teammate",
                     L"Damage increased by PrimaryBuff% for every downed teammate, and again if you are the last one up",
                     209, Relic_RingOfFury, pool::Damage},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Last Crab Standing, a relic: more damage for every downed teammate. Never drops when you play alone.
enabled = true

# Damage bonus for every downed teammate, in percent.
perDownedPercent = 25

# Extra damage bonus when you are the only one still up, in percent.
lastStandingPercent = 25

# Both stack again with each extra Last Crab Standing.

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "perDownedPercent") perDowned_ = ParseFloat(v);
        else if (k == "lastStandingPercent") last_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(perDowned_) + "% per downed teammate, +" + Num(last_) + "% if last up, " +
               RarityName(rarity_);
    }

    // Last Crab Standing level x (downed teammates x perDownedPercent, plus lastStandingPercent when every
    // teammate is down). Nothing while you are down yourself or when playing alone.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level || IsDown(ps)) return 0.f;
        int n = 0, down = 0;
        ForEachPlayer(ps, [&](void* q) {
            n++;
            if (q != ps && IsDown(q)) down++;
        });
        if (n <= 1) return 0.f;
        float pct = down * perDowned_ + (down == n - 1 ? last_ : 0.f);
        return level * pct * 0.01f;
    }

    // Never offered in a solo game: there is nobody to be downed.
    float LootWeight(void* ps, void* da) override {
        if (da_ && da == da_ && PlayerCount(ps) <= 1) return 0.f;
        return 1.f;
    }

protected:
    float PrimaryBuff() const override { return perDowned_; }

private:
    float perDowned_ = 25.f;
    float last_ = 25.f;
};

LastCrabStanding g_item;

}  // namespace
}  // namespace crabmods
