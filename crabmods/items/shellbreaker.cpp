// Shellbreaker: a melee weapon picked in the lobby, a barbed blade on a chain swung from a handle. Left click swings
// a string of five (right to left, left to right, a chop from top right, the rebound back up, a circle over the
// head); right click fires the hook to swing on (hold to swing, let go to fly off). While it's held, the crab's
// own melee does nothing; the melee mods power the swings instead, and Shellbreaker's own loot (shell_loot.cpp)
// drops too.
//
// Every machine: the weapon's data asset (a copy of the Sniper's with an invisible projectile, as the Soul Whip)
// is made once under the same name. Its handle is the weapon's mesh; the chain's links and the blade are mesh
// pieces on the weapon actor, placed each frame along a simulated chain (shell_chain.hpp). Each shot (the
// weapon's replicated shot count) starts the next swing, which pulls the blade along a path around the crab; the
// physics does the rest, so the follow-through and the rebound are the chain's own. Every machine also draws
// every player's hook rope (the host passes the throws on), shows the weapon on its pickups and plays the sounds
// (synthesized, in the pack, through necro_art.hpp's mixer). In the lobby: a pedestal left of the Soul Whip's
// (each machine its own stone; the host the pickup).
// The player's machine: the hits. Each shot's projectile is moved to where the swing lands and bursts there, for
// 100 dps over the string (each swing's share by its weight). The hook: aimed and thrown here; then light
// enemies are yanked in, heavy ones pull the crab to them, and terrain swings the crab on the chain.
// Host: the hook's hit (damage and ice), yanks, Whirlwind's drag, Riptide's damage and Searing Edge's fire.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include <crab/hook.hpp>

#include "../item.hpp"
#include "../ui.hpp"
#include "../world.hpp"
#include "necro_art.hpp"
#include "shell_chain.hpp"
#include "shell_loot.hpp"
#include "shell_swing.hpp"

