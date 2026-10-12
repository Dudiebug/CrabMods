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
// ServerDropPickup's implementation (host): takes the item out of the player's inventory and drops it as a pickup.
constexpr uintptr_t ACrabPlayerC_ServerDropPickup = 0x140d1ae30;  // void(ACrabPlayerC*, UCrabPickupDA*, FCrabInventoryInfo*)
// void(ACrabPS*, int Amount): every gain (enemy drops, salvage, ...) and cost; saturates, then broadcasts.
constexpr uintptr_t ACrabPS_AddCrystals = 0x140d20d00;
constexpr uintptr_t ACrabChest_StaticClass = 0x140d71550;
constexpr uintptr_t ACrabPS_AddWeaponMod = 0x140d210a0;       // void(ACrabPS*, FItem*)
constexpr uintptr_t ACrabPS_AddAbilityMod = 0x140d1fd60;
constexpr uintptr_t ACrabPS_AddMeleeMod = 0x140d20140;
constexpr uintptr_t ACrabPS_AddPerk = 0x140d20520;
// void(ACrabPS*, const FCrabRelic*): appends it (never merges: the same relic again is a second entry),
// broadcasts OnInventoryChanged and calls the owner's ClientOnPickedUpPickup(DA, 1).
constexpr uintptr_t ACrabPS_AddRelic = 0x140d20930;
constexpr uintptr_t ACrabPS_RemoveWeaponMod = 0x140d299d0;    // void(ACrabPS*, uint8 Type)
constexpr uintptr_t ACrabPS_RemoveAbilityMod = 0x140d29650;
constexpr uintptr_t ACrabPS_RemoveMeleeMod = 0x140d29730;
constexpr uintptr_t ACrabPS_RemovePerk = 0x140d29810;
constexpr uintptr_t ACrabPS_RemoveRelic = 0x140d298f0;
constexpr uintptr_t ACrabPS_BroadcastCrystals = 0x140d271e0;  // void(ACrabPS*): fires the crystals-changed delegate
// void(ACrabPS*, uint8 PerkType, float Amount): the held perk's AccumulatedBuff += Amount, then OnInventoryChanged.
constexpr uintptr_t ACrabPS_AddPerkAccumulatedBuff = 0x140d20f80;
constexpr uintptr_t ACrabPS_UpdateMaxHealth = 0x140d28a30;    // void(ACrabPS*): recomputes pawn max health from items
constexpr uintptr_t ACrabPS_RestoreAutoSave = 0x140d28ee0;    // void(ACrabPS*, FCrabAutoSave*)
constexpr uintptr_t FCrabInventoryInfo_CopyConstruct = 0x140cc7220;  // FInvInfo*(FInvInfo* Dst, const FInvInfo* Src)
constexpr uintptr_t TArray28_ResizeGrow = 0x140840fd0;        // void(TArray<0x28-byte elem>*), Num already bumped

// Health (UCrabHC, the character's health component)
constexpr uintptr_t UCrabHC_TakeDamage = 0x140c9afe0;        // void(UCrabHC*, FCrabDamageInfo*): armor, protections, elimination
constexpr uintptr_t UCrabHC_OnRep_HealthInfo = 0x140c9b4a0;  // void(UCrabHC*): broadcasts health changed, Previous = Current
constexpr uintptr_t ACrabEnemyC_OnRep_EnemyBuff = 0x140ca7b50;  // void(ACrabEnemyC*): the buff's health-bar text and shield look
// void(UCrabHC*, float Amount): Current = min(Current + Amount, Max), then OnRep_HealthInfo. Every heal ends here
// (host only); perk, relic and Vampire heals are cut to the missing health first, by ClampHealToMissing.
constexpr uintptr_t UCrabHC_Heal = 0x140c9acc0;
// float(ACrabC*, float Requested): Current >= Max ? 0 : min(Requested, Max - Current). Callers heal only if > 0.
constexpr uintptr_t ACrabC_ClampHealToMissing = 0x140d14930;
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
// ACrabPortal::CanInteract (vtable slot): bool(ACrabPortal*, ACrabC*). Refuses the Arcade and Holdout portals when
// GS PlayerArray has more than 1 player, then defers to ACrabInteractable::CanInteract (alive + interact
// cooldown). Checked by the server (ServerInteract) and by the interacting player's own machine.
constexpr uintptr_t ACrabPortal_VTable = 0x143234668;
constexpr int ACrabPortal_CanInteract_Slot = 0x650;
constexpr uintptr_t ACrabInteractable_CanInteract = 0x140cb19c0;
// Picks a map for an island (0x140cfad90 calls it for Arena, Horde, Parkour and Shop, host):
// FCrabIsland* (ACrabLM*, FCrabIsland* out, const TArray<FCrabIsland>* islands, TArray<FName>* recent). FCrabIsland is
// {FName map, float weight}, 12 bytes. For Parkour, islands is the biome DA's ParkourIslands and recent is
// LM_RecentParkour. The current game's biomes list no Parkour maps.
constexpr uintptr_t ACrabLM_PickIsland = 0x140cfa220;
constexpr uintptr_t ACrabLM_SpawnPortalsSoon = 0x140cf9280;  // void(ACrabLM*): a short timer, then SpawnPortals
// Island-end rewards (0x140d01610, host): for each player it fills a TArray<UClass*> of reward chests in its own
// frame (Data at caller rsp+0x40, Num +0x48, Max +0x4C, kept in r12 / esi), adds bonus chests, Care Package and
// totem chests, then asks whether the player has High Roller (0x3F) before rolling that relic's +2 / nothing /
// none. Return address of that call:
constexpr uintptr_t Ret_RewardChestsHighRoller = 0x140d029b0;
constexpr uintptr_t ACrabPS_HasRelic = 0x140d23bb0;     // bool(ACrabPS*, uint8 RelicType)
constexpr uintptr_t ACrabPS_ShowMessage = 0x140d26f40;  // void(ACrabPS*, const FString*): on that player's screen

