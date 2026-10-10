// CrabMods: new items for Crab Champions.
//   Arsenal (weapon mod): more damage for each other weapon mod you have.
//   Hoarder (perk): more damage for each weapon mod and each perk level you have (Arsenal + Collector).
//   Heavy Shot: the game's own mod, unlocked so it drops for every weapon, not just Bounce weapons.
//
// The game has no Arsenal or Hoarder assets, so they are made at runtime as copies of Streak Shot and
// Collector with new text, rarity and an unused type (see MakeDA), and added to the level manager's loot
// list so chests roll them. Their effect is applied in the player's damage multiplier, added like the
// game's own damage perks.
#include <windows.h>

#include <unordered_map>

#include <crab/hook.hpp>
#include <crab/log.hpp>
#include <crab/ue.hpp>

#include "config.hpp"
#include "items.hpp"

using namespace crab;
using namespace crabmods;

namespace {

constexpr const char kVersion[] = "0.2.0";
constexpr uint8_t kArsenalType = 200;  // ECrabWeaponModType has nothing past 94
constexpr uint8_t kHoarderType = 200;  // ECrabPerkType has nothing past 121

Config g_cfg;
void* g_arsenal = nullptr;  // data assets, rooted for the life of the process
void* g_hoarder = nullptr;
bool g_heavyShotLogged = false;

using BeginPlayFn = void (*)(void*);
using DamageBuffFn = float (*)(void*);
BeginPlayFn oBeginPlay;
DamageBuffFn oDamageBuff;

// Last bonus logged per player, so the log shows changes rather than every shot.
std::unordered_map<void*, float> g_loggedBonus;

void* CreateArsenal(void* lm) {
    auto* mods = Spawnables(lm, off::Spawnables_WeaponMods);
    void* templ = FindByType(mods, WeaponMod_StreakShot);
    if (!templ && mods && mods->Num) templ = mods->Data[0];
    if (!templ) return nullptr;
    void* da = MakeDA(StaticClass(va::UCrabWeaponModDA_StaticClass), templ, L"DA_WeaponMod_Arsenal");
    if (!da) return nullptr;
    SetFString(da, off::DA_Name, L"Arsenal");
    SetFString(da, off::DA_Description, L"Shots deal more damage for each weapon mod you have");
    SetFString(da, off::DA_LevelDescription, L"Damage increased by PrimaryBuff% for each other weapon mod you have");
    At<bool>(da, off::DA_bRequiresUnlock) = false;
    At<uint8_t>(da, off::DA_Rarity) = g_cfg.arsenal.rarity;
    At<float>(da, off::DA_BaseBuff) = g_cfg.arsenal.damagePerModPercent;
    At<bool>(da, off::DA_bHyperbolicBuff) = false;
    At<bool>(da, off::DA_bLimited) = false;
    At<bool>(da, off::DA_bGetBuffAsMultiplier) = true;
    At<uint8_t>(da, off::DA_ItemType) = kArsenalType;
    if (void* icon = GenericIcon(mods, "T_Icon_WeaponMod_", g_cfg.arsenal.rarity)) At<void*>(da, off::DA_Icon) = icon;
    CRAB_LOG("created %s from %s, icon %s", ObjectName(da).c_str(), ObjectName(templ).c_str(),
             ObjectName(At<void*>(da, off::DA_Icon)).c_str());
    return da;
}

void* CreateHoarder(void* lm) {
    auto* perks = Spawnables(lm, off::Spawnables_Perks);
    void* templ = FindByType(perks, Perk_Collector);
    if (!templ) return nullptr;
    void* da = MakeDA(StaticClass(va::UCrabPerkDA_StaticClass), templ, L"DA_Perk_Hoarder");
    if (!da) return nullptr;
    SetFString(da, off::DA_Name, L"Hoarder");
    SetFString(da, off::DA_Description, L"Damage increased for each weapon mod and perk level you have");
    SetFString(da, off::DA_LevelDescription,
               L"Damage increased by PrimaryBuff% for each weapon mod and each perk level you have");
    At<bool>(da, off::DA_bRequiresUnlock) = false;
    At<uint8_t>(da, off::DA_Rarity) = g_cfg.hoarder.rarity;
    At<float>(da, off::DA_BaseBuff) = g_cfg.hoarder.damagePercent;
    At<bool>(da, off::DA_bHyperbolicBuff) = false;
    At<bool>(da, off::DA_bLimited) = false;
    At<bool>(da, off::DA_bGetBuffAsMultiplier) = true;
    At<uint8_t>(da, off::DA_ItemType) = kHoarderType;
    if (void* icon = GenericIcon(perks, "T_Icon_Perk_", g_cfg.hoarder.rarity)) At<void*>(da, off::DA_Icon) = icon;
    CRAB_LOG("created %s from %s, icon %s", ObjectName(da).c_str(), ObjectName(templ).c_str(),
             ObjectName(At<void*>(da, off::DA_Icon)).c_str());
    return da;
}

void AddToPool(void* lm, void* da, const char* what) {
    if (!da) return;
    auto& pool = At<TArray<void*>>(lm, off::LM_AllUnlockedPickups);
    if (Contains(pool, da)) return;
    Append(pool, da);
    CRAB_LOG("added %s to the loot pool (%d pickups)", what, pool.Num);
}

// Heavy Shot's asset asks for a weapon with the Bounce tag. Clearing that lets loot rolls offer it with any
// weapon; the game's own Heavy Shot code already works on every projectile. Loot is rolled by the host.
void UnlockHeavyShot(void* lm) {
    void* da = FindByType(Spawnables(lm, off::Spawnables_WeaponMods), WeaponMod_HeavyShot);
    if (!da) return;
    bool wasLocked = At<bool>(da, off::DA_bRequiresMatchingPickupTag);
    At<bool>(da, off::DA_bRequiresMatchingPickupTag) = false;
    if (g_heavyShotLogged) return;
    g_heavyShotLogged = true;
    bool inPool = Contains(At<TArray<void*>>(lm, off::LM_AllUnlockedPickups), da);
    CRAB_LOG("Heavy Shot can now drop for every weapon (was %s)%s", wasLocked ? "Bounce weapons only" : "unrestricted",
             inPool ? "" : "; it is not unlocked in this save yet, so it still won't drop");
}

void HkBeginPlay(void* lm) {
    oBeginPlay(lm);
    if (g_cfg.arsenal.enabled && !g_arsenal) {
        if (GetModuleHandleW(L"Arsenal.dll")) {
            static bool warned = false;
            if (!warned) CRAB_LOG("the old Arsenal.dll is also installed: delete it, CrabMods includes Arsenal");
            warned = true;
        } else {
            g_arsenal = CreateArsenal(lm);
            if (!g_arsenal) CRAB_LOG("could not create Arsenal yet");
        }
    }
    if (g_cfg.hoarder.enabled && !g_hoarder) {
        g_hoarder = CreateHoarder(lm);
        if (!g_hoarder) CRAB_LOG("could not create Hoarder yet");
    }
    AddToPool(lm, g_arsenal, "Arsenal");
    AddToPool(lm, g_hoarder, "Hoarder");
    if (g_cfg.heavyShot.anyWeapon) UnlockHeavyShot(lm);
}

struct Bonus {
    float arsenal = 0.f, hoarder = 0.f;
};

// Arsenal: Arsenal level x other weapon mods. Hoarder: Hoarder level x (weapon mods + perk levels), counting
// itself like Collector does. Both as fractions (0.03 = +3%).
Bonus GetBonus(void* ps) {
    int arsenal = 0, others = 0, mods = 0;
    ForEachItem(ps, off::PS_WeaponMods, [&](void* da, int level) {
        int n = g_cfg.arsenal.countStacks ? level : 1;
        mods += n;
        if (da == g_arsenal && g_arsenal)
            arsenal += level;
        else
            others += n;
    });
    int hoarder = 0, perkLevels = 0;
    if (g_hoarder) {
        ForEachItem(ps, off::PS_Perks, [&](void* da, int level) {
            perkLevels += level;
            if (da == g_hoarder) hoarder += level;
        });
    }
    return {arsenal * others * g_cfg.arsenal.damagePerModPercent * 0.01f,
            hoarder * (mods + perkLevels) * g_cfg.hoarder.damagePercent * 0.01f};
}

// The bonus goes into DamageMultiplier for the length of the call, so it is added alongside the game's
// own damage perks (Collector included) and scaled by the same multipliers.
float HkDamageBuff(void* ps) {
    Bonus b = ps ? GetBonus(ps) : Bonus{};
    float bonus = b.arsenal + b.hoarder;
    if (bonus == 0.f) return oDamageBuff(ps);
    float& mult = At<float>(ps, off::PS_DamageMultiplier);
    float saved = mult;
    mult += bonus;
    float r = oDamageBuff(ps);
    mult = saved;
    auto& last = g_loggedBonus[ps];
    if (last != bonus) {
        last = bonus;
        CRAB_LOG("player %p: Arsenal +%.0f%%, Hoarder +%.0f%% damage (buff now %.2f)", ps, b.arsenal * 100.f,
                 b.hoarder * 100.f, r);
    }
    return r;
}

DWORD WINAPI Init(LPVOID) {
    std::wstring dir = ModuleDir();
    Log::Get().Open(dir + L"CrabMods.log");
    bool created = false, migrated = false;
    g_cfg = LoadConfig(dir + L"CrabMods.ini", dir + L"Arsenal.ini", &created, &migrated);
    if (created) CRAB_LOG("wrote default CrabMods.ini%s", migrated ? " (Arsenal settings taken from Arsenal.ini)" : "");
    if (!InitImage()) {
        CRAB_LOG("unsupported game build (PE timestamp mismatch); CrabMods is disabled");
        return 0;
    }
    if (!g_cfg.arsenal.enabled && !g_cfg.hoarder.enabled && !g_cfg.heavyShot.anyWeapon) {
        CRAB_LOG("everything is disabled in CrabMods.ini");
        return 0;
    }
    if (!HookInit()) return 0;
    Hook(va::ACrabLM_BeginPlay, &HkBeginPlay, &oBeginPlay, "ACrabLM::BeginPlay");
    Hook(va::ACrabPS_GetDamageBuff, &HkDamageBuff, &oDamageBuff, "ACrabPS::GetDamageBuff");
    CRAB_LOG("CrabMods %s loaded", kVersion);
    CRAB_LOG("  Arsenal: %s, +%.1f%% per weapon mod, rarity %d, countStacks %d", g_cfg.arsenal.enabled ? "on" : "off",
             g_cfg.arsenal.damagePerModPercent, g_cfg.arsenal.rarity, g_cfg.arsenal.countStacks);
    CRAB_LOG("  Hoarder: %s, +%.1f%% per weapon mod and perk level, rarity %d", g_cfg.hoarder.enabled ? "on" : "off",
             g_cfg.hoarder.damagePercent, g_cfg.hoarder.rarity);
    CRAB_LOG("  Heavy Shot for every weapon: %s", g_cfg.heavyShot.anyWeapon ? "on" : "off");
    return 0;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        // Hooks go in off the loader lock.
        CloseHandle(CreateThread(nullptr, 0, Init, nullptr, 0, nullptr));
    }
    return TRUE;
}
