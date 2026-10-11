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

Every new CrabMods item is a data asset created in memory; nothing is added to the pak.

1. Find a template with the same class in `SpawnablesDA`: Streak Shot for a weapon mod, Collector for a perk, Heat Sink (11) for an ability mod (`AbilityMods +0x118`, `UCrabAbilityModDA::StaticClass 0x140d6f2b0`), a ring for a relic (`Relics +0x148`, `UCrabRelicDA::StaticClass 0x140d824b0`; Ring of Defense 21, Ring of Fury 27, Fire Ring 31).
2. `NewObject(Outer = Template->Class->Outer, Class = Template->Class, Name = "DA_...", Flags, Template)`. Outer is the `/Script/CrabChampions` package, so the asset gets the stable path `/Script/CrabChampions.DA_...`.
3. Flags `RF_Public | RF_Standalone | RF_MarkAsRootSet`, then OR in `RF_WasLoaded` (`0x80000`) on the object. With it, the net driver treats the object as stably named and replicates references to it **by path**. Every player must create the same asset, or clients cannot resolve it (they see an empty slot).
4. Overwrite the fields: `Name +0x38`, `Description +0x48` (FStrings), `Icon +0x58`, `Rarity +0x62`, `LevelDescription +0xB8` (`PrimaryBuff` is replaced with the buff in the UI), `BaseBuff +0xC8`, `bGetBuffAsMultiplier +0xCE`, and the item type at `+0xD8`. Free type ids: weapon mods above `DiceShot = 94`, perks above `FlashSale = 121`, relics above `RushedRing = 67`. CrabMods uses 200 and up. `LootPool` is at `+0x61`, `SpawnWeight` (float) at `+0x68`.
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

### Max health

`UpdateMaxHealth` computes `max(0.01, (1 - buff 0x6d - buff 0x76) * (PS+0x39C MaxHealthMultiplier + perk buffs 6, 0x68-0x6c)) * (PS+0x398 BaseMaxHealth + perk 1 buff + relic 0x3b buff)`. It clamps the result to at least 1 and calls `SetMaxHealth` on an `ACrabPlayerC` pawn (no other pawn type). `0x140d22720` returns the base part, `BaseMaxHealth + perk 1 + relic 0x3b`. Percentage costs are taken of that.

Everything per player lives in two fields, changed only by these (authority only; each adds `Amount`, then calls `UpdateMaxHealth`):

| Function | Address | Callers |
|---|---|---|
| `AddBaseMaxHealth(ps, Amount)` | `0x140d20a00` | chest pay (call `0x140cafb82`); totem interact `0x140cb5860` (Health: +base or -50%, Loot -8%, Gold -33%); damage area `0x140cc1ad0` (`DamageAreaType` MaxHealth, `+0x288`); hit handler `0x140d17be0` (difficulty Eternal Punishment, modifier 13: -0.5% per hit) |
| `AddBaseMaxHealthFromPerk(ps, Amount, PerkType)` | `0x140d20b20` | the same, plus the perk's `AccumulatedBuff`. Totem Enhancement / Valued Customer; GM damage handler `0x140ce5780` (Personal Space, Bullseye: the killer); `0x140ce9650` (Fortitude, call `0x140ce9fc3`, **in a loop over every player**); hit handler (Damage Seeker); `0x140d3cf00` (Valued Customer) |
| `AddMaxHealthMultiplier(ps, Amount)` | `0x140d20e10` | only in loops over every player: `0x140ce9650`, `0x140cea670` (difficulty Relaxed, modifier 0: +1.0 at the run start) |

`ACrabChest` pay-and-open `0x140caf9f0(chest, ACrabC* opener)` returns at once if `bIsChestOpened (+0x300)` is set. Otherwise it charges `Cost (+0x2B4)` in `CurrencyType (+0x2B0)`:
- Crystal 0, Key 1.
- Health 2: `Cost% x` the opener's max, as Unblockable damage (type 0xC) through `0x140d2dca0`.
- MaxHealth 3: `AddBaseMaxHealth(-Cost% x base)`.

