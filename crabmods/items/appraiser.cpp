// Appraiser, an Epic perk: chest choices roll Epic and Legendary more often the further into the run you are. The
// game's luck for each choice (m: Epic 3.75% x m, Legendary 0.25% x m) gets +0.12 x island x level, counting islands
// up to 50. Chests only: not shops (they have their own scaling), key chests or the secret chest.
#include <algorithm>
#include <cmath>

#include "../item.hpp"
#include "../world.hpp"
#include "luck_shared.hpp"

namespace crabmods {
namespace {

class Appraiser : public AssetItem {
public:
    Appraiser()
        : AssetItem("appraiser", 192,
                    {Kind::Perk, L"DA_Perk_Appraiser", L"Appraiser", L"Chests get luckier the further you get",
                     L"Chest items are more likely to be Epic or Legendary: +PrimaryBuff luck per island (up to "
                     L"island 50)",
                     241, Perk_Collector, pool::Luck},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Appraiser, a perk: chest items roll Epic and Legendary more often the further into the run you are.
# The game's luck for each choice (1 normally: Epic 3.75% x luck, Legendary 0.25% x luck) gets this per island and
# level. Chests only: not shops, key chests or the secret chest.
enabled = true

# Luck per island per level.
luckPerIsland = 0.12
# Islands past this one add no more.
maxIsland = 50

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "luckPerIsland") per_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxIsland") max_ = std::max(0, int(ParseFloat(v)));
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(per_) + " luck per island per level (to island " + std::to_string(max_) + "), " +
               RarityName(rarity_);
    }

    void LootLuck(void* ps, void* source, float& m) override {
        int level = Level(ps);
        if (!level || !LuckyChest(source)) return;
        void* gs = GameStateOf(source);
        int island = gs ? std::clamp(At<int>(gs, off::GS_CurrentIsland), 0, max_) : 0;
        m += per_ * island * level;
    }

protected:
    float PrimaryBuff() const override { return per_; }

private:
    float per_ = 0.12f;
    int max_ = 50;
};

Appraiser g_item;

}  // namespace
}  // namespace crabmods