// Weapon fire, on the machine that controls the player (the owning client, or the host for its own crab). Its
// projectiles deal the damage; everyone else gets cosmetic copies with the same start and direction.
// void(ACrabWeapon*, const TArray<FVector>* Formation, UCrabProjectileDA*, uint8 LesserType): one shot of every
// projectile in the formation. Reads WeaponDA ShotSpawnType (+0x104) each time.
constexpr uintptr_t ACrabWeapon_FireProjectiles = 0x140d6d280;
// ACrabProjectile*(FCrabProjectileInfo* Info, AActor* Owner, FVector* StartLoc, bool bSendToServer): takes a pooled
// projectile, places it at StartLoc flying along Info->Dir, and sends the cosmetic copy. Return address of the call
// in FireProjectiles:
constexpr uintptr_t SpawnProjectile = 0x140d37220;
constexpr uintptr_t Ret_FireProjectilesSpawn = 0x140d6dcfd;
// void(ACrabPlayerC*, FHitResult* Out, float Range): line trace from the camera along its view.
constexpr uintptr_t ACrabPlayerC_CameraTrace = 0x140d130c0;
// void(UObject* Context, FVector* InOut): moves the point down onto the ground below it (WorldStatic), if any.
constexpr uintptr_t SnapToGround = 0x140d30df0;

// Destructibles (ACrabDestructible: pots, barrels, rocks, crystals, nests, landmines, ...), host only.
// void(ACrabDestructible*, FCrabDamageInfo*, uint8 HitType): its health component was hit; HitType 5 (Elimination)
// breaks it (Explode). The player who broke it is Info->DamagingC if a player, else the last player who hit it.
constexpr uintptr_t ACrabDestructible_OnHCDamaged = 0x140cc64a0;
constexpr uintptr_t ACrabDestructible_StaticClass = 0x140d73b50;

// Going down (host): void(ACrabPC*), inside the killing hit, before the player's character is swapped for a
// spectator. The PC's pawn (+0x250) is still the downed ACrabPlayerC, with its player state.
constexpr uintptr_t ACrabPC_SpawnSpectator = 0x140d0e720;
// void(ACrabPC*, const FString* Message): a ServerSendChatMessage RPC arriving at the host, which hands it to
// ACrabGM::BroadcastChatMessage (0x140ce56e0) for every player's chat.
constexpr uintptr_t ACrabPC_ServerSendChatMessage_Impl = 0x140d0d1a0;

// Explosions (host). void(UObject* WorldCtx, TArray<AActor*> Ignore /*by value: the callee frees Data*/,
// const FVector* Origin, float Radius, const FCrabDamageInfo* Info, bool bRequireLineOfSight, AActor* Filter):
// flat damage to everything in the sphere that may be hurt by Info->DamagingC, falling off to 20% at the edge.
// The radius grows with DamagingC's Magnify (perk 39) inside it, as for the game's own explosions.
constexpr uintptr_t RadialDamage = 0x140d2dd20;
constexpr uintptr_t ACrabC_GetCoreLocation = 0x140cbae30;           // FVector*(ACrabC*, FVector* out)
constexpr uintptr_t ACrabLM_MulticastPlayOneShotFX = 0x140d79d00;   // void(ACrabLM*, FX, Sound, const FVector*): everyone
constexpr uintptr_t ACrabPS_GetPerkBuff = 0x140d254b0;  // float(ACrabPS*, uint8 PerkType): Magnify (39) 0.2 a level
// Return addresses of the RadialDamage calls of the game's on-kill and on-hit explosions. In EnemyElim (source: the
// dying enemy, whose killer is its EliminatingC): Exploding (perk 78), Freezing (88), Flammable (89), Electric (90)
// and Poisonous (91) Enemies. In the player-hit handler 0x140ca7fe0 (source: the hit player's character): Explosive
// (perk 31), Poisonous (32) and Flammable (59) Armor.
constexpr uintptr_t Ret_RadialExplodingEnemies = 0x140ca4569;
constexpr uintptr_t Ret_RadialFreezingEnemies = 0x140ca4823;
constexpr uintptr_t Ret_RadialFlammableEnemies = 0x140ca496b;
constexpr uintptr_t Ret_RadialElectricEnemies = 0x140ca4ab2;
constexpr uintptr_t Ret_RadialPoisonousEnemies = 0x140ca4bf3;
constexpr uintptr_t Ret_RadialExplosiveArmor = 0x140ca88df;
constexpr uintptr_t Ret_RadialPoisonousArmor = 0x140ca89df;
constexpr uintptr_t Ret_RadialFlammableArmor = 0x140ca8adf;

// Outgoing damage (host). void(ACrabPS*, FCrabDamageInfo*): the player's damage buffs, crits (adds DamageTag 1) and
// the elemental perks' factors on the hit's debuff stacks. Only called from ACrabPlayerC::ModifyOutgoingDamage,
// which UCrabHC::TakeDamage calls for hits by players.
constexpr uintptr_t ACrabPS_CalcOutgoingDamage = 0x140d21480;
// An enemy dies (host; virtual, void(ACrabEnemyC*)): its crystals, then the killer's on-kill perks.
constexpr uintptr_t ACrabEnemyC_OnEliminated = 0x140ca4020;
// void(ACrabCrystalPickup*, int Amount, int Bonus, uint8 Tier, ACrabPS*, bool): sets up a crystal drop; collecting it
// shares Amount + Bonus between the players. Return address of the call for an enemy's crystals:
constexpr uintptr_t ACrabCrystalPickup_Init = 0x140d3b0b0;
constexpr uintptr_t Ret_CrystalInitEnemy = 0x140ca43ff;
// Grave Dodger (perk 63) saving a player (host): void(ACrabPlayerC*), the heal, after TakeDamage marks the
// protection used (HC_bDeathProtectionUsed). Called through the vtable only.
constexpr uintptr_t ACrabPlayerC_GraveDodgerHeal = 0x140d167d0;
// Totem chances (host): bool(float Percent, float Multiplier) = random 0..100 < Percent x Multiplier. Only called by
// ACrabTotem::Interact, for the Health totem and the Chance/Gamble totems; true is the good outcome.
constexpr uintptr_t TotemRoll = 0x140d32ab0;
constexpr uintptr_t Ret_TotemRollHealth = 0x140cb5cdf;
constexpr uintptr_t Ret_TotemRollChance = 0x140cb63b2;
// void(ACrabWeapon*): ammo changed; tells the HUD (the controlling machine's).
constexpr uintptr_t ACrabWeapon_OnAmmoChanged = 0x140d6eb20;

