// Snap Decision, a Greed perk: once in each biome, a coin flip. Heads, half the items in your inventory (mods and
// perks) double in level; tails, half of them are gone.
//
// Host: the flip is at the first island cleared in each biome (OnRewardChests, before the chests spawn), for
// each holder on their own. A new biome is told by the level manager's biome asset; a new run by the island
// number going down. Snap Decision itself is never picked; relics and charms are never touched. The items are
// changed with the game's own adders and removers (inventory_edit.hpp): a doubled item is added again at its
// own level.
#include <algorithm>
#include <random>
#include <string>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"
#include "inventory_edit.hpp"

namespace crabmods {
namespace {

class SnapDecision : public AssetItem {
public:
    SnapDecision()
        : AssetItem("snapDecision", 126,
                    {Kind::Perk, L"DA_Perk_SnapDecision", L"Snap Decision",
                     L"Each biome, a coin flip: half your items double in level, or half of them vanish",
                     L"Once each biome, a coin flip: half your mods and perks double in level, or half of them "
                     L"are destroyed",
                     216, 109 /* GlassCannon */, pool::Greed},
                    rarity::Greed),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Snap Decision, a Greed perk: once each biome (at the first island you clear in it), a coin flip. Heads,
# half your mods and perks double in level; tails, half of them are destroyed. Relics, charms and Snap Decision
# itself are never touched.
enabled = true

# The chance of heads (doubling), in percent.
doubleChance = 50

# The share of your mods and perks each flip touches, in percent (rounded up on heads, down on tails).
sharePercent = 50

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "doubleChance") doubleChance_ = std::clamp(ParseFloat(v), 0.f, 100.f);
        else if (k == "sharePercent") share_ = std::clamp(ParseFloat(v), 0.f, 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(doubleChance_) + "% each biome to double " + Num(share_) + "% of mods and perks, else destroy " +
               Num(share_) + "%, " + RarityName(rarity_);
    }

    void OnRewardChests(void* ps, TArray<void*>&) override {
        if (!enabled_ || !da_ || !ps || Level(ps) == 0) return;
        void* lm = LevelManagerOf(ps);
        void* gs = GameStateOf(ps);
        if (!lm || !gs) return;
        void* biome = At<void*>(lm, off::LM_BiomeDA);
        int island = At<int>(gs, off::GS_CurrentIsland);
        State& s = state_[ps];
        if (island < s.island) s.biome = nullptr;  // a new run
        s.island = island;
        if (!biome || biome == s.biome) return;
        s.biome = biome;
        Flip(ps);
    }

    std::string InventoryNote(void* ps, void* da) override {
        if (!da || da != da_) return {};
        auto it = state_.find(ps);
        return it != state_.end() && it->second.last[0] ? it->second.last : std::string();
    }

protected:
    float PrimaryBuff() const override { return doubleChance_; }

private:
    struct State {
        void* biome = nullptr;  // the last biome flipped in
        int island = -1;
        std::string last;       // the last flip, for the inventory menu
    };

    void Flip(void* ps) {
        std::vector<inv::Entry> items;
        for (Kind k : inv::kLeveled)
            for (const inv::Entry& e : inv::Held(ps, k))
                if (e.da != da_) items.push_back(e);
        bool heads = std::uniform_real_distribution<float>(0.f, 100.f)(rng_) < doubleChance_;
        float n = float(items.size()) * share_ / 100.f;
        size_t count = std::min(items.size(), size_t(heads ? std::ceil(n - 1e-4f) : std::floor(n + 1e-4f)));
        std::shuffle(items.begin(), items.end(), rng_);
        items.resize(count);
        for (const inv::Entry& e : items) {
            if (heads) inv::Add(ps, e.kind, e.da, std::min(e.level, 255 - e.level));
            else inv::Remove(ps, e.kind, e.da);
        }
        std::string what = std::to_string(count) + (count == 1 ? " item" : " items");
        state_[ps].last = heads ? "Last flip: heads, " + what + " doubled" : "Last flip: tails, " + what + " destroyed";
        CRAB_LOG("Snap Decision: %p %s", ps, state_[ps].last.c_str());
        std::wstring msg = heads ? L"Snap Decision: heads! " + std::to_wstring(count) + L" of your items doubled in level"
                                 : L"Snap Decision: tails... " + std::to_wstring(count) + L" of your items were destroyed";
        ShowMessage(ps, msg.c_str());
    }

    float doubleChance_ = 50.f;
    float share_ = 50.f;
    std::minstd_rand rng_;
    std::unordered_map<void*, State> state_;  // player state -> its flips
};

SnapDecision g_item;

}  // namespace
}  // namespace crabmods
