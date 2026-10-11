# Raise Dead: technical notes (earlier draft, design superseded by necromancer.md)

**Raise Dead** is a new ability you pick in the lobby, like Grenade or Airstrike. Every enemy you kill gives you **souls**: more for tougher enemies. You spend souls to raise **undead**: real enemies, freshly spawned, that fight for you. The other enemies fight them back. You pick what to raise: a pack of small fodder, a single crab or skull, or a weakened elite.

No new weapon or melee. The staff and Soulblade from the first draft were dropped on 2026-10-10.

All addresses are for game build `0x6921D2F7` and come from static RE (2026-10-10). **V** means verified in a decompile; **I** means inferred. The research decompiles are kept locally (git-ignored) in `re/decomp/necro/{equip,enemy,turret}/`.

## Balance

### What the game data says (static RE, `re/decomp/necro/balance/`)

The numbers below come from the game's own spawn and health code, not guesses.

**Enemy ranks.** Every enemy blueprint already has a rank (category Easy / Medium / Hard / Elite / Boss) and a `SpawnCost`: the price the wave builder pays to spawn it.

| Game rank | Enemies | Base HP | SpawnCost |
|---|---|---|---|
| Easy | Crab, IceCrab, FloatingSkull, Slug, LightningSlug, MiniScorpion, BlockerCrab (MiniPumpkin only from splits) | 15–45 | 30–100 |
| Medium | Hornet, Starfish, Ant, PoisonAnt, Grub, Limpet, Pufferfish, Skull, Laser/Blocker Skull, Fire/Ice/Poison Slug, Pumpkins | 50–150 | 125–400 |
| Hard | Shotgun/Hammer/Sniper/Laser/Minigun/Grenade/Launcher/Ice/Poison/Fire/Lightning Crab, Barrage/Ice/Lightning/Fire/Poison Skull, Fire/Ice/Lightning Ant, Scorpion | 175–300 | 850 |
| Elite | every `BP_Enemy_Elite*` | 650–825 | 2000 |
| Boss | BossAnt, BossSkull | 1000–2000 | 2500 |

**How many kills a run has** (solo, no modifiers):
- A combat island has 2 waves, sized by the island's position in its 7-island biome. Island 1 has ~13 kills; island 4 ~36; the biome's last island ~63.
- That works out to about **165 kills per biome and ~660 per 28-island loop**.
- Running totals: **~230 by island 10, ~730 by island 30, ~1,550 by island 50.** Loop 2 and later spawn 1.75× as much.
- "Thousands" happens in long looped runs, in co-op (enemy count ×1.4 for 2 players, ×2.2 for 4), on Horde islands, and with modifiers.

**Elites are rare.** Normal waves never spawn them. Solo you meet about **one per biome** (the Elite island before each biome change, plus optional Elite portals), so about 3–5 per loop, and 2–3 per Elite island once looping. There is one boss per loop.

## How it plays

### Souls

Every kill gives souls by the enemy's game rank. Any kill counts: weapon, melee, ability or turret.

| Game rank | Souls |
|---|---|
| Easy | 1 |
| Medium | 3 |
| Hard | 6 |
| Elite | 25 |
| Boss | 60 |
| On-death split (slugs, mini pumpkins) | 0 |

**Co-op:** every player with Raise Dead gets the souls for every kill on the island, not just the player who landed the hit. More players means more enemies but a smaller share of last hits each, so per-hit credit would starve co-op necromancers.

**Income** works out to **~45 souls per combat island in loop 1 and ~80 in loop 2**, about 260 per biome:

| Island in biome | Souls |
|---|---|
| 1 | ~19 |
| 2 | ~34 |
| 3 | ~40 |
| 4 | ~54 |
| Elite island | 25 |
| 7 | ~90 |

**Rules:**
- Souls carry over between islands, are capped at `maxSouls` (300), and reset in the lobby.
- Kills by your own undead give nothing.

### Raising