// Harvest areas (ACrabHarvestArea; the revive bubble is BP_HarvestArea_Revival, SpawnablesDA +0x4C8). The host
// spawns one where a player goes down; on a timer (0x140cf50f0) it looks for players within its Radius and
// counts down while one stands in it. Its effect's size is the Niagara float "Radius", set from it once
// (virtual 0x530); UNiagaraComponent::SetVariableFloat sets it again.
constexpr uintptr_t ACrabHarvestArea_StaticClass = 0x140d775a0;
constexpr uintptr_t NiagaraComponent_SetVariableFloat = 0x140845d30;  // void(UNiagaraComponent*, FName, float)
constexpr const wchar_t* GameplayStaticsPath = L"/Script/Engine.Default__GameplayStatics";

// Lobby key totem. Server: ACrabTotem::Interact(totem, ACrabC*) spends the keys and calls the
// ClientOnInteractedWithKeyTotem RPC; on that player's machine the game picks a locked item, unlocks it in the
// save and asks the server (ServerSpawnKeyTotemPickup RPC) to spawn it in front of the totem. The server spawns
// whatever it is asked for.
constexpr uintptr_t ACrabTotem_Interact = 0x140cb5860;
// AActor::ProcessEvent (an actor's virtual 0x220): every blueprint / RPC call on an actor, e.g. the player
// character's ServerInteract(ACrabInteractable*) when it is sent.
constexpr uintptr_t AActor_ProcessEvent = 0x14208b200;
constexpr uintptr_t ACrabPC_ClientOnInteractedWithKeyTotem_Impl = 0x140d095f0;  // void(ACrabPC*, ACrabTotem*)
constexpr uintptr_t ACrabPC_ServerSpawnKeyTotemPickup = 0x140d7bb70;  // RPC: void(ACrabPC*, ACrabTotem*, DA*)
constexpr uintptr_t ACrabPC_VTable = 0x143244480;
constexpr int ACrabPC_ServerSpawnKeyTotemPickup_Slot = 0xE60;  // its implementation (too short for a hook)
constexpr uintptr_t UCrabGI_UnlockPickup = 0x140ce45e0;           // void(UCrabGI*, UCrabPickupDA*): adds and saves
constexpr uintptr_t UCrabGI_StaticClass = 0x140d76320;
// Pickups. ACrabLM::SpawnPickup: ACrabInteractPickup*(ACrabLM*, const FCrabPickupInfo*, FVector* loc /*put on
// the ground*/). Interact: void(pickup, ACrabC*) when a player takes it (server). Deactivate: void(pickup).
constexpr uintptr_t ACrabLM_SpawnPickup = 0x140d00060;
constexpr uintptr_t ACrabInteractPickup_Interact = 0x140d3cf00;
constexpr uintptr_t ACrabInteractPickup_Deactivate = 0x140d3c0b0;
// Fills the pickup's card (UI at Pickup_UI) from its PickupInfo: void(pickup). A virtual (slot 0x638) run when the
// pickup appears and from OnRep_PickupInfo, so on every machine.
constexpr uintptr_t ACrabInteractPickup_SetupUI = 0x140d3c800;

// Buff lookups. InvArr_GetBuff: float(TArray<FCrabPerk/...>* Inventory, uint8 Type), the held item's buff (0 if not
// held); 106 callers, told apart by return address. ACrabPS_GetWeaponModBuff: float(ACrabPS*, uint8 Type), the same
// for PS.WeaponMods (add rcx, 0x3E0; jmp).
constexpr uintptr_t InvArr_GetBuff = 0x140d2df60;
constexpr uintptr_t ACrabPS_GetWeaponModBuff = 0x140d25fb0;
// ACrabPS::UpdateMaxHealth adds GetBuff(Perks, Banana) to the max health multiplier; return address of that call.
constexpr uintptr_t Ret_MaxHealthMultiplierBanana = 0x140d28ad3;
// void(ACrabWeapon*): FireRate (+0x2C0) = 1 - (debuffs) + GetWeaponModBuff(RapidFire) + ..., then the shot interval.
// Called when the inventory changes, on every machine. Return address of the Rapid Fire lookup:
constexpr uintptr_t ACrabWeapon_UpdateFireRate = 0x140d69f70;
constexpr uintptr_t Ret_FireRateRapidFire = 0x140d69fdc;

// The Spiked island's portal description, a 77-character literal ("... costs 25% of your base max health to
// open"). The portal UI (0x140d5ed80, on every machine) copies exactly 78 wchar_t of it, NUL included, so a
// replacement must keep that length.
constexpr uintptr_t Text_SpikedChestDescription = 0x143256cc0;
constexpr int kSpikedChestDescriptionLen = 77;
// UEngine::GetWorldFromContextObject(GEngine, Object, 1 = LogAndReturnNull) -> UWorld*, as LootRoll does it.
constexpr uintptr_t GEngine = 0x1443fe458;  // UEngine*
constexpr uintptr_t UEngine_GetWorldFromContextObject = 0x142709ab0;

