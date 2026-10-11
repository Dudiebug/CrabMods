// Arsenal, a weapon mod: more damage for each other weapon mod you have.
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
                     L"Shots deal more damage for each weapon mod you have",
                     L"Damage increased by PrimaryBuff% for each other weapon mod you have", 200,
                     WeaponMod_StreakShot},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Arsenal, a weapon mod: more damage for each other weapon mod you have.
enabled = true

# Damage bonus for each other weapon mod you hold, in percent. Each extra Arsenal stacks it again.
damagePerModPercent = 3

# Which chests it drops from: common, epic or legendary.
rarity = epic

# true: a x3 stack of a mod counts as three mods (for Arsenal and Hoarder). false: each different mod
# counts once.
countStacks = false
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePerModPercent") pct_ = ParseFloat(v);
        else if (k == "countStacks") countStacks_ = ParseBool(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per other weapon mod, " + RarityName(rarity_) +
               (countStacks_ ? ", stacks count" : "");
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

    // Arsenal level x other weapon mods.
    float DamageBonus(void* ps) override {
        if (!da_) return 0.f;
        int level = 0, others = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void* da, int l) {
            if (da == da_) level += l;
            else others += countStacks_ ? l : 1;
        });
        return level * others * pct_ * 0.01f;
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        int others = 0;
        ForEachItem(ps, off::PS_WeaponMods, [&](void* d, int l) {
            if (d != da_) others += countStacks_ ? l : 1;
        });
        return CountNote(ps, da, "Other weapon mods counted", others);
    }

    bool countStacks_ = false;

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    float pct_ = 3.f;
    bool warned_ = false;
};

Arsenal g_item;

}  // namespace

bool ArsenalCountStacks() { return g_item.countStacks_; }

}  // namespace crabmods
