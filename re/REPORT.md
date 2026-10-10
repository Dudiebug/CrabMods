# Crab Champions: static reverse-engineering report

Target: `CrabChampions-Win64-Shipping.exe` (UE 4.27, monolithic, PE timestamp `0x6921D2F7`, base `0x140000000`).
Everything below was derived statically: UHT reflection tables, vtables, Ghidra 12.1.4 analysis and ghidra-mcp. Nothing was probed at runtime.

The generated SDK, function tables and the scripts that made them are described in [README.md](README.md). The addresses the mods use are collected in [`common/crab/game.hpp`](../common/crab/game.hpp).

## Engine core (for the DLL framework)

| Symbol | Address / offset |
|---|---|
| `NamePoolData` (FNamePool) | `0x144282980` (init flag `0x144282940`), ctor `0x140ed4980` |
| `GUObjectArray` | `0x1442bec60` |
| `StaticFindObject` | `0x1410d96f0` |
| `NewObject` (`StaticConstructObject_Internal` wrapper) | `0x14078b370`: `(Outer, Class, FName, Flags, Template, bCopyTransients, InstanceGraph, ExternalPackage)` |
| `FName::FName(const TCHAR*, EFindName)` | `0x140ed46e0` |
| `FMemory::Realloc` / `FMemory::Free` | `0x140e017e0` / `0x140df0dc0` |
| `UObject::ProcessEvent` | `0x1410d10f0`, **vtable +0x220** (confirmed by every generated BP-event stub) |
| `UGameEngine::Tick` (per-frame game-thread hook) | `0x1422eefb0`, vtable +0x2B0 (UEngine slot 86) |
| World walk | `GEngine(+0x780 GameViewport) → +0x78 World → +0x120 GameState → +0x238 PlayerArray` |
| Other world fields | `UWorld+0x38 NetDriver`, `+0x180 OwningGameInstance` |
| Actor | `+0xE0 Owner`, `+0xF0 Role` (3 = Authority) |
| PlayerState | `+0x224 PlayerId`, `+0x250 UniqueId (Steam)`, `+0x280 PawnPrivate`, `+0x300 PlayerNamePrivate` |

## Inventory model (ACrabPS, size 0x578)

- `WeaponDA 0x3C0`, `AbilityDA 0x3C8`, `MeleeDA 0x3D0` (Net, OnRep_*DA).
- `NumWeaponModSlots 0x3D8`, `WeaponMods 0x3E0`, `NumAbilityModSlots 0x3F0`, `AbilityMods 0x3F8`, `NumMeleeModSlots 0x408`, `MeleeMods 0x410`, `NumPerkSlots 0x420`, `Perks 0x428`, `Relics 0x438`. All are Net + `OnRep_Inventory`.
- `HealthInfo 0x37C` (0x1C), `BaseMaxHealth 0x398`, `MaxHealthMultiplier 0x39C`, `DamageMultiplier 0x3A0`, `ScaleMultiplier 0x3A4`, `Keys 0x3B0`, `Crystals 0x470`.
- Item element = 0x28 bytes: `{DA*, FCrabInventoryInfo{u8 Level @+8, TArray<u8> Enhancements @+0x10, float AccumulatedBuff @+0x20}}`.
- **`Level` *is* the stack count.** Picking up a duplicate does `Level = min(255, Level + new.Level)`.
- Item identity is `DA+0xD8` (the per-category type enum byte). InventoryDA: `+0xC8 BaseBuff`, `+0xCC bHyperbolicBuff`, `+0xCD bLimited`, `+0xCE bGetBuffAsMultiplier`, `+0xD0 BaseDebuff`. `Buff(type) = BaseBuff% × Level` (hyperbolic: `1-(1-BaseBuff%)^Level`).
- Native multicast delegates (unreflected): `PS+0x520` inventory changed, `PS+0x538` crystals changed, `PS+0x508` keys changed, `PS+0x4A8` combo, `PS+0x4C0` eliminations.
- Unreflected counter `PS+0x35C` counts eliminations (feeds TastyOrange).

### Native functions (server-side, Role==Authority)