// --- UMG (widgets), all static. Most widget work goes through ProcessEvent with the function found by name:
// UClass::FindFunctionByName(UClass*, FName, int IncludeSuper) -> UFunction* or null (never fatal).
constexpr uintptr_t UClass_FindFunctionByName = 0x140f66a80;
// StaticFindObject(UClass* or null, UObject* Outer (null with a full path), const wchar_t* Name, bool Exact).
constexpr uintptr_t StaticFindObject = 0x1410d96f0;
// CreateWidget(APlayerController* Owner, UClass* WidgetClass, FName Name) -> UUserWidget*, initialized.
constexpr uintptr_t CreateWidget = 0x141e20e60;
// Binds a dynamic multicast delegate: void(FMulticastScriptDelegate*, UObject* Target, void* unused, FName
// Function). Function must exist on Target's class; adding the same pair twice does nothing.
constexpr uintptr_t AddUniqueDynamic = 0x140c9f390;
// UUserWidget::CancelLatentActions' exec thunk, void(UObject* Context, FFrame*, void* Result): CrabMods binds
// button clicks to it on its own widgets (the hook tells its widgets from the game's).
constexpr uintptr_t UUserWidget_execCancelLatentActions = 0x141ea93f0;
constexpr uintptr_t UButton_StaticClass = 0x141e8c4a0;
constexpr uintptr_t UBackgroundBlur_StaticClass = 0x141e8af80;
constexpr uintptr_t UUniformGridPanel_StaticClass = 0x141ea55c0;
constexpr uintptr_t UScrollBox_StaticClass = 0x141e9cfe0;
constexpr uintptr_t USizeBox_StaticClass = 0x141e9bc90;
constexpr uintptr_t UWrapBox_StaticClass = 0x141eb7500;
constexpr uintptr_t UHorizontalBox_StaticClass = 0x141e93e10;
constexpr uintptr_t UTextBlock_StaticClass = 0x141ea1930;
constexpr uintptr_t UImage_StaticClass = 0x141e93a50;
constexpr uintptr_t UEditableTextBox_StaticClass = 0x141e8f4c0;
// UKismetTextLibrary's default object: Conv_StringToText / Conv_TextToString (FText from and to FString).
constexpr const wchar_t* KismetTextLibraryPath = L"/Script/Engine.Default__KismetTextLibrary";
// The game's UI (UCrabUI and its menus, UCrabFocusMenuUI)
constexpr uintptr_t UI_SetTextFromString = 0x140d602f0;  // void(unused, UTextBlock*, const FString*)
constexpr uintptr_t UI_SetShown = 0x140d60400;           // void(UWidget*, bool): Visible or Hidden
// Player controller menus. The open menu is ACrabPC+0x718 (off::PC_OpenMenu); input follows it.
constexpr uintptr_t ACrabPC_UpdateInputMode = 0x140d0a510;  // void(ACrabPC*): UI input + cursor while a menu is open
constexpr uintptr_t ACrabPC_CloseMenuRestoreGame = 0x140d0d000;  // void(ACrabPC*): game input, OpenMenu = null, HUD shown
// UCrabConfirmationPromptUI ("ARE YOU SURE?" with YES / NO), its blueprint and its virtual table. Focus menus'
// slots: 0x478 init (owner, focus button), 0x480 refresh (key texts, gamepad focus), 0x4a0 press sound,
// 0x4c0 bool CanClose (Escape closes the menu only if true).
constexpr uintptr_t UCrabConfirmationPromptUI_StaticClass = 0x140d72160;
constexpr const wchar_t* ConfirmationPromptClassPath = L"/Game/UI/Menu/UI_ConfirmationPrompt.UI_ConfirmationPrompt_C";
constexpr uintptr_t UCrabConfirmationPromptUI_VTable = 0x1432517d8;
constexpr int FocusMenu_Init_Slot = 0x478;
constexpr int FocusMenu_Refresh_Slot = 0x480;
constexpr int FocusMenu_PressSound_Slot = 0x4a0;
constexpr int FocusMenu_CanClose_Slot = 0x4c0;
// 0x4a8 / 0x4b0: next / previous submenu (the player's Next / Previous Submenu keys, via ACrabPC+0x718).
constexpr int FocusMenu_NextSubmenu_Slot = 0x4a8;
constexpr int FocusMenu_PreviousSubmenu_Slot = 0x4b0;
// The pickup card shown over a pickup (UCrabInteractUI; class at DA+0x90). Setters as the pickup uses them.
constexpr uintptr_t InteractUI_SetIcon = 0x140d59310;     // void(UI, UTexture2D*)
constexpr uintptr_t InteractUI_SetType = 0x140d59480;     // void(UI, const FString*)
constexpr uintptr_t InteractUI_SetName = 0x140d59420;     // void(UI, const FString*)
constexpr uintptr_t InteractUI_SetDescription = 0x140d592d0;  // void(UI, const FString*)
constexpr uintptr_t InteractUI_SetRarityColor = 0x140d59430;  // void(UI, const FLinearColor*)
constexpr uintptr_t InteractUI_SetLootPool = 0x140d59390;     // void(UI, uint8 LootPool)
constexpr uintptr_t PickupDA_GetRarityColor = 0x140d3f810;    // FLinearColor*(DA, FLinearColor* Out)
// Inventory menu (UCrabInventoryUI). Refresh (its virtual 0x480) lays out every grid from the player's slot
// counts and items: slot widget i of a grid shows item i (missing widgets are skipped), widgets past the slot
// count show locked. Its init (0x478) collected every slot's button for gamepad focus; a slot widget's own
// init (its virtual 0x478) binds its hover.
constexpr uintptr_t UCrabInventoryUI_Refresh = 0x140d5d9a0;
constexpr uintptr_t UCrabInventorySlotUI_StaticClass = 0x140d78bd0;
constexpr int InventorySlotUI_Init_Slot = 0x478;
// Inventory slot counts (ACrabPS, host): a new run sets 24 / 12 / 12 / 24, a challenge run sets 8 for all.
constexpr uintptr_t ACrabPS_ResetForNewRun = 0x140d29e50;   // void(ACrabPS*): items cleared, slot counts reset
constexpr uintptr_t ACrabPS_ApplyChallengeSlots = 0x140d28d10;  // void(ACrabPS*): slot counts 8 in a challenge
// Relics have no slot count: the game allows 10 (only a pickup's check and the menu know it).
constexpr uintptr_t ACrabPS_CanPickupRelic = 0x140d22c30;  // bool(ACrabPS*, uint8 RelicType): not held && Num < 10
constexpr uintptr_t UCrabInventoryUI_SetupGrid = 0x140d5e170;  // void(UI*, grid, uint8 count): slot i > count locked
// void(UCrabInventorySlotUI*, UCrabPickupDA*, const FCrabInventoryInfo*): shows that item in the slot (Refresh
// calls it for item i of a grid, after SetupGrid).
constexpr uintptr_t InventorySlotUI_Set = 0x140d59830;
// void(UCrabInventoryUI*, UCrabPickupDA*, float AccumulatedBuff): the details' "Current damage bonus" / "Total
// health healed" line (+0x488, its row +0x480), shown only for the game's own list of items.
constexpr uintptr_t UCrabInventoryUI_ShowAccumulatedBuff = 0x140d5d060;
// TArray<FCrabRelic>*(ACrabPS*, TArray<FCrabRelic>* Out): a copy of the relics (Refresh lays out the relic grid
// from it, then frees it).
constexpr uintptr_t ACrabPS_GetRelics = 0x140d25a60;
// The pickup message on the player's own machine (a relic's, from ACrabPS::AddRelic).
constexpr uintptr_t ACrabPC_StaticClass = 0x140d7af40;
constexpr uintptr_t ACrabPC_ClientOnPickedUpPickup = 0x140d7b720;  // void(ACrabPC*, UCrabPickupDA*, uint8 Level)
constexpr uintptr_t UCrabMeleeModDA_StaticClass = 0x140d7a240;

