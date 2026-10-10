// Untouchable, a perk: damage builds up for every second you go without taking damage, and resets when you are hit.
#include <chrono>
#include <unordered_map>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

class Untouchable : public AssetItem {
public:
    Untouchable()
        : AssetItem("untouchable", 60,
                    {Kind::Perk, L"DA_Perk_Untouchable", L"Untouchable",
                     L"Damage builds up while you avoid taking damage",
                     L"Damage increased by PrimaryBuff% for every second without taking damage", 203,
                     67 /* SilverLining */},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Untouchable, a perk: damage builds up while you avoid taking damage, and resets when you are hit.
enabled = true

# Damage bonus for each second without taking damage, in percent.
damagePercent = 1

# The most it builds up to, in percent. Each extra Untouchable stacks both again.
maxPercent = 20

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = ParseFloat(v);
        else if (k == "maxPercent") max_ = ParseFloat(v);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% per second unhit, up to +" + Num(max_) + "%, " + RarityName(rarity_);
    }

    // The host sees every hit on a player.
    void OnTakeDamage(void* hc, void* info) override {
        if (At<float>(info, off::DamageInfo_Damage) <= 0.f) return;
        if (void* ps = PlayerOfHealth(hc)) Reset(ps);
    }

    // Level x (seconds since the last hit x damagePercent, up to maxPercent).
    float DamageBonus(void* ps) override {
        int level = Level(ps);
        if (!level) return 0.f;
        auto now = Clock::now();
        State& s = state_[ps];
        if (!s.seen) {
            s.seen = true;
            s.lastHit = now;
        }
        // Health dropping is a hit too, and it is visible on every machine.
        if (void* hc = HealthOf(ps)) {
            float h = At<float>(hc, off::HC_CurrentHealth);
            if (s.lastHealth >= 0.f && h < s.lastHealth) s.lastHit = now;
            s.lastHealth = h;
        }
        float seconds = std::chrono::duration<float>(now - s.lastHit).count();
        float bonus = seconds * pct_ < max_ ? seconds * pct_ : max_;
        return level * bonus * 0.01f;
    }

protected:
    float PrimaryBuff() const override { return pct_; }

    void Customize(void* da) override {
        std::string n = Num(max_);
        std::wstring text = L"Damage increased by PrimaryBuff% for every second without taking damage (up to ";
        for (char c : n) text += wchar_t(c);
        text += L"%)";
        SetFString(da, off::DA_LevelDescription, text.c_str());
    }

private:
    using Clock = std::chrono::steady_clock;
    struct State {
        Clock::time_point lastHit;
        float lastHealth = -1.f;
        bool seen = false;
    };

    void Reset(void* ps) { state_[ps].lastHit = Clock::now(); }

    float pct_ = 1.f;
    float max_ = 20.f;
    std::unordered_map<void*, State> state_;
};

Untouchable g_item;

}  // namespace
}  // namespace crabmods
