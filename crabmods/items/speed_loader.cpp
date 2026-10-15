// Speed Loader, a melee mod: a melee swing that hits an enemy refills 25% of your weapon's magazine per level (at most
// once per 1.5 s).
//
// The host sees the hit (OnOutgoingDamage, melee); ammo is counted by the machine that controls the player (it isn't
// replicated, see autoloader.cpp), so the host refills its own player's weapons and tells other players' CrabMods to
// refill theirs.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kMelee = 3;  // DamageInfo_Type
constexpr wchar_t kTag[] = L"speedLoader ";  // host to player: "speedLoader <fraction>"

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

class SpeedLoader : public AssetItem {
public:
    SpeedLoader()
        : AssetItem("speedLoader", 199,
                    {Kind::MeleeMod, L"DA_MeleeMod_SpeedLoader", L"Speed Loader", L"Melee hits reload your weapon",
                     L"Melee hits refill PrimaryBuff% of your weapon's magazine", 240, MeleeMod_BigClaws, pool::Speed,
                     tag::None, 0.75f},
                    rarity::Common) {}

    std::string DefaultIni() const override {
        return R"(# Speed Loader, a melee mod: a melee swing that hits an enemy refills part of your weapon's magazine.
# Weapons that never reload get nothing.
enabled = true

# Part of the magazine refilled per level, in percent.
percentPerLevel = 25
# The most levels it can reach; at it, Speed Loader is no longer offered.
maxLevel = 4
# Seconds between two refills, at least.
cooldown = 1.5

# Which chests it drops from: common, epic, legendary or greed.
rarity = common
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "percentPerLevel") pct_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxLevel") max_ = std::max(1, int(ParseFloat(v)));
        else if (k == "cooldown") cooldown_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(pct_) + "% of the magazine per level per melee hit, max level " + std::to_string(max_) + ", every " +
               Num(cooldown_) + " s at most, " + RarityName(rarity_);
    }

    float LootWeight(void* ps, void* da) override {
        if (!da_ || da != da_) return 1.f;
        return Level(ps) >= max_ ? 0.f : 1.f;
    }

    void OnOutgoingDamage(void* ps, void* info) override {
        if (At<uint8_t>(info, off::DamageInfo_Type) != kMelee) return;
        void* c = PawnOf(ps);
        if (!c || At<void*>(info, off::DamageInfo_DamagingC) != c) return;
        int level = std::min(Level(ps), max_);
        if (!level) return;
        double now = Now();
        double& last = last_[ps];
        if (now - last < cooldown_) return;
        last = now;
        float f = std::fmin(pct_ * level / 100.f, 1.f);
        if (Local(c)) {
            Refill(c, f);
        } else if (void* pc = At<void*>(c, off::Pawn_Controller)) {
            SendToPlayer(pc, kTag + std::to_wstring(f));
        }
    }

    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, std::size(kTag) - 1, kTag) != 0) return false;
        void* ps = At<void*>(pc, off::Controller_PlayerState);
        void* c = PawnOf(ps);
        float f = 0.f;
        try {
            f = std::stof(text.substr(std::size(kTag) - 1));
        } catch (...) {
            return true;
        }
        if (c && Local(c)) Refill(c, std::fmin(std::fmax(f, 0.f), 1.f));
        return true;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    // The character's weapons are controlled from this machine (OwningPC is only set there).
    static bool Local(void* c) {
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; i < weapons.Num; i++)
            if (weapons.Data[i] && At<void*>(weapons.Data[i], off::Weapon_OwningPC)) return true;
        return false;
    }

    // Adds a fraction of each weapon's clip, as a finished reload does with all of it.
    static void Refill(void* c, float f) {
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; i < weapons.Num; i++) {
            void* w = weapons.Data[i];
            void* da = w ? At<void*>(w, off::Weapon_WeaponDA) : nullptr;
            if (!da || At<bool>(da, off::WeaponDA_bInfiniteClipSize) || At<bool>(w, off::Weapon_bIsReloading)) continue;
            int clip = At<int>(w, off::Weapon_ClipSize);
            int& ammo = At<int>(w, off::Weapon_Ammo);
            if (clip <= 0 || ammo >= clip) continue;
            ammo = std::min(clip, ammo + std::max(1, int(std::ceil(clip * f))));
            Fn<void (*)(void*)>(va::ACrabWeapon_OnAmmoChanged)(w);
        }
    }

    float pct_ = 25.f, cooldown_ = 1.5f;
    int max_ = 4;
    std::unordered_map<void*, double> last_;  // player -> their last refill
};

SpeedLoader g_item;

}  // namespace
}  // namespace crabmods
