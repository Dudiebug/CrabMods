// Portal Shot, a perk in Portal Amulet's place: your weapon's shots are fired from the nearest enemy, as with the
// relic, and each level adds damage, so it can be upgraded. The relic itself no longer drops while this is on.
//
// Every machine: the game's weapon asks HasRelic(player, Portal Amulet) when it fires; holding the perk answers
// yes (Item::GrantsRelic).
#include "../item.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kPortalAmulet = 54;  // ECrabRelicType::PortalAmulet

class PortalShot : public AssetItem {
public:
    PortalShot()
        : AssetItem("portalShot", 186,
                    {Kind::Perk, L"DA_Perk_PortalShot", L"Portal Shot",
                     L"Weapon shots are fired from the nearest enemy",
                     L"Weapon shots are fired from the nearest enemy instead of from your weapon, and deal "
                     L"PrimaryBuff% more damage",
                     250, Perk_Collector, pool::Skill},
                    rarity::Legendary) {
        g_grantedRelics[kPortalAmulet] = true;
    }

    std::string DefaultIni() const override {
        return R"(# Portal Shot, a perk in place of the Portal Amulet relic: your weapon's shots are fired from the nearest
# enemy instead of from your weapon. Being a perk, it levels up. While it's on, Portal Amulet doesn't drop.
enabled = true

# Damage bonus for each level, in percent.
damagePercent = 20

# Which chests it drops from: common, epic, legendary or greed.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "shots from the nearest enemy, +" + Num(pct_) + "% damage per level, " + RarityName(rarity_);
    }

    bool GrantsRelic(void* ps, uint8_t type) override { return type == kPortalAmulet && Level(ps) > 0; }
    float DamageBonus(void* ps) override { return Level(ps) * pct_ * 0.01f; }
    float LootWeight(void* ps, void* da) override {
        bool amulet = da && IsA(da, StaticClass(va::UCrabRelicDA_StaticClass)) &&
                      At<uint8_t>(da, off::DA_ItemType) == kPortalAmulet;
        return amulet ? 0.f : 1.f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }
    void Customize(void* da) override { At<bool>(da, off::DA_bGetBuffAsMultiplier) = false; }

private:
    float pct_ = 20.f;
};

PortalShot g_item;

}  // namespace
}  // namespace crabmods
