// Addresses and offsets for CrabChampions-Win64-Shipping.exe, PE timestamp 0x6921D2F7.
// Every value comes from static reverse engineering of that build (see re/REPORT.md). A game update
// invalidates them, and each mod then disables itself (see InitImage).
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
// Calling a UFunction: ProcessEvent is UObject vtable slot 0x220, void(UObject*, UFunction*, void* Params). A Net
// function called on the server goes over the network to the object's owner.
constexpr int UObject_ProcessEvent_Slot = 0x220;
constexpr uintptr_t UObject_FindFunctionChecked = 0x1410c96b0;  // UFunction*(UObject*, FName)
constexpr uintptr_t FName_ClientMessage = 0x14440f450;  // FName L"ClientMessage", set during startup
// void(APlayerController*, const FString& S, FName Type, float MsgLifeTime): a ClientMessage RPC arriving
// (it prints S to the console). Params: FString S at 0, FName Type at 0x10, float at 0x18; 0x20 bytes.
constexpr uintptr_t APlayerController_ClientMessage_Impl = 0x14255c410;
// void(AGameModeBase*, APlayerController*): the K2_PostLogin event, called by PostLogin on the server for every
// player who joins, the host's own included.
constexpr uintptr_t AGameModeBase_K2_PostLogin = 0x1427e2de0;
constexpr uintptr_t FName_Ctor = 0x140ed46e0;           // FName*(FName*, const wchar_t*, EFindName)
// NewObject<UObject>(Outer, Class, Name, Flags, Template, bCopyTransientsFromClassDefaults,
// InstanceGraph, ExternalPackage) -> UObject*. FName is passed by value (8 bytes).
constexpr uintptr_t NewObject = 0x14078b370;
constexpr uintptr_t NamePoolData = 0x144282980;

// StaticClass() getters: UClass*()
constexpr uintptr_t ACrabGS_StaticClass = 0x140d76930;
constexpr uintptr_t ACrabPS_StaticClass = 0x140d80420;
constexpr uintptr_t ACrabC_StaticClass = 0x140d707f0;
constexpr uintptr_t ACrabPlayerC_StaticClass = 0x140d7d2a0;
constexpr uintptr_t ACrabSpectatorC_StaticClass = 0x140d83600;  // a dead player's pawn until revived
constexpr uintptr_t UCrabWeaponModDA_StaticClass = 0x140d8a380;
constexpr uintptr_t UCrabPerkDA_StaticClass = 0x140d7c920;
constexpr uintptr_t UCrabRelicDA_StaticClass = 0x140d824b0;
constexpr uintptr_t UCrabAbilityModDA_StaticClass = 0x140d6f2b0;

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

// Max health. UpdateMaxHealth computes it as
//   max(0.01, MaxHealthMultiplier + perk buffs) * (BaseMaxHealth + perk/relic buffs), at least 1.
// BaseMaxHealth is per player, and these are what change it (each adds Amount, then calls UpdateMaxHealth;
// authority only):
constexpr uintptr_t ACrabPS_AddBaseMaxHealth = 0x140d20a00;          // void(ACrabPS*, float Amount)
constexpr uintptr_t ACrabPS_AddBaseMaxHealthFromPerk = 0x140d20b20;  // void(ACrabPS*, float Amount, uint8 PerkType)
// Their callers, by return address (call + 5):
//   chest cost (MaxHealth currency):  AddBaseMaxHealth from ACrabChest pay, below
//   totems (Health x2 or /2, Loot -8%, Gold -33%), damage areas, Eternal Punishment (-0.5% per hit) and
//   Damage Seeker on the hit player, Personal Space / Bullseye on a kill, Valued Customer: one player each
//   Fortitude, at each new island: the game loops over every player, so it is one team event
constexpr uintptr_t Ret_ChestMaxHealthCost = 0x140cafb87;
constexpr uintptr_t Ret_FortitudeIsland = 0x140ce9fc8;
// MaxHealthMultiplier (PS+0x39C) is only changed for every player at once (difficulty Relaxed, at the
// start of a run), so it never differs between players.

// void(ACrabChest*, ACrabC* Opener): pays the chest's Cost in its CurrencyType from the opener (crystals,
// keys, Cost% of max as Unblockable damage, or Cost% of base max health) and rolls its loot. Returns at
// once if the chest is already open.
constexpr uintptr_t ACrabChest_Pay = 0x140caf9f0;


