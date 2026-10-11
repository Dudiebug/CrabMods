// Minimalist, a perk: more damage for each empty weapon mod slot.
#include "../item.hpp"

namespace crabmods {
namespace {

class Minimalist : public AssetItem {
public:
    Minimalist()
        : AssetItem("minimalist", 40,
                    {Kind::Perk, L"DA_Perk_Minimalist", L"Minimalist",
                     L"Damage increased for each empty weapon mod slot",
                     L"Damage increased by PrimaryBuff% for each empty weapon mod slot", 201, Perk_Collector},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Minimalist, a perk: more damage for each empty weapon mod slot.
enabled = true

# Damage bonus for each empty weapon mod slot, in percent. Each extra Minimalist stacks it again.
damagePercent = 5

# Which chests it drops from: epic or legendary (also common or greed).
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per empty weapon mod slot, " + RarityName(rarity_);
    }

    // Minimalist level x empty weapon mod slots. Each weapon mod entry fills one slot, whatever its level.
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        int slots = At<uint8_t>(ps, off::PS_NumWeaponModSlots);
        int used = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void*, int) { used++; });
        int empty = slots > used ? slots - used : 0;
        return level * empty * pct_ * 0.01f;
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        int slots = At<uint8_t>(ps, off::PS_NumWeaponModSlots), used = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void*, int) { used++; });
        return CountNote(ps, da, "Empty weapon mod slots", slots > used ? slots - used : 0);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 5.f;
};

Minimalist g_item;

}  // namespace
}  // namespace crabmods
