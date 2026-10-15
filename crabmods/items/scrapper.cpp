// Scrapper, a perk: skipping your reward chest (leaving an island without taking anything from it) 3 islands in a row
// (2 at level 2) makes your next reward chest go up a tier: a pool chest becomes an Epic chest, an Epic chest a
// Legendary chest. Taking from a reward chest starts the count over; a Legendary chest keeps it for the next one.
//
// Host. OnRewardChests notes the island's main reward chest class for each player (its first chest; Double or
// Nothing's copies of it count too) and upgrades one with enough scrap. The chests of that class the player opens
// note the items they offer, and taking one of those (OnPickupTaken) uses the chest. When the next island starts, a
// player who took nothing from them gets 1 scrap, and one who took something loses theirs. Pickups from anywhere
// else don't count. Scrap is kept by player name, since each island has new player states, and starts over with a
// new run.
#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr int kPickupInfoSize = 0x50;
constexpr int kEpicChest = 0x588, kLegendaryChest = 0x590;     // SpawnablesDA
constexpr int kPoolChestFirst = 0x518, kPoolChestLast = 0x550;  // Damage .. Skill
constexpr int kRandomChest = 0x568;

std::string NameOf(void* ps) { return ps ? FStringToUtf8(At<TArray<wchar_t>>(ps, off::PS_PlayerName)) : ""; }

class Scrapper : public AssetItem {
public:
    Scrapper()
        : AssetItem("scrapper", 188,
                    {Kind::Perk, L"DA_Perk_Scrapper", L"Scrapper", L"Skip reward chests to upgrade a later one",
                     L"Skip your reward chest PrimaryBuff islands in a row and your next reward chest goes up a tier "
                     L"(Epic, then Legendary). Taking from one starts over",
                     240, Perk_Collector, pool::Luck},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Scrapper, a perk: skip your reward chest (take nothing from it) enough islands in a row and your next reward
# chest goes up a tier: a pool chest becomes Epic, an Epic chest Legendary. Taking from one starts the count over.
enabled = true

# Islands in a row needed for an upgrade at level 1; each further level needs 1 less (at least 1).
scrapNeeded = 3
# The most levels it can reach; at it, Scrapper is no longer offered.
maxLevel = 2

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "scrapNeeded") need_ = std::max(1, int(ParseFloat(v)));
        else if (k == "maxLevel") max_ = std::max(1, int(ParseFloat(v)));
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::to_string(need_) + " scrap per upgrade (-1 per level), max level " + std::to_string(max_) + ", " +
               RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        if (lm == lm_) return;
        lm_ = lm;
        void* gs = GameStateOf(lm);
        int island = gs ? At<int>(gs, off::GS_CurrentIsland) : 0;
        if (island < island_) scrap_.clear();  // a new run
        island_ = island;
        // The last island's chests: skipped adds a scrap, taken from starts the count over.
        for (auto& [name, is] : islands_) {
            int& s = scrap_[name];
            if (!is.used.empty()) {
                if (s > 0) {
                    CRAB_LOG("Scrapper: %s took from a chest, scrap back to 0", name.c_str());
                    told_[name] = 0;
                }
                s = 0;
                continue;
            }
            if (s >= is.need) continue;
            s++;
            told_[name] = s;
            CRAB_LOG("Scrapper: %s skipped a chest, %d/%d scrap", name.c_str(), s, is.need);
        }
        islands_.clear();
    }

    float LootWeight(void* ps, void* da) override {
        if (!da_ || da != da_) return 1.f;
        return Level(ps) >= max_ ? 0.f : 1.f;
    }

    void OnRewardChests(void* ps, TArray<void*>& chests) override {
        int level = Level(ps);
        if (!level || chests.Num <= 0 || !chests.Data[0]) return;
        void* lm = LevelManagerOf(ps);
        void* sp = lm ? At<void*>(lm, off::LM_SpawnablesDA) : nullptr;
        if (!sp) return;
        std::string name = NameOf(ps);
        int need = Need(level);
        Island& is = islands_[name];
        is = Island{};
        is.need = need;
        void* main = chests.Data[0];
        int& s = scrap_[name];
        void* upgraded = nullptr;
        if (s >= need) upgraded = Upgrade(sp, main);
        for (int i = 0; i < chests.Num; i++) {
            if (chests.Data[i] != main) continue;
            if (upgraded) {
                chests.Data[i] = upgraded;
                upgraded = nullptr;
                s -= need;
                is.classes.insert(chests.Data[i]);
                ShowMessage(ps, L"Scrapper upgraded your chest!");
                CRAB_LOG("Scrapper: %s's chest goes up a tier (%d scrap left)", name.c_str(), s);
            }
            is.classes.insert(chests.Data[i]);
            is.count++;
        }
    }

    void OnChestLoot(void* chest, void* ps, TArray<uint8_t>& pickups) override {
        auto it = islands_.find(NameOf(ps));
        if (it == islands_.end() || !it->second.classes.count(At<void*>(chest, off::Object_Class))) return;
        for (int i = 0; pickups.Data && i < pickups.Num; i++)
            if (void* da = At<void*>(pickups.Data + i * kPickupInfoSize, 0)) it->second.offered[da] = chest;
    }

    void OnPickupTaken(void* pickup, void* c) override {
        void* pc = c ? At<void*>(c, off::Pawn_Controller) : nullptr;
        void* ps = pc ? At<void*>(pc, off::Controller_PlayerState) : nullptr;
        auto it = islands_.find(NameOf(ps));
        if (it == islands_.end()) return;
        auto o = it->second.offered.find(At<void*>(pickup, off::Pickup_PickupInfo + off::PickupInfo_DA));
        if (o != it->second.offered.end()) it->second.used.insert(o->second);
    }

    // The scrap count, shown once the player is in the new island.
    void OnTick(void* world) override {
        if (told_.empty()) return;
        ForEachPlayer(world, [&](void* ps) {
            auto it = told_.find(NameOf(ps));
            if (it == told_.end()) return;
            int need = Need(std::max(Level(ps), 1));
            std::wstring text = L"Scrapper: " + std::to_wstring(std::min(it->second, need)) + L"/" +
                                std::to_wstring(need) + L" scrap";
            ShowMessage(ps, text.c_str());
            told_.erase(it);
        });
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        auto it = scrap_.find(NameOf(ps));
        return CountNote(ps, da, "Scrap", it == scrap_.end() ? 0 : it->second);
    }

