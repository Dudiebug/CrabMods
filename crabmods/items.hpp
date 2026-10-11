// Helpers for making new pickups at runtime: copying a data asset, editing its strings and finding the
// game's generic icons.
#pragma once
#include <cwchar>

#include <crab/ue.hpp>

namespace crabmods {

using namespace crab;

inline void SetFString(void* obj, int offset, const wchar_t* text) {
    auto& s = At<TArray<wchar_t>>(obj, offset);
    int n = int(wcslen(text)) + 1;
    auto* p = static_cast<wchar_t*>(Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(nullptr, n * 2, 0));
    if (!p) return;
    memcpy(p, text, n * 2);
    if (s.Data) Fn<void (*)(void*)>(va::FMemory_Free)(s.Data);
    s = {p, n, n};
}

// Shows text on that player's screen, like High Roller's "High Roller gave you 2 more chests!" (host).
inline void ShowMessage(void* ps, const wchar_t* text) {
    TArray<wchar_t> s{};
    SetFString(&s, 0, text);
    if (!s.Data) return;
    Fn<void (*)(void*, TArray<wchar_t>*)>(va::ACrabPS_ShowMessage)(ps, &s);
    Fn<void (*)(void*)>(va::FMemory_Free)(s.Data);
}

template <class T>
bool Contains(const TArray<T>& a, T v) {

    for (int i = 0; i < a.Num; i++)
        if (a.Data[i] == v) return true;
    return false;
}

inline void Append(TArray<void*>& a, void* v) {
    if (a.Num == a.Max) {
        int max = a.Max ? a.Max * 2 : 4;
        auto* p = Fn<void* (*)(void*, size_t, uint32_t)>(va::FMemory_Realloc)(a.Data, size_t(max) * sizeof(void*), 0);
        if (!p) return;
        a.Data = static_cast<void**>(p);
        a.Max = max;
    }
    a.Data[a.Num++] = v;
}

// A list of data assets in the level manager's SpawnablesDA (off::Spawnables_*).
inline TArray<void*>* Spawnables(void* lm, int listOffset) {
    void* spawnables = At<void*>(lm, off::LM_SpawnablesDA);
    return spawnables ? &At<TArray<void*>>(spawnables, listOffset) : nullptr;
}

// The asset in the list with the given type byte (WeaponModType, PerkType, RelicType, AbilityModType: +0xD8).
inline void* FindByType(TArray<void*>* list, uint8_t type) {
    if (!list) return nullptr;
    for (int i = 0; i < list->Num; i++)
        if (void* da = list->Data[i]; da && At<uint8_t>(da, off::DA_ItemType) == type) return da;
    return nullptr;
}

// The game's generic icon for a rarity, prefix + Rare/Epic/Legendary/Greed (T_Icon_WeaponMod_Epic,
// T_Icon_Perk_Legendary, T_Icon_Relic_Greed), taken from an asset in the list that uses it. The rarity's
// background colour is part of the image.
inline void* GenericIcon(TArray<void*>* list, const char* prefix, uint8_t rarity) {
    static const char* names[] = {"", "Rare", "Epic", "Legendary", "Greed"};
    if (!list || rarity < 1 || rarity > 4) return nullptr;
    std::string want = std::string(prefix) + names[rarity];
    for (int i = 0; i < list->Num; i++) {
        void* icon = list->Data[i] ? At<void*>(list->Data[i], off::DA_Icon) : nullptr;
        if (icon && ObjectName(icon) == want) return icon;
    }
    return nullptr;
}

// A copy of templ under /Script/CrabChampions with a fixed name, rooted for the life of the process. It is
// flagged as loaded so the game replicates it by path: every player's copy, made the same way by their own
// CrabMods.dll, stands in for it.
inline void* MakeDA(void* cls, void* templ, const wchar_t* objectName) {
    void* package = At<void*>(cls, off::Object_Outer);
    uint64_t name = 0;
    Fn<void* (*)(uint64_t*, const wchar_t*, int)>(va::FName_Ctor)(&name, objectName, 1 /*FNAME_Add*/);
    using NewObjectFn = void* (*)(void*, void*, uint64_t, uint32_t, void*, bool, void*, void*);
    void* da = Fn<NewObjectFn>(va::NewObject)(package, cls, name, RF_Public | RF_Standalone | RF_MarkAsRootSet,
                                              templ, false, nullptr, nullptr);
    if (da) At<uint32_t>(da, off::Object_Flags) |= RF_WasLoaded;
    return da;
}

// Walks an inventory array (FCrabWeaponMod, FCrabAbilityMod, FCrabPerk, FCrabRelic: 0x28 bytes, DA at +0,
// level at +8).
template <class F>
void ForEachItem(void* owner, int arrayOffset, F&& f) {
    auto& a = At<TArray<uint8_t>>(owner, arrayOffset);
    if (!a.Data) return;
    for (int i = 0; i < a.Num; i++) {
        uint8_t* e = a.Data + i * off::Item_Size;
        if (void* da = *reinterpret_cast<void**>(e)) f(da, int(e[off::Item_Level]));
    }
}

}  // namespace crabmods
