// Addresses and offsets for CrabChampions-Win64-Shipping.exe, PE timestamp 0x6921D2F7.
// Every value comes from static reverse engineering of that build. A game update invalidates them, and
// CrabSync then disables itself (see InitImage).
#pragma once
#include <cstdint>

namespace crab {

constexpr uintptr_t kImageBase = 0x140000000;
constexpr uint32_t kExpectedTimestamp = 0x6921D2F7;

namespace va {
// Engine
constexpr uintptr_t UGameEngine_Tick = 0x1422eefb0;     // void(UGameEngine*, float DeltaSeconds, bool bIdleMode)
constexpr uintptr_t FMemory_Realloc = 0x140e017e0;      // void*(void* Ptr, size_t Size, uint32 Align)
constexpr uintptr_t FMemory_Free = 0x140df0dc0;         // void(void* Ptr)
constexpr uintptr_t FMulticastDelegate_Broadcast0 = 0x14078da30;  // void(FMulticastDelegate*)
constexpr uintptr_t NamePoolData = 0x144282980;

// StaticClass() getters: UClass*()
constexpr uintptr_t ACrabGS_StaticClass = 0x140d76930;
constexpr uintptr_t ACrabPS_StaticClass = 0x140d80420;
constexpr uintptr_t ACrabC_StaticClass = 0x140d707f0;
constexpr uintptr_t ACrabPlayerC_StaticClass = 0x140d7d2a0;

// Inventory
constexpr uintptr_t ACrabPlayerC_GivePickup = 0x140d168d0;    // void(ACrabPlayerC*, FCrabPickupInfo*)
constexpr uintptr_t ACrabPS_AddWeaponMod = 0x140d210a0;       // void(ACrabPS*, FItem*)
constexpr uintptr_t ACrabPS_AddAbilityMod = 0x140d1fd60;
constexpr uintptr_t ACrabPS_AddMeleeMod = 0x140d20140;
constexpr uintptr_t ACrabPS_AddPerk = 0x140d20520;
constexpr uintptr_t ACrabPS_AddRelic = 0x140d20930;
constexpr uintptr_t ACrabPS_RemoveWeaponMod = 0x140d299d0;    // void(ACrabPS*, uint8 Type)
constexpr uintptr_t ACrabPS_RemoveAbilityMod = 0x140d29650;
constexpr uintptr_t ACrabPS_RemoveMeleeMod = 0x140d29730;
constexpr uintptr_t ACrabPS_RemovePerk = 0x140d29810;
constexpr uintptr_t ACrabPS_RemoveRelic = 0x140d298f0;
constexpr uintptr_t ACrabPS_BroadcastCrystals = 0x140d271e0;  // void(ACrabPS*): fires the crystals-changed delegate
constexpr uintptr_t ACrabPS_UpdateMaxHealth = 0x140d28a30;    // void(ACrabPS*): recomputes pawn max health from items
constexpr uintptr_t ACrabPS_RestoreAutoSave = 0x140d28ee0;    // void(ACrabPS*, FCrabAutoSave*)
constexpr uintptr_t FCrabInventoryInfo_CopyConstruct = 0x140cc7220;  // FInvInfo*(FInvInfo* Dst, const FInvInfo* Src)
constexpr uintptr_t TArray28_ResizeGrow = 0x140840fd0;        // void(TArray<0x28-byte elem>*), Num already bumped

// Health (UCrabHC, the character's health component)
constexpr uintptr_t UCrabHC_TakeDamage = 0x140c9afe0;        // void(UCrabHC*, FCrabDamageInfo*): armor, protections, elimination
constexpr uintptr_t UCrabHC_OnRep_HealthInfo = 0x140c9b4a0;  // void(UCrabHC*): broadcasts health changed, Previous = Current
constexpr uintptr_t ACrabC_SetMaxHealth = 0x140cbd780;       // void(ACrabC*, float): sets HC max, clamps health down to it
}  // namespace va

namespace off {
// UEngine / UGameViewportClient / UWorld
constexpr int Engine_GameViewport = 0x780;
constexpr int Viewport_World = 0x78;
constexpr int World_GameState = 0x120;
// UObject / AActor
constexpr int Object_Class = 0x10;
constexpr int Object_Name = 0x18;
constexpr int Actor_Owner = 0xE0;
constexpr int Actor_Role = 0xF0;  // ENetRole; 3 = ROLE_Authority
// AGameStateBase / ACrabGS
constexpr int GS_PlayerArray = 0x238;
constexpr int GS_CurrentIsland = 0x2D4;
constexpr int GS_CurrentIslandType = 0x2D8;
// APlayerState / ACrabPS
constexpr int PS_PlayerId = 0x224;
constexpr int PS_Pawn = 0x280;
constexpr int PS_PlayerName = 0x300;  // FString
constexpr int PS_WeaponDA = 0x3C0;
constexpr int PS_AbilityDA = 0x3C8;
constexpr int PS_MeleeDA = 0x3D0;
constexpr int PS_NumWeaponModSlots = 0x3D8;
constexpr int PS_WeaponMods = 0x3E0;
constexpr int PS_NumAbilityModSlots = 0x3F0;
constexpr int PS_AbilityMods = 0x3F8;
constexpr int PS_NumMeleeModSlots = 0x408;
constexpr int PS_MeleeMods = 0x410;
constexpr int PS_NumPerkSlots = 0x420;
constexpr int PS_Perks = 0x428;
constexpr int PS_Relics = 0x438;
constexpr int PS_Crystals = 0x470;
constexpr int PS_OnInventoryChanged = 0x520;  // native FMulticastDelegate
// ACrabC (any character)
constexpr int C_HC = 0x4C0;             // UCrabHC*
constexpr int C_bIsEliminated = 0x668;  // bit 0
// UCrabHC
constexpr int HC_OwningC = 0xB0;              // set when the component initialises
constexpr int HC_bDead = 0xD8;                // unreflected: set by TakeDamage when health reaches 0
constexpr int HC_bDeathProtectionUsed = 0xFB; // unreflected: one-time death protection consumed
constexpr int HC_CurrentHealth = 0x108;       // HealthInfo (0xFC) + 0xC
constexpr int HC_CurrentMaxHealth = 0x10C;
// Inventory data assets
constexpr int DA_PickupType = 0x60;  // ECrabPickupType
constexpr int DA_ItemType = 0xD8;    // per-category type enum (WeaponModType, PerkType, ...)
}  // namespace off

enum class IslandType : uint8_t { None = 0, Lobby = 1, NewRun = 2, ContinueRun = 3 };  // ECrabIslandType, 0..16

inline const char* IslandTypeName(uint8_t t) {
    static const char* n[] = {"None",     "Lobby",   "NewRun", "ContinueRun", "Arena",  "Horde",
                              "Demolition", "Gauntlet", "Parkour", "Shop",      "Elite",  "Boss",
                              "Biome",    "CrabIsland", "Loop",  "Arcade",      "Holdout"};
    return t < sizeof(n) / sizeof(*n) ? n[t] : "?";
}
constexpr uint8_t ROLE_Authority = 3;

}  // namespace crab
