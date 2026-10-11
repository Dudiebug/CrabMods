// Jack of All Trades, a perk: more damage for each different pickup tag among the pickups you hold.
#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class JackOfAllTrades : public AssetItem {
public:
    JackOfAllTrades()
        : AssetItem("jackOfAllTrades", 70,
                    {Kind::Perk, L"DA_Perk_JackOfAllTrades", L"Jack of All Trades",
                     L"Damage increased for each different kind of pickup you have",
                     L"Damage increased by PrimaryBuff% for each different pickup tag you have (Fire, Critical, Speed, ...)",
                     204, Perk_Collector},
                    rarity::Legendary) {}

    std::string DefaultIni() const override {
        return R"(# Jack of All Trades, a perk: more damage for each different pickup tag you have (Healing, Critical,
# Speed, Bounce, Fire, Ice, Lightning, Poison, Arcane, Turret, Combo, ...).
enabled = true

# Damage bonus for each different tag, in percent. Each extra Jack of All Trades stacks it again.
damagePercent = 3

# Which chests it drops from: common, epic, legendary or greed.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per different pickup tag, " + RarityName(rarity_);
    }

    // Jack of All Trades level x the number of different pickup tags among all held pickups.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        return level ? level * Tags(ps) * pct_ * 0.01f : 0.f;
    }
    std::string InventoryNote(void* ps, void* da) override {
        return CountNote(ps, da, "Different pickup tags held", Tags(ps));
    }

private:
    static int Tags(void* ps) {
        uint32_t mask = 0;
        auto scan = [&](int offset) {
            ForEachItem(ps, offset, [&](void* da, int) {
                uint8_t t = At<uint8_t>(da, off::DA_PickupTag);
                if (t == tag::None || t >= 32) return;
                mask |= 1u << t;
            });
        };
        scan(off::PS_WeaponMods);
        scan(off::PS_AbilityMods);
        scan(off::PS_MeleeMods);
        scan(off::PS_Perks);
        scan(off::PS_Relics);
        int count = 0;
        for (; mask; mask &= mask - 1) count++;
        return count;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
};

JackOfAllTrades g_item;

}  // namespace
}  // namespace crabmods
