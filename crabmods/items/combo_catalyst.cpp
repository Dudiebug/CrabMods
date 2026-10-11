// Combo Catalyst, a perk: the higher your combo, the more elemental stacks your hits apply.
//
// Host: after the game works out a player's hit (CalcOutgoingDamage, which also scales its element stacks by the
// elemental perks), every element the hit applies gets extra stacks for each combo level above 1.
#include <cmath>

#include "../item.hpp"

namespace crabmods {
namespace {

constexpr int kDebuffSize = 8;     // FCrabDebuff
constexpr int kDebuffStacks = 4;   // int32

class ComboCatalyst : public AssetItem {
public:
    ComboCatalyst()
        : AssetItem("comboCatalyst", 92,
                    {Kind::Perk, L"DA_Perk_ComboCatalyst", L"Combo Catalyst",
                     L"Higher combo levels apply more elemental stacks",
                     L"Your hits apply PrimaryBuff more stacks of each element for every combo level above 1", 211,
                     Perk_Collector, pool::Elemental},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Combo Catalyst, a perk: the higher your combo level, the more elemental stacks (ice, fire, lightning,
# poison, arcane) your hits apply. It only adds to elements a hit already applies.
enabled = true

# Extra stacks of each element per combo level above 1, for each Combo Catalyst level (rounded per hit).
stacksPerComboLevel = 1

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "stacksPerComboLevel") per_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(per_) + " element stacks per combo level, " + std::string(RarityName(rarity_));
    }

    void OnOutgoingDamage(void* ps, void* info) override {
        float combo = At<float>(ps, off::PS_Combo);
        if (combo <= 1.f || per_ <= 0.f) return;
        auto& debuffs = At<TArray<uint8_t>>(info, off::DamageInfo_Debuffs);
        if (!debuffs.Data || debuffs.Num <= 0) return;
        int level = Level(ps);
        if (!level) return;
        int add = int(std::lround((combo - 1.f) * per_ * level));
        if (add <= 0) return;
        for (int i = 0; i < debuffs.Num; i++) {
            int& stacks = At<int>(debuffs.Data + i * kDebuffSize, kDebuffStacks);
            if (stacks > 0) stacks += add;
        }
    }

protected:
    float PrimaryBuff() const override { return per_; }

private:
    float per_ = 1.f;
};

ComboCatalyst g_item;

}  // namespace
}  // namespace crabmods
