// Shrapnel, a Legendary ability mod: each ability use fires a ring of 12 of your weapon's projectiles (+6 per level,
// at most 36) from where it explodes, with all your weapon mods. One ring per use.
//
// On the machine that controls the player (the ability's projectiles deal damage only there): the ability firing
// (ACrabPlayerC::SpawnAbility) arms a ring, and the first explosion of one of its projectiles (ExplodeDamage, type
// Ability) uses it. The next frame the ring is fired through the game's SpawnProjectile, owned by the player's weapon
// as a full weapon shot, so the weapon's mods, damage and elements apply and the other machines see copies.
#include <algorithm>
#include <cmath>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kAbility = 2;  // DamageInfo_Type

class Shrapnel : public AssetItem {
public:
    Shrapnel()
        : AssetItem("shrapnel", 197,
                    {Kind::AbilityMod, L"DA_AbilityMod_Shrapnel", L"Shrapnel",
                     L"Your ability's blast fires your weapon's projectiles",
                     L"Each ability use fires a ring of PrimaryBuff of your weapon's projectiles from where it explodes",
                     241, AbilityMod_HeatSink, pool::Skill, tag::None, 0.75f},
                    rarity::Legendary) {
        g_self = this;
    }

    std::string DefaultIni() const override {
        return R"(# Shrapnel, an ability mod: each ability use fires a ring of your weapon's projectiles from where it
# explodes, with all your weapon mods. One ring per use, however many explosions it makes.
enabled = true

# Projectiles in the ring at level 1.
projectiles = 12
# More projectiles for each level after the first.
projectilesPerLevel = 6
# The most projectiles in a ring.
maxProjectiles = 36

# Which chests it drops from: common, epic, legendary or greed.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "projectiles") base_ = std::max(0, int(ParseFloat(v)));
        else if (k == "projectilesPerLevel") per_ = std::max(0, int(ParseFloat(v)));
        else if (k == "maxProjectiles") max_ = std::clamp(int(ParseFloat(v)), 0, 360);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::to_string(base_) + " projectiles (+" + std::to_string(per_) + " per level, max " +
               std::to_string(max_) + ") per ability use, " + RarityName(rarity_);
    }

    void InstallHooks() override {
        Hook(va::ACrabPlayerC_SpawnAbility, &HkSpawnAbility, &oSpawnAbility, "ACrabPlayerC::SpawnAbility");
        Hook(va::ACrabProjectile_ExplodeDamage, &HkExplodeDamage, &oExplodeDamage, "ACrabProjectile::ExplodeDamage");
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        armed_ = nullptr;
        rings_.clear();
    }

    void OnFrame(void* world, float) override {
        if (rings_.empty()) return;
        std::vector<Ring> rings;
        rings.swap(rings_);
        for (const Ring& r : rings) Fire(r);
    }

protected:
    float PrimaryBuff() const override { return float(std::min(base_, max_)); }

private:
    struct Ring {
        void* c;
        Vec3 at;
        int n;
    };

    using SpawnAbilityFn = void (*)(void*);
    using ExplodeDamageFn = uint64_t (*)(void*, const Vec3*, float, void*);
    static inline SpawnAbilityFn oSpawnAbility;
    static inline ExplodeDamageFn oExplodeDamage;
    static inline Shrapnel* g_self = nullptr;

    static void HkSpawnAbility(void* c) {
        if (g_self && g_self->Enabled()) g_self->armed_ = c;
        oSpawnAbility(c);
    }

    static uint64_t HkExplodeDamage(void* proj, const Vec3* origin, float radius, void* info) {
        Vec3 at{};
        bool mine = false;
        if (g_self && g_self->Enabled() && g_self->armed_ && proj && origin && info &&
            At<uint8_t>(info, off::DamageInfo_Type) == kAbility &&
            At<void*>(info, off::DamageInfo_DamagingC) == g_self->armed_) {
            at = *origin;
            mine = true;
        }
        uint64_t r = oExplodeDamage(proj, origin, radius, info);
        if (mine) g_self->Arm(at);
        return r;
    }

    void Arm(const Vec3& at) {
        void* c = armed_;
        armed_ = nullptr;
        void* ps = c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
        int level = Level(ps);
        if (!level) return;
        int n = std::min(max_, base_ + per_ * (level - 1));
        if (n > 0) rings_.push_back({c, at, n});
    }

    void Fire(const Ring& r) {
        if (At<uint8_t>(r.c, off::C_bIsEliminated) & 1) return;
        void* weapon = nullptr;
        auto& weapons = At<TArray<void*>>(r.c, off::C_Weapons);
        for (int i = 0; i < weapons.Num && !weapon; i++)
            if (void* w = weapons.Data[i]; w && At<void*>(w, off::Weapon_OwningPC) && At<void*>(w, off::Weapon_WeaponDA))
                weapon = w;
        if (!weapon) return;
        void* da = At<void*>(At<void*>(weapon, off::Weapon_WeaponDA), off::WeaponDA_ProjectileDA);
        if (!da) return;
        constexpr float kTwoPi = 6.2831853f;
        for (int i = 0; i < r.n; i++) {
            float a = kTwoPi * i / r.n;
            Vec3 dir{std::cos(a), std::sin(a), 0.f};
            alignas(8) uint8_t info[off::ProjInfo_Size] = {};
            At<void*>(info, off::ProjInfo_DA) = da;
            At<Vec3>(info, off::ProjInfo_Dir) = dir;
            At<void*>(info, off::ProjInfo_OwningC) = r.c;
            At<uint8_t>(info, off::ProjInfo_LesserType) = 0;
            At<uint8_t>(info, off::ProjInfo_bIsParentLesser) = 1;
            Vec3 start{r.at.x + dir.x * 40.f, r.at.y + dir.y * 40.f, r.at.z + 60.f};
            Fn<void* (*)(void*, void*, Vec3*, bool)>(va::SpawnProjectile)(info, weapon, &start, true);
        }
        if (logged_++ < 5) CRAB_LOG("Shrapnel: a ring of %d at %.0f %.0f %.0f", r.n, r.at.x, r.at.y, r.at.z);
    }

    int base_ = 12, per_ = 6, max_ = 36;
    void* armed_ = nullptr;  // the character whose ability just fired and hasn't exploded yet
    std::vector<Ring> rings_;
    int logged_ = 0;
};

Shrapnel g_item;

}  // namespace
}  // namespace crabmods
