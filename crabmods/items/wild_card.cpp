// Wild Card, a Greed relic: every island you clear, a chance that your whole inventory is randomized, as a random
// totem does.
//
// Host: at each island cleared (OnRewardChests, before the chests spawn), for each holder on their own. Every mod,
// perk and relic is swapped for a random one of the same kind at the same level, so the slots and levels stay the
// same. Wild Card itself and charms stay. The new ones come from the game's full lists, less Greed items (the
// game's own randomize leaves them out too), charms and items already held; the necromancer's items only for
// Soul Whip holders.
//
// The game's own randomize (ACrabPS 0x140d27a70, the Randomize totem) isn't called: it would delete charms and
// this relic, and it lets two slots roll the same item (they merge into one).
#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "../item.hpp"
#include "../world.hpp"
#include "inventory_edit.hpp"

namespace crabmods {
namespace {

class WildCard : public AssetItem {
public:
    WildCard()
        : AssetItem("wildCard", 127,
                    {Kind::Relic, L"DA_Relic_WildCard", L"Wild Card",
                     L"Each island, a chance to randomize your entire inventory",
                     L"PrimaryBuff% chance to randomize your entire inventory when clearing islands", 216,
                     63 /* HighRoller */, pool::Greed, 0xFF, 0.3f},
                    rarity::Greed),
          rng_(std::random_device{}()) {}

    std::string DefaultIni() const override {
        return R"(# Wild Card, a Greed relic: each island you clear, a chance to randomize your entire inventory, like a random
# totem: every mod, perk and relic becomes a random other one at the same level. Wild Card and charms stay.
enabled = true

# The chance each island, in percent.
chancePercent = 20

# Which chests it drops from: greed (also common, epic or legendary).
rarity = greed
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chancePercent") chance_ = std::clamp(ParseFloat(v), 0.f, 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(chance_) + "% each island to randomize the inventory, " + RarityName(rarity_);
    }

    void OnRewardChests(void* ps, TArray<void*>&) override {
        if (!enabled_ || !da_ || !ps || Level(ps) == 0) return;
        if (std::uniform_real_distribution<float>(0.f, 100.f)(rng_) >= chance_) return;
        void* lm = LevelManagerOf(ps);
        if (!lm) return;
        int swapped = 0;
        for (Kind k : {Kind::WeaponMod, Kind::AbilityMod, Kind::MeleeMod, Kind::Perk, Kind::Relic}) {
            std::vector<inv::Entry> held = inv::Held(ps, k), out;
            std::vector<void*> skip{da_};
            for (const inv::Entry& e : held) {
                skip.push_back(e.da);
                if (e.da != da_ && !IsCharm(e.da)) out.push_back(e);
            }
            std::vector<void*> pool = inv::Pool(lm, ps, k, skip);
            std::shuffle(pool.begin(), pool.end(), rng_);
            for (const inv::Entry& e : out) inv::Remove(ps, k, e.da);
            for (size_t i = 0; i < out.size(); i++) {
                // The pool is the items not held: every slot gets a different one (while there are enough).
                void* da = i < pool.size() ? pool[i] : out[i].da;
                inv::Add(ps, k, da, out[i].level);
                swapped++;
            }
        }
        CRAB_LOG("Wild Card: %p's inventory randomized (%d items)", ps, swapped);
        ShowMessage(ps, L"Wild Card: your entire inventory was randomized!");
    }

protected:
    float PrimaryBuff() const override { return chance_; }

private:
    float chance_ = 20.f;
    std::minstd_rand rng_;
};

WildCard g_item;

}  // namespace
}  // namespace crabmods
