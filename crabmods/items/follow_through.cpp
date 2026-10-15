// Follow Through, an Epic melee mod: after you use your ability, your next melee within 3 s deals +200% damage
// (+100% per level after the first) and carries the elements of your elemental explosion ability mods.
//
// Host: ServerAbility arrives once per use, for every player. The first melee hit (type 3) of that player within the
// window starts the empowered swing; every hit of that swing (a swing hits all enemies in reach at once) gets the
// damage and the debuffs: Ice, Fire, Lightning and Poison Explosion add those elements, as many stacks per level as
// a bullet of the matching elemental weapon mod.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kMelee = 3;  // DamageInfo_Type
constexpr double kSwing = 0.15;  // seconds: the hits of one swing

// Elemental explosion ability mods (ECrabAbilityModType) -> debuff type and stacks per level.
struct Element {
    uint8_t mod, debuff;
    int stacks;
};
constexpr Element kElements[] = {{51, 1, 40}, {52, 2, 30}, {53, 3, 100}, {54, 4, 20}};

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

class FollowThrough : public AssetItem {
public:
    FollowThrough()
        : AssetItem("followThrough", 198,
                    {Kind::MeleeMod, L"DA_MeleeMod_FollowThrough", L"Follow Through",
                     L"Using your ability empowers your next melee",
                     L"After you use your ability, your next melee within 3 s deals +PrimaryBuff% damage and carries "
                     L"your elemental explosions' elements",
                     242, MeleeMod_BigClaws, pool::Damage, tag::None, 0.75f},
                    rarity::Epic) {
        g_self = this;
    }

    std::string DefaultIni() const override {
        return R"(# Follow Through, a melee mod: after you use your ability, your next melee deals more damage and carries the
# elements of your elemental explosion ability mods (Ice, Fire, Lightning, Poison Explosion).
enabled = true

# Extra damage at level 1, in percent.
damagePercent = 200
# More extra damage for each level after the first, in percent.
damagePercentPerLevel = 100
# Seconds after the ability in which the melee must land.
window = 3

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePercent") pct_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "damagePercentPerLevel") perLevel_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "window") window_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return "+" + Num(pct_) + "% (+" + Num(perLevel_) + "% per level) on the melee within " + Num(window_) +
               " s of an ability, " + RarityName(rarity_);
    }

    void InstallHooks() override {
        Hook(va::ACrabPlayerC_ServerAbility_Impl, &HkServerAbility, &oServerAbility, "ACrabPlayerC::ServerAbility");
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        ready_.clear();
        swing_.clear();
    }

    void OnOutgoingDamage(void* ps, void* info) override {
        if (At<uint8_t>(info, off::DamageInfo_Type) != kMelee) return;
        void* c = PawnOf(ps);
        if (!c || At<void*>(info, off::DamageInfo_DamagingC) != c) return;
        double now = Now();
        auto s = swing_.find(c);
        if (s == swing_.end() || now - s->second > kSwing) {
            auto r = ready_.find(c);
            if (r == ready_.end()) return;
            bool fresh = now - r->second <= window_;
            ready_.erase(r);
            if (!fresh) return;
            swing_[c] = now;
        }
        int level = Level(ps);
        if (!level) return;
        At<float>(info, off::DamageInfo_Damage) *= 1.f + (pct_ + perLevel_ * (level - 1)) / 100.f;
        auto& mods = At<TArray<uint8_t>>(ps, off::PS_AbilityMods);
        for (int i = 0; mods.Data && i < mods.Num; i++) {
            uint8_t* m = mods.Data + i * 0x28;
            void* da = At<void*>(m, 0);
            int lv = At<uint8_t>(m, 8);
            if (!da || lv <= 0) continue;
            uint8_t type = At<uint8_t>(da, off::DA_ItemType);
            for (const Element& e : kElements)
                if (e.mod == type)
                    Fn<void (*)(void*, uint64_t)>(va::DamageInfo_AddDebuff)(
                        info, uint64_t(e.debuff) | (uint64_t(uint32_t(e.stacks * lv)) << 32));
        }
    }

protected:
    float PrimaryBuff() const override { return pct_; }

private:
    using ServerAbilityFn = void (*)(void*, const void*);
    static inline ServerAbilityFn oServerAbility;
    static inline FollowThrough* g_self = nullptr;

    static void HkServerAbility(void* c, const void* loc) {
        if (g_self && g_self->Enabled() && c) g_self->ready_[c] = Now();
        oServerAbility(c, loc);
    }

    float pct_ = 200.f, perLevel_ = 100.f, window_ = 3.f;
    std::unordered_map<void*, double> ready_;  // character -> when they last used their ability
    std::unordered_map<void*, double> swing_;  // character -> when their empowered swing started
};

FollowThrough g_item;

}  // namespace
}  // namespace crabmods