| Function | Address | Notes |
|---|---|---|
| `ACrabPlayerC::GivePickup(FCrabPickupInfo*)` | `0x140d168d0` | Switch on `PickupDA+0x60` (ECrabPickupType 1..9); the single entry point for all pickups |
| `ACrabPS::AddWeaponMod(FCrabWeaponMod*)` | `0x140d210a0` | Merges by type, broadcasts `+0x520`, calls `ClientOnPickedUpPickup` |
| `ACrabPS::AddAbilityMod` | `0x140d1fd60` | same pattern |
| `ACrabPS::AddMeleeMod` | `0x140d20140` | same pattern |
| `ACrabPS::AddPerk` | `0x140d20520` | GivePickup then re-applies Collector / BigBones |
| `ACrabPS::AddCrystals(int)` | `0x140d20d00` | Saturating at uint32 max |
| `ACrabPS::AddKeys(int)` | `0x140d20df0` | |
| `ACrabPS::AddScaleMultiplier(float)` | `0x140d21020` | |
| `ACrabPS::Server{Remove*}_Implementation` | WM `0x140d299d0`, Perk `0x140d29810`, Relic `0x140d298f0`, AM `0x140d29650`, MM `0x140d29730` | Remove by type, broadcast `+0x520` |
| `ACrabPS::ServerSet{Weapon,Ability,Melee}DA_Impl` | `0x140d29c70` / `0x140d29ab0` / `0x140d29b90` | |
| `ACrabPS::ServerIncrementNumInventorySlots_Impl` | `0x140d294d0` | |
| **`ACrabPS::RestoreAutoSave(FCrabAutoSave*)`** | `0x140d28ee0` | Wholesale replaces inventory, slots, health, multipliers, combo, crystals |
| `ACrabPC::ServerRestoreAutoSave_Impl` | `0x140d0d100` | **No validation**: calls PS+PlayerC RestoreAutoSave with client data |
| `ACrabPC::ClientOnEnteredPortal_Impl` | `0x140d08da0` | Client builds an FCrabAutoSave from its replicated PS and saves it |
| `ACrabPC::ServerSendChatMessage_Impl` | `0x140d0d1a0` | → `ACrabGM::BroadcastChatMessage 0x140ce56e0` → `ClientOnReceivedChatMessage` to every PC |
| `InvArr_GetBuff(arr, type)` / `InvArr_GetLevel` | `0x140d2df60` / `0x140d2e050` | Used by every perk |
| `ACrabPS::UpdatePerkBuff(type, bApply)` | `0x140d276e0` | Collector/TastyOrange/Assassin/... popup + optional `DamageMultiplier += buff` |
| `ACrabPS::AddPerkAccumulatedBuff(type, f)` | `0x140d20f80` | |

### Persistence across islands

Island travel is not seamless for inventory. On `ClientOnEnteredPortal(…, bSaveAutoSave)` each client snapshots its own replicated PS into a local `FCrabAutoSave`. After the next map loads it pushes the snapshot back with `ServerRestoreAutoSave`. Anything the host writes into a PlayerState therefore survives travel automatically.

## Damage pipeline

```
ACrabPlayerC::ModifyOutgoingDamage (vtable +0x900, 0x140d11a10)
  -> ACrabPS::CalcOutgoingDamage(PS, FCrabDamageInfo&)        0x140d21480  (crit / elemental perks)
       -> ACrabPS::GetDamageBuff(PS) -> float                 0x140d23700
```

`GetDamageBuff` decompiles to the following:

```
add = PS.DamageMultiplier
    + Buff(13)*PS.Combo + Buff(12)[cond] + Buff(18) + Buff(27)*f(pawn) + Buff(28)[cond]
    + Buff(TastyOrange=68) * PS.ElimCounter(0x35C)
    + Buff(Collector=72)   * TotalPerkLevels
    + MoneyIsPower(73): min(Crystals, Buff)*4e-5
    + SpeedIsPower(74): min(Buff, speed)*4e-4
    + Buff(75)*(Buff(29)+Relic(49))
mul = 1 + Buff(108)+Buff(109)+Buff(110)+Buff(111)+Buff(114) [+ Relic(27) if low health]
return add * mul
```

