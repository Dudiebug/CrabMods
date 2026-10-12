// Overflow, a perk: healing you get at full health is not wasted. Every 100 health healed past your max adds damage
// until the island ends.
//
// The host counts the healing. The bonus (in percent) is mirrored into the perk's own AccumulatedBuff, which the game
// replicates, so every machine sees it; but the game also carries that value to the next island, so the host keeps
// its own per-island count and puts it back there (as Collateral Damage does).
#include <cmath>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class Overflow : public AssetItem {
public:
    Overflow()
        : AssetItem("overflow", 87,
                    {Kind::Perk, L"DA_Perk_Overflow", L"Overflow",
                     L"Healing at full health increases damage for the rest of the island",
                     L"Damage increased by PrimaryBuff% for every 100 health healed while at full health, until the "
                     L"island ends",
                     209, Perk_Collector, pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Overflow, a perk: healing past your max health builds up a damage bonus until the island ends.
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
        return "+" + Num(pct_) + "% damage per " + Num(per_) + " health healed past max this island, " +
               RarityName(rarity_);
    }

    // Host: counts the healing past max, and each full stack adds level x damagePercent to this island's bonus.
    void OnOverheal(void* ps, float amount) override {
        int level = Level(ps);
        if (!level) return;
        Track(ps);
        Island& is = island_[ps];
        is.pool += amount;
        int stacks = int(is.pool / per_);
        if (stacks <= 0) return;
        is.pool -= stacks * per_;
        is.bonus += stacks * level * pct_;
        Mirror(ps, is.bonus);
        CRAB_LOG("Overflow: player %p now +%g%% damage this island", ps, is.bonus);
    }

    float DamageBonus(void* ps) override {
        if (!da_) return 0.f;
        if (At<uint8_t>(ps, off::Actor_Role) == ROLE_Authority) {
            Track(ps);
            auto it = island_.find(ps);
            return it == island_.end() ? 0.f : it->second.bonus * 0.01f;
        }
        uint8_t* e = EntryOf(ps, off::PS_Perks, da_);
        float v = e ? *reinterpret_cast<float*>(e + off::Item_AccumulatedBuff) : 0.f;
        return std::isfinite(v) && v > 0.f ? v * 0.01f : 0.f;
    }

    // Host: a new island starts the count over, and the perk's replicated value is kept at this island's count
    // (the game restores the last island's value when it loads the players' items).
    void OnTick(void* world) override {
        if (!da_) return;
        ForEachPlayer(world, [&](void* ps) {
            if (At<uint8_t>(ps, off::Actor_Role) != ROLE_Authority) return;
            Track(ps);
            auto it = island_.find(ps);
            Mirror(ps, it == island_.end() ? 0.f : it->second.bonus);
        });
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    struct Island {
        float pool = 0.f;   // healing past max not yet worth a stack
        float bonus = 0.f;  // percent
    };

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

    float pct_ = 3.f;
    float per_ = 100.f;
    void* lm_ = nullptr;
    std::unordered_map<void*, Island> island_;
};

Overflow g_item;

}  // namespace
}  // namespace crabmods