It then rolls the loot (`0x140caf1c0`).


## Projectiles and Heavy Shot

- Every player weapon fires `ACrabProjectile`s: each `UCrabWeaponDA` has a projectile DA at `+0xC0`.
- Projectile setup: `0x140d64840`. It sets `UCrabPMC+0x124` (gravity) from the projectile DA's `GravityScale` (`+0x5C`), then, for a player's projectile, copies the player's weapon mods onto it and applies each through `0x140d61a70` (a switch on the mod type). `0x140d61a70` first does damage multiplier ×= `0x140d25c30(PS)`, the weapon mods' damage buffs, which include Heavy Shot's.
- Each projectile carries an unreflected `FCrabProjectileModInfo` at `proj+0x280`: `+0x280` damage multiplier, `+0x294` speed, `+0x29C` gravity multiplier.
- Fast Shot (case `0xB`): speed += buff, gravity multiplier -= buff.
- Heavy Shot (case `0x1E`): `UCrabPMC+0x124` (gravity) += 2.0, gravity multiplier += debuff, plus its damage buff.
- `UCrabPMC` is the game's own movement component, not a `UProjectileMovementComponent`. Its velocity update is `0x140c9bb40` (found through its vtable): gravity = projectile DA `GravityScale` (`+0x5C`) × max(0, gravity multiplier), applied only after `GravityDelay` (DA `+0x60`). Setup folds the multiplier in at the end: `PMC+0x124 *= max(0, proj+0x29C)`.
- So with Heavy Shot, gravity = (`GravityScale` + 2) × (1 + debuff − Fast Shot's buff), on every weapon. Most weapons have `GravityScale` 0 and fly straight without it. Very fast projectiles (speed 100000: Sniper, Marksman Rifle, Laser Cannons) live too briefly to drop much.

## Loot weights

- `LootRoll` `0x140d33240`: `void(Source, u8 LootPool, u8 Rarity, TArray<ACrabPS*>* Players, int Choices, float, TArray<FCrabPickupInfo>* Out)`. For each of `Choices` slots it picks one of `Players`, copies what that player holds through `ACrabPS::GetPickups` (`0x140d253a0`), filters the candidates (pool, rarity, tags, already offered) and picks one with `WeightedPick`.
- The weights are each candidate's `SpawnWeight` (`DA+0x68`), **read from the asset at roll time**, after `GetPickups` returns. Per-player loot odds can therefore be set by rewriting `SpawnWeight` on every asset right after `GetPickups` returns inside a roll (CrabMods' hook), and restoring them when the outermost `LootRoll` returns.
- `WeightedPick` `0x140d32450`: `int(TArray<float>*)`, the index by weight, -1 if empty. It can pick a weight-0 entry when `rand()` returns 0, so a hook that wants weight 0 to mean "never" replaces the pick.
- The pool filter also reads the asset at roll time: a candidate is kept when its `LootPool` (`DA+0x61`) equals the roll's pool (`[rsp+0x21]`), or the pool is Random (`0xb`) (main loop `0x140d341f3`; the bonus Epic/Legendary rolls at `0x140d34019`/`0x140d34148` also accept pools in mask `0x102800`). Pools `0xe`..`0x10` become Random and `0x11` becomes Health before the loop; `0x13` becomes Random at `0x140d33e30`. Rarity (`DA+0x62`) is checked after the pool. So setting `DA+0x61` to the roll's pool after `GetPickups` adds a pickup to that chest for this roll (CrabMods' Specialists).
- The world for a roll comes from `UEngine::GetWorldFromContextObject(GEngine 0x1443fe458, Source, 1)` (`0x142709ab0`). The level manager is `GameState+0x270`; `GameState+0x238` is the `PlayerArray`.

## Chest choices

`ACrabChest` roll `0x140caf1c0`: `void(ACrabChest*, ACrabPS* Opener)`. It works out the number of choices (the chest's own count, then the opener's perks and relics, at least 1) and calls `LootRoll` once with that count. Hooking the chest roll to note the opener, then `LootRoll` to rewrite `Choices` on its first call, changes the count **after** every game modifier. `Choices = 0` gives an empty chest.

## Island rewards

- `ACrabLM::SpawnPortals` `0x140d00210` rolls a reward pool for each Arena, Horde and Demolition portal, by weight and without repeats, then calls `ACrabPortal::SetInfo` (`0x140cb2cc0`, `void(ACrabPortal*, FCrabPortalInfo*)`, server only) for each. `FCrabPortalInfo+0x19` is the reward's `ECrabLootPool`.
- Reward weights: Damage, Health, Random 1.25; Critical, Elemental, Speed, Luck, Economy, Skill 0.75; Relic 0.5; Upgrade 0.4; Greed 0.2. Anvil and Spiked come with their own rules.
- To bias rewards, CrabMods re-picks the pool in `SetInfo` with these weights times its items' factors, skipping pools already given out during that `SpawnPortals`.

## Spiked Chest

- `UCrabSpawnablesDA+0x580` `SpikedChest` is the `TSubclassOf<ACrabChest>` for the Spiked reward (`BP_Chest_Spiked`): LootPool SpikedChest (14), Epic, `CurrencyType` MaxHealth (3), `Cost` 25. Every chest's cost is its own `Cost` (`+0x2B4`, an int, not replicated); nothing in the code changes it after the class defaults (the native constructor sets 0, the blueprint 25). Setting `Cost` on the class default object (`UClass+0x118`, checked by its class pointer and `RF_ClassDefaultObject`) changes every spiked chest spawned afterwards.
- Paying (`0x140caf9f0`, host) for MaxHealth: `AddBaseMaxHealth(-Cost% × GetBaseMax)`. The chest's own widget (`0x140caf760`) skips the cost label for the Spiked pool.
- The portal's reward description is built on every machine by `0x140d5ed80` (called from the portal UI update `0x140cb29f0`, run by `SetInfo` and the replicated info). For the Spiked pool it copies a fixed 77-character literal at `0x143256cc0` ("... costs 25% of your base max health to open"), inlined as 78 `wchar_t` (the NUL included). Rewriting the literal in memory, at the same length, changes the text.


## Island reward chests (Double or Nothing)

- 0x140d01610 (host) runs at the end of an island. For each player in the GameState's PlayerArray it builds a `TArray<UClass*>` of chests in its own frame: Data at rsp+0x40, Num +0x48, Max +0x4C (after its `sub rsp, 0x410`), also kept in r12 (Data) and esi (Num), which the rest of the loop uses. The main chest class comes from SpawnablesDA by reward pool, then bonus chests, Care Package and totem chests are added.
- Then `mov dl, 0x3F; mov rcx, rdi; call 0x140d23bb0` (ACrabPS HasRelic, bool(ps, uint8 RelicType): walks PS+0x438 for DA+0xD8 == type), return address 0x140d029b0. High Roller: rand() <= 33.33% adds 2 more of the main chest, <= 66.66% nothing, otherwise empties the array and calls 0x140cf9280(LM) (a short timer, then SpawnPortals 0x140d00210). Messages go through 0x140d26f40(ps, FString*) (to the PS's owner controller); the caller frees the FString.
- Afterwards a key chest may be added, then positions (0x140d33070, count = esi) and one spawn per entry; the array (r12) is freed with FMemory::Free at the end of each player.
- CrabMods hooks HasRelic with an asm thunk: at that return address it passes the caller's frame, esi and r12 to the items (checking they match the array), then reloads esi/r12 from the frame.

## Kills

- `ACrabPS+0x358 Eliminations` (int, replicated with `OnRep`) counts the player's kills. It is incremented by `0x140d26c00`, called from the game mode's damage handler `0x140ce5780` when a hit eliminates an enemy. The unreflected `+0x35C` counts too (it feeds Tasty Orange).
- The counter replicates to everyone, so any machine can see kills by watching it change.

## Ability cooldown (ACrabPlayerC)

| | |
|---|---|
| `+0x7D0` | Ability DA |
| `+0xA10` | float: cooldown seconds left, 0 = not counting down |
| `+0xA14` / `+0xA18` | int: charges available / max charges |
| `+0xA28` | melee cooldown |
| `0x140d1c530` | Start: if the cooldown is 0 and charges < max, cooldown = the ability's Cooldown × the player's cooldown stats, at least 0.25 s |
| `0x140d1e9e0` | Tick, a 0.25 s timer while recharging: cooldown -= 0.25 (to 0); at 0 a charge comes back and, if still short, the next cooldown starts |

The fields are not replicated: the owning player's machine counts them down. Lowering `+0xA10` in the tick (CrabMods keeps it ≥ 0.01 so the game's own "reached 0" path runs) shortens the cooldown.

## Downed players

A downed (eliminated, waiting for revive) player's `PlayerState+0x280` pawn is an `ACrabSpectatorC` (`StaticClass 0x140d83600`) until they are revived. An `ACrabC` pawn with `+0x668` bit 0 (`bIsEliminated`) set also counts as down. Both replicate.

The spectator is spawned and possessed only by `0x140d0e720`, which is called only by the GM damage handler `0x140ce5780` when `TakeDamage` reports an elimination. That happens in the same call as the killing hit, so a poll never sees the dead `ACrabC`, only the spectator. The other uses of its class (`0x140d0b5c0`, `0x140d0d260`) are `IsA` checks on a controller's pawn.


## Sending data to every player

No new RPC is needed: `APlayerController::ClientMessage(FString S, FName Type, float MsgLifeTime)` is a reliable client RPC that every player controller has.
- `AGameModeBase::K2_PostLogin` `0x1427e2de0` (`void(GM*, PC*)`, a BlueprintImplementableEvent stub) is called by `PostLogin` on the server for every player who joins, the host's own controller included. Hooking it gives each new player's controller.
- To send: `UObject::FindFunctionChecked` (`0x1410c96b0`, `UFunction*(UObject*, FName)`) with the `ClientMessage` FName (a global at `0x14440f450`, set at startup), then `ProcessEvent` (vtable +0x220). Params: `FString S` at 0, `FName Type` at 0x10, `float` at 0x18; 0x20 bytes.
- On the receiving side, `APlayerController::ClientMessage_Implementation` `0x14255c410` (`void(PC*, const FString*, FName, float)`, not overridden by `ACrabPC`) runs; hooking it lets a mod take its own tagged messages before they reach the console.

## Healing (Overflow)

- `UCrabHC::Heal` `0x140c9acc0` (`void(UCrabHC*, float)`): `Cur = min(Cur + Amount, Max)`, then OnRep. Health pickups, healing areas, Grave Dodger and Slow Recovery's pending pool (through `0x140d151c0`, which also applies Amber Resin's multiplier) reach it with their full amount.
- `0x140d14930` `float(ACrabC*, float requested)` returns `Cur >= Max ? 0 : min(requested, Max - Cur)`. Regenerator, Grim Reaper, Endurance, Scavenger, Vampire, Ring of Parrying and Health Shot clamp through it first and skip the heal when it returns 0, so `Heal` never sees them at full health.
- Hooking both gives all healing lost to the cap: `Cur + Amount − Max` in the first, `requested − result` in the second (no overlap). Every heal path runs on the host. `HC+0xB0` is the owning `ACrabC`; its player state is `C+0x240`.

