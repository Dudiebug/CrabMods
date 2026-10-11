// Autoloader, a perk: each kill has a chance to reload your weapon instantly.
//
// Ammo is counted by the machine that controls the player (it isn't replicated), so that machine does it: four times
// a second it looks at the player's kill count (replicated) and rolls once per new kill. A success fills the clip of
// every weapon that isn't full or reloading, as a finished reload does, and updates the HUD.
#include <cmath>
#include <random>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Autoloader : public AssetItem {
public:
    Autoloader()
        : AssetItem("autoloader", 135,
                    {Kind::Perk, L"DA_Perk_Autoloader", L"Autoloader", L"Kills can reload your weapon instantly",
                     L"Each kill has a PrimaryBuff% chance to instantly reload your weapon", 213, Perk_Collector,
                     pool::Speed},
                    rarity::Epic),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Autoloader, a perk: each kill has a chance to reload your weapon instantly.
enabled = true

# The chance per kill, in percent. More levels raise it hyperbolically: with 10, two levels give 16.7%, three 23.1%.
chancePercent = 10

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chancePercent") chance_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% per kill to reload instantly, " + std::string(RarityName(rarity_));
    }

    void OnTick(void* world) override {
        ForEachPlayer(world, [&](void* ps) {
            int kills = At<int>(ps, off::PS_Eliminations);
            auto [it, fresh] = kills_.try_emplace(ps, kills);
            int last = it->second;
            it->second = kills;
            if (fresh || kills <= last || !da_) return;
            void* c = PawnOf(ps);
            if (!c || !Local(c)) return;
            int level = Level(ps);
            if (!level) return;
            float p = 1.f - 1.f / (1.f + chance_ / 100.f * level);
            bool reload = false;
            for (int i = 0; i < kills - last && !reload; i++)
                reload = Chance(p) || (EncoreRoll(ps) && Chance(p));
            if (reload) Reload(c);
        });
    }

protected:
    float PrimaryBuff() const override { return chance_; }

private:
    // The character's weapons are controlled from this machine (OwningPC is only set there).
    static bool Local(void* c) {
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; i < weapons.Num; i++)
            if (weapons.Data[i] && At<void*>(weapons.Data[i], off::Weapon_OwningPC)) return true;
        return false;
    }

    static void Reload(void* c) {
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; i < weapons.Num; i++) {
            void* w = weapons.Data[i];
            void* da = w ? At<void*>(w, off::Weapon_WeaponDA) : nullptr;
            if (!da || At<bool>(da, off::WeaponDA_bInfiniteClipSize) || At<bool>(w, off::Weapon_bIsReloading)) continue;
            int clip = At<int>(w, off::Weapon_ClipSize);
            int& ammo = At<int>(w, off::Weapon_Ammo);
            if (clip <= 0 || ammo >= clip) continue;
            ammo = clip;
            Fn<void (*)(void*)>(va::ACrabWeapon_OnAmmoChanged)(w);
        }
    }

    bool Chance(float p) { return std::uniform_real_distribution<float>(0.f, 1.f)(rng_) < p; }

    float chance_ = 10.f;
    std::minstd_rand rng_;
    std::unordered_map<void*, int> kills_;  // player -> kill count at the last tick
};

Autoloader g_item;

}  // namespace
}  // namespace crabmods