namespace crabmods {
namespace {

using shell::V3;
namespace loot = shell_loot;

constexpr wchar_t kSpawnablesPath[] = L"/Game/Blueprint/DataAsset/DA_Spawnables.DA_Spawnables";
constexpr wchar_t kEnemyClassPath[] = L"/Script/CrabChampions.CrabEnemyC";
constexpr wchar_t kMsg[] = L"shell ";  // our messages, both ways
constexpr size_t kMsgLen = std::size(kMsg) - 1;
constexpr int Weapon_FireRate = 0x2C0;  // float: the weapon's fire rate multiplier (game.hpp, UpdateFireRate)
constexpr int Weapon_Interval = 0x2C4;  // float: seconds between shots, 1 / min(500, FireRate x DA BaseFireRate)
constexpr int kMaxLinks = 120;          // link pieces per weapon; a longer chain stretches them
constexpr float kDeg = 3.14159265f / 180.f;
constexpr float kRiptideSeconds = 4.f;
constexpr uint8_t kIce = 1, kFire = 2, kLightning = 3, kPoison = 4, kArcane = 5;  // ECrabDebuffType

double Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

int VirtualKey(std::string name) {
    for (char& ch : name) ch = char(toupper(uint8_t(ch)));
    if (name.size() == 1 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9'))) return name[0];
    if (name.size() >= 2 && name[0] == 'F') {
        int n = atoi(name.c_str() + 1);
        if (n >= 1 && n <= 24) return VK_F1 + n - 1;
    }
    static const std::pair<const char*, int> named[] = {
        {"MOUSE1", VK_LBUTTON}, {"MOUSE2", VK_RBUTTON}, {"MOUSE3", VK_MBUTTON}, {"MOUSE4", VK_XBUTTON1},
        {"MOUSE5", VK_XBUTTON2}, {"SHIFT", VK_SHIFT},   {"CTRL", VK_CONTROL},   {"ALT", VK_MENU},
        {"SPACE", VK_SPACE},     {"TAB", VK_TAB},       {"CAPSLOCK", VK_CAPITAL},
    };
    for (auto& [n, vk] : named)
        if (name == n) return vk;
    return 0;
}

bool KeyDown(int vk) { return vk && (GetAsyncKeyState(vk) & 0x8000); }

V3 Of(const Vec3& v) { return {v.x, v.y, v.z}; }
Vec3 ToVec(V3 v) { return {v.x, v.y, v.z}; }
// A direction from yaw and elevation (degrees; UE: yaw grows to the right).
V3 Dir(float yaw, float elev) {
    return {std::cos(elev * kDeg) * std::cos(yaw * kDeg), std::cos(elev * kDeg) * std::sin(yaw * kDeg),
            std::sin(elev * kDeg)};
}

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

void Launch(void* c, V3 v) {
    static necro::Method launch(L"LaunchCharacter");
    struct {
        Vec3 v;
        bool xy, z;
    } p{ToVec(v), true, true};
    launch(c, &p);
}

V3 Velocity(void* actor) {
    static necro::Method getVelocity(L"GetVelocity");
    Vec3 v{};
    getVelocity(actor, &v);
    return Of(v);
}

float AimYaw(void* c, float* pitch = nullptr) {
    static necro::Method aimRotation(L"GetBaseAimRotation");
    Vec3 r{};
    aimRotation(c, &r);
    if (pitch) *pitch = r.x > 180.f ? r.x - 360.f : r.x;
    return r.y;
}

float HalfHeight(void* c) {
    static necro::Method halfHeight(L"GetScaledCapsuleHalfHeight");
    float half = 0.f;
    if (void* capsule = At<void*>(c, off::Character_Capsule)) halfHeight(capsule, &half);
    return half;
}

std::wstring NameOf(void* ps) {
    auto& s = At<TArray<wchar_t>>(ps, off::PS_PlayerName);
    std::wstring out;
    for (int i = 0; s.Data && i < s.Num && s.Data[i]; i++) out.push_back(s.Data[i]);
    return out;
}

// Adds stacks of an element to a hit (FCrabDebuff: uint8 type, int32 stacks at +4).
void AddDebuff(void* info, uint8_t type, int stacks) {
    if (stacks <= 0) return;
    auto& a = At<TArray<uint8_t>>(info, off::DamageInfo_Debuffs);
    for (int i = 0; a.Data && i < a.Num; i++)
        if (a.Data[i * 8] == type) {
            At<int32_t>(a.Data + i * 8, 4) += stacks;
            return;
        }
    if (a.Num == a.Max) {
        int max = a.Max ? a.Max * 2 : 2;
        auto* p = Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(a.Data, size_t(max) * 8, 0);
        if (!p) return;
        a.Data = static_cast<uint8_t*>(p);
        a.Max = max;
    }
    uint8_t* e = a.Data + a.Num++ * 8;
    std::memset(e, 0, 8);
    e[0] = type;
    At<int32_t>(e, 4) = stacks;
}

// The five swings of the string: the blade's path around the crab, as yaw (from the aim) and elevation, from and
// to. rate: how hard the blade is pulled along it (1/s); body: how much of that the links get; weight: the swing's
// share of the damage (they average 1 over the string, so the string is dps). Over: the circle over the head
// (its centre above the crab, not at the hand), which hits all around.
struct Swing {
    float yaw0, yaw1, elev0, elev1, rate, body, weight;
    bool over;
};
constexpr Swing kSwings[] = {
    {80.f, -80.f, -5.f, -5.f, 45.f, 0.6f, 0.91f, false},    // right to left
    {-80.f, 80.f, -5.f, -5.f, 45.f, 0.6f, 0.91f, false},    // left to right
    {45.f, -40.f, 70.f, -35.f, 45.f, 0.6f, 0.91f, false},   // the chop: top right to bottom left
    {-40.f, 40.f, -35.f, 50.f, 10.f, 0.3f, 0.68f, false},   // the rebound: mostly the chain's own momentum
    {90.f, -270.f, 10.f, 10.f, 40.f, 0.5f, 1.59f, true},    // the circle over the head
};
constexpr int kSwingCount = int(std::size(kSwings));

class Shellbreaker : public Item {
public:
    Shellbreaker() : Item("shellbreaker", 205), rng_(std::random_device{}()) {
        self_ = this;
        spawnKey_ = VirtualKey(spawnKeyName_);
        hookKey_ = VirtualKey(hookKeyName_);
    }

    std::string DefaultIni() const override {
        return R"(# Shellbreaker: a barbed blade on a chain, picked in the lobby (left of the Soul Whip). Left click swings a
# string of five (right to left, left to right, a chop, the rebound, a circle over the head); right click fires the
# hook: hold it to swing, let go to fly off. Replaces your melee while held. Its own weapon mods, perks and relics drop for its
# holders, and of the game's weapon mods only those that work for a blade (damage and fire rate).
enabled = true

# Damage per second over the string, and the average time of a swing in seconds (fire rate bonuses make the
# swings faster). Each swing takes and deals its share: the sweeps and the chop 0.91 of the average, the rebound
# 0.68, the circle 1.59 (0.32 / 0.24 / 0.56 s at 0.35).
dps = 75
swingSeconds = 0.35

# Where a swing hits: this far in front of the crab, in a burst of this radius (cm). The circle over the head
# hits all around, in circleRadius.
reach = 2200
hitRadius = 450
circleRadius = 2400

# The melee mods power the swings (damage, Big Claws' range, the Claws' elements, Vampire) at this share of
# their worth, and drop this many times as often for Shellbreaker holders.
meleeModShare = 0.25
meleeModWeight = 3

# Links in the chain at rest (the blade's distance from the hand).
chainLinks = 40

# The hook: how far it reaches (cm), its cooldown (s; only hooking an enemy uses a charge), how fast the blade
# flies out and the damage of its hit on an enemy.
hookRange = 6000
hookCooldown = 6
hookSpeed = 10000
hookDamage = 60
# Enemies up to this category are yanked to you (1 fodder, 2 medium, 3 hard, 4 elite); heavier ones pull you to
# them. Speeds in cm/s; reelSpeed: holding left click while swinging on the hook shortens the chain.
yankMaxCategory = 2
yankSpeed = 1600
pullSpeed = 2200
reelSpeed = 600
# Swinging: press the hook key to fire at the hook point (the orange diamond on screen), hold it to swing, let go
# to fly off. An enemy within enemyAssist degrees of your crosshair (a red diamond) is hooked instead, while you
# have a charge. With autoAnchor the hook point is found for you: the best spot near your crosshair (or above it)
# within anchorRange (cm), anchorIdeal away at best; skyAnchor also catches a point in the air ahead when there's
# nothing else. Without autoAnchor the hook goes where you aim. swingPump: how hard your movement keys speed the
# swing up (cm/s per second), and swingMaxSpeed the fastest a swing goes; letting go multiplies your speed by
# releaseBoost and adds releaseLift upward. swingKeepHeight: the swing holds you at the height you started at (at
# least a little off the ground) as you pass under the hook point, and swings you up at the end of the chain;
# false: a plain pendulum, where catchLift is the share the chain shortens to after it catches.
autoAnchor = true
skyAnchor = false
anchorRange = 5000
anchorIdeal = 2200
enemyAssist = 7
swingPump = 1200
swingMaxSpeed = 4000
swingKeepHeight = true
releaseBoost = 1.2
releaseLift = 450
catchLift = 0.85

# Chance-based weapon mods roll x this.
procRoll = 1

# Our sounds' volume, 0..1 (on top of the game's Master and Gameplay sliders).
soundVolume = 0.8

# Your keys (yours alone; the host's don't replace them): the hook, and a test key that puts a Shellbreaker in
# front of you in the lobby ("none" switches it off).
hookKey = Mouse2
spawnKey = none
)";
    }
    void Set(const std::string& k, const std::string& v) override {
        if (k == "enabled") enabled_ = ParseBool(v);
        else if (k == "dps") dps_ = std::max(0.f, ParseFloat(v));
        else if (k == "meleeModShare") meleeModShare_ = std::max(0.f, ParseFloat(v));
        else if (k == "meleeModWeight") meleeModWeight_ = std::max(0.f, ParseFloat(v));
        else if (k == "swingSeconds") swingSeconds_ = std::clamp(ParseFloat(v), 0.05f, 2.f);
        else if (k == "reach") reach_ = std::max(0.f, ParseFloat(v));
        else if (k == "hitRadius") hitRadius_ = std::max(10.f, ParseFloat(v));
        else if (k == "circleRadius") circleRadius_ = std::max(10.f, ParseFloat(v));
        else if (k == "chainLinks") chainLinks_ = std::clamp(int(ParseFloat(v)), 2, 80);
        else if (k == "hookRange") hookRange_ = std::max(100.f, ParseFloat(v));
        else if (k == "hookCooldown") hookCooldown_ = std::max(0.f, ParseFloat(v));
        else if (k == "hookSpeed") hookSpeed_ = std::max(100.f, ParseFloat(v));
        else if (k == "hookDamage") hookDamage_ = std::max(0.f, ParseFloat(v));
        else if (k == "yankMaxCategory") yankMax_ = std::clamp(int(ParseFloat(v)), 0, 5);
        else if (k == "yankSpeed") yankSpeed_ = std::max(0.f, ParseFloat(v));
        else if (k == "pullSpeed") pullSpeed_ = std::max(0.f, ParseFloat(v));
        else if (k == "reelSpeed") reelSpeed_ = std::max(0.f, ParseFloat(v));
        else if (k == "autoAnchor") autoAnchor_ = ParseBool(v);
        else if (k == "skyAnchor") skyAnchor_ = ParseBool(v);
        else if (k == "anchorRange") anchorRange_ = std::max(300.f, ParseFloat(v));
        else if (k == "anchorIdeal") anchorIdeal_ = std::max(200.f, ParseFloat(v));
        else if (k == "swingMaxSpeed") swingMaxSpeed_ = std::max(500.f, ParseFloat(v));
        else if (k == "swingKeepHeight") swingKeepHeight_ = ParseBool(v);
        else if (k == "enemyAssist") enemyAssist_ = std::clamp(ParseFloat(v), 0.f, 30.f);
        else if (k == "swingPump") swingPump_ = std::max(0.f, ParseFloat(v));
        else if (k == "releaseBoost") releaseBoost_ = std::clamp(ParseFloat(v), 0.f, 3.f);
        else if (k == "releaseLift") releaseLift_ = std::max(0.f, ParseFloat(v));
        else if (k == "catchLift") catchLift_ = std::clamp(ParseFloat(v), 0.3f, 1.f);
        else if (k == "procRoll") procRoll_ = std::max(0.f, ParseFloat(v));
        else if (k == "soundVolume") mixer_.SetVolume(std::clamp(ParseFloat(v), 0.f, 2.f));
        else if (k == "hookKey") hookKeyName_ = v, hookKey_ = VirtualKey(v);
        else if (k == "spawnKey") spawnKeyName_ = v, spawnKey_ = VirtualKey(v);
        RefreshWeapon();
    }
    bool LocalSetting(const std::string& k) const override {
        return k == "hookKey" || k == "spawnKey" || k == "soundVolume";
    }
    bool Enabled() const override { return enabled_; }
    std::string Describe() const override {
        return "Shellbreaker " + Num(dps_) + " dps, " + Num(swingSeconds_) + " s swings, " +
               std::to_string(chainLinks_) + " links; hook " + Num(hookRange_ / 100.f) + " m every " +
               Num(hookCooldown_) + " s on " + hookKeyName_;
    }
    std::string Title() const override { return "Shellbreaker"; }
    const char* CatalogKind() const override { return "Weapon"; }
    static bool HoldsIt(void* ps) { return self_ && self_->enabled_ && self_->Holds(ps); }

    void InstallHooks() override {
        Hook(va::ACrabPlayerC_MeleePressed, &HkMeleePressed, &oMeleePressed, "ACrabPlayerC melee pressed");
    }

    void OnBeginPlay(void* lm) override {
        EnsureWeapon();
        RefreshWeapon();
        world_ = WorldOf(lm);
        rigs_.clear();  // the old level's weapons and pickups (and our pieces on them) are gone
        shows_.clear();
        shots_.clear();
        ropes_.clear();
        hook_ = Hook_{};
        charges_ = -1;  // full again
        hud_ = nullptr;  // the new level's HUD is built again
    }

    void OnTick(void* world) override {
        EnsureWeapon();
        enemyClass_ = enemyClass_ ? enemyClass_ : ui::FindObject(kEnemyClassPath);
        void* gs = GameStateOf(world);
        if (!gs || !weapon_) return;
        // The pedestal, as the Soul Whip's: gone once the lobby is (the islands stream into the lobby's world);
        // in the lobby, checked once a second and put back if it's missing.
        bool lobby = At<uint8_t>(gs, off::GS_CurrentIslandType) == uint8_t(IslandType::Lobby);
        if (!pedestal_.empty() && !lobby) {
            static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
            static void* meshActorClass = ui::FindObject(L"/Script/Engine.StaticMeshActor");
            std::vector<void*> present = AllActorsOf(world, meshActorClass), pickups = AllActorsOf(world, pickupClass);
            present.insert(present.end(), pickups.begin(), pickups.end());
            for (void* a : pedestal_)
                if (std::find(present.begin(), present.end(), a) != present.end()) ui::Call(a, L"K2_DestroyActor");
            pedestal_.clear();
            stone_ = nullptr;
        }
        if (lobby && Now() >= nextPedestalCheck_) {
            nextPedestalCheck_ = Now() + 1.0;
            if (void* lm = LevelManagerOf(world)) AddLobbyPedestal(lm);
        }
    }

    // --- Hits (the shooter's machine) -----------------------------------------------------------------------

    void OnShot(void* weapon, void* ps, Vec3& start, Vec3& dir) override {
        if (!enabled_ || !weapon_ || At<void*>(weapon, off::Weapon_WeaponDA) != weapon_) return;
        void* c = At<void*>(weapon, off::Weapon_OwningC);
        void* pc = At<void*>(weapon, off::Weapon_OwningPC);
        if (!c) return;
        // Left click while the hook is out reels it in: the swing's shot does nothing.
        if (hook_.phase != Phase::None) {
            start.z -= 1e6f;
            return;
        }
        ShotState& st = shots_[weapon];
        int32_t fired = At<int32_t>(weapon, off::Weapon_TimesFired);
        bool fresh = fired != st.fired;
        if (fresh) {  // a new shot (its other projectiles share the swing)
            double was = st.swing >= 0 ? SwingTime(weapon) * kSwings[st.swing].weight : 0.0;
            st.fired = fired;
            st.swing = NextSwing(st.swing, st.last, was);
            st.last = Now();
            // Each swing takes its own time (its weight of the string's): the gap to the next shot is this swing's.
            // The weapon's Fire (0x140d6a5d0) re-arms its one-shot fire timer from the interval right after the
            // projectiles (this call) are out, so this one counts. Computed as UpdateFireRate does, on this weapon
            // only (the asset is everyone's).
            float rate = std::max(0.05f, At<float>(weapon, Weapon_FireRate));
            At<float>(weapon, Weapon_Interval) =
                1.f / std::min(500.f, rate / (swingSeconds_ * kSwings[st.swing].weight));
        }
        const Swing& sw = kSwings[st.swing];
        Vec3 at{};
        LocationOf(c, at);
        float f = ChainFactor(ps);
        // The shot is only the impact's look, small and harmless: the swing's damage is the host's (Strike), over
        // the whole area the chain sweeps. A burst the swing's size whited out the screen.
        At<float>(projDA_, off::ProjDA_ExplosionDamage) = 0.f;
        At<float>(projDA_, off::ProjDA_ExplosionRadius) = kFxRadius;
        float pitch = 0.f, yaw = AimYaw(c, &pitch);
        if (sw.over) {
            start = at;
            // Whirlwind: the host drags the enemies around in.
            if (fresh && pc && loot::Level(ps, loot::Whirlwind) > 0) {
                wchar_t buf[96];
                swprintf(buf, std::size(buf), L"%lsgather %.0f %.0f %.0f", kMsg, at.x, at.y, at.z);
                SendToHost(pc, buf);
            }
        } else {
            V3 fw = Dir(yaw, std::clamp(pitch, -30.f, 30.f));
            start = ToVec(Of(at) + fw * (reach_ * f));
            dir = ToVec(fw);
        }
        if (fresh && pc) {
            wchar_t buf[96];
            swprintf(buf, std::size(buf), L"%lsswing %d %.0f %.0f %.0f", kMsg, st.swing, start.x, start.y, start.z);
            SendToHost(pc, buf);
        }
    }

    // A swing's hit (host): everything in its area takes its share of the string's damage, as the player, through
    // the game's radial damage, as a melee hit (what breaks shields and the things a claw breaks; damage perks,
    // crits and kills apply there). Weapon mods' damage is applied here, as the game does for a projectile, and the
    // melee mods count at meleeModShare of their worth: their damage and range, and their elements and Vampire's
    // tag (as the game fills a melee hit). The area's centre is the shooter's word, kept within the swing's reach.
    void Strike(void* c, void* ps, int swing, V3 center) {
        if (swing < 0 || swing >= kSwingCount) return;
        const auto& sw = kSwings[swing];
        Vec3 at{};
        if (!LocationOf(c, at)) return;
        auto share = [&](uintptr_t fn) { return 1.f + (Fn<float (*)(void*)>(fn)(c) - 1.f) * meleeModShare_; };
        float f = ChainFactor(ps), wide = (1.f + loot::Total(ps, loot::WideArc) / 100.f) *
                                          share(va::ACrabPlayerC_MeleeRangeMult);
        V3 from = Of(at), d = center - from;
        float l = shell::Length(d), most = sw.over ? 0.f : reach_ * f;
        if (l > most + 1.f) center = most > 0.f && l > 0.f ? from + d * (most / l) : from;
        float damage = dps_ * swingSeconds_ * sw.weight;
        damage *= 1.f + loot::Total(ps, loot::SerratedLinks) / 100.f;
        damage *= 1.f + loot::Total(ps, loot::Momentum) * float(swing) / 100.f;
        if (swing == 2 || swing == 3) damage *= 1.f + loot::Total(ps, loot::Executioner) / 100.f;
        if (sw.over) damage *= 1.f + loot::Total(ps, loot::Rhythm) / 100.f;
        damage *= Fn<float (*)(void*)>(va::ACrabPS_WeaponModDamage)(ps);
        damage *= share(va::ACrabPlayerC_MeleeDamageMult);
        float radius = (sw.over ? circleRadius_ * f : hitRadius_) * wide;
        alignas(8) uint8_t info[off::DamageInfo_Size];
        std::memset(info, 0, sizeof info);
        At<uint8_t>(info, off::DamageInfo_Type) = 3;  // Melee
        At<void*>(info, off::DamageInfo_DamagingC) = c;
        Fn<void (*)(void*, void*)>(va::ACrabPlayerC_FillMeleeInfo)(c, info);
        AddDebuff(info, kFire, int(std::lround(loot::Total(ps, loot::SearingEdge))));
        AddDebuff(info, kPoison, int(std::lround(loot::Total(ps, loot::VenomBarbs))));
        AddDebuff(info, kLightning, int(std::lround(loot::Total(ps, loot::StaticChain))));
        AddDebuff(info, kIce, int(std::lround(loot::Total(ps, loot::RimeEdge))));
        AddDebuff(info, kArcane, int(std::lround(loot::Total(ps, loot::HexLinks))));
        auto hit = [&](V3 p, float dmg) {
            Vec3 where = ToVec(p);
            At<float>(info, off::DamageInfo_Damage) = dmg;
            At<Vec3>(info, off::DamageInfo_Location) = where;
            TArray<void*> ignore{};  // passed by value: the game frees it, so it stays empty
            Fn<void (*)(void*, TArray<void*>*, const Vec3*, float, const void*, bool, void*)>(va::RadialDamage)(
                c, &ignore, &where, radius, info, false, nullptr);
        };
        hit(center, damage);
        // Backlash: the sweeps and the chop strike as far behind the crab too.
        if (float back = loot::Total(ps, loot::Backlash) / 100.f; back > 0.f && !sw.over)
            hit(from - (center - from), damage * back);
        auto free = [&](int o) {
            if (void* p = At<TArray<uint8_t>>(info, o).Data) Fn<void (*)(void*)>(va::FMemory_Free)(p);
        };
        free(off::DamageInfo_Debuffs);
        free(off::DamageInfo_Tags);
    }

    // --- Host ---------------------------------------------------------------------------------------------------

    float DamageBonus(void* ps) override {
        auto it = riptide_.find(ps);
        if (it == riptide_.end() || Now() - it->second > kRiptideSeconds) return 0.f;
        return loot::Total(ps, loot::Riptide) / 100.f;
    }

    bool OnHostMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, kMsgLen, kMsg) != 0) return false;
        const wchar_t* s = text.c_str() + kMsgLen;
        void* ps = At<void*>(pc, off::Controller_PlayerState);
        void* c = ps ? PawnOf(ps) : nullptr;
        Vec3 a;
        int k;
        if (std::wcscmp(s, L"give") == 0) Give(pc, c);
        else if (!c || !Holds(ps)) return true;
        else if (swscanf(s, L"throw %f %f %f", &a.x, &a.y, &a.z) == 3) Relay(pc, ps, true, Of(a));
        else if (std::wcscmp(s, L"back") == 0) Relay(pc, ps, false, {});
        else if (swscanf(s, L"land %d %f %f %f", &k, &a.x, &a.y, &a.z) == 4) Land(c, ps, k, Of(a));
        else if (swscanf(s, L"gather %f %f %f", &a.x, &a.y, &a.z) == 3) Gather(c, ps, Of(a));
        else if (swscanf(s, L"swing %d %f %f %f", &k, &a.x, &a.y, &a.z) == 4) Strike(c, ps, k, Of(a));
        else if (swscanf(s, L"launch %f %f %f", &a.x, &a.y, &a.z) == 3) {
            V3 v = Of(a);
            float l = shell::Length(v);
            if (l > 4000.f) v = v * (4000.f / l);
            Launch(c, v);
        }
        return true;
    }

    // Another player's hook, from the host: "rope 1 x y z name" (thrown at x y z) or "rope 0 0 0 0 name" (back).
    bool OnClientMessage(void* pc, const std::wstring& text) override {
        if (text.compare(0, kMsgLen, kMsg) != 0) return false;
        const wchar_t* s = text.c_str() + kMsgLen;
        int on, used = 0;
        Vec3 a;
        if (swscanf(s, L"rope %d %f %f %f %n", &on, &a.x, &a.y, &a.z, &used) == 4 && used > 0)
            SeeRope(s + used, on != 0, Of(a));
        return true;
    }

    // --- Every frame ------------------------------------------------------------------------------------------

    void OnFrame(void* world, float dt) override {
        if (!enabled_) return;
        world_ = world;
        FollowGameVolume(world);
        void* lm = LevelManagerOf(world);
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        bool local = IsA(c, StaticClass(va::ACrabPlayerC_StaticClass));
        void* ps = local ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
        void* pc = local ? At<void*>(c, off::Pawn_Controller) : nullptr;
        bool ready = pc && InGame(pc);
        bool spawn = ready && KeyDown(spawnKey_);
        if (spawn && !spawnDown_) SendToHost(pc, std::wstring(kMsg) + L"give");
        spawnDown_ = spawn;
        // Picking it up.
        void* held = ps ? At<void*>(ps, off::PS_WeaponDA) : nullptr;
        if (weapon_ && held == weapon_ && lastHeld_ && lastHeld_ != weapon_) Play("A_Shell_Equip_", 0.9f);
        lastHeld_ = held;
        if (local) UpdateHook(world, c, ps, pc, ready && Holds(ps), dt);
        if (local) UpdateHud(pc, ps, Holds(ps));
        UpdateRigs(world, local ? c : nullptr, dt);
        UpdatePickups(world);
    }