## Fire rate and max health (Community Support, Thick Shell)

- `ACrabWeapon::UpdateFireRate` `0x140d69f70` runs on every machine when a player's items change: fire rate = 1 − buff(`0x1F`) + `ACrabPS::GetWeaponModBuff(PS, 0x20 RapidFire)` (`0x140d25fb0`, return address `0x140d69fdc`) + .... Adding to that call's result at that return address adds fire rate; calling `UpdateFireRate` on each of the pawn's weapons (`C+0x538`) applies a change.
- `ACrabPS::UpdateMaxHealth` (host): multiplier = `PS+0x39C` + `InvArr_GetBuff(PS+0x428 perks, 6 Banana)` (`0x140d2df60`, return address `0x140d28ad3`) + ...; adding to that call's result adds a max health multiplier.
- A perk's `FCrabPerk+0x20` (AccumulatedBuff, float) replicates with the perks and is saved and restored from island to island. `0x140d20f80` `void(PS, u8 PerkType, float)` adds to it and marks the array dirty.

## Weapon fire (Bullet Rain, Hellshooter)

- `ACrabWeapon::FireProjectiles` `0x140d6d280` runs on the machine that controls the player; it spawns each projectile through `0x140d37220` (`(info, owner, FVector* start, bool send)`; return address `0x140d6dcfd`) with the direction at `info+0x08`, then a cosmetic RPC sends start and direction to the others.
- `WeaponDA+0x104` (`ShotSpawnType`) 1 makes the weapon fire from above the aim point (Ice Staff). Setting it for the call of `FireProjectiles` gives that to any weapon.
- `ACrabPlayerC` camera trace `0x140d130c0` gives the aim point (`FHitResult`: `+0x18` ImpactPoint, `+0x48` TraceEnd, `+0x5D` bBlockingHit); `0x140d30df0` snaps a point to the ground.