TastyOrange is natively **+1% per elimination on the current island** (resets per island). Collector is **+3% per perk level** (from `DA_Perk_*` BaseBuff). Both are computed live on every call; on pickup the game only calls `UpdatePerkBuff(PS, Collector, bApply=false)` to show the popup. The permanent `DamageMultiplier +=` path (`bApply=true`) is used by stacking perks such as Assassin, Checklist, Gemstone and SilverLining.

## Networking

- Game net driver: `SteamSocketsNetDriver` (ISteamNetworkingSockets P2P) with IpNetDriver fallback. It's a listen server and fully host-authoritative.
- `steam_api64.dll` is Steamworks **v151** and exports `SteamNetworkingMessages_SteamAPI_v002`, `SteamNetworkingSockets_v009` and `SteamFriends_v017`. A DLL can open its own Steam P2P side channel keyed by the same SteamIDs, with no relay server and no interference with the game's channel.

## Loot pool / adding new pickups

- `ACrabLM::BeginPlay` = `0x140cf73d0` (ACrabLM vtable `0x143243770` +0x338). It ends by rebuilding `LM.AllUnlockedPickups` (+0x420, Transient) from the save game (`FUN_140d31db0` → UCrabSG): `UnlockedWeaponMods +0xE0`, `UnlockedAbilityMods +0xF0`, `UnlockedMeleeMods +0x100`, `UnlockedPerks +0x110`, `UnlockedRelics +0x120` (append helper `0x140cf5da0`).
- `ACrabLM.SpawnablesDA` (+0x388, UCrabSpawnablesDA) holds the master lists (`WeaponMods +0x108`, `Perks +0x138`, …).
- To inject a new pickup without touching the save file, hook BeginPlay, call the original, then append the new DA to `AllUnlockedPickups`.
- `ECrabWeaponModType` is defined up to `DiceShot = 94`, so new type ids are free above that.

## Making new items at runtime

Both new CrabMods items (Arsenal, Hoarder) are data assets created in memory; nothing is added to the pak.

1. Find a template with the same class in `SpawnablesDA` (Streak Shot for a weapon mod, Collector for a perk).
2. `NewObject(Outer = Template->Class->Outer, Class = Template->Class, Name = "DA_...", Flags, Template)`. Outer is the `/Script/CrabChampions` package, so the asset gets the stable path `/Script/CrabChampions.DA_...`.
3. Flags `RF_Public | RF_Standalone | RF_MarkAsRootSet`, then OR in `RF_WasLoaded` (`0x80000`) on the object. With it, the net driver treats the object as stably named and replicates references to it **by path**. Every player must create the same asset, or clients cannot resolve it (they see an empty slot).
4. Overwrite the fields: `Name +0x38`, `Description +0x48` (FStrings), `Icon +0x58`, `Rarity +0x62`, `LevelDescription +0xB8` (`PrimaryBuff` is replaced with the buff in the UI), `BaseBuff +0xC8`, `bGetBuffAsMultiplier +0xCE`, and the item type at `+0xD8`. Free type ids: weapon mods above `DiceShot = 94`, perks above `FlashSale = 121`.
5. Append it to `LM.AllUnlockedPickups` on every `ACrabLM::BeginPlay`.

The rarity colour is part of each icon image. The game ships plain per-rarity icons (`T_Icon_WeaponMod_<Rarity>`, `T_Icon_Perk_<Rarity>`) used by items without their own art; they can be found through any loaded item that uses them.

## Loot rolls: pickup tags

`UCrabPickupDA +0x63 PickupTag`, `+0x64 bRequiresMatchingPickupTag`. The roll is `0x140d33240`. It picks a player and copies their `ACrabPS.PSPickups` (`+0x340`, `TArray<UCrabPickupDA*>`, every pickup they hold) through `0x140d253a0`. It then checks the flag on each candidate asset (`0x140d34050`, `0x140d3417f`, `0x140d3439e`: `cmp byte [da+0x64], 0`); if it is set, the candidate is offered only if some held pickup has the same `+0x63` tag. The weapon itself plays no part: no weapon asset sets a `PickupTag`. Clearing `+0x64` in memory makes the item roll for everyone. Only the host rolls loot, so only the host's copy matters.