// Ability cooldown (ACrabPlayerC fields, not replicated: counted down by the owner, and maybe the host too)
// void(ACrabPlayerC*): a 0.25 s timer. AbilityCooldown -= 0.25 (to 0); on reaching 0 one charge comes back and,
// if charges are still short, the next cooldown starts. Also counts down the melee cooldown (+0xA28).
constexpr uintptr_t ACrabPlayerC_TickCooldowns = 0x140d1e9e0;
// void(ACrabPlayerC*): if AbilityCooldown is 0 and charges < max, starts one: Ability.Cooldown x the player's
// cooldown stats, at least 0.25 s.
constexpr uintptr_t ACrabPlayerC_StartAbilityCooldown = 0x140d1c530;

// Loot and damage
constexpr uintptr_t ACrabLM_BeginPlay = 0x140cf73d0;  // void(ACrabLM*): fills AllUnlockedPickups
// float(ACrabPS*): (DamageMultiplier + additive damage perks) * (1 + multiplier perks). Returns
// DamageMultiplier alone when the player has no pawn.
constexpr uintptr_t ACrabPS_GetDamageBuff = 0x140d23700;
// void(Source, uint8 LootPool, uint8 Rarity, TArray<ACrabPS*>* Players, int Choices, float, TArray<FCrabPickupInfo>* Out).
// For each of Choices slots it picks one of Players, reads what that player holds (ACrabPS_GetPickups), filters the
// candidates and picks one weighted by DA_SpawnWeight, read from each asset at roll time.
constexpr uintptr_t LootRoll = 0x140d33240;
constexpr uintptr_t WeightedPick = 0x140d32450;       // int(TArray<float>*): index by weight, -1 if none
constexpr uintptr_t ACrabPS_GetPickups = 0x140d253a0;  // TArray<UCrabPickupDA*>*(ACrabPS*, TArray* Out): copies PSPickups
constexpr uintptr_t ACrabChest_RollLoot = 0x140caf1c0; // void(ACrabChest*, ACrabPS* Opener): LootRoll for the opener
constexpr uintptr_t ACrabLM_SpawnPortals = 0x140d00210;  // void(ACrabLM*): rolls every portal's reward, then SetInfo
constexpr uintptr_t ACrabPortal_SetInfo = 0x140cb2cc0;   // void(ACrabPortal*, FCrabPortalInfo*), server only
// The Spiked island's portal description, a 77-character literal ("... costs 25% of your base max health to
// open"). The portal UI (0x140d5ed80, on every machine) copies exactly 78 wchar_t of it, NUL included, so a
// replacement must keep that length.
constexpr uintptr_t Text_SpikedChestDescription = 0x143256cc0;
constexpr int kSpikedChestDescriptionLen = 77;
// UEngine::GetWorldFromContextObject(GEngine, Object, 1 = LogAndReturnNull) -> UWorld*, as LootRoll does it.
constexpr uintptr_t GEngine = 0x1443fe458;  // UEngine*
constexpr uintptr_t UEngine_GetWorldFromContextObject = 0x142709ab0;
}  // namespace va

