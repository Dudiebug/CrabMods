// Arsenal: a new weapon mod for Crab Champions. Each Arsenal you hold adds damage for every other weapon
// mod you have.
//
// The game has no Arsenal asset, so one is made at runtime: a copy of Streak Shot's data asset with a new
// name, text, rarity and an unused WeaponModType. It is created under /Script/CrabChampions with a fixed
// name and flagged as loaded, so the game replicates it by path and every player's copy (made the same
// way by their own Arsenal.dll) stands in for it. It is added to the level manager's loot list so chests
// roll it. Its effect is applied in the player's damage multiplier, added like the game's own damage perks.
#include <windows.h>

#include <cwchar>
#include <unordered_map>

#include <crab/hook.hpp>
#include <crab/log.hpp>
#include <crab/ue.hpp>

#include "config.hpp"

using namespace crab;
using namespace arsenal;

namespace {

constexpr const char kVersion[] = "0.1.0";
constexpr uint8_t kArsenalType = 200;  // ECrabWeaponModType has nothing past 94
constexpr const wchar_t kObjectName[] = L"DA_WeaponMod_Arsenal";

Config g_cfg;
void* g_da = nullptr;  // the Arsenal data asset, rooted for the life of the process

using BeginPlayFn = void (*)(void*);
using DamageBuffFn = float (*)(void*);
BeginPlayFn oBeginPlay;
DamageBuffFn oDamageBuff;

// Last bonus logged per player, so the log shows changes rather than every shot.
std::unordered_map<void*, float> g_loggedBonus;

void SetFString(void* obj, int offset, const wchar_t* text) {
    auto& s = At<TArray<wchar_t>>(obj, offset);
    int n = int(wcslen(text)) + 1;
    auto* p = static_cast<wchar_t*>(Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(nullptr, n * 2, 0));
    if (!p) return;
    memcpy(p, text, n * 2);
    if (s.Data) Fn<void (*)(void*)>(va::FMemory_Free)(s.Data);
    s = {p, n, n};
}

template <class T>
bool Contains(const TArray<T>& a, T v) {
    for (int i = 0; i < a.Num; i++)
        if (a.Data[i] == v) return true;
    return false;
}

void Append(TArray<void*>& a, void* v) {
    if (a.Num == a.Max) {
        int max = a.Max ? a.Max * 2 : 4;
        auto* p = Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(a.Data, size_t(max) * sizeof(void*), 0);
        if (!p) return;
        a.Data = static_cast<void**>(p);
        a.Max = max;
    }
    a.Data[a.Num++] = v;
}

void* FindTemplate(void* lm) {
    void* spawnables = At<void*>(lm, off::LM_SpawnablesDA);
    if (!spawnables) return nullptr;
    auto& mods = At<TArray<void*>>(spawnables, off::Spawnables_WeaponMods);
    void* fallback = nullptr;
    for (int i = 0; i < mods.Num; i++) {
        void* da = mods.Data[i];
        if (!da) continue;
        if (At<uint8_t>(da, off::DA_WeaponModType) == WeaponMod_StreakShot) return da;
        if (!fallback) fallback = da;
    }
    return fallback;
}

// The game's plain bullet icon for a rarity (T_Icon_WeaponMod_Rare/_Epic/_Legendary), taken from a weapon
// mod that uses it. Mods without their own art use these, and the background colour is baked in.
void* RarityIcon(void* lm, uint8_t rarity) {
    static const char* names[] = {"", "T_Icon_WeaponMod_Rare", "T_Icon_WeaponMod_Epic", "T_Icon_WeaponMod_Legendary"};
    if (rarity < 1 || rarity > 3) return nullptr;
    void* spawnables = At<void*>(lm, off::LM_SpawnablesDA);
    if (!spawnables) return nullptr;
    auto& mods = At<TArray<void*>>(spawnables, off::Spawnables_WeaponMods);
    for (int i = 0; i < mods.Num; i++) {
        void* icon = mods.Data[i] ? At<void*>(mods.Data[i], off::DA_Icon) : nullptr;
        if (icon && ObjectName(icon) == names[rarity]) return icon;
    }
    return nullptr;
}

void* CreateArsenal(void* templ) {
    void* cls = StaticClass(va::UCrabWeaponModDA_StaticClass);
    void* package = At<void*>(cls, off::Object_Outer);  // /Script/CrabChampions
    uint64_t name = 0;
    Fn<void* (*)(uint64_t*, const wchar_t*, int)>(va::FName_Ctor)(&name, kObjectName, 1 /*FNAME_Add*/);
    using NewObjectFn = void* (*)(void*, void*, uint64_t, uint32_t, void*, bool, void*, void*);
    void* da = Fn<NewObjectFn>(va::NewObject)(package, cls, name, RF_Public | RF_Standalone | RF_MarkAsRootSet,
                                              templ, false, nullptr, nullptr);
    if (!da) return nullptr;
    At<uint32_t>(da, off::Object_Flags) |= RF_WasLoaded;

    SetFString(da, off::DA_Name, L"Arsenal");
    SetFString(da, off::DA_Description, L"Shots deal more damage for each weapon mod you have");
    SetFString(da, off::DA_LevelDescription, L"Damage increased by PrimaryBuff% for each other weapon mod you have");
    At<bool>(da, off::DA_bRequiresUnlock) = false;
    At<uint8_t>(da, off::DA_Rarity) = g_cfg.rarity;
    At<float>(da, off::DA_BaseBuff) = g_cfg.damagePerModPercent;
    At<bool>(da, off::DA_bHyperbolicBuff) = false;
    At<bool>(da, off::DA_bLimited) = false;
    At<bool>(da, off::DA_bGetBuffAsMultiplier) = true;
    At<uint8_t>(da, off::DA_WeaponModType) = kArsenalType;
    return da;
}

void HkBeginPlay(void* lm) {
    oBeginPlay(lm);
    if (!g_da) {
        void* templ = FindTemplate(lm);
        if (!templ) {
            CRAB_LOG("no weapon mod to copy from yet");
            return;
        }
        g_da = CreateArsenal(templ);
        CRAB_LOG("created %s from %s: %p", ObjectName(g_da).c_str(), ObjectName(templ).c_str(), g_da);
        if (!g_da) return;
        if (void* icon = RarityIcon(lm, g_cfg.rarity)) At<void*>(g_da, off::DA_Icon) = icon;
        CRAB_LOG("icon %s", ObjectName(At<void*>(g_da, off::DA_Icon)).c_str());
    }
    auto& pool = At<TArray<void*>>(lm, off::LM_AllUnlockedPickups);
    if (!Contains(pool, g_da)) {
        Append(pool, g_da);
        CRAB_LOG("added Arsenal to the loot pool (%d pickups)", pool.Num);
    }
}

// Arsenal level times the number of other weapon mods, as a fraction (0.03 = +3%).
float Bonus(void* ps) {
    auto& mods = At<TArray<uint8_t>>(ps, off::PS_WeaponMods);
    if (!g_da || !mods.Data) return 0.f;
    int arsenal = 0, others = 0;
    for (int i = 0; i < mods.Num; i++) {
        uint8_t* m = mods.Data + i * off::WeaponMod_Size;
        void* da = *reinterpret_cast<void**>(m);
        int level = m[off::WeaponMod_Level];
        if (!da) continue;
        if (da == g_da)
            arsenal += level;
        else
            others += g_cfg.countStacks ? level : 1;
    }
    return arsenal * others * g_cfg.damagePerModPercent * 0.01f;
}

// The bonus goes into DamageMultiplier for the length of the call, so it is added alongside the game's
// own damage perks and scaled by the same multipliers.
float HkDamageBuff(void* ps) {
    float bonus = ps ? Bonus(ps) : 0.f;
    if (bonus == 0.f) return oDamageBuff(ps);
    float& mult = At<float>(ps, off::PS_DamageMultiplier);
    float saved = mult;
    mult += bonus;
    float r = oDamageBuff(ps);
    mult = saved;
    auto& last = g_loggedBonus[ps];
    if (last != bonus) {
        last = bonus;
        CRAB_LOG("player %p: Arsenal +%.0f%% damage (buff now %.2f)", ps, bonus * 100.f, r);
    }
    return r;
}

DWORD WINAPI Init(LPVOID) {
    std::wstring dir = ModuleDir();
    Log::Get().Open(dir + L"Arsenal.log");
    bool created = false;
    g_cfg = LoadConfig(dir + L"Arsenal.ini", &created);
    if (created) CRAB_LOG("wrote default Arsenal.ini");
    if (!InitImage()) {
        CRAB_LOG("unsupported game build (PE timestamp mismatch); Arsenal is disabled");
        return 0;
    }
    if (!g_cfg.enabled) {
        CRAB_LOG("disabled in Arsenal.ini");
        return 0;
    }
    if (!HookInit()) return 0;
    Hook(va::ACrabLM_BeginPlay, &HkBeginPlay, &oBeginPlay, "ACrabLM::BeginPlay");
    Hook(va::ACrabPS_GetDamageBuff, &HkDamageBuff, &oDamageBuff, "ACrabPS::GetDamageBuff");
    CRAB_LOG("Arsenal %s loaded: +%.1f%% per weapon mod, rarity %d, countStacks %d", kVersion,
             g_cfg.damagePerModPercent, g_cfg.rarity, g_cfg.countStacks);
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
