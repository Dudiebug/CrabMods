// Lucky Charm, a Legendary relic: the items in the chests you open have a chance to be level 2.
//
// Host: an opened chest rolls its loot for the opener into its PickupsToSpawn and spawns them right after
// (main.cpp hands the list to OnChestLoot in between). Each level-1 weapon, ability or melee mod or perk in it
// gets the chance; relics and weapons stay level 1.
//
// The game's pickup card never shows a level, so every machine adds it to the name of a pickup above level 1
// ("Vitality (Lv 2)").
#include <random>
#include <iterator>
#include <string>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

constexpr int kPickupInfoSize = 0x50;  // FCrabPickupInfo
constexpr int kPickupLevel = 0x08;     // its FCrabInventoryInfo.Level (uint8)

class LuckyCharm : public AssetItem {
public:
    LuckyCharm()
        : AssetItem("luckyCharm", 182,
                    {Kind::Relic, L"DA_Relic_LuckyCharm", L"Lucky Charm", L"Items from chests can be level 2",
                     L"Items from the chests you open have a PrimaryBuff% chance to be level 2", 212, Relic_FireRing,
                     pool::Luck},
                    rarity::Legendary),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Lucky Charm, a relic: the mods and perks in the chests you open have a chance to be level 2.
enabled = true

# Chance for each item in a chest to be level 2, in percent.
chancePercent = 25

# Which chests it drops from: common, epic, legendary or greed.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chancePercent") chance_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% for chest items to be level 2, " + std::string(RarityName(rarity_));
    }

    void OnChestLoot(void* chest, void* ps, TArray<uint8_t>& pickups) override {
        if (Level(ps) == 0 || chance_ <= 0.f || !pickups.Data) return;
        int upgraded = 0;
        for (int i = 0; i < pickups.Num; i++) {
            uint8_t* info = pickups.Data + i * kPickupInfoSize;
            void* da = At<void*>(info, 0);
            uint8_t& level = At<uint8_t>(info, kPickupLevel);
            if (!da || level != 1 || !Levels(da)) continue;
            if (std::uniform_real_distribution<float>(0.f, 100.f)(rng_) >= chance_) continue;
            level = 2;
            upgraded++;
        }
        if (upgraded) CRAB_LOG("Lucky Charm: %d of %d chest items are level 2", upgraded, pickups.Num);
    }

    void InstallHooks() override {
        Hook(va::ACrabInteractPickup_SetupUI, &HkSetupUI, &oSetupUI, "ACrabInteractPickup::SetupUI");
    }

protected:
    float PrimaryBuff() const override { return chance_; }

private:
    using SetupUIFn = void (*)(void*);
    static inline SetupUIFn oSetupUI;

    // After the game fills the card from the DA, puts the pickup's level after its name. A charm's card (any
    // charm, this is the only hook on the card) says "Charm" in the charms' colour.
    static void HkSetupUI(void* pickup) {
        GreedRunCard(pickup, true);
        oSetupUI(pickup);
        LevelCard(pickup);
        GreedRunCard(pickup, false);
    }
    static void LevelCard(void* pickup) {
        void* card = At<void*>(pickup, off::Pickup_UI);
        void* da = At<void*>(pickup, off::Pickup_PickupInfo + off::PickupInfo_DA);
        uint8_t level = At<uint8_t>(pickup, off::Pickup_PickupInfo + off::PickupInfo_Level);
        if (card && IsCharm(da)) {
            wchar_t type[] = L"Charm";
            TArray<wchar_t> ts{type, int(std::size(type)), int(std::size(type))};
            Fn<void (*)(void*, const void*)>(va::InteractUI_SetType)(card, &ts);
            Fn<void (*)(void*, const float*)>(va::InteractUI_SetRarityColor)(card, kCharmColor);
        }
        if (!card || !da || level < 2) return;
        const auto& name = At<TArray<wchar_t>>(da, off::DA_Name);
        std::wstring s = name.Data && name.Num > 1 ? std::wstring(name.Data, name.Num - 1) : L"";
        s += L" (Lv " + std::to_wstring(level) + L")";
        TArray<wchar_t> fs{s.data(), int(s.size()) + 1, int(s.size()) + 1};
        Fn<void (*)(void*, const void*)>(va::InteractUI_SetName)(card, &fs);
    }

    // Kinds of item that stack in levels.
    static bool Levels(void* da) {
        if (At<bool>(da, off::DA_bLimited)) return false;
        for (Kind k : {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk})
            if (IsA(da, StaticClass(Info(k).staticClass))) return true;
        return false;
    }

    float chance_ = 25.f;
    std::minstd_rand rng_;
};

LuckyCharm g_item;

}  // namespace
}  // namespace crabmods
