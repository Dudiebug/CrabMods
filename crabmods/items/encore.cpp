// Encore, an Epic relic: the perks that go off by themselves have a chance to go off a second time.
//
// Host. The explosions some perks set off go through RadialDamage, called from the game's on-kill code (Exploding,
// Freezing, Flammable, Electric and Poisonous Enemies) and its on-hit code (Explosive, Poisonous and Flammable
// Armor); each call is told apart by its return address, and a lucky one is made a second time at the same spot.
// Grave Dodger saving you can be given back for the island. The other items ask EncoreRoll for their own second
// chances: a failed Rigged Totems or Autoloader roll is rolled again, and Finishing Move pays its bonus twice.
#include <cmath>
#include <random>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Encore : public AssetItem {
public:
    Encore()
        : AssetItem("encore", 186,
                    {Kind::Relic, L"DA_Relic_Encore", L"Encore", L"Your perks can go off twice",
                     L"Perks that go off by themselves (on-kill and armor explosions, Grave Dodger, ...) have a "
                     L"PrimaryBuff% chance to go off a second time",
                     214, Relic_FireRing},
                    rarity::Epic),
          rng_(std::random_device{}()) {
        self_ = this;
    }

    std::string DefaultIni() const override {
        return R"(# Encore, a relic: the perks that go off by themselves have a chance to go off a second time: on-kill
# explosions (Exploding, Freezing, Flammable, Electric, Poisonous Enemies), armor (Explosive, Poisonous, Flammable),
# Grave Dodger (it can save you again that island), and the CrabMods perks Rigged Totems, Autoloader (a failed roll
# is rolled again) and Finishing Move (its bonus counts twice).
enabled = true

# The chance, in percent.
chancePercent = 15

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chancePercent") chance_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% for perks to go off twice, " + std::string(RarityName(rarity_));
    }

    void InstallHooks() override {
        Hook(va::RadialDamage, &HkRadial, &oRadial, "RadialDamage");
        Hook(va::ACrabPlayerC_GraveDodgerHeal, &HkGraveDodger, &oGraveDodger, "Grave Dodger heal");
    }

    bool Roll(void* ps) {
        if (!enabled_ || !da_ || !ps || chance_ <= 0.f || Level(ps) == 0) return false;
        return std::uniform_real_distribution<float>(0.f, 100.f)(rng_) < chance_;
    }

protected:
    float PrimaryBuff() const override { return chance_; }

private:
    using RadialFn = void (*)(void*, TArray<void*>*, const Vec3*, float, const void*, bool, void*);
    using HealFn = void (*)(void*);
    static inline RadialFn oRadial;
    static inline HealFn oGraveDodger;
    static inline Encore* self_;

    static void* PlayerOf(void* c) {
        if (!IsA(c, StaticClass(va::ACrabC_StaticClass))) return nullptr;
        void* ps = At<void*>(c, off::Pawn_PlayerState);
        return IsA(ps, StaticClass(va::ACrabPS_StaticClass)) ? ps : nullptr;
    }

    // The player whose perk set off this explosion, or null for any other explosion.
    static void* PerkOwner(uintptr_t ret, void* source) {
        switch (ret) {
        case va::Ret_RadialExplodingEnemies:
        case va::Ret_RadialFreezingEnemies:
        case va::Ret_RadialFlammableEnemies:
        case va::Ret_RadialElectricEnemies:
        case va::Ret_RadialPoisonousEnemies:
            return source ? PlayerOf(At<void*>(source, off::Enemy_EliminatingC)) : nullptr;
        case va::Ret_RadialExplosiveArmor:
        case va::Ret_RadialPoisonousArmor:
        case va::Ret_RadialFlammableArmor:
            return PlayerOf(source);
        }
        return nullptr;
    }

    // The ignore list is passed by value (the game frees it), so the second explosion gets its own copy.
    static void HkRadial(void* source, TArray<void*>* ignore, const Vec3* loc, float radius, const void* info,
                         bool los, void* filter) {
        uintptr_t ret = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        void* ps = loc && info ? PerkOwner(ret, source) : nullptr;
        if (!ps || !self_->Roll(ps)) return oRadial(source, ignore, loc, radius, info, los, filter);
        TArray<void*> again{};
        if (ignore)
            for (int i = 0; i < ignore->Num; i++) Append(again, ignore->Data[i]);
        Vec3 at = *loc;
        oRadial(source, ignore, loc, radius, info, los, filter);
        oRadial(source, &again, &at, radius, info, los, filter);
    }

    // Grave Dodger just saved the player (TakeDamage marked it used for the island); Encore can unmark it.
    static void HkGraveDodger(void* c) {
        oGraveDodger(c);
        void* ps = PlayerOf(c);
        void* hc = ps ? At<void*>(c, off::C_HC) : nullptr;
        if (!hc || !self_->Roll(ps)) return;
        At<uint8_t>(hc, off::HC_bDeathProtectionUsed) = 0;
        CRAB_LOG("Encore: Grave Dodger can save %p again", ps);
        ShowMessage(ps, L"Encore! Grave Dodger can save you again this island");
    }

    float chance_ = 15.f;
    std::minstd_rand rng_;
};

Encore g_item;

}  // namespace

bool EncoreRoll(void* ps) { return g_item.Roll(ps); }

}  // namespace crabmods
