// Feedback Loop, an ability mod: each enemy your ability kills takes 0.1 s per level off its cooldown, at most 50%
// of the cooldown per use.
//
// Host: a hit of type Ability (the ability's own explosion; its explosion mods' extra projectiles come as Lesser hits
// and don't count) marks the enemy as the player's; if that was the enemy's last hit when it dies, the kill counts.
// The cooldown is counted on the machine that controls the player (not replicated), so four times a second the host
// sends each player the seconds their kills earned, and that machine takes them off (the host's own player directly).
#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kAbility = 2;  // DamageInfo_Type
constexpr wchar_t kTag[] = L"feedbackLoop ";  // host to player: "feedbackLoop <seconds>"

class FeedbackLoop : public AssetItem {
public:
    FeedbackLoop()
        : AssetItem("feedbackLoop", 196,
                    {Kind::AbilityMod, L"DA_AbilityMod_FeedbackLoop", L"Feedback Loop",
                     L"Ability kills shorten its cooldown",
                     L"Each enemy your ability kills takes PrimaryBuff seconds off its cooldown (at most half of it per "
                     L"use)",
                     240, AbilityMod_HeatSink, pool::Skill, tag::None, 0.75f},
                    rarity::Common) {}

    std::string DefaultIni() const override {
        return R"(# Feedback Loop, an ability mod: each enemy your ability kills takes time off its cooldown.
enabled = true

# Seconds off the cooldown per ability kill, per level.
secondsPerKill = 0.1
# The most it can take off one cooldown, in percent of it.
maxPercentPerUse = 50

# Which chests it drops from: common, epic, legendary or greed.
rarity = common
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "secondsPerKill") seconds_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "maxPercentPerUse") maxPct_ = std::fmin(std::fmax(ParseFloat(v), 0.f), 100.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "-" + Num(seconds_) + " s per ability kill per level, at most " + Num(maxPct_) + "% per use, " +
               RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        last_.clear();
        owed_.clear();
    }

    // Host: whose ability hit each enemy last (any other hit clears it).
    void OnOutgoingDamage(void* ps, void* info) override {
        void* e = At<void*>(info, off::DamageInfo_DamagedActor);
        if (!e) return;
        if (At<uint8_t>(info, off::DamageInfo_Type) == kAbility && At<void*>(info, off::DamageInfo_DamagingC) == PawnOf(ps))
            last_[e] = ps;
        else
            last_.erase(e);
    }

    void OnEnemyEliminating(void* enemy) override {
        auto it = last_.find(enemy);
        if (it == last_.end()) return;
        void* ps = it->second;
        last_.erase(it);
        if (int level = Level(ps)) owed_[ps] += seconds_ * level;
    }

    void OnTick(void* world) override {
        if (owed_.empty()) return;
        for (auto& [ps, s] : owed_) {
            void* c = PawnOf(ps);
            if (!c || s <= 0.f) continue;
            if (Local(c)) Cut(c, s);
            else if (void* pc = At<void*>(c, off::Pawn_Controller)) SendToPlayer(pc, kTag + std::to_wstring(s));
        }
        owed_.clear();
    }

    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, std::size(kTag) - 1, kTag) != 0) return false;
        void* c = PawnOf(At<void*>(pc, off::Controller_PlayerState));
        try {
            if (c && Local(c)) Cut(c, std::fmax(std::stof(text.substr(std::size(kTag) - 1)), 0.f));
        } catch (...) {
        }
        return true;
    }

protected:
    float PrimaryBuff() const override { return seconds_; }

private:
    static bool Local(void* c) {
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; i < weapons.Num; i++)
            if (weapons.Data[i] && At<void*>(weapons.Data[i], off::Weapon_OwningPC)) return true;
        return false;
    }

    // Takes s seconds off the running cooldown, up to maxPercentPerUse of it per use. A cooldown above what was left
    // after the last cut is a new use (it only counts down otherwise).
    void Cut(void* c, float s) {
        float& cd = At<float>(c, off::PlayerC_AbilityCooldown);
        if (cd <= 0.f) return;
        if (c != useC_ || cd > after_ + 0.01f) {
            useC_ = c;
            start_ = cd;
            cut_ = 0.f;
        }
        float d = std::fmin(s, std::fmax(start_ * maxPct_ / 100.f - cut_, 0.f));
        if (d <= 0.f) return;
        // Never exactly 0: the game's tick then finishes the cooldown and gives the charge back.
        float before = cd;
        cd = std::fmax(cd - d, 0.01f);
        cut_ += before - cd;
        after_ = cd;
    }

    float seconds_ = 0.1f, maxPct_ = 50.f;
    std::unordered_map<void*, void*> last_;  // host: enemy -> the player whose ability hit it last
    std::unordered_map<void*, float> owed_;  // host: player -> seconds earned since the last tick
    void* useC_ = nullptr;                   // this machine's player: their current cooldown
    float start_ = 0.f, cut_ = 0.f, after_ = 0.f;
};

FeedbackLoop g_item;

}  // namespace
}  // namespace crabmods