// Necromancer (Soul Whip, undead), host unless noted
// AActor*(ACrabLM*, UClass* EnemyClass, FVector* Loc, FRotator* Rot, bool bCountsForIsland): finds a free spot near
// Loc, spawns the enemy 250 above it and scales it for this island (0x140ca5ca0). true also counts it towards the
// island's end (LM+0x514) and binds its death; false leaves it out of both.
constexpr uintptr_t ACrabLM_SpawnEnemy = 0x140cfe3b0;
// bool(const FCrabDamageInfo*, ACrabC* Victim): may this hit hurt Victim. Teams are "has a player state" or not, so
// enemies never hurt enemies.
constexpr uintptr_t DamageInfo_CanDamage = 0x140d2e6a0;
// void(ACrabEnemyAIC*): picks the nearest player character as BestEnemyC (when it has none alive), then focuses it.
constexpr uintptr_t ACrabEnemyAIC_SelectTarget = 0x140ca0570;
constexpr uintptr_t ACrabEnemyC_SetBestEnemy = 0x140caa5b0;  // void(ACrabEnemyC*, ACrabC*): the replicated +0x800
constexpr uintptr_t ACrabEnemyAIC_FocusBestEnemy = 0x140ca1bd0;  // void(ACrabEnemyAIC*)
// void(ACrabPlayerC*, const FVector_NetQuantize*): the ServerAbility RPC arriving at the host.
constexpr uintptr_t ACrabPlayerC_ServerAbility_Impl = 0x140d1a430;  // only calls MulticastAbility (others' telegraph)
// void(ACrabPlayerC*): the "Ability" input action on the owning machine (any key): spends a charge, starts the
// cooldown, plays the telegraph, sends ServerAbility and sets the timer that spawns the ability (0x140d1bd10).
constexpr uintptr_t ACrabPlayerC_AbilityPressed = 0x140d10c00;
// void(UCrabGI*): builds the lobby's weapon, ability and melee lists from SpawnablesDA (once, guarded by GI+0x248).
constexpr uintptr_t UCrabGI_BuildLoadoutLists = 0x140cda5c0;
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
constexpr int Actor_RootComponent = 0x130;     // USceneComponent*
constexpr int SceneComponent_Location = 0x1D0;  // FVector, world (ComponentToWorld translation)
constexpr int Character_Capsule = 0x290;       // UCapsuleComponent*
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
// FCrabEnemyStats.HealthMultiplier (host): set for each wave; a spawned enemy's max health is its HC's
// BaseMaxHealth times this (and a few buffs).
constexpr int LM_EnemyHealthMultiplier = 0x4F0;
// UCrabSpawnablesDA
constexpr int Spawnables_WeaponMods = 0x108;  // TArray<UCrabWeaponModDA*>
constexpr int Spawnables_AbilityMods = 0x118;  // TArray<UCrabAbilityModDA*>
constexpr int Spawnables_MeleeMods = 0x128;    // TArray<UCrabMeleeModDA*>
constexpr int Spawnables_Perks = 0x138;       // TArray<UCrabPerkDA*>
constexpr int Spawnables_Relics = 0x148;      // TArray<UCrabRelicDA*>
constexpr int Spawnables_SpikedChest = 0x580;  // TSubclassOf<ACrabChest>: BP_Chest_Spiked (Cost 25, MaxHealth)
// The portal classes SpawnPortals picks from (TSubclassOf<ACrabPortal>; the portal class sets its island type).
// Demolition is offered on islands 1 to 3 only; nothing offers Parkour (BP_Portal_Parkour_C).
constexpr int Spawnables_DemolitionPortal = 0x638;
constexpr int Spawnables_ParkourPortal = 0x648;
constexpr int LM_BiomeDA = 0x358;          // UCrabBiomeDA*: the run's current biome
constexpr int LM_RecentParkour = 0x450;    // TArray<FName>: Parkour maps played lately
constexpr int Biome_ParkourIslands = 0x58;  // TArray<FCrabIsland>: empty in the current game
constexpr int World_StreamingLevels = 0x88;  // TArray<ULevelStreaming*>
constexpr int LevelStreaming_PackageNameToLoad = 0x50;  // FName
constexpr int LevelStreamingDynamic_Initially = 0x148;  // bInitiallyLoaded 0x01, bInitiallyVisible 0x02
// APlayerState / ACrabPS
constexpr int PS_PlayerId = 0x224;
constexpr int PS_Pawn = 0x280;
constexpr int PS_PlayerName = 0x300;  // FString
constexpr int PS_Eliminations = 0x358;  // int: enemies this player has killed (replicated)
// Combo level (float): 1, then 2/3/4/5 at 25/75/175/325 ComboCounter (+0x350, int); 6/7/8 with Combo Ring.
constexpr int PS_Combo = 0x354;
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
constexpr int Item_AccumulatedBuff = 0x20;  // float (FCrabInventoryInfo+0x18): replicated and kept across islands
// APawn
constexpr int Pawn_PlayerState = 0x240;
constexpr int HarvestArea_NC = 0x220;              // UNiagaraComponent*
constexpr int HarvestArea_Type = 0x238;            // uint8 ECrabHarvestAreaType: 2 Revival
constexpr int HarvestArea_Radius = 0x23C;          // float, 1500 unless the blueprint sets it
constexpr int HarvestArea_PlayerToRevive = 0x298;  // ACrabPS*, replicated
// ACrabChest
constexpr int Chest_CurrencyType = 0x2B0;  // ECrabCurrencyType: Crystal 0, Key 1, Health 2, MaxHealth 3
constexpr int Chest_Cost = 0x2B4;          // int
constexpr int Chest_bIsChestOpened = 0x300;
// TArray<FCrabPickupInfo>: ACrabChest::RollLoot has LootRoll fill it for the opener, then spawns them
constexpr int Chest_PickupsToSpawn = 0x308;