protected:
    float PrimaryBuff() const override { return float(need_); }

private:
    struct Island {
        int need = 3;
        int count = 0;                              // reward chests of the main class
        std::set<void*> classes;                    // their classes (the main one, and an upgrade)
        std::unordered_map<void*, void*> offered;   // item asset -> the chest that offered it
        std::set<void*> used;                       // chests something was taken from
    };

    int Need(int level) const { return std::max(1, need_ - (level - 1)); }

    // The chest class one tier up, or nullptr (a Legendary chest, or one that isn't a pool or Epic chest).
    static void* Upgrade(void* sp, void* cls) {
        void* epic = At<void*>(sp, kEpicChest);
        if (cls == epic) return At<void*>(sp, kLegendaryChest);
        for (int o = kPoolChestFirst; o <= kPoolChestLast; o += 8)
            if (At<void*>(sp, o) == cls) return epic;
        return At<void*>(sp, kRandomChest) == cls ? epic : nullptr;
    }

    int need_ = 3, max_ = 2;
    void* lm_ = nullptr;
    int island_ = 0;
    std::unordered_map<std::string, int> scrap_;     // player name -> scrap this run
    std::unordered_map<std::string, Island> islands_;  // player name -> this island's reward chests
    std::unordered_map<std::string, int> told_;      // player name -> scrap to show
};

Scrapper g_item;

}  // namespace
}  // namespace crabmods
