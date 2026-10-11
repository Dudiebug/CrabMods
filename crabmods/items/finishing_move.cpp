// Finishing Move, a perk: enemies you kill with a critical hit drop more crystals.
//
// Host: every hit of a player's is checked for the critical tag (OnOutgoingDamage) and remembered per enemy, so when
// an enemy dies (ACrabEnemyC's elimination, which sets up its crystal drop), the last hit tells whether it was a
// critical kill. The drop's amount is then raised for the killer's Finishing Move levels.
#include <cmath>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kCriticalTag = 1;  // ECrabDamageTag

class FinishingMove : public AssetItem {
public:
    FinishingMove()
        : AssetItem("finishingMove", 94,
                    {Kind::Perk, L"DA_Perk_FinishingMove", L"Finishing Move",
                     L"Critical kills drop more crystals",
                     L"Enemies killed with a critical hit drop PrimaryBuff% more crystals", 212, Perk_Collector,
                     pool::Critical},
                    rarity::Epic) {
        self_ = this;
    }

    std::string DefaultIni() const override {
        return R"(# Finishing Move, a perk: enemies you kill with a critical hit drop more crystals.
enabled = true

# More crystals for each Finishing Move level, in percent.
crystalPercent = 50

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "crystalPercent") pct_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% crystals from critical kills, " + std::string(RarityName(rarity_));
    }

    void InstallHooks() override {
        Hook(va::ACrabEnemyC_OnEliminated, &HkEliminated, &oEliminated, "ACrabEnemyC elimination");
        Hook(va::ACrabCrystalPickup_Init, &HkCrystalInit, &oCrystalInit, "ACrabCrystalPickup init");
    }
    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        lastCrit_.clear();
    }

    void OnOutgoingDamage(void* ps, void* info) override {
        void* enemy = At<void*>(info, off::DamageInfo_DamagedActor);
        if (!enemy || Level(ps) == 0) return;
        lastCrit_[enemy] = Contains(At<TArray<uint8_t>>(info, off::DamageInfo_Tags), kCriticalTag);
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    using EliminatedFn = void (*)(void*);
    using CrystalInitFn = void (*)(void*, int, int, uint8_t, void*, bool);
    static inline EliminatedFn oEliminated;
    static inline CrystalInitFn oCrystalInit;
    static inline FinishingMove* self_;
    static inline void* dying_ = nullptr;  // the enemy whose elimination is running

    static void HkEliminated(void* enemy) {
        void* outer = dying_;
        dying_ = enemy;
        oEliminated(enemy);
        dying_ = outer;
        self_->lastCrit_.erase(enemy);
    }

    static void HkCrystalInit(void* pickup, int amount, int bonus, uint8_t tier, void* ps, bool b) {
        FinishingMove& self = *self_;
        uintptr_t ret = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
        if (ret == va::Ret_CrystalInitEnemy && dying_ && self.enabled_ && self.da_) {
            auto it = self.lastCrit_.find(dying_);
            void* c = At<void*>(dying_, off::Enemy_EliminatingC);
            void* killer = c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
            int level = it != self.lastCrit_.end() && it->second && IsA(killer, StaticClass(va::ACrabPS_StaticClass))
                            ? self.Level(killer)
                            : 0;
            if (level) {
                float extra = self.pct_ / 100.f * level;
                if (EncoreRoll(killer)) extra *= 2.f;
                amount = int(std::lround(amount * (1.f + extra)));
                bonus = int(std::lround(bonus * (1.f + extra)));
            }
        }
        oCrystalInit(pickup, amount, bonus, tier, ps, b);
    }

    float pct_ = 50.f;
    std::unordered_map<void*, bool> lastCrit_;  // enemy -> was the last hit on it a critical one
};

FinishingMove g_item;

}  // namespace
}  // namespace crabmods
