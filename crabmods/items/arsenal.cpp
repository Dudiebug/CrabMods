// Arsenal, a weapon mod: more damage for each level of the other weapon mods you have (Collector, for weapon mods).
#include <windows.h>

#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class Arsenal : public AssetItem {
public:
    Arsenal()
        : AssetItem("arsenal", 10,
                    {Kind::WeaponMod, L"DA_WeaponMod_Arsenal", L"Arsenal",
                     L"Shots deal more damage for each weapon mod level you have",
                     L"Damage increased by PrimaryBuff% for each level of your other weapon mods", 200,
                     WeaponMod_StreakShot},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Arsenal, a weapon mod: more damage for each level of the other weapon mods you have (a x3 stack is 3).
enabled = true

# Damage bonus for each level of the other weapon mods you hold, in percent. Each extra Arsenal stacks it again.
damagePerModPercent = 3

# Which chests it drops from: common, epic or legendary.
rarity = epic

)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePerModPercent") pct_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per other weapon mod level, " + RarityName(rarity_);
    }

    // The old standalone Arsenal.dll makes the same asset; with both installed only it does.
    void OnBeginPlay(void* lm) override {
        if (GetModuleHandleW(L"Arsenal.dll")) {
            if (!warned_) CRAB_LOG("the old Arsenal.dll is also installed: delete it, CrabMods includes Arsenal");
            warned_ = true;
            return;
        }
        AssetItem::OnBeginPlay(lm);
    }

    // Arsenal level x the other weapon mods' levels.
    float DamageBonus(void* ps) override {
        if (!da_) return 0.f;
        int level = 0, others = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void* da, int l) {
            if (da == da_) level += l;
            else others += l;
        });
        return level * others * pct_ * 0.01f;
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        int others = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void* d, int l) {
            if (d != da_) others += l;
        });
        return CountNote(ps, da, "Other weapon mod levels counted", others);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
    bool warned_ = false;
};

Arsenal g_item;

}  // namespace

}  // namespace crabmods
