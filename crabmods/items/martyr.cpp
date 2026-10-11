// Martyr, a relic: while you're down, your revive bubble hurts the enemies in it. Magnify makes the bubble
// bigger, so the area it hurts and the area a teammate can revive you from grow together.
//
// Every machine: a downed holder's revive bubble (the game's revival harvest area; who it revives replicates)
// gets its radius times the holder's Magnify, and its effect is resized to match.
// Host: four times a second, the game's radial damage from the bubble's centre over that radius. Its source is
// the downed character, with its player state put back for the call, so only enemies are hurt and the kills
// are the player's.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kRevival = 2;   // ECrabHarvestAreaType
constexpr float kTickSeconds = 0.25f;  // OnTick

// Every actor of a class in the world (UGameplayStatics::GetAllActorsOfClass).
std::vector<void*> AllActorsOf(void* world, uintptr_t staticClassVa) {
    static void* statics = ui::FindObject(va::GameplayStaticsPath);
    struct {
        void* context;
        void* cls;
        TArray<void*> out;
    } p{world, StaticClass(staticClassVa), {}};
    std::vector<void*> actors;
    if (!statics || !ui::Call(statics, L"GetAllActorsOfClass", &p)) return actors;
    for (int i = 0; p.out.Data && i < p.out.Num; i++)
        if (p.out.Data[i]) actors.push_back(p.out.Data[i]);
    if (p.out.Data) Fn<void (*)(void*)>(va::FMemory_Free)(p.out.Data);
    return actors;
}

class Martyr : public AssetItem {
public:
    Martyr()
        : AssetItem("martyr", 100,
                    {Kind::Relic, L"DA_Relic_Martyr", L"Martyr", L"Your revive bubble hurts enemies while you're down",
                     L"While you're down, enemies in your revive bubble take PrimaryBuff damage a second. Magnify "
                     L"makes the bubble bigger",
                     208, Relic_RingOfDefense, pool::Health},
                    rarity::Epic) {}

    std::string DefaultIni() const override {
        return R"(# Martyr, a relic: while you're down, your revive bubble hurts the enemies in it. Magnify makes the bubble
# bigger (for reviving you too).
enabled = true

# Damage a second to each enemy in the bubble, for each Martyr level. It falls off to 20% at the edge, like
# the game's explosions.
damagePerSecond = 60

# true: the holder's revive bubble grows with their Magnify (+20% a level), as the game's explosions do.
magnifyBubble = true

# Which chests it drops from: common, epic, legendary or greed.
rarity = epic
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "damagePerSecond") dps_ = ParseFloat(v);
        else if (k == "magnifyBubble") magnify_ = ParseBool(v);
        else if (k == "damage" || k == "radiusMeters" || k == "knockback") {}  // the explosion's (0.4)
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return Num(dps_) + " damage a second in your revive bubble while down" +
               (magnify_ ? ", bubble grows with Magnify, " : ", ") + RarityName(rarity_);
    }

    // Host: the downed character, the damage's source while its player is down.
    void OnDown(void* c, void* ps) override {
        if (Level(ps)) bodies_[ps] = c;
    }

    void OnTick(void* world) override {
        if (!da_) return;
        bool held = false;
        ForEachPlayer(world, [&](void* ps) { held = held || Level(ps) > 0; });
        if (!held) return bodies_.clear();
        std::vector<void*> areas = AllActorsOf(world, va::ACrabHarvestArea_StaticClass);
        std::vector<void*> bodies;  // the characters in the world, looked up when there's damage to deal
        bool looked = false;
        ForgetRevived(world);
        for (void* area : areas) {
            if (At<uint8_t>(area, off::HarvestArea_Type) != kRevival) continue;
            void* ps = At<void*>(area, off::HarvestArea_PlayerToRevive);
            int level = ps ? Level(ps) : 0;
            if (!level) continue;
            float base = BaseRadius(area);
            float magnify = 1.f + Fn<float (*)(void*, uint8_t)>(va::ACrabPS_GetPerkBuff)(ps, Perk_Magnify);
            float radius = magnify_ ? base * magnify : base;
            Resize(area, radius);

            if (At<uint8_t>(area, off::Actor_Role) != ROLE_Authority || dps_ <= 0.f) continue;
            auto it = bodies_.find(ps);
            if (it == bodies_.end()) continue;
            if (!looked) bodies = AllActorsOf(world, va::ACrabPlayerC_StaticClass), looked = true;
            if (std::find(bodies.begin(), bodies.end(), it->second) == bodies.end()) {
                CRAB_LOG("Martyr: player %p's character is gone, no more damage", ps);
                bodies_.erase(it);
                continue;
            }
            // The game's radial damage grows its radius by the source's Magnify itself.
            Hurt(area, it->second, ps, dps_ * level * kTickSeconds, radius / magnify);
        }
    }