**Cost is tied to the game's own SpawnCost:** one undead costs `SpawnCost / 20` souls, rounded. The anchor is the user's number: **an elite costs 100**.

| Tier | Cost | You get | Rate at ~45 souls per island |
|---|---|---|---|
| **Fodder** (Easy) | ~5 for a pack of 3 | 3 | as many as you like |
| **Medium** | 15 | 1 | ~3 per island |
| **Hard** (the big crabs and skulls) | 42 | 1 | about one per island |
| **Elite** | 100 | 1 at 50% health and damage | one every ~2 islands in loop 1, about one per island in loop 2 |

- **Loop scaling:** costs × `(1 + 0.75 × loop)` (`loopCostScale`), so loop 2's 1.75× income doesn't make everything cheap.
- **What you can raise:** you can only raise kinds you've killed this run. You meet your first elite around island 6, so the elite tier unlocks around the end of the first biome.
- **Which kind:** within a tier, the default is the kind you've killed most, and a key cycles to the next.

**Limits:**
- At most `maxUndead` (12) weight per player: Easy 1, Medium 2, Hard 3, Elite 6. The oldest die first.
- Undead last `undeadLifetime` (45) seconds and die when the island ends.
- Bosses, the Immortal Snail and the Target Dummy can never be raised.

**Undead health:** undead use the current island's health multiplier, like any spawned enemy. A raised elite at island 10 has about 825 × 5.66 × 0.5 ≈ 2,300 HP.
### Controls

Everything works with the **ability key alone**, and the extra keys are optional shortcuts. Keys are set in `CrabMods.ini` and only work while Raise Dead is equipped.

| Key | Default | Does |
|---|---|---|
| Ability key | (the game's) | Raise the **selected tier** at the crosshair |
| `keyEasy` | `Z` | Raise a fodder pack now |
| `keyMedium` | `X` | Raise a Medium enemy now |
| `keyHard` | `C` | Raise a Hard enemy (big crab or skull) now |
| `keyElite` | `V` | Raise an elite now |
| `keyCycle` | `G` | Select the next tier. Pressed twice quickly, it cycles the kind within the tier. |

The ability's own cooldown is short (`cooldown`, 2 s): souls are the real cost. A raise you can't afford shows a message and keeps the cooldown.

### Undead behaviour

- An undead is a fresh copy of the enemy's class, spawned at the crosshair with a revival effect and tinted ghostly. **No ragdolls or corpses are kept**: the game already removes dead enemies, and we only spawn new ones.
- **Undead target:** the nearest hostile enemy. With none left they idle near you.
- **Enemies target undead:** an enemy switches to an undead when it is clearly closer than any player (`enemyAggroBias`). Undead tank for you.
- **Players** can't hurt undead (and don't auto-aim at them). Undead can't hurt players.
- Undead drop no crystals or items, and never spawn the children their kind would normally spawn on death.

### Settings (`[raiseDead]` in CrabMods.ini)

```ini
enabled = true
cooldown = 2
maxSouls = 300
# Souls per kill, by the enemy's own rank.
soulsEasy = 1
soulsMedium = 3
soulsHard = 6
soulsElite = 25
soulsBoss = 60
# true: every player with Raise Dead gets the souls of every kill. false: only the killer does.
sharedSouls = true
# One undead costs the enemy's SpawnCost divided by this (elite 2000 / 20 = 100, hard crab 850 / 20 = 42).
spawnCostPerSoul = 20
# Costs grow by this much per loop (loop 2 = x1.75).
loopCostScale = 0.75
easyPerCast = 3
maxUndead = 12
undeadLifetime = 45
# Raised elites: health and damage as a factor of a real elite.
eliteHealth = 0.5
eliteDamage = 0.5
# Enemies attack an undead when it is this many times closer than the nearest player.
enemyAggroBias = 1.5
keyEasy = Z
keyMedium = X
keyHard = C
keyElite = V
keyCycle = G
```
## Architecture

### A. The ability (new `crabmods/equipment.hpp` + `items/raise_dead.cpp`)

