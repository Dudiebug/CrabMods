// Thermal Shock, a weapon mod: hitting a burning enemy with ice, or a frozen one with fire, shatters both for a burst.
//
// Host: after the game works out a player's hit (OnOutgoingDamage, after buffs and crits), the hit's debuffs say
// which elements it is about to apply and the enemy's DebuffState says which it already holds (the hit's own stacks
// land later in TakeDamage). Fire onto ice, or ice onto fire, multiplies the hit and takes all of the enemy's fire
// and ice stacks away, the hit's included. Each enemy can be shocked once per cooldown, whoever shocks it; levels
// shorten the cooldown, down to a floor, after which the mod is no longer offered.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr int kDebuffSize = 8;    // FCrabDebuff
constexpr int kDebuffStacks = 4;  // int32
constexpr uint8_t kIce = 1, kFire = 2;  // ECrabDebuffType
constexpr uint8_t kRadial = 1, kMelee = 3;  // DamageInfo_Type

struct DebuffState {
    int32_t ice, fire, lightning, poison, arcane;
};

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

class ThermalShock : public AssetItem {
public:
    ThermalShock()
        : AssetItem("thermalShock", 93,
                    {Kind::WeaponMod, L"DA_WeaponMod_ThermalShock", L"Thermal Shock",
                     L"Fire on frozen enemies, or ice on burning ones, shatters both",
                     L"Hitting an enemy that has ice stacks with fire, or fire stacks with ice, deals PrimaryBuff% more "
                     L"damage and removes its fire and ice. Once per enemy every few seconds",
                     203, WeaponMod_StreakShot, pool::Elemental, tag::None, 1.f},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Thermal Shock, a weapon mod: when your weapon hits an enemy that has ice stacks with fire, or fire stacks
# with ice, the hit deals more damage and the enemy loses all its fire and ice stacks (and its burn and freeze).
enabled = true

# Extra damage of a shocking hit, in percent of the hit.
bonusPercent = 250
# Extra damage instead for explosion and splash hits, in percent.
splashPercent = 100
# Seconds before the same enemy can be shocked again (by anyone).
cooldown = 1.5
# Each Thermal Shock level after the first divides the cooldown by 1 + this percent more.
cooldownPerLevel = 25
# The shortest the cooldown gets; at it, Thermal Shock is no longer offered.
minCooldown = 0.5

# Which chests it drops from: common, epic or legendary.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "bonusPercent") bonus_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "splashPercent") splash_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "cooldown") cooldown_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "cooldownPerLevel") perLevel_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "minCooldown") minCooldown_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(bonus_) + "% (splash +" + Num(splash_) + "%), " + Num(cooldown_) + " s per enemy (-" +
               Num(perLevel_) + "%/level, min " + Num(minCooldown_) + " s), " + RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        lastShock_.clear();
    }
    void OnEnemyEliminated(void* enemy) override { lastShock_.erase(enemy); }

    // Not offered once the cooldown is at its floor: more levels would do nothing.
    float LootWeight(void* ps, void* da) override {
        if (!da_ || da != da_) return 1.f;
        int level = Level(ps);
        return level && Cooldown(level) <= minCooldown_ ? 0.f : 1.f;
    }

    void OnOutgoingDamage(void* ps, void* info) override {
        int level = Level(ps);
        if (!level) return;
        // Only the holder's own weapon: not their melee, undead, turrets or revive bubble.
        uint8_t type = At<uint8_t>(info, off::DamageInfo_Type);
        void* c = PawnOf(ps);
        if (type == kMelee || !c || At<void*>(info, off::DamageInfo_DamagingC) != c) return;
        void* enemy = At<void*>(info, off::DamageInfo_DamagedActor);
        if (!enemy || enemy == c || !IsA(enemy, StaticClass(va::ACrabC_StaticClass))) return;
        if (IsA(At<void*>(enemy, off::Pawn_PlayerState), StaticClass(va::ACrabPS_StaticClass))) return;

        auto& debuffs = At<TArray<uint8_t>>(info, off::DamageInfo_Debuffs);
        if (!debuffs.Data || debuffs.Num <= 0) return;
        bool fire = false, ice = false;
        for (int i = 0; i < debuffs.Num; i++) {
            uint8_t* d = debuffs.Data + i * kDebuffSize;
            if (At<int>(d, kDebuffStacks) <= 0) continue;
            fire |= d[0] == kFire;
            ice |= d[0] == kIce;
        }
        auto& state = At<DebuffState>(enemy, off::C_DebuffState);
        if (!((fire && state.ice > 0) || (ice && state.fire > 0))) return;

        double now = Now();
        auto it = lastShock_.find(enemy);
        if (it != lastShock_.end() && now - it->second < Cooldown(level)) return;
        lastShock_[enemy] = now;

        At<float>(info, off::DamageInfo_Damage) *= 1.f + (type == kRadial ? splash_ : bonus_) * 0.01f;
        for (int i = 0; i < debuffs.Num; i++) {
            uint8_t* d = debuffs.Data + i * kDebuffSize;
            if (d[0] == kFire || d[0] == kIce) At<int>(d, kDebuffStacks) = 0;
        }
        DebuffState before = state;
        state.ice = state.fire = 0;
        // The host's own screen: clients get the change through replication and run the same function.
        Fn<void (*)(void*, const DebuffState*)>(va::ACrabC_OnRep_DebuffState)(enemy, &before);
        if (shocks_++ < 3) CRAB_LOG("thermalShock: shocked %s (ice %d, fire %d)", ObjectName(enemy).c_str(), before.ice, before.fire);
    }

protected:
    float PrimaryBuff() const override { return bonus_; }

private:
    float Cooldown(int level) const {
        return std::max(minCooldown_, cooldown_ / (1.f + perLevel_ * 0.01f * float(level - 1)));
    }

    float bonus_ = 250.f, splash_ = 100.f, cooldown_ = 1.5f, perLevel_ = 25.f, minCooldown_ = 0.5f;
    std::unordered_map<void*, double> lastShock_;  // enemy -> when it was last shocked
    int shocks_ = 0;                               // logged: the first few only
};

ThermalShock g_item;

}  // namespace
}  // namespace crabmods
