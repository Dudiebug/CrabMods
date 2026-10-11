// Enhanced Turrets as an ability mod: the game's Enhanced Turrets perk (more turret damage) moved to where the
// turret's other upgrades are. The perk itself no longer drops while this is on.
//
// Every machine: a turret's projectiles take their damage bonus from ACrabPS::GetPerkBuff(EnhancedTurrets); the
// ability mod's buff is added to that lookup (Item::PerkBuff). It copies the perk's buff, pickup tag (so it only
// drops for turret abilities), loot pool and buff rules from the game's asset.
#include "../item.hpp"

namespace crabmods {
namespace {

class EnhancedTurrets : public AssetItem {
public:
    EnhancedTurrets()
        : AssetItem("enhancedTurrets", 187,
                    {Kind::AbilityMod, L"DA_AbilityMod_EnhancedTurrets", L"Enhanced Turrets",
                     L"Your turrets deal more damage", L"Turrets deal PrimaryBuff% more damage", 210,
                     AbilityMod_HeatSink},
                    rarity::Common) {
        g_grantedPerks[Perk_EnhancedTurrets] = true;
    }

    std::string DefaultIni() const override {
        return R"(# Enhanced Turrets, an ability mod in place of the game's Enhanced Turrets perk: your turrets deal more
# damage. It drops like the perk did (only with a turret ability), but as an ability mod. While it's on, the
# perk doesn't drop.
enabled = true

# Turret damage bonus for each level, in percent. Left out, it is the perk's own.
# damagePercent = 25

# Which chests it drops from: common, epic, legendary or greed.
rarity = common
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v), pctSet_ = true;
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "turret damage " + (pctSet_ ? "+" + Num(pct_) + "% per level" : std::string("as the perk")) + ", " +
               RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        if (!perk_) {
            perk_ = FindByType(Spawnables(lm, off::Spawnables_Perks), Perk_EnhancedTurrets);
            if (perk_) {
                if (!pctSet_) pct_ = At<float>(perk_, off::DA_BaseBuff);
                CRAB_LOG("Enhanced Turrets: the perk's buff is %g (multiplier %d, hyperbolic %d), tag %d",
                         At<float>(perk_, off::DA_BaseBuff), At<bool>(perk_, off::DA_bGetBuffAsMultiplier),
                         At<bool>(perk_, off::DA_bHyperbolicBuff), At<uint8_t>(perk_, off::DA_PickupTag));
            }
        }
        AssetItem::OnBeginPlay(lm);
    }

    float PerkBuff(void* ps, uint8_t type) override {
        if (type != Perk_EnhancedTurrets || !da_ || !Level(ps)) return 0.f;
        return Fn<float (*)(void*, uint8_t)>(va::InvArr_GetBuff)(static_cast<uint8_t*>(ps) + off::PS_AbilityMods,
                                                                  spec_.type);
    }
    float LootWeight(void* ps, void* da) override { return da && da == perk_ ? 0.f : 1.f; }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_ || !ps) return {};
        return "Current turret damage bonus: +" + Num(Round(PerkBuff(ps, Perk_EnhancedTurrets) * 100.f)) + "%";
    }

protected:
    float PrimaryBuff() const override { return pct_; }
    // The perk's buff rules and drop conditions.
    void Customize(void* da) override {
        if (!perk_) return;
        At<bool>(da, off::DA_bGetBuffAsMultiplier) = At<bool>(perk_, off::DA_bGetBuffAsMultiplier);
        At<bool>(da, off::DA_bHyperbolicBuff) = At<bool>(perk_, off::DA_bHyperbolicBuff);
        At<uint8_t>(da, off::DA_PickupTag) = At<uint8_t>(perk_, off::DA_PickupTag);
        At<bool>(da, off::DA_bRequiresMatchingPickupTag) = At<bool>(perk_, off::DA_bRequiresMatchingPickupTag);
        At<uint8_t>(da, off::DA_LootPool) = At<uint8_t>(perk_, off::DA_LootPool);
    }

private:
    float pct_ = 25.f;
    bool pctSet_ = false;
    void* perk_ = nullptr;  // the game's Enhanced Turrets perk
};

EnhancedTurrets g_item;

}  // namespace
}  // namespace crabmods