`EquipmentItem` makes a runtime copy of `DA_Ability_AirStrike` (StaticClass `UCrabAbilityDA 0x140d6f110`) and of its projectile.

- **Why AirStrike:** it lands at the crosshair (`AbilitySpawnType +0xB9 = 1`) and has a telegraph, which suits a summoning circle.
- **The copy's changes:**
  - Name, Description, Icon and Tint (purple).
  - `bRequiresUnlock +0x30 = 0`.
  - `Cooldown +0xC0`.
  - The projectile copy deals no damage (`ExplosionDamage +0xB0 = 0`) and plays the revival effect.
- **Into the game:** append it to `SpawnablesDA +0xE8` (Abilities). `GI 0x140cda610` then lists it as unlocked (V: it keeps every entry with `bRequiresUnlock = 0`), so it shows in the lobby.
- **The save problem:** the save remembers your loadout by path (`SG +0x90`, autosave `+0xC0`). The DA must therefore exist before the save loads, not at `ACrabLM::BeginPlay` like the other items. Phase 0 finds the right moment.
- **Guard:** if a player's ability slot points at our DA while Raise Dead is disabled, the host swaps in Grenade, so a slot is never empty.

### B. Souls (host)

- **Kill hook:** the GM damage handler `0x140ce5780`.
  - Condition: `HitType == 5` (a kill), and the killer `info+0x40` has a player state `C+0x240`.
  - Check: the receiving player's ability is ours (`PS+0x3C8`, V).
  - Effect: add the victim's soul value (to every player with Raise Dead when `sharedSouls`) and record its class in the "known kinds".
  - Note: with `sharedSouls`, a kill by any player counts, not only by players holding the ability.
- **Rank** is the enemy's own `EnemyCategory` (`ACrabEnemyC +0x778`: 1 Easy, 2 Medium, 3 Hard, 4 Elite, 5 Boss; SDK). Raise cost uses its `SpawnCost` (`+0x748` `EnemySpawnSettings` + 0x1C = `+0x764`). No name tables are needed.
  - A lesser child is spotted by how it was spawned: `EC_SpawnLesser 0x140caa810` marks them (I). Otherwise a pre-hook there tags the spawned actors.