protected:
    float PrimaryBuff() const override { return dps_; }

private:
    // The bubble's own radius: its class's default.
    static float BaseRadius(void* area) {
        void* cls = At<void*>(area, off::Object_Class);
        void* cdo = cls ? At<void*>(cls, off::Class_DefaultObject) : nullptr;
        if (IsA(cdo, StaticClass(va::ACrabHarvestArea_StaticClass))) return At<float>(cdo, off::HarvestArea_Radius);
        return At<float>(area, off::HarvestArea_Radius);
    }

    // The radius the host looks for reviving players in, and the effect's size (set from it when it was made).
    static void Resize(void* area, float radius) {
        float& r = At<float>(area, off::HarvestArea_Radius);
        if (std::fabs(r - radius) < 1.f) return;
        r = radius;
        if (void* nc = At<void*>(area, off::HarvestArea_NC))
            Fn<void (*)(void*, uint64_t, float)>(va::NiagaraComponent_SetVariableFloat)(nc, ui::Name(L"Radius"),
                                                                                       radius);
        CRAB_LOG("Martyr: revive bubble %p is %g m across", area, radius / 50.f);
    }

    static void Hurt(void* area, void* body, void* ps, float damage, float radius) {
        Vec3 at{};
        if (!LocationOf(area, at)) return;
        alignas(8) uint8_t info[off::DamageInfo_Size];
        std::memset(info, 0, sizeof info);  // no tags, debuffs or knockback
        At<float>(info, off::DamageInfo_Damage) = damage;
        At<uint8_t>(info, off::DamageInfo_Type) = 4;  // Lesser: not throttled per enemy
        At<Vec3>(info, off::DamageInfo_Location) = at;
        At<void*>(info, off::DamageInfo_DamagingC) = body;
        // A possessed character's player state goes when its player spectates; the damage reads it for who
        // may be hurt, Magnify and the kills.
        void*& owner = At<void*>(body, off::Pawn_PlayerState);
        void* before = owner;
        owner = ps;
        TArray<void*> ignore{};  // passed by value: the game frees it, so it stays empty
        Fn<void (*)(void*, TArray<void*>*, const Vec3*, float, const void*, bool, void*)>(va::RadialDamage)(
            area, &ignore, &at, radius, info, false, nullptr);
        owner = before;
    }

    // Players no longer down (revived, a new level) or gone.
    void ForgetRevived(void* world) {
        if (bodies_.empty()) return;
        std::unordered_map<void*, void*> kept;
        ForEachPlayer(world, [&](void* ps) {
            auto it = bodies_.find(ps);
            if (it != bodies_.end() && IsDown(ps)) kept.insert(*it);
        });
        bodies_ = std::move(kept);
    }

    float dps_ = 60.f;
    bool magnify_ = true;
    std::unordered_map<void*, void*> bodies_;  // player state -> downed character (host)
};

Martyr g_item;

}  // namespace
}  // namespace crabmods
