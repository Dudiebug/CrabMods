// Addresses and offsets for CrabChampions-Win64-Shipping.exe, PE timestamp 0x6921D2F7.
// Every value comes from static reverse engineering of that build. A game update invalidates them, and
// the mod then disables itself (see InitImage).
#pragma once
#include <cstdint>

namespace crab {

constexpr uintptr_t kImageBase = 0x140000000;
constexpr uint32_t kExpectedTimestamp = 0x6921D2F7;

namespace va {
// Engine
constexpr uintptr_t FMemory_Realloc = 0x140e017e0;  // void*(void* Ptr, size_t Size, uint32 Align)
constexpr uintptr_t FMemory_Free = 0x140df0dc0;     // void(void* Ptr)
constexpr uintptr_t FName_Ctor = 0x140ed46e0;       // FName*(FName*, const wchar_t*, EFindName)
// NewObject<UObject>(Outer, Class, Name, Flags, Template, bCopyTransientsFromClassDefaults,
// InstanceGraph, ExternalPackage) -> UObject*. FName is passed by value (8 bytes).
constexpr uintptr_t NewObject = 0x14078b370;
constexpr uintptr_t NamePoolData = 0x144282980;

// StaticClass() getters: UClass*()
constexpr uintptr_t UCrabWeaponModDA_StaticClass = 0x140d8a380;
constexpr uintptr_t UCrabPerkDA_StaticClass = 0x140d7c920;

// Game
constexpr uintptr_t ACrabLM_BeginPlay = 0x140cf73d0;     // void(ACrabLM*): fills AllUnlockedPickups
// float(ACrabPS*): (DamageMultiplier + additive damage perks) * (1 + multiplier perks). Returns
// DamageMultiplier alone when the player has no pawn.
constexpr uintptr_t ACrabPS_GetDamageBuff = 0x140d23700;
}  // namespace va

namespace off {
// UObject
constexpr int Object_Flags = 0x08;
constexpr int Object_Class = 0x10;
constexpr int Object_Name = 0x18;
constexpr int Object_Outer = 0x20;
// ACrabLM
constexpr int LM_SpawnablesDA = 0x388;
constexpr int LM_AllUnlockedPickups = 0x420;  // TArray<UCrabPickupDA*>: what chests and rewards roll from
// UCrabSpawnablesDA
constexpr int Spawnables_WeaponMods = 0x108;  // TArray<UCrabWeaponModDA*>
constexpr int Spawnables_Perks = 0x138;       // TArray<UCrabPerkDA*>
// ACrabPS
constexpr int PS_DamageMultiplier = 0x3A0;
constexpr int PS_WeaponMods = 0x3E0;  // TArray<FCrabWeaponMod>, 0x28 bytes each: DA at +0, level at +8
constexpr int PS_Perks = 0x428;      // TArray<FCrabPerk>, same 0x28-byte layout
constexpr int WeaponMod_Size = 0x28;  // FCrabWeaponMod / FCrabPerk
constexpr int WeaponMod_Level = 0x08;
// UCrabPickupDA / UCrabInventoryDA / UCrabWeaponModDA
constexpr int DA_bRequiresUnlock = 0x30;
constexpr int DA_Name = 0x38;         // FString
constexpr int DA_Description = 0x48;  // FString
constexpr int DA_Icon = 0x58;         // UTexture2D*; the rarity's background colour is part of the image
constexpr int DA_Rarity = 0x62;       // ECrabRarity: 1 Common, 2 Epic, 3 Legendary
constexpr int DA_PickupTag = 0x63;
// When set, loot rolls offer the pickup only if the player's weapon has the same PickupTag. The roll reads
// it from the asset each time (0x140d34050, 0x140d3417f, 0x140d3439e).
constexpr int DA_bRequiresMatchingPickupTag = 0x64;
constexpr int DA_LevelDescription = 0xB8;  // FString; "PrimaryBuff" is replaced with the buff
constexpr int DA_BaseBuff = 0xC8;
constexpr int DA_bHyperbolicBuff = 0xCC;
constexpr int DA_bLimited = 0xCD;
constexpr int DA_bGetBuffAsMultiplier = 0xCE;  // BaseBuff is a percentage
constexpr int DA_WeaponModType = 0xD8;
constexpr int DA_PerkType = 0xD8;  // UCrabPerkDA
}  // namespace off

// UObject flags (UE 4.27 EObjectFlags)
constexpr uint32_t RF_Public = 0x1;
constexpr uint32_t RF_Standalone = 0x2;
constexpr uint32_t RF_MarkAsRootSet = 0x80;
constexpr uint32_t RF_WasLoaded = 0x80000;  // makes the name stable for networking: replicated by path

constexpr uint8_t WeaponMod_StreakShot = 27;  // ECrabWeaponModType; the enum ends at DiceShot = 94
constexpr uint8_t WeaponMod_HeavyShot = 30;   // only drops for Bounce weapons unless unlocked
constexpr uint8_t Perk_Collector = 72;        // ECrabPerkType; the enum ends at FlashSale = 121

}  // namespace crab