## Destructibles (Collateral Damage)

- `ACrabDestructible::OnHCDamaged` `0x140cc64a0` (`void(self, FCrabDamageInfo*, u8 HitType)`, host): `HitType 5` breaks it (unless `+0x398` bExploded is set). The breaker is `info+0x40` DamagingC if a player, else `+0x340`, the last player who hit it.
- `GS+0x278` MatchState 3 is an island's fight. Destructibles spawned by players' items are the classes at SpawnablesDA `+0x270..+0x288` (Landmine, HomingBarrel, SporeMushroom, UltraMushroom).

## Explosions (Martyr)

- `0x140d2dd20` `void(WorldCtx, TArray<AActor*> Ignore /*by value, freed by the callee*/, const FVector* Origin, float Radius, const FCrabDamageInfo* Info, bool bRequireLOS, AActor*)`: the game's radial damage. Flat damage (no damage buffs), falling off to 20% at the edge for radius > 250; only targets that `Info->DamagingC` may hurt (exactly one of the two has a player state).
- Magnify: the core `0x140d2d380` multiplies the radius by `1 + GetPerkBuff(DamagingC's PS, 39)` (0.2 per level), except for Hazard damage; that is Magnify's only read.
- `FCrabDamageInfo` (0x48): `+0x08` Damage, `+0x0C` type (1 Radial, 3 Melee, 4 Lesser: not throttled per enemy), `+0x10` Tags, `+0x20` Debuffs, `+0x30` location, `+0x3C` knockback, `+0x40` DamagingC.
- `0x140d79d00` `(LM, FX, Sound, const FVector*)` is the multicast that plays an effect on every machine; SpawnablesDA `+0x88/+0x90` is the generic explosion (Explosive Armor's).
- `0x140d0e720` (`void(ACrabPC*)`, host; see Downed players) still has the dead character at `PC+0x250` with its player state, so an explosion fired there is credited to the player.

## Revive bubble (Martyr)

- The revive bubble is an `ACrabHarvestArea` (StaticClass `0x140d775a0`, vtable `0x143242a40`) of type 2 (Revival), the class at SpawnablesDA `+0x4C8` (`BP_HarvestArea_Revival`). The GM damage handler `0x140ce5780` spawns it where a player goes down, sets `+0x298` PlayerToRevive (replicated) through `0x140cf50e0`, then calls `0x140d0e720` (the spectator).
- Fields: `+0x220` HarvestAreaNC (Niagara), `+0x238` type, `+0x23C` Radius (1500 in the constructor), `+0x260` UpdateRate, `+0x264` / `+0x268` countdown start / current, `+0x250` the overlapping player.
- BeginPlay (`0x140cf4530`, host) starts a timer on `0x140cf50f0`, which overlaps a sphere of `Radius` for `ACrabPlayerC`s each update and counts down while one is in it (type 2 revives PlayerToRevive at 0). The radius is read every update, so changing it on the host changes who can revive.
- Virtual `0x530` (`0x140cf4ec0`, every machine, when it's made) sets the Niagara float `"Radius"` from `+0x23C` and the colour `"Tint"` from `+0x240`. `0x140845d30` is `UNiagaraComponent::SetVariableFloat(FName, float)`.
- After `0x140d0e720` possesses the spectator, the downed `ACrabPlayerC` still exists (the spectator watches it), but with no player state (`+0x240`, cleared on unpossess). Martyr puts it back for its damage call.
- `0x140d254b0` `float(ACrabPS*, uint8 PerkType)` is the perk buff (Magnify 39: 0.2 a level).

## Melee mods

- Each `UCrabMeleeModDA` (SpawnablesDA `+0x128`) keeps its numbers in `BaseBuff` / `BaseDebuff`, which the game reads when it applies the mod and in its description. Types: Vampire 1, Big Claws 2, Sharp Claws 3, Iron Claws 4 (debuff = cooldown penalty), Time Claws 5, Blender 6, elemental claws 7..11, Brawler 12.

## Key totem

- Server: `ACrabTotem::Interact` `0x140cb5860` (Key type 3, `+0x298`) spends the keys, adds the player's id to `+0x2D8`, and calls the `ClientOnInteractedWithKeyTotem` RPC. On that player's machine `0x140d095f0` picks a locked item from its save (weapons first), unlocks it (`UCrabGI::UnlockPickup` `0x140ce45e0`) and calls `ServerSpawnKeyTotemPickup` (`0x140d7bb70`; implementation `ACrabPC` vtable `+0xE60`), which spawns any item it is given, unchecked, with no owner.
- An item is locked when it is in a SpawnablesDA list (`+0xD8 ... +0x148`) but not in the save's matching unlocked list (`UCrabSG` `+0xB0 ... +0x120`; the save is `GI+0x228`, GI `World+0x180`).
- `ACrabLM::SpawnPickup` `0x140d00060` spawns a pickup from an `FCrabPickupInfo` (`+0x38` OwningPS: only that player may take it). `ACrabInteractPickup` Interact `0x140d3cf00` (server) counts the take at `+0x300`; `0x140d3c0b0` deactivates a pickup (what a chest does to the rest of its ring).

## UI from code (key totem menu, inventory slots)

- UMG functions are called through `ProcessEvent` (vtable `+0x220`) with the `UFunction` from `UClass::FindFunctionByName` `0x140f66a80` `(UClass*, FName, IncludeSuper)`. `StaticFindObject` `0x1410d96f0` `(Class|null, Outer|null, const wchar_t* FullPath, bool Exact)`, `CreateWidget` `0x141e20e60` `(APlayerController*, UClass*, FName)`. Engine widgets are `NewObject`s with the user widget's `WidgetTree` (`+0x1D8`, root at tree `+0x28`) as outer. StaticClass getters: `UButton` `0x141e8c4a0`, `UBackgroundBlur` `0x141e8af80`, `UUniformGridPanel` `0x141ea55c0`, `UScrollBox` `0x141e9cfe0`, `USizeBox` `0x141e9bc90`, `UWrapBox` `0x141eb7500` (skips Collapsed children, so filtering is collapsing), `UHorizontalBox` `0x141e93e10`, `UTextBlock` `0x141ea1930` (font `+0x188`), `UEditableTextBox` `0x141e8f4c0` (Text `+0x108`, WidgetStyle `+0x130`: its brushes, padding, font and colours are what the box uses; the Font / colour fields after it are deprecated; OnTextChanged `+0xA08`). FText to and from FString: `Conv_TextToString` / `Conv_StringToText` on `/Script/Engine.Default__KismetTextLibrary`.
- `0x140c9f390` `(FMulticastScriptDelegate*, UObject* Target, unused, FName Function)` is AddUniqueDynamic. CrabMods binds its buttons' `OnClicked` (`UButton+0x3C8`) to `UUserWidget::CancelLatentActions` on the button's card and hooks that function's exec thunk `0x141ea93f0` to tell its clicks apart. A text box's OnTextChanged binds the same way (a function with no parameters ignores the delegate's).
- Focus menu tint: its native tick (virtual `0x330`, `0x140d45230`) sets every focusable `UButton`'s ColorAndOpacity and BackgroundColor to `+0x2E0` (the focused or hovered one) or `+0x2F0` (the rest; half see-through) each frame; text blocks get the FSlateColors at `+0x300` / `+0x328`. Write those per instance to change it.
- Focus menus (`UCrabFocusMenuUI`): `+0x2C8` focusable widgets (gamepad), `+0x2D8` owning PC, `+0x358` MenuCanvasPanel. Virtuals: `0x478` init (binds its buttons; call once), `0x480` refresh (key texts, gamepad focus), `0x4a0` press sound, `0x4a8` / `0x4b0` next / previous submenu (the Next / Previous Submenu keys, `0x140d0bf90` calls the open menu's; the base steps `+0x3C0` through `+0x3B8` SubmenuHorizontalBox), `0x4c0` `bool CanClose` (Escape `0x140d0bcd0`, the back and action buttons ask it first). The open menu is `ACrabPC+0x718` ActiveFocusMenuUI. Opening, as `0x140d073f0` does: AddToViewport, `UI_SetShown` `0x140d60400 (w, 1)`, `0x478`, `0x480`, `PC+0x718 = w`, `0x140d0a510(pc)` (input mode, cursor), `UI_SetShown(PC+0x6A0 GameplayUI, 0)`. `0x140d0d000(pc)` closes it again.
- `UCrabConfirmationPromptUI` (vtable `0x1432517d8`, blueprint `/Game/UI/Menu/UI_ConfirmationPrompt.UI_ConfirmationPrompt_C`, loaded with the pause menu, which contains one): `+0x3E0` message text, `+0x3E8` YES, `+0x3F0` NO (both in one horizontal box).
- Pickup cards (`UCrabInteractUI`, class at `DA+0x90`): setters `0x140d59310` icon, `0x140d59480` type string, `0x140d59420` name, `0x140d592d0` description, `0x140d59430` rarity colour (from `0x140d3f810(DA, &color)`), `0x140d59390` loot pool. `+0x2F8` is the "PICK UP" text; its third parent is the bottom row.
- Inventory slot counts are `uint8` at `ACrabPS+0x3D8 / 0x3F0 / 0x408 / 0x420` (weapon, ability, melee mods, perks), each 8 bytes before its item array. `0x140d29e50` sets 24 / 12 / 12 / 24 for a new run; `0x140d28d10` sets 8 for all in a challenge with modifier 9; `RestoreAutoSave` copies them from the auto save; nothing caps them. The byte after each count is padding no code touches (CrabMods keeps the slots it added there).
- Inventory menu (`UCrabInventoryUI`): grids at `+0x400 / 0x408 / 0x410 / 0x418` (relics `+0x420`), owning PS `+0x3E0`. Refresh `0x140d5d9a0` (virtual `0x480`) calls `0x140d5e170` per grid, which inits every `UCrabInventorySlotUI` child `i` (from 1) with `locked = count < i`; item `i` goes in child `i`. Init `0x140d5aaa0` appends each slot's button (`slot+0x2F0`) to the focusables and calls the slot's virtual `0x478` (binds its hover). Grid children carry their row / column in their `UUniformGridSlot` (`+0x3C` / `+0x40`).
