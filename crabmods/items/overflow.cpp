// Overflow, a perk: healing you get at full health is not wasted. Every 100 health healed past your max gives a
// permanent damage bonus for the rest of the run.
//
// The bonus is kept in the perk's own AccumulatedBuff (in percent), which the game replicates to every player
// and carries from island to island, so it lasts the run and every machine sees it.
#include <unordered_map>

#include "../item.hpp"

namespace crabmods {
namespace {

class Overflow : public AssetItem {
public:
    Overflow()
        : AssetItem("overflow", 87,
                    {Kind::Perk, L"DA_Perk_Overflow", L"Overflow",
                     L"Healing at full health permanently increases damage",
                     L"Damage permanently increased by PrimaryBuff% for every 100 health healed while at full health",
                     209, Perk_Collector, pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Overflow, a perk: healing past your max health builds up a permanent damage bonus for the run.
enabled = true

# Damage bonus for every healthPerStack health healed past your max, in percent. Each extra Overflow level
# makes every new stack worth this much more.
damagePercent = 3

# How much healing past your max health one stack takes.
healthPerStack = 100

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else if (k == "healthPerStack") per_ = ParseFloat(v) > 1.f ? ParseFloat(v) : 1.f;
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% damage per " + Num(per_) + " health healed past max, " + RarityName(rarity_);
    }

    // Host: counts the healing past max, and each full stack adds level x damagePercent to the perk's bonus.
    void OnOverheal(void* ps, float amount) override {
        int level = Level(ps);
        if (!level) return;
        float& pool = pool_[ps];
        pool += amount;
        int stacks = int(pool / per_);
        if (stacks <= 0) return;
        pool -= stacks * per_;
        float add = stacks * level * pct_;
        Fn<void (*)(void*, uint8_t, float)>(va::ACrabPS_AddPerkAccumulatedBuff)(ps, spec_.type, add);
        uint8_t* e = EntryOf(ps, off::PS_Perks, da_);
        CRAB_LOG("Overflow: player %p +%g%% damage (now +%g%%)", ps, add,
                 e ? *reinterpret_cast<float*>(e + off::Item_AccumulatedBuff) : 0.f);
    }

    float DamageBonus(void* ps) override {
        uint8_t* e = da_ ? EntryOf(ps, off::PS_Perks, da_) : nullptr;
        return e ? *reinterpret_cast<float*>(e + off::Item_AccumulatedBuff) * 0.01f : 0.f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
    float per_ = 100.f;
    std::unordered_map<void*, float> pool_;  // healing past max not yet worth a stack, per player
};

Overflow g_item;

}  // namespace
}  // namespace crabmods
