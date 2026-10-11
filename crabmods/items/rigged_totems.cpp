// Rigged Totems, a perk: chance totems are more likely to go your way.
//
// Host: the Health, Chance and Gamble totems decide with one roll (TotemRoll, true is the good outcome) inside
// ACrabTotem::Interact, which key_totem.cpp tells us the user of. For a player with this perk the roll's chance is
// raised, up to certain.
#include <algorithm>
#include <cmath>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

class RiggedTotems : public AssetItem {
public:
    RiggedTotems()
        : AssetItem("riggedTotems", 184,
                    {Kind::Perk, L"DA_Perk_RiggedTotems", L"Rigged Totems",
                     L"Chance totems are more likely to go your way",
                     L"Health, Chance and Gamble totems are PrimaryBuff% more likely to go your way", 214,
                     Perk_Collector, pool::Luck},
                    rarity::Epic) {
        self_ = this;
    }

    std::string DefaultIni() const override {
        return R"(# Rigged Totems, a perk: Health, Chance and Gamble totems are more likely to go your way.
enabled = true

# How much more likely, in percent, for each Rigged Totems level (a 50% totem becomes 62.5% with 25).
chanceBonusPercent = 25

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "chanceBonusPercent") pct_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% totem luck, " + std::string(RarityName(rarity_));
    }

    void InstallHooks() override { Hook(va::TotemRoll, &HkRoll, &oRoll, "TotemRoll"); }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    using RollFn = bool (*)(float, float);
    static inline RollFn oRoll;
    static inline RiggedTotems* self_;

    static bool HkRoll(float percent, float mult) {
        RiggedTotems& self = *self_;
        uintptr_t ret = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        void* ps = g_totemUser ? At<void*>(g_totemUser, off::Pawn_PlayerState) : nullptr;
        bool ours = (ret == Addr(va::Ret_TotemRollHealth) || ret == Addr(va::Ret_TotemRollChance)) && self.enabled_ && self.da_ &&
                    IsA(ps, StaticClass(va::ACrabPS_StaticClass));
        int level = ours ? self.Level(ps) : 0;
        if (!level) return oRoll(percent, mult);
        float p = percent * (1.f + self.pct_ / 100.f * level);
        if (mult > 0.f) p = std::min(p, 100.f / mult);
        bool won = oRoll(p, mult);
        if (!won && EncoreRoll(ps)) {
            won = oRoll(p, mult);
            if (won) ShowMessage(ps, L"Encore! Rigged Totems rolled again and won");
        }
        CRAB_LOG("Rigged Totems: %p rolled %.1f%% (was %.1f%%): %s", ps, p * mult, percent * mult, won ? "won" : "lost");
        return won;
    }

    float pct_ = 25.f;
};

RiggedTotems g_item;

}  // namespace
}  // namespace crabmods
