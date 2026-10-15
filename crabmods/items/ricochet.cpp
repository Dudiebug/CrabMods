// Ricochet, a weapon mod: a shot that hits an enemy ricochets on to the nearest other enemy, and on from there, one
// ricochet per level (at most 4). Each ricochet deals 50% of the shot's damage.
//
// Host: a player's direct weapon hit (OnOutgoingDamage; not explosions, melee, lesser projectiles or the
// Flamethrower) starts a chain. Its damage before buffs is taken from OnOutgoingDamageBase. A moment later the chain
// jumps to the nearest live enemy within range that it hasn't hit yet, and hits it through the game's own damage
// (UCrabHC::TakeDamage, from the player's character), so the player's buffs and crits apply to it again, as to any
// hit, and kills count as theirs. Ricochets don't start chains of their own, and carry no elements.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <vector>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"
#include "shared.hpp"

namespace crabmods {
namespace {

constexpr uint8_t kDirect = 0, kLesser = 4;  // DamageInfo_Type
constexpr wchar_t kEnemyClassPath[] = L"/Script/CrabChampions.CrabEnemyC";

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Every actor of a class in the world (UGameplayStatics::GetAllActorsOfClass).
std::vector<void*> AllActorsOf(void* world, void* cls) {
    static void* statics = ui::FindObject(va::GameplayStaticsPath);
    struct {
        void* context;
        void* cls;
        TArray<void*> out;
    } p{world, cls, {}};
    std::vector<void*> actors;
    if (!statics || !cls || !ui::Call(statics, L"GetAllActorsOfClass", &p)) return actors;
    for (int i = 0; p.out.Data && i < p.out.Num; i++)
        if (p.out.Data[i]) actors.push_back(p.out.Data[i]);
    if (p.out.Data) Fn<void (*)(void*)>(va::FMemory_Free)(p.out.Data);
    return actors;
}

bool Alive(void* c) {
    if (!c || (At<uint8_t>(c, off::C_bIsEliminated) & 1)) return false;
    void* hc = At<void*>(c, off::C_HC);
    return hc && At<float>(hc, off::HC_CurrentHealth) > 0.f;
}

// A chain between two jumps.
struct Chain {
    void* c;                    // the shooter's character (ACrabC*)
    float base;                 // the shot's damage before buffs
    int left;                   // ricochets still to go
    double at;                  // when the next one jumps
    Vec3 from;                  // where it jumps from: the last enemy hit
    std::vector<void*> hit;     // enemies it has hit, which it won't hit again
};

class Ricochet : public AssetItem {
public:
    Ricochet()
        : AssetItem("ricochet", 95,
                    {Kind::WeaponMod, L"DA_WeaponMod_Ricochet", L"Ricochet",
                     L"Shots ricochet from enemy to enemy",
                     L"Shots that hit an enemy ricochet to PrimaryBuff more nearby enemies, each ricochet dealing 50% of "
                     L"the shot's damage",
                     238, WeaponMod_StreakShot, pool::Damage, tag::None, 1.f},
                    rarity::Legendary) {}

    std::string DefaultIni() const override {
        return R"(# Ricochet, a weapon mod: when your shot hits an enemy, it ricochets to the nearest other enemy, and on from
# there. Each Ricochet level is one more ricochet. Not for explosions, melee or the Flamethrower.
enabled = true

# Ricochets for each Ricochet level.
ricochetsPerLevel = 1
# The most ricochets a shot can make; at it, Ricochet is no longer offered.
maxRicochets = 4
# Damage of each ricochet, in percent of the shot's damage (before your buffs, which apply to it again).
damagePercent = 50
# How far a ricochet looks for its next enemy, in cm (100 = 1 m).
range = 1500
# Seconds between one ricochet and the next.
delay = 0.08

# Which chests it drops from: common, epic or legendary.
rarity = legendary
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "ricochetsPerLevel") perLevel_ = std::max(0, int(ParseFloat(v)));
        else if (k == "maxRicochets") max_ = std::max(0, int(ParseFloat(v)));
        else if (k == "damagePercent") pct_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "range") range_ = std::fmax(ParseFloat(v), 0.f);
        else if (k == "delay") delay_ = std::fmax(ParseFloat(v), 0.f);
        else AssetItem::Set(k, v);
    }
    std::string Describe() const override {
        return std::to_string(perLevel_) + " ricochet(s) per level, max " + std::to_string(max_) + ", " + Num(pct_) +
               "% damage each, " + Num(range_ / 100.f) + " m, " + RarityName(rarity_);
    }

    void OnBeginPlay(void* lm) override {
        AssetItem::OnBeginPlay(lm);
        chains_.clear();
        enemyClass_ = enemyClass_ ? enemyClass_ : ui::FindObject(kEnemyClassPath);
    }

    float LootWeight(void* ps, void* da) override {
        if (!da_ || da != da_) return 1.f;
        return Ricochets(Level(ps)) >= max_ ? 0.f : 1.f;
    }

    // The same hit, before the game's buffs: its damage is what ricochets are a share of.
    void OnOutgoingDamageBase(void* ps, void* info) override { base_ = At<float>(info, off::DamageInfo_Damage); }

