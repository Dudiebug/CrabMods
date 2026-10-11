// Purist, a relic: more damage while every elemental pickup you hold is the same element.
#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Purist : public AssetItem {
public:
    Purist()
        : AssetItem("purist", 90,
                    {Kind::Relic, L"DA_Relic_Purist", L"Purist",
                     L"Damage increased while all your elemental pickups share one element",
                     L"Damage increased by PrimaryBuff% while all your elemental pickups are the same element", 207,
                     Relic_FireRing, pool::Elemental, tag::None},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Purist, a relic: more damage while every elemental pickup you hold is the same element (Fire, Ice,
# Lightning, Poison or Arcane).
enabled = true

# Damage bonus in percent. Each extra Purist level stacks it again.
damagePercent = 10

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% with a single element, " + RarityName(rarity_);
    }

    // Purist level x the bonus, only while the elemental pickups held (all inventory kinds) are one element.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        return level && Elements(ps) == 1 ? level * pct_ * 0.01f : 0.f;
    }
    std::string InventoryNote(void* ps, void* da) override {
        return CountNote(ps, da, "Elements held", Elements(ps));
    }

private:
    static int Elements(void* ps) {
        bool seen[5] = {};
        auto scan = [&](void* da, int) {
            uint8_t t = At<uint8_t>(da, off::DA_PickupTag);
            if (t == tag::Fire) seen[0] = true;
            else if (t == tag::Ice) seen[1] = true;
            else if (t == tag::Lightning) seen[2] = true;
            else if (t == tag::Poison) seen[3] = true;
            else if (t == tag::Arcane) seen[4] = true;
        };
        ForEachItem(ps, off::PS_WeaponMods, scan);
        ForEachItem(ps, off::PS_AbilityMods, scan);
        ForEachItem(ps, off::PS_MeleeMods, scan);
        ForEachItem(ps, off::PS_Perks, scan);
        ForEachItem(ps, off::PS_Relics, scan);
        int distinct = 0;
        for (bool s : seen) distinct += s;
        return distinct;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 10.f;
};

Purist g_item;

}  // namespace
}  // namespace crabmods