private:
    // --- The asset ------------------------------------------------------------------------------------------

    void EnsureWeapon() {
        if (weapon_ || !enabled_) return;
        void* sp = ui::FindObject(kSpawnablesPath);
        if (!sp) return;
        auto& weapons = At<TArray<void*>>(sp, off::Spawnables_Weapons);
        void *sniper = nullptr, *scepter = nullptr;
        for (int i = 0; weapons.Data && i < weapons.Num; i++) {
            std::string n = ObjectName(weapons.Data[i]);
            if (n == "DA_Weapon_Sniper") sniper = weapons.Data[i];
            else if (n == "DA_Weapon_LightningScepter") scepter = weapons.Data[i];
        }
        if (!sniper) return;
        weapon_ = MakeDA(At<void*>(sniper, off::Object_Class), sniper, L"DA_Weapon_Shellbreaker");
        if (!weapon_) return;
        if (void* proj = At<void*>(sniper, off::WeaponDA_ProjectileDA))
            projDA_ = MakeDA(At<void*>(proj, off::Object_Class), proj, L"DA_Projectile_Shellbreaker");
        if (!projDA_) {
            CRAB_LOG("shellbreaker: couldn't copy the Sniper's projectile; no Shellbreaker");
            weapon_ = nullptr;
            return;
        }
        At<void*>(weapon_, off::WeaponDA_ProjectileDA) = projDA_;
        // The hit: nothing to see, it bursts where it starts (OnShot puts it where the swing lands).
        for (int f : {off::ProjDA_FX, off::ProjDA_TrailFX, off::ProjDA_Sound, off::ProjDA_ExplosionFX,
                      off::ProjDA_ExplosionSound})
            At<void*>(projDA_, f) = nullptr;
        At<float>(projDA_, off::ProjDA_Speed) = 10.f;
        At<float>(projDA_, off::ProjDA_MaxSpeed) = 0.f;
        At<float>(projDA_, off::ProjDA_GravityScale) = 0.f;
        At<float>(projDA_, off::ProjDA_CollisionRadius) = 1.f;
        At<float>(projDA_, off::ProjDA_Lifetime) = 0.01f;
        At<int32_t>(projDA_, off::ProjDA_MaxPiercings) = 0;
        At<bool>(projDA_, off::ProjDA_bExplodeAfterLifetime) = true;
        SetFString(weapon_, off::DA_Name, L"Shellbreaker");
        SetFString(weapon_, off::DA_Description,
                   L"A barbed blade on a chain. Hold right click to swing on the hook. Replaces your melee");
        At<bool>(weapon_, off::DA_bRequiresUnlock) = false;
        At<bool>(weapon_, off::WeaponDA_bInfiniteClipSize) = true;
        At<uint8_t>(weapon_, off::WeaponDA_FireMode) = 0;  // automatic: hold to keep swinging
        if (scepter) {
            std::memcpy(&At<uint8_t>(weapon_, 0xF0), &At<uint8_t>(scepter, 0xF0), 0x10);  // aiming FOV, arm offset
            At<uint8_t>(weapon_, off::WeaponDA_CrosshairType) = At<uint8_t>(scepter, off::WeaponDA_CrosshairType);
            At<void*>(weapon_, off::WeaponDA_FireMontage) = At<void*>(scepter, off::WeaponDA_FireMontage);
        }
        At<void*>(weapon_, off::WeaponDA_PostFireClearChamberMontage) = nullptr;
        At<void*>(weapon_, off::WeaponDA_PostFireClearChamberSound) = nullptr;
        At<float>(weapon_, off::WeaponDA_VerticalRecoil) = 0.f;
        At<float>(weapon_, off::WeaponDA_HorizontalRecoil) = 0.f;
        At<void*>(weapon_, off::WeaponDA_GunshotSound) = nullptr;
        At<void*>(weapon_, off::WeaponDA_EquipSound) = nullptr;
        At<void*>(weapon_, off::WeaponDA_MuzzleFlashFX) = nullptr;
        At<void*>(weapon_, off::WeaponDA_CosmeticFX) = nullptr;
        At<void*>(weapon_, off::DA_PrimaryFX) = nullptr;  // its pickup: the weapon itself (UpdatePickups), in amber
        At<ui::LinearColor>(weapon_, off::DA_Tint) = {1.f, 0.6f, 0.25f, 1.f};
        // Where a swing lands: a small impact from the game's own weapons (the first of these one of them uses).
        for (const char* want : {"FX_Impact_Small", "FX_Impact_Tiny", "FX_Impact_LightningScepter"}) {
            for (int i = 0; weapons.Data && i < weapons.Num && !At<void*>(projDA_, off::ProjDA_ExplosionFX); i++) {
                void* proj = weapons.Data[i] ? At<void*>(weapons.Data[i], off::WeaponDA_ProjectileDA) : nullptr;
                void* fx = proj ? At<void*>(proj, off::ProjDA_ExplosionFX) : nullptr;
                if (fx && ObjectName(fx) == want) At<void*>(projDA_, off::ProjDA_ExplosionFX) = fx;
            }
        }
        if (void* fx = At<void*>(projDA_, off::ProjDA_ExplosionFX)) CRAB_LOG("shellbreaker: hit FX %s", ObjectName(fx).c_str());
        else CRAB_LOG("shellbreaker: no small impact FX found; the swings land unseen");
        BlockMeleeNotify();
        FindMontages(sp);
        CRAB_LOG("shellbreaker: created %s from %s", ObjectName(weapon_).c_str(), ObjectName(sniper).c_str());
        RefreshWeapon();
        EnsureArt();
    }

    void RefreshWeapon() {
        if (!weapon_ || !projDA_) return;
        At<float>(weapon_, off::WeaponDA_BaseFireRate) = 1.f / swingSeconds_;
        At<float>(weapon_, off::WeaponDA_ModTriggerRollMultiplier) = procRoll_;
        At<float>(projDA_, off::ProjDA_ExplosionDamage) = 0.f;  // the look only: Strike deals the damage
        At<float>(projDA_, off::ProjDA_ExplosionRadius) = kFxRadius;
    }

    void EnsureArt() {
        if (artTried_ || !weapon_) return;
        artTried_ = true;
        if (!pack_.Load()) return;
        necro::Blob rig = pack_.Get("shell.rig");
        if (!rig || rig.size < 36) {
            CRAB_LOG("shellbreaker: the rig is missing; it keeps the Sniper's look");
            return;
        }
        std::memcpy(&anchor_, rig.data, 12);
        std::memcpy(&anchorDir_, rig.data + 12, 12);
        std::memcpy(&pitch_, rig.data + 24, 4);
        std::memcpy(&bladeReach_, rig.data + 32, 4);
        mid_ = necro::TextureMaterial(necro::Texture(pack_.Get("palette.png")));
        void* handle = necro::Mesh(pack_.Get("shell_handle.mesh"), mid_, L"SM_Shellbreaker_Handle");
        linkMesh_ = necro::Mesh(pack_.Get("shell_link.mesh"), mid_, L"SM_Shellbreaker_Link");
        bladeMesh_ = necro::Mesh(pack_.Get("shell_blade.mesh"), mid_, L"SM_Shellbreaker_Blade");
        if (!handle || !linkMesh_ || !bladeMesh_ || pitch_ <= 0.f) {
            CRAB_LOG("shellbreaker: couldn't build the meshes; it keeps the Sniper's look");
            linkMesh_ = bladeMesh_ = nullptr;
            return;
        }
        At<void*>(weapon_, off::WeaponDA_Mesh) = handle;
        handle_ = handle;
        CRAB_LOG("shellbreaker: art ready (link pitch %.1f cm, blade %.1f cm)", pitch_, bladeReach_);
    }

    // --- Swings (every machine) ---------------------------------------------------------------------------

    static double SwingTime(void* weapon) {
        float rate = std::max(0.2f, At<float>(weapon, Weapon_FireRate));
        return self_->swingSeconds_ / rate;
    }
    // The string goes on while the swings follow each other; a pause starts it over.
    static int NextSwing(int swing, double last, double swingTime) {
        if (swing < 0 || Now() - last > swingTime + 0.6) return 0;
        return (swing + 1) % kSwingCount;
    }
    // Long Chain: the chain's length over its resting one (swings, circle and hook grow with it), at most 2.5x.
    float ChainFactor(void* ps) const {
        return std::min(2.5f, (float(chainLinks_) + loot::Total(ps, loot::LongChain)) / float(chainLinks_));
    }

    enum class Phase { None, Flying, Swinging, Returning };  // Swinging: the blade stuck, holding the chain

    // A hook as every machine draws it.
    struct Rope {
        Phase phase = Phase::None;
        V3 pin{}, target{};
        float length = 0.f;
        bool fresh = false;  // just thrown: starts from the hand
    };

    struct Rig {
        shell::Chain chain;
        std::vector<void*> links;
        std::vector<char> linkHidden;
        void* blade = nullptr;
        int32_t fired = 0;
        int swing = -1;
        double start = 0.0, length = 0.0;  // the swing's start and time
        float yaw = 0.f;                    // the aim when it started
        Phase lastPhase = Phase::None;
        bool hidden = false, seen = false, broken = false;
        char bladeHidden = 0;
    };

    float Scale(void* c) const {
        void* mesh = nullptr;
        auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
        for (int i = 0; weapons.Data && i < weapons.Num && !mesh; i++)
            if (weapons.Data[i]) mesh = At<void*>(weapons.Data[i], off::Weapon_Mesh);
        if (!mesh) return 1.f;
        V3 sc = Of(At<Vec3>(mesh, off::SceneComponent_Scale));
        float s = (std::fabs(sc.x) + std::fabs(sc.y) + std::fabs(sc.z)) / 3.f;
        return s > 0.01f ? s : 1.f;
    }

    void UpdateRigs(void* world, void* localC, float dt) {
        if (!linkMesh_) return;
        for (auto& [w, r] : rigs_) r.seen = false;
        ForEachPlayer(world, [&](void* ps) {
            void* c = PawnOf(ps);
            if (!c) return;
            auto& weapons = At<TArray<void*>>(c, off::C_Weapons);
            for (int i = 0; weapons.Data && i < weapons.Num; i++) {
                void* w = weapons.Data[i];
                if (!w || At<void*>(w, off::Weapon_WeaponDA) != weapon_) continue;
                auto it = rigs_.find(w);
                if (it == rigs_.end()) {
                    it = rigs_.emplace(w, Rig{}).first;
                    it->second.fired = At<int32_t>(w, off::Weapon_TimesFired);
                }
                it->second.seen = true;
                if (!it->second.broken) StepRig(w, c, ps, it->second, c == localC, dt);
            }
        });
        for (auto it = rigs_.begin(); it != rigs_.end();) it = it->second.seen ? std::next(it) : rigs_.erase(it);
    }

    void StepRig(void* weapon, void* c, void* ps, Rig& r, bool local, float dt) {
        void* mesh = At<void*>(weapon, off::Weapon_Mesh);
        if (!mesh) return;
        static necro::Method isVisible(L"IsVisible");
        bool visible = false;
        isVisible(mesh, &visible);
        if (visible == r.hidden) {
            r.hidden = !visible;
            for (size_t i = 0; i < r.links.size(); i++) SetHidden(r.links[i], r.linkHidden[i], r.hidden);
            if (r.blade) SetHidden(r.blade, r.bladeHidden, r.hidden);
        }
        if (r.hidden) return;

        necro::Q4 q;
        std::memcpy(&q, &At<uint8_t>(mesh, off::SceneComponent_Rotation), 16);
        V3 loc = Of(At<Vec3>(mesh, off::SceneComponent_Location));
        V3 sc = Of(At<Vec3>(mesh, off::SceneComponent_Scale));
        float s = (std::fabs(sc.x) + std::fabs(sc.y) + std::fabs(sc.z)) / 3.f;
        V3 hand = loc + necro::Rotate(q, Of(anchor_) * s);
        V3 dir = shell::Normal(necro::Rotate(q, Of(anchorDir_)));
        float rest = chainLinks_ * ChainFactor(ps) * pitch_ * s;
        if (!r.chain.Live()) r.chain.Reset(hand, dir, rest);

        Vec3 feet{};
        LocationOf(c, feet);
        float half = HalfHeight(c);

        // A shot: the next swing of the string.
        int32_t fired = At<int32_t>(weapon, off::Weapon_TimesFired);
        if (fired != r.fired) {
            r.fired = fired;
            r.swing = NextSwing(r.swing, r.start, r.length);
            r.length = SwingTime(weapon) * kSwings[r.swing].weight;
            r.start = Now();
            r.yaw = AimYaw(c);
            if (!(local && hook_.phase != Phase::None)) {
                PlayAt(kSwings[r.swing].over ? "A_Shell_Circle_" : "A_Shell_Swing_", ToVec(hand), 0.8f);
                PlayBody(c, ps, r.swing, r.length);
            }
        }

        // The hook: this machine's own from the hook itself, the others' from the host's word.
        Phase phase;
        V3 pin;
        float hookLength;
        if (local) {
            phase = hook_.phase, pin = hook_.pin, hookLength = hook_.length;
        } else {
            Rope& rope = ropes_[ps];
            StepRope(rope, hand, rest, dt);
            phase = rope.phase, pin = rope.pin, hookLength = rope.length;
        }
        if (!local && phase == Phase::Flying && r.lastPhase != Phase::Flying) {
            PlayAt("A_Shell_Throw_", ToVec(hand));
            PlayMontage(c, ps, Toss, 0.35);
        }
        if (!local && phase == Phase::Swinging && r.lastPhase == Phase::Flying) PlayAt("A_Shell_Bite_", ToVec(pin));
        r.lastPhase = phase;

        if (phase != Phase::None) {
            r.chain.Release();
            if (phase == Phase::Returning) {
                r.chain.Unpin();
                r.chain.SetLength(std::max(rest, hookLength));
            } else {
                r.chain.Pin(pin);
                r.chain.SetLength(std::max(rest, shell::Length(pin - hand)));
            }
        } else {
            r.chain.Unpin();
            r.chain.SetLength(rest);
            double u = r.swing >= 0 && r.length > 0.0 ? (Now() - r.start) / r.length : 2.0;
            if (u < 1.0) {
                const Swing& sw = kSwings[r.swing];
                float e = float(u * u * (3.0 - 2.0 * u));  // smoothstep
                float yaw = r.yaw + sw.yaw0 + (sw.yaw1 - sw.yaw0) * e;
                float elev = sw.elev0 + (sw.elev1 - sw.elev0) * e;
                V3 pivot = sw.over ? Of(feet) + V3{0, 0, half + 60.f * s} : hand;
                r.chain.Pull(pivot + Dir(yaw, elev) * rest, sw.rate, sw.body);
            } else {
                r.chain.Release();
            }
        }
        r.chain.Step(hand, dir, feet.z - half, dt);
        PlaceChain(weapon, r, s, dir, necro::Rotate(q, {1, 0, 0}));
    }

    // The crab's body (every machine): montages borrowed from the game's own melee weapons and abilities, each sped
    // up or slowed to the swing's time. The sweeps are the Katana's slash, the chop the Hammer's overhead slam, the
    // circle the Hammer's wind-up swing, the hook's throw the Grenade's (the game's Grappling Hook uses it too); the
    // rebound keeps the weapon's fire montage, which the game plays on every shot. None of these sequences has root
    // motion (checked in the pak). The Katana's, the slam's and the claw's carry the "Melee" notify, switched off for
    // Shellbreaker's holders (BlockMeleeNotify); if that failed they're skipped and only the Hammer's swing and the
    // throw play. A missing montage falls back to the crab's own claw swing.
    enum Body { Sweep, Chop, Circle, Toss, kBodies };
    static void FindMontages(void* spawnables) {
        constexpr int Spawnables_Abilities = 0xE8, Spawnables_Melee = 0xF8, MeleeDA_Montage = 0xC8,
                      MeleeDA_SlamMontage = 0xE8, AbilityDA_Montage = 0xC8;
        auto each = [&](int field, auto&& f) {
            auto& a = At<TArray<void*>>(spawnables, field);
            for (int i = 0; a.Data && i < a.Num; i++)
                if (a.Data[i]) f(ObjectName(a.Data[i]), a.Data[i]);
        };
        each(Spawnables_Melee, [&](const std::string& n, void* da) {
            if (n == "DA_Melee_Katana") body_[Sweep] = At<void*>(da, MeleeDA_Montage);
            else if (n == "DA_Melee_Hammer")
                body_[Chop] = At<void*>(da, MeleeDA_SlamMontage), body_[Circle] = At<void*>(da, MeleeDA_Montage);
        });
        each(Spawnables_Abilities, [&](const std::string& n, void* da) {
            if (n == "DA_Ability_Grenade" || (n == "DA_Ability_GrapplingHook" && !body_[Toss]))
                body_[Toss] = At<void*>(da, AbilityDA_Montage);
        });
        auto name = [](void* m) { return m ? ObjectName(m) : std::string("(claw)"); };
        CRAB_LOG("shellbreaker: body montages sweep %s, chop %s, circle %s, throw %s", name(body_[Sweep]).c_str(),
                 name(body_[Chop]).c_str(), name(body_[Circle]).c_str(), name(body_[Toss]).c_str());
    }
    static void PlayBody(void* c, void* ps, int swing, double length) {
        static const int body[kSwingCount] = {Sweep, Sweep, Chop, -1, Circle};
        if (body[swing] >= 0) PlayMontage(c, ps, Body(body[swing]), length);
    }
    static void PlayMontage(void* c, void* ps, Body which, double length) {
        constexpr int MeleeDA_Montage = 0xC8;
        void* montage = body_[which];
        void* melee = ps ? At<void*>(ps, off::PS_MeleeDA) : nullptr;
        if (!montage && melee) montage = At<void*>(melee, MeleeDA_Montage);  // the claw
        bool notifies = !body_[which] || which == Sweep || which == Chop;
        void* mesh = c ? At<void*>(c, off::Character_Mesh) : nullptr;
        if (!mesh || !montage || (notifies && noClaw_)) return;
        static necro::Method getAnim(L"GetAnimInstance"), play(L"Montage_Play"), playLength(L"GetPlayLength");
        void* anim = nullptr;
        getAnim(mesh, &anim);
        if (!anim) return;
        float len = 0.f;
        playLength(montage, &len);
        struct {
            void* montage;
            float rate;
            uint8_t returnType;  // MontageLength
            float start;
            bool stopAll;
            float ret;
        } p{montage, len > 0.f ? std::clamp(len / float(std::max(length, 0.05)), 0.5f, 4.f) : 1.f, 0, 0.f, true, 0.f};
        static_assert(offsetof(decltype(p), rate) == 0x8 && offsetof(decltype(p), start) == 0x10 &&
                      offsetof(decltype(p), stopAll) == 0x14 && offsetof(decltype(p), ret) == 0x18);
        play(anim, &p);
    }

    // The melee montage's hit (UCrabPlayerAnimInstance::AnimNotify_Melee, a native event the montage's notify calls
    // through ProcessEvent) does nothing for Shellbreaker's holders: its exec thunk is swapped for ours, which
    // passes everyone else's on. Checked first: a native function whose thunk is in the game's image.
    using ExecFn = void (*)(void* context, void* stack, void* result);
    static inline ExecFn oNotifyMelee;
    static void HkNotifyMelee(void* anim, void* stack, void* result) {
        constexpr int PlayerAnim_OwningPlayerC = 0x2D0, Frame_Code = 0x20;
        void* c = anim ? At<void*>(anim, PlayerAnim_OwningPlayerC) : nullptr;
        void* ps = c ? At<void*>(c, off::Pawn_PlayerState) : nullptr;
        if (self_ && self_->enabled_ && ps && self_->Holds(ps)) {
            if (uint8_t*& code = At<uint8_t*>(stack, Frame_Code)) code++;  // P_FINISH
            return;
        }
        oNotifyMelee(anim, stack, result);
    }
    void BlockMeleeNotify() {
        constexpr int Function_Flags = 0xB0, Function_Func = 0xD8;  // UE 4.27 UFunction
        constexpr uint32_t FUNC_Native = 0x400;
        if (oNotifyMelee) return;
        void* cls = ui::FindObject(L"/Script/CrabChampions.CrabPlayerAnimInstance");
        void* f = cls ? Fn<void* (*)(void*, uint64_t, int)>(va::UClass_FindFunctionByName)(cls, ui::Name(L"AnimNotify_Melee"), 0)
                      : nullptr;
        auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        uintptr_t func = f ? At<uintptr_t>(f, Function_Func) : 0;
        if (!f || !(At<uint32_t>(f, Function_Flags) & FUNC_Native) || func < base ||
            func >= base + nt->OptionalHeader.SizeOfImage) {
            CRAB_LOG("shellbreaker: couldn't find AnimNotify_Melee's thunk; swings play no claw animation");
            noClaw_ = true;
            return;
        }
        oNotifyMelee = reinterpret_cast<ExecFn>(func);
        At<ExecFn>(f, Function_Func) = &HkNotifyMelee;
        CRAB_LOG("shellbreaker: the claw's melee hit is off for Shellbreaker's holders");
    }

    // Another player's hook: flying out from the hand at the hook's speed, stuck where it lands until the host
    // says it's back, then reeled in.
    void StepRope(Rope& rope, V3 hand, float rest, float dt) {
        if (rope.fresh) rope.pin = hand, rope.fresh = false;
        if (rope.phase == Phase::Flying) {
            V3 d = rope.target - rope.pin;
            float step = hookSpeed_ * dt, l = shell::Length(d);
            if (l > step) rope.pin = rope.pin + d * (step / l);
            else rope.pin = rope.target, rope.phase = Phase::Swinging;
            if (Now() - lastRopeSeen_ > 12.0) rope.phase = Phase::Returning;  // never heard back
        } else if (rope.phase == Phase::Returning) {
            rope.length -= 4000.f * dt;
            if (rope.length <= rest) rope = Rope{};
        }
    }

    // The links along the chain, one every pitch (stretched when the chain is longer than the pieces), every
    // other one turned a quarter round; the blade at the end.
    void PlaceChain(void* weapon, Rig& r, float s, V3 dir, V3 up) {
        const int n = r.chain.Segments();
        float total = 0.f;
        for (int i = 0; i < n; i++) total += shell::Length(r.chain.Point(i + 1) - r.chain.Point(i));
        float pitch = pitch_ * s;
        int count = std::clamp(int(std::lround(total / pitch)), 1, kMaxLinks);
        while (int(r.links.size()) < count) {
            void* piece = AddPiece(weapon, linkMesh_);
            if (!piece) {
                CRAB_LOG("shellbreaker: couldn't add the chain to a weapon");
                r.broken = true;
                return;
            }
            r.links.push_back(piece);
            r.linkHidden.push_back(0);
        }
        if (!r.blade) {
            r.blade = AddPiece(weapon, bladeMesh_);
            if (!r.blade) {
                r.broken = true;
                return;
            }
        }
        float step = total / count;
        int seg = 0;
        float segStart = 0.f;  // distance along the chain where segment seg starts
        auto at = [&](float d) {
            while (seg < n - 1) {
                float l = shell::Length(r.chain.Point(seg + 1) - r.chain.Point(seg));
                if (segStart + l >= d) break;
                segStart += l;
                seg++;
            }
            V3 a = r.chain.Point(seg), b = r.chain.Point(seg + 1);
            float l = shell::Length(b - a);
            float t = l > 1e-4f ? std::clamp((d - segStart) / l, 0.f, 1.f) : 0.f;
            return a + (b - a) * t;
        };
        V3 a = r.chain.Point(0);
        for (int i = 0; i < count; i++) {
            V3 b = at((i + 1) * step);
            PlaceLink(r.links[i], a, b, dir, up, i % 2 != 0, s);
            SetHidden(r.links[i], r.linkHidden[i], false);
            a = b;
        }
        for (size_t i = count; i < r.links.size(); i++) SetHidden(r.links[i], r.linkHidden[i], true);
        SetHidden(r.blade, r.bladeHidden, false);
        PlaceBlade(r.blade, r.chain.Point(n), r.chain.BladeDir(), up, s);
    }

    // A link from a to b, rolled as little as possible from the last (up), every other one a quarter turn more.
    void PlaceLink(void* piece, V3 a, V3 b, V3 dir, V3& up, bool turned, float s) const {
        V3 x = shell::Normal(b - a, dir);
        V3 z = up - x * shell::Dot(up, x);
        if (shell::Length(z) < 1e-3f) z = shell::Cross(x, {0, 1, 0});
        z = shell::Normal(z);
        up = z;
        V3 y = shell::Cross(z, x);
        if (turned) std::swap(y, z), z = z * -1.f;
        SetWorld(piece, a, necro::FromAxes(x, y, z), {shell::Length(b - a) / pitch_, s, s});
    }

    static void PlaceBlade(void* blade, V3 at, V3 x, V3 up, float s) {
        V3 z = up - x * shell::Dot(up, x);
        if (shell::Length(z) < 1e-3f) z = shell::Cross(x, {0, 1, 0});
        z = shell::Normal(z);
        SetWorld(blade, at, necro::FromAxes(x, shell::Cross(z, x), z), {s, s, s});
    }

    static void SetHidden(void* piece, char& current, bool hidden) {
        if (bool(current) == hidden) return;
        current = hidden;
        static necro::Method setHidden(L"SetHiddenInGame");
        struct {
            bool hidden, children;
        } h{hidden, false};
        setHidden(piece, &h);
    }

    static void* AddPiece(void* actor, void* mesh) {
        static void* componentClass = ui::FindObject(L"/Script/Engine.StaticMeshComponent");
        struct alignas(16) {
            void* cls;
            bool manual;
            uint8_t pad[7];
            necro::Transform t;
            bool deferred;
            uint8_t pad2[7];
            void* ret;
        } p{componentClass, true, {}, {}, false, {}, nullptr};
        static_assert(offsetof(decltype(p), t) == 0x10 && offsetof(decltype(p), deferred) == 0x40 &&
                      offsetof(decltype(p), ret) == 0x48);
        if (!ui::Call(actor, L"AddComponentByClass", &p) || !p.ret) return nullptr;
        void* piece = p.ret;
        uint8_t movable = 2, noCollision = 0;
        bool no = false;
        ui::Call(piece, L"SetMobility", &movable);
        ui::Call(piece, L"SetCollisionEnabled", &noCollision);
        ui::Call(piece, L"SetCastShadow", &no);
        struct {
            void* mesh;
            bool ret;
        } m{mesh, false};
        ui::Call(piece, L"SetStaticMesh", &m);
        return piece;
    }

    static void SetWorld(void* component, V3 loc, necro::Q4 rot, V3 scale) {
        static necro::Method setTransform(L"K2_SetWorldTransform");
        struct alignas(16) {
            necro::Transform t;
            bool sweep;
            uint8_t hit[off::Hit_Size + 3];
            bool teleport;
            uint8_t pad[3];
        } p{};
        static_assert(offsetof(decltype(p), hit) == 0x31 && offsetof(decltype(p), teleport) == 0xBC);
        std::memcpy(p.t.rot, &rot, 16);
        p.t.loc[0] = loc.x, p.t.loc[1] = loc.y, p.t.loc[2] = loc.z;
        p.t.scale[0] = scale.x, p.t.scale[1] = scale.y, p.t.scale[2] = scale.z;
        p.teleport = true;
        setTransform(component, &p);
    }

    // --- On its pickups (every machine): the weapon itself, spinning, the chain coiled under the handle ----------

    struct Show {
        std::vector<void*> parts;  // the handle, the blade, then the links
        bool hidden = false, seen = false;
    };

    void UpdatePickups(void* world) {
        if (!linkMesh_ || !handle_) return;
        static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
        for (auto& [a, sh] : shows_) sh.seen = false;
        for (void* a : AllActorsOf(world, pickupClass)) {
            if (At<void*>(a, off::InteractPickup_PickupInfo + off::PickupInfo_DA) != weapon_) continue;
            auto it = shows_.find(a);
            if (it == shows_.end()) {
                it = shows_.emplace(a, Show{}).first;
                for (int i = 0; i < chainLinks_ + 2; i++) {
                    void* part = AddPiece(a, i == 0 ? handle_ : i == 1 ? bladeMesh_ : linkMesh_);
                    if (!part) {
                        CRAB_LOG("shellbreaker: couldn't show the weapon on its pickup");
                        it->second.parts.clear();
                        break;
                    }
                    it->second.parts.push_back(part);
                }
            }
            it->second.seen = true;
            StepShow(a, it->second);
        }
        for (auto it = shows_.begin(); it != shows_.end();) it = it->second.seen ? std::next(it) : shows_.erase(it);
    }

    void StepShow(void* pickup, Show& sh) {
        if (sh.parts.size() != size_t(chainLinks_ + 2)) return;
        bool hidden = At<bool>(pickup, off::InteractPickup_bDeactivated);
        if (hidden != sh.hidden) {
            sh.hidden = hidden;
            for (void* part : sh.parts) {
                char cur = !hidden;
                SetHidden(part, cur, hidden);
            }
        }
        if (hidden) return;
        Vec3 at;
        if (!LocationOf(pickup, at)) return;
        const float k = 0.8f;  // a little smaller than in the hand
        double t = Now();
        float yaw = float(std::fmod(t * 1.2, 6.2831853));
        necro::Q4 spin{0.f, 0.f, std::sin(yaw / 2), std::cos(yaw / 2)};
        V3 center{at.x, at.y, at.z + 40.f + 8.f * float(std::sin(t * 2.0))};
        V3 origin = center - necro::Rotate(spin, Of(anchor_) * k);
        SetWorld(sh.parts[0], origin, spin, {k, k, k});
        // The chain: a loose coil turning down from the handle's eye.
        V3 a = center;
        const float radius = 16.f * k, dropPerLink = 2.2f * k, link = pitch_ * k;
        V3 up = necro::Rotate(spin, {0, 0, 1});
        V3 last = necro::Rotate(spin, {1, 0, 0});
        for (int i = 0; i < chainLinks_; i++) {
            float th = (i + 1) * link / radius;
            V3 p = center + necro::Rotate(spin, {radius * std::sin(th), radius * (1.f - std::cos(th)),
                                                 -dropPerLink * (i + 1)});
            last = shell::Normal(p - a, last);
            PlaceLink(sh.parts[i + 2], a, p, last, up, i % 2 != 0, k);
            a = p;
        }
        PlaceBlade(sh.parts[1], a, shell::Normal(last + V3{0, 0, -1.2f}), up, k);
    }

    // --- The lobby's pedestal (as the Soul Whip's, one place further left) ------------------------------------

    static Vec3 Rotation(void* actor) {
        Vec3 r{};
        ui::Call(actor, L"K2_GetActorRotation", &r);
        return r;
    }
    static void Quat(const Vec3& r, float q[4]) {
        const float d = 3.14159265f / 360.f;
        float sp = std::sin(r.x * d), cp = std::cos(r.x * d), sy = std::sin(r.y * d), cy = std::cos(r.y * d),
              sr = std::sin(r.z * d), cr = std::cos(r.z * d);
        q[0] = cr * sp * sy - sr * cp * cy;
        q[1] = -cr * sp * cy - sr * cp * sy;
        q[2] = cr * cp * sy - sr * sp * cy;
        q[3] = cr * cp * cy + sr * sp * sy;
    }
    static void* Spawn(void* context, void* cls, const necro::Transform& t, void (*setup)(void*, void*), void* arg) {
        static void* statics = ui::FindObject(va::GameplayStaticsPath);
        struct {
            void* context;
            void* cls;
            necro::Transform t;
            uint8_t collision;  // AlwaysSpawn
            void* owner;
            void* ret;
        } p{context, cls, t, 1, nullptr, nullptr};
        if (!statics || !cls || !ui::Call(statics, L"BeginDeferredActorSpawnFromClass", &p) || !p.ret) return nullptr;
        if (setup) setup(p.ret, arg);
        struct {
            void* actor;
            necro::Transform t;
            void* ret;
        } f{p.ret, t, nullptr};
        ui::Call(statics, L"FinishSpawningActor", &f);
        return p.ret;
    }
    // A copy of a lobby weapon's pickup (the host's, which the game sends to everyone) with Shellbreaker on it.
    static void* SpawnLobbyPickup(void* lm, void* from, const necro::Transform& t) {
        return Spawn(lm, At<void*>(from, off::Object_Class), t, [](void* pickup, void* from) {
                uint8_t* to = &At<uint8_t>(pickup, off::InteractPickup_PickupInfo);
                const uint8_t* src = &At<uint8_t>(from, off::InteractPickup_PickupInfo);
                Fn<void* (*)(void*, const void*)>(va::FCrabInventoryInfo_CopyConstruct)(to + off::PickupInfo_Level,
                                                                                        src + off::PickupInfo_Level);
                std::memcpy(to + off::PickupInfo_MaxPickups, src + off::PickupInfo_MaxPickups,
                            off::PickupInfo_Size - off::PickupInfo_MaxPickups);
                At<void*>(to, off::PickupInfo_DA) = self_->weapon_;
            }, from);
    }

    void AddLobbyPedestal(void* lm) {
        static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
        static void* meshActorClass = ui::FindObject(L"/Script/Engine.StaticMeshActor");
        void *crossbow = nullptr, *mine = nullptr;
        std::vector<Vec3> weapons;  // the game's other lobby weapons
        for (void* a : AllActorsOf(lm, pickupClass)) {
            void* da = At<void*>(a, off::InteractPickup_PickupInfo + off::PickupInfo_DA);
            if (da && da == weapon_) mine = a;
            if (!da || da == weapon_ || ObjectName(At<void*>(a, off::Object_Class)) != "BP_Pickup_Lobby_C") continue;
            std::string name = ObjectName(da);
            if (name == "DA_Weapon_Crossbow") crossbow = a;
            else if (name != "DA_Weapon_SoulWhip" &&
                     ObjectName(At<void*>(da, off::Object_Class)) == ObjectName(At<void*>(weapon_, off::Object_Class)))
                if (Vec3 p; LocationOf(a, p)) weapons.push_back(p);
        }
        Vec3 at;
        if (!crossbow || !LocationOf(crossbow, at)) return;
        Vec3 next{};
        float best = 1e30f;
        for (const Vec3& p : weapons)
            if (DistanceSquared(p, at) < best) best = DistanceSquared(p, at), next = p;
        if (best == 1e30f) return;
        // Two gaps left of the Crossbow: the Soul Whip has the first.
        Vec3 step{2.f * (at.x - next.x), 2.f * (at.y - next.y), 0.f};
        Vec3 spot{at.x + step.x, at.y + step.y, at.z};

        // The stone under the Crossbow, copied (every machine its own).
        bool haveStone = false;
        void* stone = nullptr;
        best = 1e30f;
        for (void* a : AllActorsOf(lm, meshActorClass)) {
            haveStone |= a == stone_;
            Vec3 p;
            if (ObjectName(a).rfind("S_Pedestal_Generic", 0) != 0 || !LocationOf(a, p)) continue;
            float d = (p.x - at.x) * (p.x - at.x) + (p.y - at.y) * (p.y - at.y);
            if (d < best) best = d, stone = a;
        }
        if (!haveStone && stone && best < 400.f * 400.f) {
            necro::Transform t;
            Vec3 p;
            LocationOf(stone, p);
            t.loc[0] = p.x + step.x, t.loc[1] = p.y + step.y, t.loc[2] = p.z;
            Quat(Rotation(stone), t.rot);
            ui::Call(stone, L"GetActorScale3D", t.scale);
            void* copy = Spawn(lm, meshActorClass, t, [](void* copy, void* from) {
                void* to = At<void*>(copy, off::StaticMeshActor_Component);
                void* src = At<void*>(from, off::StaticMeshActor_Component);
                if (!to || !src) return;
                uint8_t movable = 2;
                ui::Call(to, L"SetMobility", &movable);
                struct { void* mesh; bool ret; } m{At<void*>(src, off::StaticMeshComponent_StaticMesh), false};
                ui::Call(to, L"SetStaticMesh", &m);
                int32_t n = 0;
                ui::Call(src, L"GetNumMaterials", &n);
                for (int32_t i = 0; i < n; i++) {
                    struct { int32_t i; void* ret; } g{i, nullptr};
                    ui::Call(src, L"GetMaterial", &g);
                    struct { int32_t i; void* mat; } sm{i, g.ret};
                    ui::Call(to, L"SetMaterial", &sm);
                }
            }, stone);
            if (copy) pedestal_.push_back(copy);
            stone_ = copy;
        }

        // The pickup: the host's, sent to everyone.
        if (At<uint8_t>(lm, off::Actor_Role) != ROLE_Authority || mine) return;
        necro::Transform t;
        t.loc[0] = spot.x, t.loc[1] = spot.y, t.loc[2] = spot.z;
        Quat(Rotation(crossbow), t.rot);
        if (void* pickup = SpawnLobbyPickup(lm, crossbow, t)) {
            pedestal_.push_back(pickup);
            CRAB_LOG("shellbreaker: pedestal at %.0f %.0f %.0f", spot.x, spot.y, spot.z);
        }
    }

    // Host, the test key: a Shellbreaker pickup in front of the player (in the lobby).
    void Give(void* pc, void* c) {
        static void* pickupClass = ui::FindObject(L"/Script/CrabChampions.CrabInteractPickup");
        void* lm = LevelManagerOf(pc);
        if (!weapon_ || !c || !lm) return;
        void* from = nullptr;
        for (void* a : AllActorsOf(lm, pickupClass))
            if (ObjectName(At<void*>(a, off::Object_Class)) == "BP_Pickup_Lobby_C" &&
                At<void*>(a, off::InteractPickup_PickupInfo + off::PickupInfo_DA)) {
                from = a;
                break;
            }
        if (!from) {
            if (void* ps = At<void*>(pc, off::Controller_PlayerState))
                ShowMessage(ps, L"Shellbreaker: the spawn key works in the lobby");
            return;
        }
        Vec3 me;
        LocationOf(c, me);
        V3 f = Dir(AimYaw(c), 0.f);
        necro::Transform t;
        t.loc[0] = me.x + f.x * 250.f, t.loc[1] = me.y + f.y * 250.f, t.loc[2] = me.z;
        SpawnLobbyPickup(lm, from, t);
    }

    // --- The hook (the player's machine) --------------------------------------------------------------------

    enum class Catch { Miss, Terrain, Yank, Pull };
    struct Hook_ {
        Phase phase = Phase::None;
        Catch what = Catch::Miss;
        V3 pin{}, target{};
        void* enemy = nullptr;
        float length = 0.f;  // the chain's, while it swings the crab or comes back
        shell::Swing swing;  // while swinging (shell_swing.hpp)
        double since = 0.0;
    };

    int MaxCharges(void* ps) const { return 1 + int(std::lround(loot::Total(ps, loot::Grappler))); }
    // Quick Draw: hyperbolic, never below a sixth.
    double Cooldown(void* ps) const {
        return std::max(hookCooldown_ / 6.0, hookCooldown_ / (1.0 + loot::Total(ps, loot::QuickDraw) / 100.0));
    }
    void Recharge(void* ps) {
        int max = MaxCharges(ps);
        if (charges_ < 0 || charges_ > max) charges_ = max;
        if (charges_ >= max) {
            rechargeAt_ = 0.0;
            return;
        }
        if (rechargeAt_ == 0.0) rechargeAt_ = Now() + Cooldown(ps);
        else if (Now() >= rechargeAt_) {
            charges_++;
            rechargeAt_ = charges_ < max ? Now() + Cooldown(ps) : 0.0;
            if (charges_ == 1) Play("A_Shell_Equip_", 0.35f);  // ready again
        }
    }

    void UpdateHook(void* world, void* c, void* ps, void* pc, bool ready, float dt) {
        bool rmb = ready && KeyDown(hookKey_), lmb = ready && KeyDown(VK_LBUTTON);
        bool rmbPress = rmb && !rmbDown_;
        rmbDown_ = rmb;
        Recharge(ps);
        Hook_& h = hook_;
        Vec3 me{};
        LocationOf(c, me);
        if (ready && h.phase == Phase::None && autoAnchor_) SeekAnchor(c, ps, Of(me));
        else point_.found = false;
        if (!ready && h.phase == Phase::None) return;
        V3 hand = Of(me);
        for (auto& [w, r] : rigs_)
            if (At<void*>(w, off::Weapon_OwningC) == c && r.chain.Live()) hand = r.chain.Point(0);

        switch (h.phase) {
        case Phase::None:
            if (rmbPress) Throw(world, c, ps, pc, hand);
            break;
        case Phase::Flying: {
            // Let go before it catches: no swing.
            if (h.what == Catch::Terrain && !rmb) {
                Return(pc, h);
                break;
            }
            V3 to = h.what == Catch::Yank || h.what == Catch::Pull ? EnemyAt(h) : h.target;
            V3 d = to - h.pin;
            float step = hookSpeed_ * dt, l = shell::Length(d);
            if (l > step) {
                h.pin = h.pin + d * (step / l);
                break;
            }
            h.pin = to;
            Arrive(c, ps, pc, Of(me));
            break;
        }
        case Phase::Swinging: {
            if (!rmb || !ready) {
                // Letting go flings you on: faster, and a little up.
                V3 v = Velocity(c);
                CRAB_LOG("shellbreaker: swing ended after %.1f s (%s), speed %.0f", Now() - h.since,
                         !ready ? "not ready" : "key up", shell::Length(v));
                if (ready) LaunchSelf(c, pc, v * releaseBoost_ + V3{0, 0, releaseLift_});
                Return(pc, h);
                break;
            }
            // The chain holds the crab at its length, the movement keys pump the swing (shell_swing.hpp).
            if (lmb) h.swing.Reel(reelSpeed_, dt);
            shell::SwingStepResult step = h.swing.Step(Of(me), Velocity(c), MoveInput(c), dt, SwingTune());
            h.length = h.swing.length;
            if (step.taut) Launch(c, step.velocity);
            break;
        }
        case Phase::Returning:
            h.length -= 4000.f * dt;
            if (h.length <= chainLinks_ * ChainFactor(ps) * pitch_ * Scale(c)) h = Hook_{};
            break;
        }
    }

    // The way the player is steering (the movement keys or stick), flat, length 0..1.
    static V3 MoveInput(void* c) {
        static necro::Method input(L"GetLastMovementInputVector");
        Vec3 v{};
        input(c, &v);
        V3 f{v.x, v.y, 0.f};
        float l = shell::Length(f);
        return l > 1.f ? f * (1.f / l) : f;
    }

    // --- The hook point (the player's machine) ---------------------------------------------------------------
    //
    // As in Insomniac's Spider-Man, the swing needs no exact aiming: a fan of traces around the crosshair (and
    // above it) finds the best thing to catch, scored mostly by how close it is to the crosshair, then by how near
    // it is to anchorIdeal and how high. Every 0.1 s; a diamond on screen shows it.

    struct Anchor {
        bool found = false, sky = false;
        void* enemy = nullptr;  // an enemy near the crosshair: hooking it uses a charge
        int hits = 0;  // traces that hit anything, the last time it looked
        V3 at{};
        double next = 0.0;
    };

    // A line trace on the Camera channel (terrain and objects, not pawns), ignoring the crab.
    static bool Trace(void* c, V3 from, V3 to, V3& hit) {
        static void* lib = ui::FindObject(L"/Script/Engine.Default__KismetSystemLibrary");
        static necro::Method trace(L"LineTraceSingle");
        struct TraceParams {
            void* context;
            Vec3 start, end;
            uint8_t channel, complex, pad0[6];
            TArray<void*> ignore;
            uint8_t debug, pad1[3];
            uint8_t hit[off::Hit_Size];
            bool ignoreSelf;
            uint8_t pad2[3];
            float color[4], hitColor[4], drawTime;
            bool ret;
            uint8_t pad3[3];
        };
        static_assert(offsetof(TraceParams, hit) == 0x3C && offsetof(TraceParams, ret) == 0xEC);
        TraceParams p{};
        p.context = c;
        p.start = ToVec(from), p.end = ToVec(to);
        p.channel = 1;  // TraceTypeQuery2: Camera
        p.ignoreSelf = true;
        if (!lib || !trace(lib, &p) || !p.ret || !(p.hit[off::Hit_bBlockingHit] & 1)) return false;
        Vec3 at;
        std::memcpy(&at, p.hit + off::Hit_ImpactPoint, sizeof at);
        hit = Of(at);
        return true;
    }

    void SeekAnchor(void* c, void* ps, V3 me) {
        if (Now() < point_.next) return;
        point_.next = Now() + 0.1;
        float pitch = 0.f, yaw = AimYaw(c, &pitch);
        V3 aim = Dir(yaw, pitch), eye = me + V3{0, 0, 60.f};
        float range = anchorRange_ * ChainFactor(ps), ideal = std::min(anchorIdeal_ * ChainFactor(ps), range);
        float best = -1e30f;
        int hits = 0;
        point_.found = false;
        point_.enemy = nullptr;
        // An enemy near the crosshair, in sight, while there's a charge: it comes first.
        if (enemyClass_ && charges_ > 0) {
            float hookRange = hookRange_ * ChainFactor(ps), closest = enemyAssist_;
            for (void* e : AllActorsOf(c, enemyClass_)) {
                Vec3 ep;
                if (!Alive(e) || !LocationOf(e, ep)) continue;
                V3 d = Of(ep) - eye;
                float dist = shell::Length(d);
                if (dist < 1.f || dist > hookRange) continue;
                float off = std::acos(std::clamp(shell::Dot(d * (1.f / dist), aim), -1.f, 1.f)) / kDeg;
                if (off >= closest) continue;
                V3 wall;
                if (Trace(c, eye, Of(ep), wall) && shell::Length(wall - eye) < dist - 250.f) continue;  // behind a wall
                closest = off;
                point_.enemy = e;
                point_.at = Of(ep);
            }
            if (point_.enemy) {
                point_.found = true;
                point_.sky = false;
                return;
            }
        }
        // Right where the crosshair points, when that is something to catch.
        {
            V3 hit;
            if (Trace(c, eye, eye + aim * range, hit) && hit.z - me.z >= 200.f && shell::Length(hit - me) >= 300.f) {
                hits++;
                best = -0.5f * std::fabs(shell::Length(hit - me) - ideal) / ideal + 0.2f * (hit.z - me.z) / range + 0.5f;
                point_.at = hit, point_.found = true, point_.sky = false;
            }
        }
        // Around the crosshair, and up from it: never below 10 degrees up, since the hook needs something above.
        for (float up : {0.f, 10.f, 20.f, 32.f, 45.f})
            for (float side : {0.f, -10.f, 10.f, -22.f, 22.f}) {
                float elev = std::clamp(pitch + up, 10.f, 80.f);
                V3 ray = Dir(yaw + side, elev), hit;
                if (!Trace(c, eye, eye + ray * range, hit)) continue;
                hits++;
                V3 d = hit - me;
                float dist = shell::Length(d);
                if (d.z < 200.f || dist < 300.f) continue;  // the ground, or right on top of you
                float off = std::acos(std::clamp(shell::Dot(ray, aim), -1.f, 1.f)) / kDeg;  // degrees off the crosshair
                float score = -off / 20.f - 0.5f * std::fabs(dist - ideal) / ideal + 0.2f * d.z / range;
                if (score > best) best = score, point_.at = hit, point_.found = true, point_.sky = false;
            }
        point_.hits = hits;  // (out of 26)
        if (!point_.found && skyAnchor_) {
            // Nothing to catch: a point in the air ahead, the way the crosshair looks, high enough to swing forward.
            point_.at = me + Dir(yaw, std::clamp(pitch + 35.f, 40.f, 60.f)) * ideal;
            point_.found = point_.sky = true;
        }
    }

    shell::SwingTuning SwingTune() const {
        shell::SwingTuning t;
        t.pump = swingPump_;
        t.maxSpeed = swingMaxSpeed_;
        t.catchLift = catchLift_;
        t.keepHeight = swingKeepHeight_;
        return t;
    }

    V3 EnemyAt(Hook_& h) {
        Vec3 p;
        if (h.enemy && Alive(h.enemy) && LocationOf(h.enemy, p)) h.target = Of(p);
        return h.target;
    }

    int YankMax(void* ps) const { return loot::Level(ps, loot::KrakensGrasp) > 0 ? std::max(yankMax_, 4) : yankMax_; }

    void Throw(void* world, void* c, void* ps, void* pc, V3 hand) {
        float range = hookRange_ * ChainFactor(ps);
        alignas(16) uint8_t hit[off::Hit_Size] = {};
        At<float>(hit, off::Hit_Time) = 1.f;
        Fn<void (*)(void*, void*, float)>(va::ACrabPlayerC_CameraTrace)(c, hit, range);
        V3 from = Of(At<Vec3>(hit, off::Hit_TraceStart));
        bool blocked = hit[off::Hit_bBlockingHit] & 1;
        V3 to = Of(At<Vec3>(hit, blocked ? off::Hit_ImpactPoint : off::Hit_TraceEnd));
        Hook_ h{};
        h.phase = Phase::Flying;
        h.pin = hand;
        h.target = to;
        h.what = blocked ? Catch::Terrain : Catch::Miss;
        h.since = Now();
        // An enemy near the crosshair's line, the nearest along it: only with a charge.
        V3 line = to - from;
        float len = shell::Length(line), best = 1e30f;
        if (point_.enemy && charges_ > 0 && Alive(point_.enemy)) {
            Vec3 p;
            if (LocationOf(point_.enemy, p)) {
                h.enemy = point_.enemy;
                h.target = Of(p);
                h.what = At<uint8_t>(h.enemy, off::Enemy_Category) <= YankMax(ps) ? Catch::Yank : Catch::Pull;
            }
        }
        if (!h.enemy && enemyClass_ && len > 1.f && charges_ > 0)
            for (void* e : AllActorsOf(world, enemyClass_)) {
                Vec3 p;
                if (!Alive(e) || !LocationOf(e, p)) continue;
                V3 rel = Of(p) - from;
                float t = std::clamp(shell::Dot(rel, line) / (len * len), 0.f, 1.f);
                float miss = shell::Length(rel - line * t);
                if (miss > 250.f || t * len >= best) continue;
                best = t * len;
                h.enemy = e;
                h.target = Of(p);
                h.what = At<uint8_t>(e, off::Enemy_Category) <= YankMax(ps) ? Catch::Yank : Catch::Pull;
            }
        if (h.enemy) {
            charges_--;
            Recharge(ps);
        } else if (autoAnchor_) {
            if (!point_.found || point_.enemy) return;  // nothing to catch
            h.target = point_.at;
            h.what = Catch::Terrain;
        }
        hook_ = h;
        Play("A_Shell_Throw_");
        PlayMontage(c, ps, Toss, 0.35);
        wchar_t buf[96];
        swprintf(buf, std::size(buf), L"%lsthrow %.0f %.0f %.0f", kMsg, h.target.x, h.target.y, h.target.z);
        SendToHost(pc, buf);
    }

    void Arrive(void* c, void* ps, void* pc, V3 me) {
        Hook_& h = hook_;
        if (h.what != Catch::Miss) PlayAt("A_Shell_Bite_", ToVec(h.pin));
        if (h.what != Catch::Miss) {
            wchar_t buf[96];
            int k = h.what == Catch::Terrain ? 0 : h.what == Catch::Yank ? 1 : 2;
            swprintf(buf, std::size(buf), L"%lsland %d %.0f %.0f %.0f", kMsg, k, h.pin.x, h.pin.y, h.pin.z);
            SendToHost(pc, buf);
        }
        switch (h.what) {
        case Catch::Yank:
            Return(pc, h);
            break;
        case Catch::Pull: {
            V3 d = shell::Normal(h.pin - me, {1, 0, 0});
            LaunchSelf(c, pc, d * pullSpeed_ + V3{0, 0, 350.f});
            Return(pc, h);
            break;
        }
        case Catch::Terrain: {
            // The chain takes up slack as it bites, until the arc clears the floor, and the swing starts forward.
            V3 ground;
            float floorZ = Trace(c, me, me - V3{0, 0, 5000.f}, ground) ? ground.z + HalfHeight(c) : me.z - 5000.f;
            V3 v = h.swing.Begin(h.pin, me, Velocity(c), floorZ, SwingTune());
            Launch(c, v);
            h.phase = Phase::Swinging;
            h.length = h.swing.length;
            h.since = Now();
            CRAB_LOG("shellbreaker: swing caught %.0f cm away (%s; %d of 26 traces hit), %.0f above the floor, chain to %.0f",
                     h.length, point_.sky ? "sky" : "terrain", point_.hits, me.z - floorZ, h.swing.goal);
            break;
        }
        case Catch::Miss:
            Return(pc, h);
            break;
        }
    }

    void Return(void* pc, Hook_& h) {
        // The chain comes back from as far out as it is.
        for (auto& [w, r] : rigs_)
            if (r.chain.Live()) h.length = std::max(h.length, shell::Length(h.pin - r.chain.Point(0)));
        h.phase = Phase::Returning;
        SendToHost(pc, std::wstring(kMsg) + L"back");
    }

    // A launch of this machine's crab, and the same from the host so it agrees.
    void LaunchSelf(void* c, void* pc, V3 v) {
        Launch(c, v);
        if (At<uint8_t>(c, off::Actor_Role) == ROLE_Authority) return;
        wchar_t buf[96];
        swprintf(buf, std::size(buf), L"%lslaunch %.0f %.0f %.0f", kMsg, v.x, v.y, v.z);
        SendToHost(pc, buf);
    }

    // --- The hook (host) ------------------------------------------------------------------------------------

    // Every other machine draws the hook: "rope ..." to each other player's machine; the host's own draws it here.
    void Relay(void* from, void* ps, bool on, V3 at) {
        lastRopeSeen_ = Now();
        wchar_t buf[160];
        std::wstring name = NameOf(ps);
        swprintf(buf, std::size(buf), L"%lsrope %d %.0f %.0f %.0f %ls", kMsg, on ? 1 : 0, at.x, at.y, at.z,
                 name.c_str());
        void* lm = world_ ? LevelManagerOf(world_) : nullptr;
        void* localC = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        void* localPc = localC ? At<void*>(localC, off::Pawn_Controller) : nullptr;
        if (from != localPc) SeeRope(name.c_str(), on, at);
        ForEachPlayer(world_, [&](void* other) {
            void* c = PawnOf(other);
            void* pc = c ? At<void*>(c, off::Pawn_Controller) : nullptr;
            if (pc && pc != from && pc != localPc) SendToPlayer(pc, buf);
        });
    }

    void SeeRope(const wchar_t* name, bool on, V3 at) {
        lastRopeSeen_ = Now();
        if (!world_) return;
        ForEachPlayer(world_, [&](void* ps) {
            if (NameOf(ps) != name) return;
            Rope& rope = ropes_[ps];
            if (on) {
                rope = Rope{};
                rope.phase = Phase::Flying;
                rope.target = at;
                rope.fresh = true;
            } else if (rope.phase != Phase::None) {
                rope.length = 0.f;
                for (auto& [w, r] : rigs_)
                    if (At<void*>(w, off::Weapon_OwningPS) == ps && r.chain.Live())
                        rope.length = shell::Length(rope.pin - r.chain.Point(0));
                rope.phase = Phase::Returning;
            }
        });
    }

    // The hook landed: on an enemy, Riptide starts, and its hit (and a yank, for light ones).
    void Land(void* c, void* ps, int kind, V3 at) {
        if (kind == 0 || !enemyClass_) return;
        riptide_[ps] = Now();
        void* e = NearestEnemy(c, at, 400.f, 5);
        if (!e) return;
        Hurt(e, c, ps, hookDamage_ * (1.f + loot::Total(ps, loot::Undertow) / 100.f),
             int(std::lround(loot::Total(ps, loot::FrostChain))));
        if (kind != 1 || At<uint8_t>(e, off::Enemy_Category) > YankMax(ps)) return;
        Vec3 me;
        if (!LocationOf(c, me)) return;
        std::vector<void*> yanked{e};
        // Kraken's Grasp: the nearest others come along.
        int extra = int(std::lround(loot::Total(ps, loot::KrakensGrasp)));
        Vec3 ep;
        if (extra > 0 && LocationOf(e, ep)) {
            std::vector<std::pair<float, void*>> close;
            for (void* o : AllActorsOf(c, enemyClass_)) {
                Vec3 p;
                if (o == e || !Alive(o) || !LocationOf(o, p) || At<uint8_t>(o, off::Enemy_Category) > 4) continue;
                float d = DistanceSquared(p, ep);
                if (d < 800.f * 800.f) close.push_back({d, o});
            }
            std::sort(close.begin(), close.end(), [](auto& a, auto& b) { return a.first < b.first; });
            for (int i = 0; i < extra && i < int(close.size()); i++) yanked.push_back(close[i].second);
        }
        for (void* y : yanked) {
            Vec3 p;
            if (!LocationOf(y, p)) continue;
            V3 d = Of(me) - Of(p);
            d.z = 0.f;
            Launch(y, shell::Normal(d, {1, 0, 0}) * yankSpeed_ + V3{0, 0, 450.f});
        }
    }

    // Whirlwind: the enemies around the circle are dragged toward the player (not bosses).
    void Gather(void* c, void* ps, V3 at) {
        float speed = loot::Total(ps, loot::Whirlwind);
        if (speed <= 0.f || !enemyClass_) return;
        float reach = circleRadius_ * ChainFactor(ps) * 1.5f;
        for (void* e : AllActorsOf(c, enemyClass_)) {
            Vec3 p;
            if (!Alive(e) || !LocationOf(e, p) || At<uint8_t>(e, off::Enemy_Category) > 4) continue;
            V3 d = at - Of(p);
            d.z = 0.f;
            float l = shell::Length(d);
            if (l > reach || l < 60.f) continue;
            Launch(e, d * (speed / l) + V3{0, 0, 120.f});
        }
    }

    void* NearestEnemy(void* context, V3 at, float within, int maxCategory) {
        void* best = nullptr;
        float bestD = within * within;
        for (void* e : AllActorsOf(context, enemyClass_)) {
            Vec3 p;
            if (!Alive(e) || !LocationOf(e, p) || At<uint8_t>(e, off::Enemy_Category) > maxCategory) continue;
            float d = DistanceSquared(p, ToVec(at));
            if (d < bestD) bestD = d, best = e;
        }
        return best;
    }

    // The hook's hit: a small burst on the enemy, as the player (the game's buffs, crits and kills apply).
    static void Hurt(void* enemy, void* c, void* ps, float damage, int ice) {
        Vec3 at{};
        if (!LocationOf(enemy, at)) return;
        alignas(8) uint8_t info[off::DamageInfo_Size];
        std::memset(info, 0, sizeof info);
        At<float>(info, off::DamageInfo_Damage) = damage;
        At<uint8_t>(info, off::DamageInfo_Type) = 4;  // Lesser: not throttled per enemy
        At<Vec3>(info, off::DamageInfo_Location) = at;
        At<void*>(info, off::DamageInfo_DamagingC) = c;
        AddDebuff(info, kIce, ice);
        TArray<void*> ignore{};  // passed by value: the game frees it, so it stays empty
        Fn<void (*)(void*, TArray<void*>*, const Vec3*, float, const void*, bool, void*)>(va::RadialDamage)(
            c, &ignore, &at, 90.f, info, false, nullptr);
        if (void* d = At<TArray<uint8_t>>(info, off::DamageInfo_Debuffs).Data) Fn<void (*)(void*)>(va::FMemory_Free)(d);
    }

    // --- The hook on the HUD (the player's machine): bottom left (where the Soul Whip's soul bar goes; the two
    // are never held together), a pip per charge and the next one's recharge under them. The melee slot, which
    // does nothing while Shellbreaker is held, is hidden.

    static constexpr float kHudX = 38.f, kHudY = -105.f, kHudW = 150.f, kHudH = 5.f, kPip = 14.f;
    static constexpr int kMaxPips = 8;

    void UpdateHud(void* pc, void* ps, bool holds) {
        void* hud = pc ? At<void*>(pc, off::PC_HUD) : nullptr;
        if (hud != hud_) {
            hud_ = hud;
            hudWidgets_.clear();
            pips_.clear();
            marker_ = markerBack_ = nullptr;
            hudFailed_ = meleeHidden_ = false;
        }
        if (!hud) return;
        if (holds != meleeHidden_) {
            const int widgets[] = {0x420 /* MeleeKeyText */, 0x428 /* MeleeControllerIcon */, 0x430 /* MeleeIcon */,
                                   0x438 /* MeleeCooldownText */};
            for (int i = 0; i < 4; i++) {
                void* w = At<void*>(hud, widgets[i]);
                if (!w) continue;
                if (holds) {
                    meleeVis_[i] = ui::SelfHitTestInvisible;
                    ui::Call(w, L"GetVisibility", &meleeVis_[i]);
                }
                ui::SetVisibility(w, holds ? ui::Hidden : ui::Visibility(meleeVis_[i]));
            }
            meleeHidden_ = holds;
        }
        if (hudWidgets_.empty() && holds && !hudFailed_ && !BuildHud(hud)) hudFailed_ = true;
        for (void* w : hudWidgets_) ui::SetVisibility(w, holds ? ui::HitTestInvisible : ui::Hidden);
        if (!holds || hudWidgets_.empty()) return;
        int max = MaxCharges(ps), have = std::clamp(charges_ < 0 ? max : charges_, 0, max);
        for (int i = 0; i < int(pips_.size()); i++) {
            ui::SetVisibility(pips_[i], i < max ? ui::HitTestInvisible : ui::Hidden);
            Tint(pips_[i], i < have ? ui::LinearColor{1.f, 0.65f, 0.25f, 1.f} : ui::LinearColor{0.45f, 0.4f, 0.38f, 0.8f});
        }
        double cd = Cooldown(ps);
        float frac = have >= max ? 1.f
                     : rechargeAt_ > 0.0 && cd > 0.0 ? float(std::clamp(1.0 - (rechargeAt_ - Now()) / cd, 0.0, 1.0))
                                                     : 0.f;
        SetSize(hudFill_, kHudW * frac, kHudH);
        Tint(hudFill_, have > 0 ? ui::LinearColor{1.f, 0.65f, 0.25f, 1.f} : ui::LinearColor{0.75f, 0.5f, 0.3f, 1.f});
        // What the hook key does right now (swinging needs no charge; the bar and pips are for hooking enemies).
        wchar_t buf[64];
        std::wstring key = Widen(hookKeyName_);
        if (hook_.phase == Phase::Swinging) swprintf(buf, std::size(buf), L"SWINGING  let go of %ls", key.c_str());
        else if (hook_.phase != Phase::None) swprintf(buf, std::size(buf), L"HOOK");
        else if (point_.enemy) swprintf(buf, std::size(buf), L"HOOK ENEMY  %ls", key.c_str());
        else if (point_.found && !point_.sky) swprintf(buf, std::size(buf), L"SWING  %ls", key.c_str());
        else if (point_.found) swprintf(buf, std::size(buf), L"SWING  %ls  (air)", key.c_str());
        else swprintf(buf, std::size(buf), L"NOTHING TO SWING ON");
        if (have < max) {
            size_t n = wcslen(buf);
            swprintf(buf + n, std::size(buf) - n, L"   enemy %.1f", std::max(0.0, rechargeAt_ - Now()));
        }
        ui::SetText(hudText_, buf);
        UpdateMarker(pc);
    }

    void UpdateMarker(void* pc) {
        if (!marker_) return;
        static void* layout = ui::FindObject(L"/Script/UMG.Default__WidgetLayoutLibrary");
        struct {
            void* pc;
            Vec3 at;
            float x, y;
            bool viewportRelative, ret;
            uint8_t pad[2];
        } p{pc, ToVec(point_.at), 0.f, 0.f, false, false, {}};
        Vec3 ep;
        if (point_.enemy && Alive(point_.enemy) && LocationOf(point_.enemy, ep)) p.at = ep;
        bool show = point_.found && hook_.phase == Phase::None && layout && ui::Call(layout, L"ProjectWorldLocationToWidgetPosition", &p) && p.ret;
        for (void* w : {markerBack_, marker_})
            if (w) ui::SetVisibility(w, show ? ui::HitTestInvisible : ui::Hidden);
        if (!show) return;
        float pos[2] = {p.x, p.y};
        for (void* w : {markerBack_, marker_})
            if (w) ui::Call(ui::SlotOf(w), L"SetPosition", pos);
        // An enemy: red; something to catch: bright orange; only the air: a paler, see-through one.
        Tint(marker_, point_.enemy ? ui::LinearColor{1.f, 0.15f, 0.1f, 1.f}
                      : point_.sky ? ui::LinearColor{0.85f, 0.95f, 1.f, 0.7f}
                                   : ui::LinearColor{1.f, 0.6f, 0.1f, 1.f});
        if (markerBack_) Tint(markerBack_, ui::LinearColor{0.05f, 0.03f, 0.02f, point_.sky ? 0.5f : 0.9f});
    }

    bool BuildHud(void* hud) {
        void* tree = At<void*>(hud, off::UserWidget_WidgetTree);
        void* root = tree ? At<void*>(tree, off::WidgetTree_RootWidget) : nullptr;
        if (!root) return false;
        void* trough = Place(hud, root, va::UImage_StaticClass, kHudX, kHudY, 0.f, kHudW, kHudH);
        if (!trough) {
            CRAB_LOG("shellbreaker: the HUD's root isn't a canvas (%s); no hook display", ObjectName(root).c_str());
            hudWidgets_.clear();
            return false;
        }
        Tint(trough, {0.42f, 0.37f, 0.36f, 0.85f});
        hudFill_ = Place(hud, root, va::UImage_StaticClass, kHudX, kHudY, 0.f, kHudW, kHudH);
        for (int i = 0; i < kMaxPips; i++)
            if (void* pip = Place(hud, root, va::UImage_StaticClass, kHudX + i * (kPip + 5.f), kHudY - 16.f, 0.f, kPip, kPip))
                pips_.push_back(pip);
        hudText_ = Place(hud, root, va::UTextBlock_StaticClass, kHudX, kHudY - 40.f, 0.f, 0.f, 0.f);
        ui::Font font{};
        if (hudText_ && ui::FontOf(At<void*>(hud, 0x340 /* CrystalsText */), 18, font)) ui::SetFont(hudText_, font);
        // The hook point: a diamond, placed over it every frame (anchored to the top left, centred on the spot).
        // A dark one behind it outlines it on bright skies and sand.
        markerBack_ = Place(hud, root, va::UImage_StaticClass, 0.f, 0.f, 0.5f, 34.f, 34.f);
        marker_ = Place(hud, root, va::UImage_StaticClass, 0.f, 0.f, 0.5f, 24.f, 24.f);
        for (void* w : {markerBack_, marker_}) {
            if (!w) continue;
            float anchors[4] = {0.f, 0.f, 0.f, 0.f}, alignment[2] = {0.5f, 0.5f}, angle = 45.f;
            ui::Call(ui::SlotOf(w), L"SetAnchors", anchors);
            ui::Call(ui::SlotOf(w), L"SetAlignment", alignment);
            ui::Call(w, L"SetRenderTransformAngle", &angle);
        }
        CRAB_LOG("shellbreaker: hook display added to the HUD");
        return true;
    }

    // A widget in the HUD's canvas, anchored to the bottom left corner; w = 0: sized to its content.
    void* Place(void* hud, void* root, uintptr_t cls, float x, float y, float alignX, float w, float h) {
        void* widget = ui::NewWidget(hud, cls);
        void* slot = widget ? ui::AddToCanvas(root, widget) : nullptr;
        if (!slot) {
            if (widget) ui::RemoveFromParent(widget);
            return nullptr;
        }
        float anchors[4] = {0.f, 1.f, 0.f, 1.f};
        ui::Call(slot, L"SetAnchors", anchors);
        float align[2] = {alignX, 0.5f};
        ui::Call(slot, L"SetAlignment", align);
        bool autoSize = w <= 0.f;
        ui::Call(slot, L"SetAutoSize", &autoSize);
        hudWidgets_.push_back(widget);
        float pos[2] = {x, y};
        ui::Call(slot, L"SetPosition", pos);
        if (!autoSize) SetSize(widget, w, h);
        return widget;
    }
    static void SetSize(void* widget, float w, float h) {
        float size[2] = {std::max(w, 0.f), h};
        ui::Call(ui::SlotOf(widget), L"SetSize", size);
    }
    static void Tint(void* image, ui::LinearColor c) {
        if (image) ui::Call(image, L"SetColorAndOpacity", &c);
    }
    static std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

    // --- Sounds (every machine) --------------------------------------------------------------------------------

    necro::Blob Pick(const char* prefix) {
        auto all = pack_.All(prefix);
        if (all.empty()) return {};
        return all[rng_() % all.size()];
    }
    void Play(const char* prefix, float gain = 1.f) {
        if (pack_.Load()) mixer_.Play(Pick(prefix), gain, 0.f);
    }
    // In the world: quieter with distance from this machine's camera, and panned.
    void PlayAt(const char* prefix, Vec3 at, float gain = 1.f) {
        if (!pack_.Load()) return;
        static necro::Method cameraLocation(L"GetCameraLocation"), cameraRotation(L"GetCameraRotation");
        void* lm = world_ ? LevelManagerOf(world_) : nullptr;
        void* c = lm ? At<void*>(lm, off::LM_LocalC) : nullptr;
        void* pc = c ? At<void*>(c, off::Pawn_Controller) : nullptr;
        void* cam = pc ? At<void*>(pc, off::PC_CameraManager) : nullptr;
        float pan = 0.f;
        if (cam) {
            Vec3 from{}, rot{};
            cameraLocation(cam, &from);
            cameraRotation(cam, &rot);
            V3 to = Of(at) - Of(from);
            float dist = shell::Length(to);
            float fade = std::clamp(1.f - (dist - 1000.f) / 5000.f, 0.f, 1.f);
            gain *= fade * fade;
            float yaw = rot.y * kDeg;
            if (dist > 1.f) pan = shell::Dot(to * (1.f / dist), {-std::sin(yaw), std::cos(yaw), 0.f}) * 0.8f;
        }
        mixer_.Play(Pick(prefix), gain, pan);
    }
    void FollowGameVolume(void* world) {
        void* gi = world ? At<void*>(world, off::World_GameInstance) : nullptr;
        void* sg = gi && IsA(gi, StaticClass(va::UCrabGI_StaticClass)) ? At<void*>(gi, off::GI_SettingsSG) : nullptr;
        if (!sg) return;
        auto unit = [](float v) { return std::clamp(v > 1.001f ? v / 100.f : v, 0.f, 1.f); };  // 0..1 or percent
        mixer_.SetGameVolume(unit(At<float>(sg, off::SettingsSG_MasterVolume)) *
                             unit(At<float>(sg, off::SettingsSG_GameplayVolume)));
    }

    // --- Helpers ----------------------------------------------------------------------------------------------

    bool Holds(void* ps) const {
        return weapon_ && IsA(ps, StaticClass(va::ACrabPS_StaticClass)) && At<void*>(ps, off::PS_WeaponDA) == weapon_;
    }

    static bool InGame(void* pc) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (!fg || (GetWindowThreadProcessId(fg, &pid), pid != GetCurrentProcessId())) return false;
        if (At<void*>(pc, off::PC_OpenMenu)) return false;
        void* gsui = At<void*>(pc, off::PC_GameStateUI);
        void* chat = gsui ? At<void*>(gsui, off::GameStateUI_ChatInput) : nullptr;
        bool typing = false;
        if (chat) ui::Call(chat, L"HasKeyboardFocus", &typing);
        return !typing;
    }

    // Shellbreaker replaces the melee: its key does nothing while it's held (stopped on the player's machine,
    // before the lunge and the RPC).
    static void HkMeleePressed(void* c) {
        if (c && self_->enabled_ && self_->Holds(At<void*>(c, off::Pawn_PlayerState))) return;
        oMeleePressed(c);
    }

    // For its holders: melee mods (which power the swings) drop meleeModWeight times as often, and of the game's
    // weapon mods only those that work for a chain blade drop (kBladeMods); the bullets' own (formations, ammo,
    // shots' paths, on-impact spawns and procs) don't.
    float LootWeight(void* ps, void* da) override {
        if (!da || !Holds(ps) || loot::IsShellItem(da)) return 1.f;
        if (IsA(da, StaticClass(va::UCrabMeleeModDA_StaticClass))) return meleeModWeight_;
        if (!IsA(da, StaticClass(va::UCrabWeaponModDA_StaticClass))) return 1.f;
        uint8_t type = At<uint8_t>(da, off::DA_ItemType);
        for (uint8_t t : kBladeMods)
            if (t == type) return 1.f;
        return 0.f;
    }
    // ECrabWeaponModType: plain damage (the weapon-mod damage the swings take), fire rate (the swings' speed).
    static constexpr uint8_t kBladeMods[] = {15 /*WindUp*/, 18 /*AerialShot*/, 29 /*Uppercut*/, 31 /*HeavyHitter*/,
                                             32 /*RapidFire*/, 33 /*EscalatingShot*/, 54 /*DamageShot*/,
                                             55 /*Supercharged*/, 56 /*Juiced*/};

    using MeleePressedFn = void (*)(void*);
    static inline MeleePressedFn oMeleePressed;
    static inline Shellbreaker* self_;
    static inline bool noClaw_ = false;
    static inline void* body_[4] = {};  // the borrowed montages (FindMontages); null: the claw's  // the melee hit couldn't be blocked: no claw montage

    // Settings
    bool enabled_ = true;
    static constexpr float kFxRadius = 150.f;  // the swing shot's burst: the impact's look only
    float meleeModShare_ = 0.25f, meleeModWeight_ = 3.f;
    float dps_ = 75.f, swingSeconds_ = 0.35f, reach_ = 2200.f, hitRadius_ = 450.f, circleRadius_ = 2400.f;
    int chainLinks_ = 40;
    float hookRange_ = 6000.f, hookCooldown_ = 6.f, hookSpeed_ = 10000.f, hookDamage_ = 60.f;
    int yankMax_ = 2;
    float yankSpeed_ = 1600.f, pullSpeed_ = 2200.f, reelSpeed_ = 600.f, procRoll_ = 1.f;
    bool autoAnchor_ = true, skyAnchor_ = false, swingKeepHeight_ = true;
    float swingMaxSpeed_ = 4000.f, enemyAssist_ = 7.f;
    float anchorRange_ = 5000.f, anchorIdeal_ = 2200.f, swingPump_ = 1200.f, releaseBoost_ = 1.2f, releaseLift_ = 450.f,
          catchLift_ = 0.85f;
    std::string hookKeyName_ = "Mouse2", spawnKeyName_ = "none";
    int hookKey_ = 0, spawnKey_ = 0;

    // State
    void* weapon_ = nullptr;  // DA_Weapon_Shellbreaker
    void* projDA_ = nullptr;
    void* enemyClass_ = nullptr;
    void* world_ = nullptr;
    void* lastHeld_ = nullptr;
    bool artTried_ = false;
    necro::Pack pack_{101};
    necro::Mixer mixer_;
    std::minstd_rand rng_;
    void *mid_ = nullptr, *handle_ = nullptr, *linkMesh_ = nullptr, *bladeMesh_ = nullptr;
    Vec3 anchor_{}, anchorDir_{1, 0, 0};  // the handle's eye in the hand socket's space, and the way it points
    float pitch_ = 7.5f, bladeReach_ = 0.f;
    std::unordered_map<void*, Rig> rigs_;    // weapon -> its chain
    std::unordered_map<void*, Show> shows_;  // pickup -> the weapon shown on it
    std::unordered_map<void*, Rope> ropes_;  // other players (ACrabPS*) -> their hook, as the host told
    double lastRopeSeen_ = 0.0;
    struct ShotState {
        int32_t fired = -1;
        int swing = -1;
        double last = 0.0;
    };
    std::unordered_map<void*, ShotState> shots_;  // this machine's weapons -> their place in the string
    Hook_ hook_;
    Anchor point_;  // the hook point
    int charges_ = -1;
    double rechargeAt_ = 0.0;
    std::unordered_map<void*, double> riptide_;  // host: player -> when their hook last landed
    // HUD
    void *hud_ = nullptr, *hudFill_ = nullptr, *hudText_ = nullptr, *marker_ = nullptr, *markerBack_ = nullptr;
    std::vector<void*> hudWidgets_, pips_;
    uint8_t meleeVis_[4] = {};
    bool hudFailed_ = false, meleeHidden_ = false;
    // Lobby
    double nextPedestalCheck_ = 0.0;
    void* stone_ = nullptr;
    std::vector<void*> pedestal_;
    bool spawnDown_ = false, rmbDown_ = false;
};

Shellbreaker g_item;

}  // namespace

bool HoldsShellbreaker(void* ps) { return Shellbreaker::HoldsIt(ps); }

}  // namespace crabmods