    void OnOutgoingDamage(void* ps, void* info) override {
        if (hopping_) {
            hopDamage_ = At<float>(info, off::DamageInfo_Damage);
            return;
        }
        int n = Ricochets(Level(ps));
        if (n <= 0 || pct_ <= 0.f || At<uint8_t>(info, off::DamageInfo_Type) != kDirect) return;
        void* c = PawnOf(ps);
        if (!c || At<void*>(info, off::DamageInfo_DamagingC) != c || Flamethrower(ps)) return;
        void* enemy = At<void*>(info, off::DamageInfo_DamagedActor);
        if (!IsEnemy(enemy) || base_ <= 0.f || chains_.size() >= kMaxChains) return;
        Chain ch{c, base_, n, Now() + delay_, {}, {enemy}};
        if (!LocationOf(enemy, ch.from)) return;
        chains_.push_back(std::move(ch));
    }

    void OnFrame(void* world, float) override {
        if (chains_.empty()) return;
        double now = Now();
        std::vector<void*> enemies;
        bool listed = false;
        size_t n = chains_.size();
        for (size_t i = 0; i < n; i++) {
            Chain ch = std::move(chains_.front());
            chains_.pop_front();
            if (ch.at > now) {
                chains_.push_back(std::move(ch));
                continue;
            }
            if (!Alive(ch.c)) continue;
            if (!listed) {
                enemies = AllActorsOf(world, enemyClass_);
                listed = true;
            }
            void* next = Nearest(enemies, ch);
            if (!next) continue;
            Vec3 at;
            LocationOf(next, at);
            Hit(next, ch, at);
            ch.hit.push_back(next);
            ch.from = at;
            if (--ch.left > 0) {
                ch.at = now + delay_;
                chains_.push_back(std::move(ch));
            }
        }
    }

protected:
    float PrimaryBuff() const override { return float(std::min(perLevel_, max_)); }

private:
    static constexpr size_t kMaxChains = 256;  // chains waiting at once; more hits than that start none

    int Ricochets(int level) const { return std::min(max_, perLevel_ * level); }

    bool IsEnemy(void* e) const {
        if (!e || !IsA(e, StaticClass(va::ACrabC_StaticClass))) return false;
        if (enemyClass_ && !IsA(e, enemyClass_)) return false;
        return Alive(e) && !IsUndead(e);
    }

    bool Flamethrower(void* ps) {
        void* da = At<void*>(ps, off::PS_WeaponDA);
        if (da != lastWeapon_) {
            lastWeapon_ = da;
            lastIsFlamethrower_ = da && ObjectName(da).find("Flamethrower") != std::string::npos;
        }
        return lastIsFlamethrower_;
    }

    void* Nearest(const std::vector<void*>& enemies, const Chain& ch) const {
        void* best = nullptr;
        float d = range_ * range_;
        for (void* e : enemies) {
            if (std::find(ch.hit.begin(), ch.hit.end(), e) != ch.hit.end() || !IsEnemy(e)) continue;
            Vec3 p;
            if (!LocationOf(e, p)) continue;
            float de = DistanceSquared(ch.from, p);
            if (de < d) d = de, best = e;
        }
        return best;
    }

    // One ricochet: a hit from the shooter's character, through the game's damage, buffs and crits included.
    void Hit(void* enemy, const Chain& ch, const Vec3& at) {
        void* hc = At<void*>(enemy, off::C_HC);
        if (!hc) return;
        alignas(8) uint8_t info[off::DamageInfo_Size] = {};
        At<void*>(info, off::DamageInfo_DamagedActor) = enemy;
        At<void*>(info, off::DamageInfo_DamagingC) = ch.c;
        At<float>(info, off::DamageInfo_Damage) = ch.base * pct_ / 100.f;
        At<uint8_t>(info, off::DamageInfo_Type) = kLesser;  // not throttled per enemy, like the game's own extra hits
        At<Vec3>(info, off::DamageInfo_Location) = at;
        hopping_ = true;
        hopDamage_ = -1.f;
        Fn<void (*)(void*, void*)>(va::UCrabHC_TakeDamage)(hc, info);
        hopping_ = false;
        if (logged_++ < 5)
            CRAB_LOG("ricochet: %s hit for %.1f (shot %.1f before buffs; %.1f after the game's buffs%s)",
                     ObjectName(enemy).c_str(), ch.base * pct_ / 100.f, ch.base, hopDamage_,
                     hopDamage_ < 0.f ? ": the game didn't run them" : "");
    }

    int perLevel_ = 1, max_ = 4;
    float pct_ = 50.f, range_ = 1500.f, delay_ = 0.08f;

    void* enemyClass_ = nullptr;
    void* lastWeapon_ = nullptr;
    bool lastIsFlamethrower_ = false;
    float base_ = 0.f;        // the current hit's damage before buffs
    bool hopping_ = false;    // a ricochet's own hit is running
    float hopDamage_ = 0.f;   // its damage after the game's buffs (logged)
    int logged_ = 0;
    std::deque<Chain> chains_;
};

Ricochet g_item;

}  // namespace
}  // namespace crabmods
