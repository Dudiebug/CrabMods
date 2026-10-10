// Recharge, an ability mod: each kill takes time off your ability cooldown.
#include <windows.h>

#include <algorithm>
#include <unordered_map>

#include "../item.hpp"

namespace crabmods {
namespace {

class Recharge : public AssetItem {
public:
    Recharge()
        : AssetItem("recharge", 130,
                    {Kind::AbilityMod, L"DA_AbilityMod_Recharge", L"Recharge", L"Kills shorten your ability cooldown",
                     L"Each kill takes PrimaryBuff seconds off your ability cooldown", 200, AbilityMod_HeatSink},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Recharge, an ability mod: each kill takes time off your ability cooldown.
enabled = true

# Seconds taken off the cooldown per kill. Each extra Recharge stacks it again.
secondsPerKill = 0.1

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "secondsPerKill") seconds_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "-" + Num(seconds_) + " s cooldown per kill, " + RarityName(rarity_);
    }

    // Kills since the last tick (PS_Eliminations is replicated, so it is right on every machine) times Recharge
    // level times secondsPerKill come off the cooldown.
    // The tick runs every 0.25 s only while a cooldown counts down. Kills since the previous tick shorten it;
    // kills made while the ability was ready (a gap between ticks) do not carry over to the next cooldown.
    void OnCooldownTick(void* c, void* ps) override {
        int kills = At<int>(ps, off::PS_Eliminations);
        uint64_t now = GetTickCount64();
        auto [it, fresh] = seen_.try_emplace(ps, Seen{kills, now});
        Seen last = it->second;
        it->second = {kills, now};
        if (fresh || kills <= last.kills || now - last.at > 1000) return;
        int level = Level(ps);
        if (!level) return;
        float& cd = At<float>(c, off::PlayerC_AbilityCooldown);
        if (cd <= 0.f) return;
        // Never exactly 0: the game's tick then finishes the cooldown and gives the charge back.
        cd = std::max(cd - (kills - last.kills) * level * seconds_, 0.01f);
    }

protected:
    float PrimaryBuff() const override { return seconds_; }

private:
    float seconds_ = 0.1f;
    struct Seen {
        int kills;    // the player's kill count at the last tick
        uint64_t at;  // GetTickCount64 then
    };
    std::unordered_map<void*, Seen> seen_;
};

Recharge g_item;

}  // namespace
}  // namespace crabmods