// ACrabC (any character)
constexpr int C_HC = 0x4C0;             // UCrabHC*
constexpr int HC_BaseMaxHealth = 0xC0;  // float, the class default's
// ACrabDestructible
constexpr int Destructible_LastPlayer = 0x340;  // ACrabC*: the last player who damaged it
constexpr int Destructible_bExploded = 0x398;
constexpr int Destructible_HC = 0x238;                    // UCrabHC*
constexpr int Destructible_NestEnemies = 0x350;           // TArray: a nest's enemies, spawned when it breaks
constexpr int Destructible_ChallengeEnemies = 0x3D0;      // TArray: a spawner's (Resin Hornet nest, challenge crystal)
// Player-spawned destructibles (UCrabSpawnablesDA classes): Landmine, HomingBarrel, SporeMushroom, UltraMushroom
constexpr int Spawnables_PlayerDestructibles = 0x270;
constexpr int Spawnables_NumPlayerDestructibles = 4;
// AGameStateBase match state (ECrabMatchState): 3 = StartedIsland (fighting), 4 = ClearedIsland
constexpr int GS_MatchState = 0x278;
// ACrabWeapon
constexpr int Weapon_OwningPS = 0x240;
constexpr int Weapon_WeaponDA = 0x260;
constexpr int Weapon_OwningC = 0x268;
constexpr int Weapon_OwningPC = 0x270;  // only on the machine that controls the player
constexpr int Weapon_Spread = 0x2A0;    // float, degrees
// Ammo is not replicated: the controlling machine counts it. A finished reload sets Ammo = ClipSize.
constexpr int Weapon_ClipSize = 0x2CC;     // int, computed from the DA's BaseClipSize and mods
constexpr int Weapon_Ammo = 0x2F0;         // int
constexpr int Weapon_bIsReloading = 0x370;
constexpr int WeaponDA_bInfiniteClipSize = 0x14C;
constexpr int WeaponDA_ShotSpawnType = 0x104;  // 0 Muzzle, 1 AboveCrosshair (Ice Staff: falls from 2500 above the aim)
// FCrabProjectileInfo
constexpr int ProjInfo_Dir = 0x08;  // FVector
// FHitResult (0x88)
constexpr int Hit_Size = 0x88;
constexpr int Hit_Time = 0x04;
constexpr int Hit_ImpactPoint = 0x18;
constexpr int Hit_TraceEnd = 0x48;
constexpr int Hit_bBlockingHit = 0x5D;  // bit 0
constexpr int C_Weapons = 0x538;        // TArray<ACrabWeapon*>
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
constexpr int HC_ArmorPlates = 0xFC;          // HealthInfo.CurrentArmorPlates, int
constexpr int HC_ArmorPlateHealth = 0x100;    // float
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
constexpr int DA_Tint = 0x6C;         // FLinearColor: the pickup's glow
constexpr int DA_PrimaryFX = 0x80;    // UNiagaraSystem*: a weapon's pickup shows its model through this
constexpr int DA_LevelDescription = 0xB8;  // FString; "PrimaryBuff" is replaced with the buff
constexpr int DA_BaseBuff = 0xC8;
constexpr int DA_bHyperbolicBuff = 0xCC;
constexpr int DA_bLimited = 0xCD;
constexpr int DA_bGetBuffAsMultiplier = 0xCE;  // BaseBuff is a percentage
constexpr int DA_BaseDebuff = 0xD0;
constexpr int DA_bGetDebuffAsMultiplier = 0xD5;
constexpr int DA_ItemType = 0xD8;  // per-category type enum (WeaponModType, PerkType, ...)
constexpr int Controller_Pawn = 0x250;
constexpr int C_LM = 0x518;
// UCrabSpawnablesDA: the explosion effect and sound Explosive Armor uses
constexpr int Spawnables_GenericExplosionFX = 0x88;
constexpr int Spawnables_GenericExplosionSound = 0x90;
// ACrabTotem
constexpr int Totem_Type = 0x298;                  // ECrabTotemType: 3 = Key
constexpr int Totem_UniqueIDInteractions = 0x2D8;  // TArray<FString>: who has used it
constexpr int Pawn_Controller = 0x258;
constexpr int World_GameInstance = 0x180;
constexpr int GI_SettingsSG = 0x240;              // UCrabSettingsSG*: the settings menu's values
constexpr int SettingsSG_MasterVolume = 0x138;    // float
constexpr int SettingsSG_GameplayVolume = 0x13C;  // float: sound effects
constexpr int GI_SaveGame = 0x228;  // UCrabSG*
constexpr int SceneComponent_Rotation = 0x1C0;  // FQuat of ComponentToWorld (x, y, z, w)
constexpr int SceneComponent_Scale = 0x1E0;     // FVector of ComponentToWorld
constexpr int Pickup_NumTimesPickedUp = 0x300;  // uint8
// UCrabSpawnablesDA lists and the save's unlocked lists (UCrabSG), in the same order: weapons, abilities,
// melee weapons, weapon mods, ability mods, melee mods, perks, relics. An item is locked when it is in the
// first and not the second.
constexpr int Spawnables_Unlockables[8] = {0xD8, 0xE8, 0xF8, 0x108, 0x118, 0x128, 0x138, 0x148};
constexpr int SG_Unlocked[8] = {0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x100, 0x110, 0x120};
// FCrabPickupInfo (0x50 bytes)
constexpr int Pickup_UI = 0x238;          // ACrabInteractable.InteractUI (UCrabInteractUI*)
constexpr int Pickup_PickupInfo = 0x290;  // ACrabInteractPickup.PickupInfo
constexpr int PickupInfo_Size = 0x50;
constexpr int InteractPickup_PickupInfo = 0x290;     // ACrabInteractPickup: FCrabPickupInfo (Net)
constexpr int InteractPickup_bDeactivated = 0x301;  // bool: picked up, waiting to come back
constexpr int StaticMeshActor_Component = 0x220;     // AStaticMeshActor::StaticMeshComponent
constexpr int StaticMeshComponent_StaticMesh = 0x488;
constexpr int PickupInfo_DA = 0x00;
constexpr int PickupInfo_Level = 0x08;
constexpr int PickupInfo_MaxPickups = 0x28;
constexpr int PickupInfo_SalvageReward = 0x30;
constexpr int PickupInfo_OwningPS = 0x38;  // only this player may take it
constexpr int PickupInfo_CosmeticSpawnLoc = 0x40;  // where it is seen flying from
// FCrabDamageInfo (0x48 bytes)
constexpr int DamageInfo_Size = 0x48;
constexpr int DamageInfo_Type = 0x0C;       // uint8: 1 Radial, 3 Melee, 4 Lesser (not throttled per enemy)
constexpr int DamageInfo_Location = 0x30;   // FVector
constexpr int DamageInfo_Knockback = 0x3C;  // float
// FCrabDamageInfo
constexpr int DamageInfo_DamagedActor = 0x00;
constexpr int DamageInfo_Damage = 0x08;  // float
constexpr int DamageInfo_DamagingC = 0x40;  // ACrabC*
constexpr int DamageInfo_Tags = 0x10;     // TArray<uint8> ECrabDamageTag: 1 = Critical
constexpr int DamageInfo_Debuffs = 0x20;  // TArray<FCrabDebuff>: 8 bytes, uint8 Type (1 Ice .. 5 Arcane), int32 Stacks at +4
// ACrabEnemyC
constexpr int Enemy_EliminatingC = 0x930;  // ACrabC*: who killed it
// FCrabPortalInfo
constexpr int PortalInfo_RewardLootPool = 0x19;
constexpr int Portal_IslandType = 0x290;  // ACrabPortal.IslandType (ECrabIslandType)
// UMG and the game's UI objects
constexpr int Widget_Slot = 0x28;            // UPanelSlot*
constexpr int UserWidget_WidgetTree = 0x1D8;
constexpr int Button_WidgetStyle = 0x128;   // FButtonStyle (SetStyle's parameter)
constexpr int Button_OnClicked = 0x3C8;     // FMulticastScriptDelegate
// FButtonStyle (0x278): its Normal / Hovered / Pressed brushes' tint colours (FLinearColor)
constexpr int ButtonStyle_Size = 0x278;
constexpr int ButtonStyle_NormalTint = 0x28;
constexpr int ButtonStyle_HoveredTint = 0xB0;
constexpr int ButtonStyle_PressedTint = 0x138;
constexpr int FocusMenu_Focusable = 0x2C8;  // TArray<UWidget*>: what gamepad focus moves between
// Native tick (virtual 0x330): every focusable button gets ColorAndOpacity and BackgroundColor = the focused
// colour (the one with focus) or the unfocused one (the rest), each tick.
constexpr int FocusMenu_FocusedColor = 0x2E0;    // FLinearColor
constexpr int FocusMenu_UnfocusedColor = 0x2F0;  // FLinearColor
constexpr int FocusMenu_Canvas = 0x358;     // MenuCanvasPanel
constexpr int Prompt_MessageText = 0x3E0;
constexpr int Prompt_YesButton = 0x3E8;
constexpr int Prompt_NoButton = 0x3F0;
constexpr int PC_HUD = 0x6A0;               // the gameplay UI, hidden while a menu is open
constexpr int PC_CameraManager = 0x2B8;     // APlayerController::PlayerCameraManager
constexpr int PC_OpenMenu = 0x718;
constexpr int DA_InteractUIClass = 0x90;
constexpr int InteractUI_InteractMessageText = 0x2F8;  // "PICK UP" (its row is the card's bottom border)
constexpr int WidgetTree_RootWidget = 0x28;
constexpr int InteractUI_NameText = 0x2C8;
constexpr int TextBlock_Font = 0x188;             // FSlateFontInfo
// UEditableTextBox: Text (FText, kept up to date as it's typed in), its look, OnTextChanged
constexpr int EditableTextBox_Text = 0x108;
constexpr int EditableTextBox_Style = 0x130;      // FEditableTextBoxStyle
constexpr int EditableTextBox_MinWidth = 0x9E4;   // float MinimumDesiredWidth
constexpr int EditableTextBox_OnTextChanged = 0xA08;
// FEditableTextBoxStyle: background brushes (normal, hovered, focused), padding, font, text and background colours
constexpr int TextBoxStyle_Brushes[3] = {0x8, 0x90, 0x118};
constexpr int TextBoxStyle_Padding = 0x228;
constexpr int TextBoxStyle_Font = 0x238;
constexpr int TextBoxStyle_ForegroundColor = 0x290;  // FSlateColor
constexpr int TextBoxStyle_BackgroundColor = 0x2B8;  // FSlateColor
// FSlateBrush (0x88): tint (FSlateColor, an FLinearColor first), the renderer's resource handle (a TSharedPtr)
constexpr int SlateBrush_Size = 0x88;
constexpr int SlateBrush_Tint = 0x20;
constexpr int SlateBrush_ResourceHandle = 0x70;
constexpr int ButtonStyle_NormalBrush = 0x8;
// FSlateFontInfo (0x58): its composite font (a TSharedPtr, unused with a font asset) and size
constexpr int FontInfo_Size = 0x58;
constexpr int FontInfo_FontObject = 0x0;
constexpr int FontInfo_CompositeFont = 0x30;
constexpr int FontInfo_PointSize = 0x48;
// UWrapBox: wrap at WrapSize (when bExplicitWrapSize) instead of its width
constexpr int WrapBox_WrapSize = 0x12C;
constexpr int WrapBox_bExplicitWrapSize = 0x131;
constexpr int InventoryUI_Grids[4] = {0x400, 0x408, 0x410, 0x418};  // weapon mod, ability mod, melee mod, perk grids
constexpr int InventoryUI_RelicGrid = 0x420;
constexpr int InventoryUI_OwningPS = 0x3E0;
constexpr int InventoryUI_AccumulatedBuffRow = 0x480;   // UHorizontalBox
constexpr int InventoryUI_AccumulatedBuffText = 0x488;  // UTextBlock
constexpr int InventorySlotUI_Button = 0x2F0;
constexpr int UniformGridSlot_Row = 0x3C;
constexpr int UniformGridSlot_Column = 0x40;

