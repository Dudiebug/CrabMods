// Hoarder, a perk: more damage for each weapon mod level and each perk level you have (Arsenal + Collector).
#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Hoarder : public AssetItem {
public:
    Hoarder()
        : AssetItem("hoarder", 20,
                    {Kind::Perk, L"DA_Perk_Hoarder", L"Hoarder",
                     L"Damage increased for each weapon mod and perk level you have",
                     L"Damage increased by PrimaryBuff% for each weapon mod level and perk level you have", 200,
                     Perk_Collector},
                    rarity::Legendary) {}

    std::string DefaultIni() const override {
        return R"(# Hoarder, a perk: more damage for each weapon mod level and each perk level you have.
enabled = true

# Damage bonus for each weapon mod level and each perk level, in percent. Each extra Hoarder stacks it again.
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
        return "+" + Num(pct_) + "% per weapon mod and perk level, " + RarityName(rarity_);
    }

    // Hoarder level x (weapon mod levels + perk levels), counting itself like Collector does.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        int count = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void*, int l) { count += l; });
        ForEachItem(ps, off::PS_Perks, [&](void*, int l) { count += l; });
        return level * count * pct_ * 0.01f;
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        int count = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void*, int l) { count += l; });
        ForEachItem(ps, off::PS_Perks, [&](void*, int l) { count += l; });
        return CountNote(ps, da, "Weapon mod and perk levels counted", count);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
};

Hoarder g_item;

}  // namespace
}  // namespace crabmods
