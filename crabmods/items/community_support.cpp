// Community Support, a perk: everyone near a holder fires faster, one Rapid Fire level for each holder in range
// (yourself included), so it stacks when the players near you have it too.
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class CommunitySupport : public AssetItem {
public:
    CommunitySupport()
        : AssetItem("communitySupport", 160,
                    {Kind::Perk, L"DA_Perk_CommunitySupport", L"Community Support",
                     L"You and the players near you fire faster",
                     L"Fire rate increased by PrimaryBuff% for you and every player near you. Stacks for each "
                     L"nearby player with Community Support",
                     207, Perk_Collector, pool::Speed},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Community Support, a perk: you and every player near you fire faster. Each player in range who has
# it (you included) adds one Rapid Fire's worth, so it stacks when your friends have it too.
enabled = true

# Fire rate bonus for each Community Support in range, in percent (Rapid Fire's is 12). Each extra level a
# player has counts again.
fireRatePercent = 12

# How close the players must be, in meters.
rangeMeters = 20

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "fireRatePercent") pct_ = ParseFloat(v);
        else if (k == "rangeMeters") range_ = ParseFloat(v) * 100.f;  // 100 units to the meter
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% fire rate per holder within " + Num(range_ / 100.f) + " m, " +
               RarityName(rarity_);
    }

    // Community Support levels of every player up and within range of this one (this one included).
    float FireRateBonus(void* ps) override {
        if (!da_) return 0.f;
        Vec3 me;
        if (!LocationOf(PawnOf(ps), me)) return 0.f;
        int n = 0;
        ForEachPlayer(ps, [&](void* q) {
            int level = Level(q);
            Vec3 at;
            if (!level || IsDown(q) || !LocationOf(PawnOf(q), at)) return;
            if (DistanceSquared(me, at) <= range_ * range_) n += level;
        });
        return n * pct_ * 0.01f;
    }

    // The game only recomputes fire rate when items change, so players moving in and out of range are checked
    // here and their weapons updated when their bonus changes.
    void OnTick(void* world) override {
        if (!da_) return;
        ForEachPlayer(world, [&](void* ps) {
            float b = FireRateBonus(ps);
            auto [it, fresh] = applied_.try_emplace(ps, 0.f);
            if (it->second == b) return;
            it->second = b;
            RefreshFireRate(ps);
        });
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 12.f;
    float range_ = 2000.f;
    std::unordered_map<void*, float> applied_;  // the bonus each player's weapons have now
};

CommunitySupport g_item;

}  // namespace
}  // namespace crabmods