// Necromancer
constexpr int Controller_PlayerState = 0x228;
constexpr int LM_LocalC = 0x390;  // ACrabPlayerC*: this machine's own character
constexpr int Spawnables_Weapons = 0xD8;  // TArray<UCrabWeaponDA*>: the lobby's weapons
constexpr int WeaponDA_Mesh = 0xB0;       // UStaticMesh*
constexpr int WeaponDA_ProjectileDA = 0xC0;
constexpr int WeaponDA_CrosshairType = 0x100;
constexpr int WeaponDA_FireMode = 0x102;
constexpr int WeaponDA_BaseFireRate = 0x118;
constexpr int WeaponDA_BaseSpread = 0x120;
constexpr int WeaponDA_VerticalRecoil = 0x134;
constexpr int WeaponDA_HorizontalRecoil = 0x138;
constexpr int WeaponDA_BaseClipSize = 0x148;
constexpr int WeaponDA_ReloadDuration = 0x150;
constexpr int WeaponDA_EquipSound = 0x160;  // USoundCue*
constexpr int WeaponDA_CosmeticFX = 0x168;  // UNiagaraSystem*
constexpr int WeaponDA_FireMontage = 0x170;
constexpr int WeaponDA_MuzzleFlashFX = 0x188;
constexpr int WeaponDA_GunshotSound = 0x190;
constexpr int WeaponDA_PostFireClearChamberMontage = 0x1B0;
constexpr int WeaponDA_PostFireClearChamberSound = 0x1B8;
constexpr int ProjDA_Lifetime = 0x30;
constexpr int ProjDA_CollisionRadius = 0x34;
constexpr int ProjDA_Speed = 0x38;
constexpr int ProjDA_MaxSpeed = 0x3C;
constexpr int ProjDA_GravityScale = 0x5C;
constexpr int ProjDA_MaxPiercings = 0x7C;
constexpr int ProjDA_bExplodeAfterLifetime = 0xA0;  // bool; it also bursts where it hits
constexpr int ProjDA_ExplosionRadius = 0xA8;
constexpr int ProjDA_ExplosionDamage = 0xB0;
constexpr int ProjDA_FX = 0xE8;           // UNiagaraSystem*: the projectile itself
constexpr int ProjDA_TrailFX = 0xF0;
constexpr int ProjDA_Sound = 0xF8;        // USoundCue*: while it flies
constexpr int ProjDA_bRandomizeTint = 0x100;
constexpr int ProjDA_ExplosionFX = 0x108;
constexpr int ProjDA_ExplosionSound = 0x110;
constexpr int Weapon_Mesh = 0x220;            // UStaticMeshComponent*
constexpr int Weapon_TimesFired = 0x338;      // int32, replicated: counts every shot on every machine
constexpr int Enemy_MeshDMI = 0x728;          // UMaterialInstanceDynamic*
constexpr int Enemy_bIsExplosiveEnemy = 0x760;  // EnemySpawnSettings (0x748) + 0x18: slugs and pufferfish, which blow up
constexpr int Character_Mesh = 0x280;         // USkeletalMeshComponent*
constexpr int Enemy_OwningAIC = 0x770;
constexpr int Enemy_Category = 0x778;         // ECrabEnemyCategory: 1 Easy, 2 Medium, 3 Hard, 4 Elite, 5 Boss
constexpr int Enemy_EnemyName = 0x780;        // FString
constexpr int Enemy_EnemyBuff = 0x7c8;        // ECrabEnemyBuff, replicated: 1 Shielded, 2 Armored, 3 Healthy, ... (rolled in init, 0x140ca5ca0)
constexpr int Enemy_BestEnemyC = 0x800;       // ACrabC*, replicated
constexpr int Enemy_SpawnFX = 0x8c8;  // UNiagaraSystem* EnemySpawnFX: played on the ground at BeginPlay (0x140ca3390)
constexpr int Enemy_bIgnoredByPlayerTargeting = 0x901;
constexpr int Enemy_bIsLesserEnemy = 0x902;   // split off another enemy on its death
constexpr int Enemy_ChanceToSpawnOnEliminated = 0x944;  // float
constexpr int EnemyAIC_OwnedC = 0x358;
constexpr int EnemyAIC_BestEnemyC = 0x368;
constexpr int GameStateUI_ChatInput = 0x358;  // UEditableTextBox*
constexpr int PC_GameStateUI = 0x690;
constexpr int GameplayUI_VersionText = 0x398;  // UTextBlock*: the game's version, in the corner
constexpr int GameplayUI_AbilityKeyText = 0x3F0;
constexpr int GameplayUI_AbilityIcon = 0x400;
constexpr int GameplayUI_AbilityCooldownText = 0x408;
constexpr int GameplayUI_AbilityStackText = 0x418;
}  // namespace off

enum class IslandType : uint8_t { None = 0, Lobby = 1, NewRun = 2, ContinueRun = 3, Arcade = 15, Holdout = 16 };  // ECrabIslandType, 0..16

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
constexpr uint8_t MeleeMod_BigClaws = 2;      // ECrabMeleeModType; the enum ends at Brawler = 12
constexpr uint8_t Perk_Banana = 6;            // max health multiplier +20%
constexpr uint8_t Perk_Magnify = 39;          // area of effect +20%
// Turret damage: a turret's projectile damage x (1 + GetPerkBuff(EnhancedTurrets)), set in 0x140d64840.
constexpr uint8_t Perk_EnhancedTurrets = 51;
constexpr uint8_t Relic_RingOfDefense = 21;   // ECrabRelicType
constexpr uint8_t Relic_RingOfFury = 27;
constexpr uint8_t Relic_FireRing = 31;        // ECrabRelicType; the enum ends at RushedRing = 67

}  // namespace crab