namespace off {
// UEngine / UGameViewportClient / UWorld
constexpr int Engine_GameViewport = 0x780;
constexpr int Viewport_World = 0x78;
constexpr int World_GameState = 0x120;
// UObject / AActor
constexpr int Object_Flags = 0x08;
constexpr int Object_Class = 0x10;
constexpr int Object_Name = 0x18;
constexpr int Object_Outer = 0x20;
constexpr int Class_DefaultObject = 0x118;  // UClass::ClassDefaultObject (UE 4.27); checked before use
constexpr int Actor_Owner = 0xE0;
constexpr int Actor_Role = 0xF0;  // ENetRole; 3 = ROLE_Authority
// AGameStateBase / ACrabGS
constexpr int GS_PlayerArray = 0x238;  // TArray<APlayerState*>
constexpr int GS_LM = 0x270;           // ACrabLM*, the level manager
constexpr int GS_CurrentIsland = 0x2D4;
constexpr int GS_CurrentIslandType = 0x2D8;
// ACrabLM (level manager)
constexpr int LM_SpawnablesDA = 0x388;
constexpr int LM_AllUnlockedPickups = 0x420;  // TArray<UCrabPickupDA*>: what chests and rewards roll from
// UCrabSpawnablesDA
constexpr int Spawnables_WeaponMods = 0x108;  // TArray<UCrabWeaponModDA*>
constexpr int Spawnables_AbilityMods = 0x118;  // TArray<UCrabAbilityModDA*>
constexpr int Spawnables_Perks = 0x138;       // TArray<UCrabPerkDA*>
constexpr int Spawnables_Relics = 0x148;      // TArray<UCrabRelicDA*>
constexpr int Spawnables_SpikedChest = 0x580;  // TSubclassOf<ACrabChest>: BP_Chest_Spiked (Cost 25, MaxHealth)
// APlayerState / ACrabPS
constexpr int PS_PlayerId = 0x224;
constexpr int PS_Pawn = 0x280;
constexpr int PS_PlayerName = 0x300;  // FString
constexpr int PS_Eliminations = 0x358;  // int: enemies this player has killed (replicated)
constexpr int PS_PSPickups = 0x340;  // TArray<UCrabPickupDA*>: every pickup held, what PickupTag checks look at
constexpr int PS_BaseMaxHealth = 0x398;  // float
constexpr int PS_DamageMultiplier = 0x3A0;

constexpr int PS_WeaponDA = 0x3C0;
constexpr int PS_AbilityDA = 0x3C8;
constexpr int PS_MeleeDA = 0x3D0;
constexpr int PS_NumWeaponModSlots = 0x3D8;
constexpr int PS_WeaponMods = 0x3E0;  // TArray<FCrabWeaponMod>
constexpr int PS_NumAbilityModSlots = 0x3F0;
constexpr int PS_AbilityMods = 0x3F8;
constexpr int PS_NumMeleeModSlots = 0x408;
constexpr int PS_MeleeMods = 0x410;
constexpr int PS_NumPerkSlots = 0x420;
constexpr int PS_Perks = 0x428;  // TArray<FCrabPerk>
constexpr int PS_Relics = 0x438;  // TArray<FCrabRelic>
constexpr int PS_Crystals = 0x470;
constexpr int PS_OnInventoryChanged = 0x520;  // native FMulticastDelegate
// Inventory items (FCrabWeaponMod, FCrabPerk, ...): 0x28 bytes, DA at +0, level (stack count) at +8
constexpr int Item_Size = 0x28;
constexpr int Item_Level = 0x08;
// APawn
constexpr int Pawn_PlayerState = 0x240;
// ACrabChest
constexpr int Chest_CurrencyType = 0x2B0;  // ECrabCurrencyType: Crystal 0, Key 1, Health 2, MaxHealth 3
constexpr int Chest_Cost = 0x2B4;          // int
constexpr int Chest_bIsChestOpened = 0x300;

// ACrabC (any character)
constexpr int C_HC = 0x4C0;             // UCrabHC*
constexpr int C_bIsEliminated = 0x668;  // bit 0
// ACrabPlayerC
constexpr int PlayerC_AbilityDA = 0x7D0;
constexpr int PlayerC_AbilityCooldown = 0xA10;    // float: seconds left, 0 = not counting down
constexpr int PlayerC_AbilityCharges = 0xA14;     // int: uses available now
constexpr int PlayerC_AbilityMaxCharges = 0xA18;  // int
// UCrabHC
constexpr int HC_OwningC = 0xB0;              // set when the component initialises
constexpr int HC_bDead = 0xD8;                // unreflected: set by TakeDamage when health reaches 0
constexpr int HC_bDeathProtectionUsed = 0xFB; // unreflected: one-time death protection consumed
constexpr int HC_CurrentHealth = 0x108;       // HealthInfo (0xFC) + 0xC
constexpr int HC_CurrentMaxHealth = 0x10C;
// UCrabPickupDA / UCrabInventoryDA and subclasses
constexpr int DA_bRequiresUnlock = 0x30;
constexpr int DA_Name = 0x38;         // FString
constexpr int DA_Description = 0x48;  // FString
constexpr int DA_Icon = 0x58;         // UTexture2D*; the rarity's background colour is part of the image
constexpr int DA_PickupType = 0x60;   // ECrabPickupType
constexpr int DA_Rarity = 0x62;       // ECrabRarity: 1 Common, 2 Epic, 3 Legendary
constexpr int DA_PickupTag = 0x63;
constexpr int DA_LootPool = 0x61;     // ECrabLootPool
// When set, loot rolls offer the pickup only if the player already holds a pickup with the same PickupTag
// (ACrabPS.PSPickups, PS+0x340). The roll (0x140d33240) reads it from the asset each time (0x140d34050,
// 0x140d3417f, 0x140d3439e).
constexpr int DA_bRequiresMatchingPickupTag = 0x64;
constexpr int DA_SpawnWeight = 0x68;  // float, relative chance in a loot roll
constexpr int DA_LevelDescription = 0xB8;  // FString; "PrimaryBuff" is replaced with the buff
constexpr int DA_BaseBuff = 0xC8;
constexpr int DA_bHyperbolicBuff = 0xCC;
constexpr int DA_bLimited = 0xCD;
constexpr int DA_bGetBuffAsMultiplier = 0xCE;  // BaseBuff is a percentage
constexpr int DA_BaseDebuff = 0xD0;
constexpr int DA_bGetDebuffAsMultiplier = 0xD5;
constexpr int DA_ItemType = 0xD8;  // per-category type enum (WeaponModType, PerkType, ...)
// FCrabDamageInfo
constexpr int DamageInfo_DamagedActor = 0x00;
constexpr int DamageInfo_Damage = 0x08;  // float
constexpr int DamageInfo_DamagingC = 0x40;  // ACrabC*
// FCrabPortalInfo
constexpr int PortalInfo_RewardLootPool = 0x19;
}  // namespace off

