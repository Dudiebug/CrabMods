// Culling Blow, a melee mod: a melee hit that leaves an enemy under 10% health per level (at most 30%) kills it
// outright. Elites: half that. Bosses: never.
//
// Host: on a player's melee hit (OnOutgoingDamage, after their buffs), if the health it would leave is under the
// threshold, the hit's damage is raised to what the enemy has left (armor included), so the hit itself kills and
// the kill is theirs like any other.
#include <algorithm>
#include <cmath>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kMelee = 3;  // DamageInfo_Type
constexpr uint8_t kElite = 4, kBoss = 5;  // Enemy_Category
constexpr wchar_t kEnemyClassPath[] = L"/Script/CrabChampions.CrabEnemyC";

class CullingBlow : public AssetItem {
public:
    CullingBlow()
        : AssetItem("cullingBlow", 191,
                    {Kind::MeleeMod, L"DA_MeleeMod_CullingBlow", L"Culling Blow", L"Melee finishes off weak enemies",
                     L"Melee hits kill enemies left under PrimaryBuff% health (half for elites, not bosses)", 241,
                     MeleeMod_BigClaws, pool::Damage, tag::None, 0.75f},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Culling Blow, a melee mod: a melee hit that leaves an enemy under a share of its health kills it outright.
# Elites need half that; bosses are never culled.
enabled = true

# Health threshold per level, in percent of the enemy's max health.
percentPerLevel = 10
# The most levels it can reach; at it, Culling Blow is no longer offered.
maxLevel = 3
# Elites' threshold, in percent of the normal one.
elitePercent = 50

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "percentPerLevel") pct_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxLevel") max_ = std::max(1, int(ParseFloat(v)));
        else if (k == "elitePercent") elite_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(pct_) + "% health per level, max level " + std::to_string(max_) + ", elites " + Num(elite_) +
               "% of that, " + RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        enemyClass_ = enemyClass_ ? enemyClass_ : ui::FindObject(kEnemyClassPath);
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
        void* e = At<void*>(info, off::DamageInfo_DamagedActor);
        if (!e || !enemyClass_ || !IsA(e, enemyClass_) || IsUndead(e)) return;
        uint8_t cat = At<uint8_t>(e, off::Enemy_Category);
        if (cat == kBoss) return;
        float t = pct_ * level / 100.f * (cat == kElite ? elite_ / 100.f : 1.f);
        void* hc = At<void*>(e, off::C_HC);
        if (!hc || t <= 0.f) return;
        float health = At<float>(hc, off::HC_CurrentHealth), max = At<float>(hc, off::HC_CurrentMaxHealth);
        float armor = std::max(At<int>(hc, off::HC_ArmorPlates), 0) * std::fmax(At<float>(hc, off::HC_ArmorPlateHealth), 0.f);
        float& dmg = At<float>(info, off::DamageInfo_Damage);
        if (health <= 0.f || max <= 0.f || armor > 0.f) return;  // armored enemies: break the armor first
        float left = health - dmg;
        if (left <= 0.f || left / max >= t) return;
        dmg = health * 1.05f + 1.f;
        if (logged_++ < 5) CRAB_LOG("Culling Blow: %s culled at %.0f%% health", ObjectName(e).c_str(), left / max * 100.f);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 10.f, elite_ = 50.f;
    int max_ = 3;
    void* enemyClass_ = nullptr;
    int logged_ = 0;
};

CullingBlow g_item;

}  // namespace
}  // namespace crabmods