- **Storage:** souls and known kinds live in a host-side table keyed by the player's Steam id (`PS+0x250`). It survives island travel and is cleared in the lobby, the same pattern as CrabSync's ledger.
- **Display:** the host sends the soul count to the player with `SendToPlayer` whenever it changes. The player sees it as a short message on gain or cast. Phase 0 looks for a HUD number we can drive instead (the ability's charge counter `C+0xA14/A18` is the first candidate).

### C. Raise requests (player → host)

- **Ability key:** free. `ServerAbility_Impl 0x140d1a430` runs on the host with the aim location (V). The host raises the player's selected tier there.
- **Hotkeys and tier selection:**
  1. The player's CrabMods reads the keys with `GetAsyncKeyState`, only while the game window has focus and no menu or chat is open.
  2. It takes the aim point from the camera trace `0x140d130c0` (`FHitResult +0x18`).
  3. It sends `raise <tier> <x y z>` or `select <tier> <kind>` to the host.
- **Transport:** there is no player→host channel yet (only host→player). Plan: send a tagged chat message through the game's reliable `ServerSendChatMessage`. A host hook on `ServerSendChatMessage_Impl 0x140d0d1a0` takes tagged messages and stops them before `BroadcastChatMessage`. This is new: it becomes a shared helper in `main.cpp` beside `SendToPlayer`.
- The host checks every request: the ability is equipped, enough souls, a known kind, the limit, and the island is in its fight.

### D. Undead (host, with visuals on every machine)

**Spawn.** Call `LM_SpawnEnemy 0x140cfe3b0(LM, class, &loc, &rot, bCountsForIsland = false)`.

- With `false` the island counter `LM+0x514` and the death delegate are never touched (V), so undead never hold the island open or count as kills.
- After the spawn, set:
  - `+0x901 = 1` (players don't target it).
  - `+0x944/+0x948 = 0` (no children).
  - Elite health × `eliteHealth`.
- Registry entry: `{actor, owner, weight, expires}`.

**Damage.** Detour `DmgInfo_CanDamage 0x140d2e6a0`. Today a "team" is only "has a player state" (V).

| Attacker → victim | Result |
|---|---|
| undead ↔ hostile enemy | true |
| undead ↔ player | false |
| undead ↔ undead | false |
| anything else | the original check |

Elite damage × `eliteDamage` in `OnTakeDamage` (already an Item event).

**Targeting.** Post-hook `AIC_SelectTarget 0x140ca0570`.

- **Undead:** nearest living hostile enemy.
- **Hostile enemy:** nearest undead if `distance × enemyAggroBias < nearest player`.
- **Apply:** write `AIC+0x368`, then the setter `0x140caa5b0` (it writes the replicated `EnemyC+0x800`), then SetFocus `0x140ca1bd0`.
- **Re-check:** `OnTick` (4 Hz) re-runs the pick, because the game only re-picks when a target dies or the AI idles. It also expires undead (killing them through `UCrabHC::TakeDamage`, so they die the normal way).

**Death.** Pre-hook `EnemyC OnEliminated 0x140ca4020` for undead: null `EliminatingC +0x930`, so there are no drops.

**Island end:** kill all undead when MatchState leaves 3 and on the next level start.

**Visuals (every machine).**

- Clients spot undead because their replicated target `+0x800` is an `ACrabEnemyC`. To be safe, the host also sends the list.
- Tint: `MeshDMI +0x728` vector parameter.
- Effect and sound: `LM_MulticastPlayOneShotFX 0x140d79d00` with `FX_HarvestArea_Revival` and `A_Skull_Death`.

## Phases

0. **Static RE gaps** (no code):
   - When the save loads relative to DLL start, and the earliest point the ability DA can exist.
   - Whether the lobby list reads `SG +0xC0`.
   - What the save does with an unknown DA path.
   - Whether `LM_SpawnEnemy` auto-possesses an AI controller (EnemyC `BeginPlay 0x140ca3390`).
   - What `0x140ca5ca0` (difficulty scaling) does.
   - `0x140cbaf80` (the CanDamage exception).
   - The `ACrabEnemyC` StaticClass mismatch (`0x140d70560` vs `0x140d74cf0`).
   - How lesser children are flagged.
   - Whether enemy projectiles can hit enemy pawns at all.
   - A HUD number for souls.
   - Confirm the chat-message path works as a hidden channel.
1. **Ability + souls.** The DA in the lobby, soul counting, the soul message, the player→host channel, the keys. Raising only logs what it would do.
2. **Undead.** Spawn, the CanDamage detour, both targeting directions, the tick, drops, lifetime, limit, island end, tint, effects.
3. **Polish and release.** Balance, README, CHANGELOG, packaging README, a REPORT.md section, version bump and tag.

## Risks

1. **Ranged undead may not hit.** Enemy projectiles may pass through enemies (collision channel or `ActorsToIgnore +0x2F0`). If so, ranged undead (most skulls, sniper and laser crabs) would do nothing.
   - Fix: in `Proj_Hit 0x140d66f90`, apply the hit ourselves when the shooter is undead.
   - Fallback: restrict raising to melee kinds.
   - This needs an in-game test.
2. **Save timing.** If the DA can't exist before the save loads, a saved Raise Dead loadout may load as an empty ability slot. Phase 0 checks this.
3. **Some AI actions may ignore the target** and go for the nearest player directly. Actions aim at `+0x800` (V), but this needs an in-game test.
4. **The chat channel** could leak a tagged line into chat if the hook misses. Messages are prefixed and short, so the worst case is a visible odd line.
5. **Everyone needs the DLL**, as with every CrabMods item (the DA replicates by path).
6. **It hooks hot paths** (damage check, targeting) much more than earlier items, so it **should be played once before release**.