enum class IslandType : uint8_t { None = 0, Lobby = 1, NewRun = 2, ContinueRun = 3 };  // ECrabIslandType, 0..16

inline const char* IslandTypeName(uint8_t t) {
    static const char* n[] = {"None",     "Lobby",   "NewRun", "ContinueRun", "Arena",  "Horde",
                              "Demolition", "Gauntlet", "Parkour", "Shop",      "Elite",  "Boss",
                              "Biome",    "CrabIsland", "Loop",  "Arcade",      "Holdout"};
    return t < sizeof(n) / sizeof(*n) ? n[t] : "?";
}
constexpr uint8_t ROLE_Authority = 3;

namespace rarity {  // ECrabRarity
constexpr uint8_t Common = 1, Epic = 2, Legendary = 3, Greed = 4;
}
namespace tag {  // ECrabPickupTag
constexpr uint8_t None = 0, Healing = 1, DamageOverTime = 2, Critical = 3, Speed = 4, Bounce = 5, Ice = 6, Fire = 7,
                  Lightning = 8, Poison = 9, Arcane = 10, Turret = 11, Combo = 12, GlueShot = 13;
}
namespace pool {  // ECrabLootPool (also a portal's reward)
constexpr uint8_t None = 0, Damage = 1, Critical = 2, Elemental = 3, Speed = 4, Luck = 5, Health = 6, Economy = 7,
                  Skill = 8, Greed = 9, Upgrade = 10, Random = 11, Anvil = 12, Relic = 13, Spiked = 14;
}

// UObject flags (UE 4.27 EObjectFlags)
constexpr uint32_t RF_Public = 0x1;
constexpr uint32_t RF_Standalone = 0x2;
constexpr uint32_t RF_MarkAsRootSet = 0x80;
constexpr uint32_t RF_WasLoaded = 0x80000;  // makes the name stable for networking: replicated by path
constexpr uint32_t RF_ClassDefaultObject = 0x10;


// Item type ids (DA_ItemType)
constexpr uint8_t WeaponMod_StreakShot = 27;  // ECrabWeaponModType; the enum ends at DiceShot = 94
constexpr uint8_t WeaponMod_HeavyShot = 30;   // PickupTag Bounce: only drops once you hold Bouncing Shot
constexpr uint8_t Perk_Collector = 72;        // ECrabPerkType; the enum ends at FlashSale = 121
constexpr uint8_t AbilityMod_HeatSink = 11;   // ECrabAbilityModType
constexpr uint8_t Relic_RingOfDefense = 21;   // ECrabRelicType
constexpr uint8_t Relic_RingOfFury = 27;
constexpr uint8_t Relic_FireRing = 31;        // ECrabRelicType; the enum ends at RushedRing = 67

}  // namespace crab
