// Bullet Rain and Hellshooter, weapon mods: your shots no longer leave your gun. Bullet Rain drops them from the
// sky onto the spot you aim at; Hellshooter fires them up out of the ground under it.
//
// Both act on the machine that controls the player, whose projectiles deal the damage; the game sends the other
// players a copy of each shot with the same start and direction.
// - Bullet Rain switches the weapon to the game's own "above the crosshair" firing (Ice Staff's) for each shot:
//   the camera's aim point, 2500 units up (under any ceiling), falling straight down.
// - Hellshooter traces from the camera to the aim point, finds the ground under it, and starts each projectile
//   just above the ground, flying straight up.
// With both, the shots alternate: every other one falls from the sky, the rest come up out of the ground.
#include <random>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class ShotMod : public AssetItem {
public:
    ShotMod(const char* section, int order, const AssetSpec& spec, const char* ini)
        : AssetItem(section, order, spec, rarity::Epic), ini_(ini) {}

    std::string DefaultIni() const override { return ini_; }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% damage, " + RarityName(rarity_);
    }
    float DamageBonus(void* ps) override { return Level(ps) * pct_ * 0.01f; }

protected:
    float PrimaryBuff() const override { return pct_; }

    const char* ini_;
    float pct_ = 10.f;
};

class BulletRain : public ShotMod {
public:
    BulletRain()
        : ShotMod("bulletRain", 180,
                  {Kind::WeaponMod, L"DA_WeaponMod_BulletRain", L"Bullet Rain",
                   L"Your shots rain down from the sky onto where you aim",
                   L"Shots fall from the sky onto where you aim, and deal PrimaryBuff% more damage", 201,
                   WeaponMod_StreakShot},
                  R"(# Bullet Rain, a weapon mod: your shots fall from the sky onto the spot you aim at, the way Ice Staff
# fires, instead of leaving your gun.
enabled = true

# Damage bonus, in percent. Each extra Bullet Rain stacks it again.
damagePercent = 10

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)") {}

    bool ShotsFromAbove(void* ps) override { return Level(ps) > 0; }
};

class Hellshooter : public ShotMod {
public:
    explicit Hellshooter(const BulletRain& rain)
        : ShotMod("hellshooter", 185,
                  {Kind::WeaponMod, L"DA_WeaponMod_Hellshooter", L"Hellshooter",
                   L"Your shots erupt from the ground under where you aim",
                   L"Shots fire up out of the ground under where you aim, and deal PrimaryBuff% more damage", 202,
                   WeaponMod_StreakShot},
                  R"(# Hellshooter, a weapon mod: your shots fire straight up out of the ground under the spot you aim at,
# instead of leaving your gun.
enabled = true

# Damage bonus, in percent. Each extra Hellshooter stacks it again.
damagePercent = 10

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)"),
          rain_(rain),
          rng_(std::random_device{}()) {}

    void OnShot(void* weapon, void* ps, Vec3& start, Vec3& dir) override {
        if (!Level(ps)) return;
        // With Bullet Rain too, every other shot is left to fall from the sky.
        if (rain_.Enabled() && rain_.Level(ps) && (alternate_[ps] ^= true)) return;
        void* c = At<void*>(weapon, off::Weapon_OwningC);
        if (!IsA(c, StaticClass(va::ACrabPlayerC_StaticClass))) return;
        alignas(16) uint8_t hit[off::Hit_Size] = {};
        At<float>(hit, off::Hit_Time) = 1.f;
        Fn<void (*)(void*, void*, float)>(va::ACrabPlayerC_CameraTrace)(c, hit, kRange);
        Vec3 at = (hit[off::Hit_bBlockingHit] & 1) ? At<Vec3>(hit, off::Hit_ImpactPoint) : At<Vec3>(hit, off::Hit_TraceEnd);
        Fn<void (*)(void*, Vec3*)>(va::SnapToGround)(c, &at);
        // Spread out like the game's own falling shots: the weapon's spread x 50 units each way.
        float j = At<float>(weapon, off::Weapon_Spread) * 50.f;
        if (j > 0.f) {
            std::uniform_real_distribution<float> d(-j, j);
            at.x += d(rng_);
            at.y += d(rng_);
        }
        start = {at.x, at.y, at.z + 20.f};
        dir = {0.f, 0.f, 1.f};
    }

private:
    static constexpr float kRange = 7500.f;  // as far as the game's falling shots aim when nothing is hit
    const BulletRain& rain_;
    std::minstd_rand rng_;
    std::unordered_map<void*, bool> alternate_;  // per player: the last shot was left to Bullet Rain
};

BulletRain g_rain;
Hellshooter g_hell(g_rain);

}  // namespace
}  // namespace crabmods
