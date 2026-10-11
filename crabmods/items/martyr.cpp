// Martyr, a relic: when you go down you explode, damaging the enemies around you. It is the game's own
// explosion (as Explosive Armor's), so Magnify makes it bigger and the kills are yours.
#include <cstring>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class Martyr : public AssetItem {
public:
    Martyr()
        : AssetItem("martyr", 100,
                    {Kind::Relic, L"DA_Relic_Martyr", L"Martyr", L"When you go down, you explode",
                     L"When you go down, explode for PrimaryBuff damage to enemies around you. Magnify increases "
                     L"its size",
                     208, Relic_RingOfDefense, pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Martyr, a relic: when you go down you explode, damaging the enemies around you. Magnify makes the
# explosion bigger, like the game's own explosions.
enabled = true

# Explosion damage for each Martyr level. It falls off to 20% at the edge, like the game's explosions.
damage = 150

# Explosion radius before Magnify, in meters (Explosive Armor's is 25).
radiusMeters = 25

# How hard it pushes enemies away (Explosive Armor's is 2500).
knockback = 2500

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damage") damage_ = ParseFloat(v);
        else if (k == "radiusMeters") radius_ = ParseFloat(v) * 100.f;  // 100 units to the meter
        else if (k == "knockback") knockback_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(damage_) + " damage within " + Num(radius_ / 100.f) + " m when you go down, " +
               RarityName(rarity_);
    }

    // Host, while the downed character still has its player state: it is the explosion's source, so only
    // enemies are hurt, the radius gets its Magnify and the kills count for the player.
    void OnDown(void* c, void* ps) override {
        int level = Level(ps);
        if (!level) return;
        Vec3 at{};
        Fn<Vec3* (*)(void*, Vec3*)>(va::ACrabC_GetCoreLocation)(c, &at);
        alignas(8) uint8_t info[off::DamageInfo_Size];
        std::memset(info, 0, sizeof info);  // no tags or debuffs
        At<float>(info, off::DamageInfo_Damage) = damage_ * level;
        At<uint8_t>(info, off::DamageInfo_Type) = 4;  // Lesser: not throttled per enemy, like Shockwave's
        At<Vec3>(info, off::DamageInfo_Location) = at;
        At<float>(info, off::DamageInfo_Knockback) = knockback_;
        At<void*>(info, off::DamageInfo_DamagingC) = c;
        TArray<void*> ignore{};  // passed by value: the game frees it, so it stays empty
        Fn<void (*)(void*, TArray<void*>*, const Vec3*, float, const void*, bool, void*)>(va::RadialDamage)(
            c, &ignore, &at, radius_, info, false, nullptr);
        void* lm = At<void*>(c, off::C_LM);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        if (sp)
            Fn<void (*)(void*, void*, void*, const Vec3*)>(va::ACrabLM_MulticastPlayOneShotFX)(
                lm, At<void*>(sp, off::Spawnables_GenericExplosionFX),
                At<void*>(sp, off::Spawnables_GenericExplosionSound), &at);
        CRAB_LOG("Martyr: player %p went down and exploded for %g", ps, damage_ * level);
    }

protected:
    float PrimaryBuff() const override { return damage_; }

private:
    float damage_ = 150.f;
    float radius_ = 2500.f;
    float knockback_ = 2500.f;
};

Martyr g_item;

}  // namespace
}  // namespace crabmods
