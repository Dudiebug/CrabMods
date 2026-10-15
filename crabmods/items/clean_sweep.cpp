// Clean Sweep, an Epic perk: clearing an island without taking damage (the game's own flawless) makes that island's
// reward chests roll at +5 luck per level (Epic 3.75% x luck, Legendary 0.25% x luck; luck is 1 normally).
//
// Host: when the island's reward chests are built (OnRewardChests), a player who took no damage on it (the game's
// DamageTakenOnThisIsland; Parkour islands are never flawless) has those chest classes noted; their rolls on this
// island get the luck.
#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"
#include "luck_shared.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kParkour = 8;  // GS_CurrentIslandType

class CleanSweep : public AssetItem {
public:
    CleanSweep()
        : AssetItem("cleanSweep", 193,
                    {Kind::Perk, L"DA_Perk_CleanSweep", L"Clean Sweep", L"Flawless islands give luckier reward chests",
                     L"After an island without taking damage, its reward chests are more likely to give Epic and "
                     L"Legendary items (+PrimaryBuff luck)",
                     242, Perk_Collector, pool::Luck},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Clean Sweep, a perk: clearing an island without taking damage makes its reward chests luckier. The
# game's luck for each choice (1 normally: Epic 3.75% x luck, Legendary 0.25% x luck) gets this much per level.
enabled = true

# Luck per level.
luckPerLevel = 5
# The most levels it can reach; at it, Clean Sweep is no longer offered.
maxLevel = 4

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "luckPerLevel") per_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxLevel") max_ = std::max(1, int(ParseFloat(v)));
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(per_) + " luck per level on flawless islands' chests, max level " + std::to_string(max_) +
               ", " + RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        flawless_.clear();
    }

    float LootWeight(void* ps, void* da) override {
        if (!da_ || da != da_) return 1.f;
        return Level(ps) >= max_ ? 0.f : 1.f;
    }

    void OnRewardChests(void* ps, TArray<void*>& chests) override {
        if (!Level(ps) || At<int>(ps, off::PS_DamageTakenOnThisIsland) != 0) return;
        void* gs = GameStateOf(ps);
        if (!gs || At<uint8_t>(gs, off::GS_CurrentIslandType) == kParkour) return;
        auto& classes = flawless_[ps];
        for (int i = 0; i < chests.Num; i++) classes.insert(chests.Data[i]);
        ShowMessage(ps, L"Clean Sweep: flawless! Your chests are luckier");
    }

    void LootLuck(void* ps, void* source, float& m) override {
        auto it = flawless_.find(ps);
        if (it == flawless_.end() || !LuckyChest(source) || !it->second.count(At<void*>(source, off::Object_Class)))
            return;
        m += per_ * std::min(Level(ps), max_);
    }

protected:
    float PrimaryBuff() const override { return per_; }

private:
    float per_ = 5.f;
    int max_ = 4;
    std::unordered_map<void*, std::set<void*>> flawless_;  // player -> their flawless island's reward chest classes
};

CleanSweep g_item;

}  // namespace
}  // namespace crabmods
