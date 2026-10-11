// Collateral Damage, a perk: every destructible you break (pots, barrels, rocks, crystals, nests, ...) during an
// island's fight adds to your damage until the island ends, like Tasty Orange does for kills.
//
// The host counts the breaks. The bonus (in percent) is mirrored into the perk's own AccumulatedBuff, which the game
// replicates, so every machine sees it; but the game also carries that value to the next island, so the host keeps
// its own per-island count and puts it back there.
#include <cmath>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class CollateralDamage : public AssetItem {
public:
    CollateralDamage()
        : AssetItem("collateralDamage", 85,
                    {Kind::Perk, L"DA_Perk_CollateralDamage", L"Collateral Damage",
                     L"Breaking things increases damage for the rest of the island",
                     L"Damage increased by PrimaryBuff% for every destructible you break, until the island ends", 208,
                     Perk_Collector, pool::Damage},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Collateral Damage, a perk: every pot, barrel, rock, crystal or nest you break during an island's fight
# adds damage until the island ends.
enabled = true

# Damage bonus for every destructible broken, in percent. Each extra level makes every new break worth this
# much more.
damagePercent = 5

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% damage per destructible broken this island, " + RarityName(rarity_);
    }

    // Host.
    void OnDestructibleBroken(void* ps) override {
        int level = Level(ps);
        if (!level) return;
        Track(ps);
        float& b = island_[ps];
        b += level * pct_;
        Mirror(ps, b);
    }

    float DamageBonus(void* ps) override {
        if (!da_) return 0.f;
        if (At<uint8_t>(ps, off::Actor_Role) == ROLE_Authority) {
            Track(ps);
            auto it = island_.find(ps);
            return it == island_.end() ? 0.f : it->second * 0.01f;
        }
        uint8_t* e = EntryOf(ps, off::PS_Perks, da_);
        float v = e ? *reinterpret_cast<float*>(e + off::Item_AccumulatedBuff) : 0.f;
        return std::isfinite(v) && v > 0.f ? v * 0.01f : 0.f;  // CrabSync 1.0.5 and older could wreck it
    }

    // Host: a new island starts the count over, and the perk's replicated value is kept at this island's count
    // (the game restores the last island's value when it loads the players' items).
    void OnTick(void* world) override {
        if (!da_) return;
        ForEachPlayer(world, [&](void* ps) {
            if (At<uint8_t>(ps, off::Actor_Role) != ROLE_Authority) return;
            Track(ps);
            auto it = island_.find(ps);
            Mirror(ps, it == island_.end() ? 0.f : it->second);
        });
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    // Forgets every count when the level manager changes (a new island).
    void Track(void* ps) {
        void* lm = LevelManagerOf(ps);
        if (lm == lm_) return;
        lm_ = lm;
        island_.clear();
    }
    void Mirror(void* ps, float value) {
        uint8_t* e = EntryOf(ps, off::PS_Perks, da_);
        if (!e) return;
        float& acc = *reinterpret_cast<float*>(e + off::Item_AccumulatedBuff);
        if (acc == value) return;
        Fn<void (*)(void*, uint8_t, float)>(va::ACrabPS_AddPerkAccumulatedBuff)(ps, spec_.type, value - acc);
    }

    float pct_ = 5.f;
    void* lm_ = nullptr;
    std::unordered_map<void*, float> island_;  // this island's bonus per player, in percent
};

CollateralDamage g_item;

}  // namespace
}  // namespace crabmods