Tags pair "enabler" items with their follow-ups, for example Bouncing Shot (the only `Bounce` item without the flag) with Heavy Shot, and turret ability mods with the turret upgrades.

Heavy Shot (`ECrabWeaponModType` 30): Common, BaseBuff 50, BaseDebuff 15, `PickupTag = Bounce`, `bRequiresMatchingPickupTag = true`: it is offered only once you hold Bouncing Shot.

## Perks

- `UCrabPerkDA::StaticClass` `0x140d7c920`; `PerkType` at `+0xD8`. `SpawnablesDA.Perks` at `+0x138`.
- `ACrabPS.Perks` at `+0x428`, the same 0x28-byte elements as the other inventory arrays.
- `perk_sites.json` lists, for each perk, every function that checks it.
- Collector (72) is computed live in `GetDamageBuff`: its buff × the sum of all perk levels (including its own). A bonus added to `PS.DamageMultiplier` for the duration of a `GetDamageBuff` call therefore stacks additively with Collector and the other additive perks, and is scaled by the multiplier perks.

## Health (UCrabHC)

| | |
|---|---|
| `ACrabC +0x4C0` | `UCrabHC*`; `ACrabC +0x668` bit 0 `bIsEliminated` |
| `UCrabHC +0xB0` | Owning character (set when the component initialises) |
| `UCrabHC +0xFC` | `HealthInfo`: `CurrentHealth +0x108`, `CurrentMaxHealth +0x10C` |
| `UCrabHC +0xD8` | Unreflected `bDead`, set by `TakeDamage` at 0 health |
| `UCrabHC +0xFB` | Unreflected: one-time death protection used |
| `UCrabHC::TakeDamage(FCrabDamageInfo*)` | `0x140c9afe0`: armor, protections, elimination |
| `UCrabHC::OnRep_HealthInfo` | `0x140c9b4a0`: broadcasts health changed |
| `ACrabC::SetMaxHealth(float)` | `0x140cbd780`: sets the max and clamps health down to it |
| `ACrabPS::UpdateMaxHealth` | `0x140d28a30`: recomputes the pawn's max health from items |

Killing a character through `TakeDamage` with enough damage plays out exactly like a normal death, including game over when everyone is down.

## Projectiles and Heavy Shot

- Every player weapon fires `ACrabProjectile`s: each `UCrabWeaponDA` has a projectile DA at `+0xC0`.
- Projectile setup: `0x140d64840`. It sets `UCrabPMC+0x124` (gravity) from the projectile DA's `GravityScale` (`+0x5C`), then, for a player's projectile, copies the player's weapon mods onto it and applies each through `0x140d61a70` (a switch on the mod type). `0x140d61a70` first does damage multiplier ×= `0x140d25c30(PS)`, the weapon mods' damage buffs, which include Heavy Shot's.
- Each projectile carries an unreflected `FCrabProjectileModInfo` at `proj+0x280`: `+0x280` damage multiplier, `+0x294` speed, `+0x29C` gravity multiplier.
- Fast Shot (case `0xB`): speed += buff, gravity multiplier -= buff.
- Heavy Shot (case `0x1E`): `UCrabPMC+0x124` (gravity) += 2.0, gravity multiplier += debuff, plus its damage buff.
- `UCrabPMC` is the game's own movement component, not a `UProjectileMovementComponent`. Its velocity update is `0x140c9bb40` (found through its vtable): gravity = projectile DA `GravityScale` (`+0x5C`) × max(0, gravity multiplier), applied only after `GravityDelay` (DA `+0x60`). Setup folds the multiplier in at the end: `PMC+0x124 *= max(0, proj+0x29C)`.
- So with Heavy Shot, gravity = (`GravityScale` + 2) × (1 + debuff − Fast Shot's buff), on every weapon. Most weapons have `GravityScale` 0 and fly straight without it. Very fast projectiles (speed 100000: Sniper, Marksman Rifle, Laser Cannons) live too briefly to drop much.
