# Shellbreaker: design

Status: proposal, 2026-10-12. Nothing in game is built yet; the model is. The numbers below are starting points that still have to go through `plans/balance/`.

Shellbreaker is a melee primary, kept simple: **a handle in the claw, a chain, and a blade on the chain's end**. Left click swings it, right click aims a hook. It **replaces your melee** while you hold it, the way the Soul Whip replaces your ability.

## 1. The weapon

- **The model** (`art/shellbreaker/gen_weapon.py`), to scale with the crab (0.9 m tall, 1.5 m across):
  - **Handle:** about 30 cm. A leather grip, a shell-orange end knob with three claw tips, and a gold cap with an eye for the chain.
  - **Chain:** heavy 7.5 cm iron links from the handle's eye, 20 of them: about 1.5 m at rest. With the arm and blade, the blade sits about 2.1 m from the claw.
  - **Blade:** a 48 cm leaf blade, double-edged, on a shell-orange socket with two back-swept barbs (they're what catch in the hook). A glowing ridge runs down its middle; an element can tint it.
- **The chain changes length.** The game adds links as it pays out and drops them as it reels in, on the whip's rope physics (`necro_lash.hpp`). Swings use the resting length; the hook pays out up to 20 m. A longer chain is a natural upgrade later (section 5).

## 2. Controls

| Input | What it does |
|---|---|
| **Left click** | The swing string (section 3). Hold to keep swinging. |
| **Right click (hold)** | Aim the hook. A reticle shows where it will land and what it will do: pull, get pulled, or swing. |
| **Left click while aiming** | Throw the hook (section 4). Letting go of right click without throwing cancels. |
| **Melee key** | Nothing. Shellbreaker *is* your melee: your equipped melee is disabled while you hold it. |
| **Ability key** | Your normal ability, unchanged. |

- **Melee mods don't drop** for players holding Shellbreaker, the same way some items don't drop for whip holders. Any you already have stay in your inventory and work again if you switch weapons.

## 3. The swing string (left click)

Quick, physics-driven swings that flow into each other. The chain stays at its resting length, so each swing reaches about 2.5–3 m and hits everything in its arc.

| # | Swing | Time | Damage per hit |
|---|---|---|---|
| 1 | **Sweep** right to left | 0.20 s | 20 |
| 2 | **Sweep back** left to right | 0.20 s | 20 |
| 3 | **Chop** from top right to bottom left | 0.20 s | 20 |
| 4 | **Rebound:** the blade's momentum carries it back up, bottom left to top right | 0.15 s | 15 |
| 5 | **Overhead circle:** one full loop above your head, hitting all around you | 0.35 s | 35 |

- The full string is 1.1 s. Holding left click loops it; stopping for 0.6 s resets it to swing 1.
- **The rule: base 75 DPS, whatever you're doing** (was 100; lowered because every swing is area damage, like the Rocket Launcher's 68 or the Arcane Wand's 50). Every swing deals **dps × its length in seconds** to each enemy in its area (4.5 m radius, 22 m out; the circle 24 m all around), through the game's radial damage on the host.
- **Fire rate** speeds the whole string up (more DPS, same per-hit damage), and **damage** items scale each hit, like any gun.
- The rebound (4) comes from the rope physics, not a canned animation: the chop's momentum swings the blade back up.

## 4. The hook (right click)

Aim with right click, throw with left click. **Cooldown 6 s.** The blade flies out up to 20 m, paying out chain, and does what it hits:

- **A small or medium enemy:** yanked to you and stunned 0.5 s, landing in front of your next swing. The hook hit deals one sweep's damage (20).
- **An elite or a boss:** too heavy, so **you** are pulled to it instead.
- **A wall, tree or rock:** the barbs bite and you **swing on the chain**:
  - Pendulum physics: your momentum carries you through the arc.
  - **Left click reels in**: a shorter chain swings faster.
  - **Jump lets go** and flings you along your swing; the blade pulls free and snaps back to you.
  - The blade is the anchor, so there's no attacking mid-swing. The swing is for getting around.
- **Nothing (a miss):** the blade reels back. Half the cooldown is refunded.

## 5. Growing it later

No separate upgrade system. Shellbreaker gets stronger the way every weapon does (weapon mods, elements, crits, fire rate), because each hit fires the Soul Whip's invisible hit projectile. Later, a small set of Shellbreaker items (like the whip's necro items) can add to the moves, for example:

- **Longer chain:** wider swings, a longer hook.
- **Second hook charge.**
- **The overhead circle pulls enemies in.**
- **The hook stuns longer** or sets enemies burning.

## 6. How it's built: the whip's way

Everything is made the way the Soul Whip was: authored in Blender and Python, baked into CrabMods.dll's pack, and played from code. There's no Unreal Editor and no extra pak.

- **Crab animation.** Each swing is posed on the real SK_Crab skeleton in Blender (`crabrig.py` already loads, poses and skins it) at the game's real speeds. It's baked to a `*.crabanim` file in the pack: bone rotations per frame, plus timing marks for the hits. In game the mod gives the crab a `PoseableMeshComponent` with the same mesh during the move. It copies the crab's current pose, plays our frames on top, then hands back.
- **The handle, chain and blade.** Meshes are built from triangle lists like the whip's (`handle`, `link` and `blade` `.crabmesh`). The links are laid along the whip's rope physics, every other one turned a quarter, with a variable length. The swings drive the rope's end; the physics does the rest.
- **Effects:** a blade trail on each swing, glowing like the whip's soul light.
- **Sounds** are made by the same generator as the necromancy sounds (chain rattle, blade whoosh, hook bite) and preview-mixed the same way before they ship.
- **Hits** fire the same invisible hit projectile as the Soul Whip bolt, so weapon mods, elements, crits and on-hit effects still apply.
- **Multiplayer:** the host owns the hook's cooldown and damage. Every machine plays each player's swings, rope and hook when told they started, as with the whip's rope.

## 7. Order of work

1. **Two tests in game:**
   - Play one baked swing on the crab through a `PoseableMeshComponent`.
   - Move the crab along a hook swing (and yank an enemy).
2. **Preview video in Blender** at real speed, with sound: the swing string and a hook swing.
3. **First version:** the swing string, the hook, melee disabled, melee mods kept out of the loot.
4. **Later:** Shellbreaker items (section 5).

## Open questions

1. **Right click:** what does the game already bind to it, and does that need blocking while Shellbreaker is held?
2. **Area at single-target DPS:** every swing hits everything in its arc at the full 100. The balance audit cut the Soul Whip from 100 to 70 for hitting an area (`balance/audit.md`). Fine as a melee weapon's reward for being close, or trim it?
3. **RE:** does moving the crab ourselves every frame (the hook swing) work smoothly in multiplayer? That's test 1.
4. **Rope limits:** how long a chain (how many links) the rope sim handles before it gets unstable.

## Full build (2026-10-12, unreleased, untested in game)

Code: `crabmods/items/shellbreaker.cpp` (ini `[shellbreaker]`), the chain sim `shell_chain.hpp`, the loot
`shell_loot.cpp` / `shell_loot.hpp`. Art and sounds are in `crabmods/assets/shell.pack` (resource 101). It's baked by
`art/shellbreaker/bake_pack.py`, after `gen_weapon.py` and `gen_sounds.py`. Built into build/CrabMods.dll; not
installed, packaged, tagged or released.

**Getting it**
- A lobby pedestal two places left of the Crossbow, one past the Soul Whip's. Each machine copies the stone itself;
  the host spawns the pickup. It's removed when the lobby ends.
- `spawnKey` (default none) still spawns a test pickup in front of you.
- On pickups, the model itself is shown: the handle spins and bobs, with the chain coiled under it and the blade at
  the end.

**The string (LMB)**
- Five physics swings, `swingSeconds` each.
- Damage per swing is dps × swingSeconds × weight. Weights are 0.91 / 0.91 / 0.91 / 0.68 / 1.59 (the plan's
  20/20/20/15/35), averaging 1, so the string is the dps.
- Fire-rate bonuses speed the swings up.
- The string restarts after a pause longer than a swing + 0.6 s.

**The hook (RMB: press to fire, hold to swing, let go to fly off)**
- The hook point (`autoAnchor`), every 0.1 s, from the crab's eye along the aim (GetBaseAimRotation):
  - With a charge: the enemy closest to the crosshair within `enemyAssist`° and hookRange, not behind a wall (a
    trace to it stops short by more than 2.5 m). Red diamond; Throw hooks it.
  - Else the crosshair's own trace, then 25 traces ±22° sideways and 0–45° above its pitch (never below 10° up),
    out to anchorRange. Hits less than 2 m above or 3 m away are skipped. Scored −(degrees off the
    crosshair)/20 − 0.5·|d − anchorIdeal|/anchorIdeal + 0.2·up/range. Orange diamond.
  - Else, with `skyAnchor` (off by default), a point in the air ahead at anchorIdeal, 40–60° up.
- The swing (shell_swing.hpp, simulated in tests/shell_swing_test.cpp): on catch the chain is the crab's distance,
  and shortens at 1500 cm/s to min(catchLift·length, pin height − floor − 150) so the arc clears the floor (floor:
  a trace down, + the capsule's half height); the crab gets at least 1300 cm/s toward the hook point. Each frame,
  past the length, the velocity across the chain is kept (plus steering × swingPump), the speed along it is
  replaced by a pull of min(8·overshoot, 1500), and the total is capped at swingMaxSpeed.
- `swingKeepHeight` (default): holdZ = max(start z, floor + 150), at most 150 below the pin. While the crab's flat
  distance from the pin is within the chain's catch length at holdZ, its vertical speed is set to
  clamp(8·(holdZ − z), ±1500), its flat speed kept (plus steering × swingPump); beyond it, a pendulum at the catch
  length, which swings it up. Reeling raises holdZ.
- Charges: 1 + Grappler, spent only on enemies. Cooldown is `hookCooldown` ÷ (1 + Quick Draw), never below a sixth.
- What it does depends on what it hits:
  - An enemy near the crosshair (with a charge), light (category ≤ `yankMaxCategory`, or ≤ 4 with Kraken's Grasp):
    yanked to you.
  - A heavy enemy pulls you to it.
  - Otherwise the hook point, a swing: the chain catches at `catchLift` of the distance; movement input across the
    chain adds `swingPump` cm/s²; LMB reels in; letting go of RMB launches at velocity × `releaseBoost` +
    `releaseLift` up. Letting go before it catches cancels the swing.
- On any enemy, the host deals `hookDamage` 60 (+ Undertow) as a Lesser burst, plus Frost Chain's ice.
- Every machine draws everyone's hook rope. The host relays `shell rope ...` messages to the other players.

**Sounds**
- Synthesized (gen_sounds.py) and played through necro's mixer: swing ×4, circle, throw ×2, bite ×2, and equip
  (equip also plays, quieter, when a hook charge returns).
- Swing, throw and bite sounds are positional, for every player.
- They follow the game's Master × Gameplay volume, times `soundVolume`.

**Its loot (`shell_loot.cpp`)**
- 12 items that only drop for holders. Melee mods stop dropping for them.
- Type bytes 220–224 for weapon mods and 220–223 for perks; relics 220–222. Item orders 230–241.

| Item | Kind, rarity | Per level (ini) | Effect |
|---|---|---|---|
| Long Chain | weapon mod, Common | links = 4 | chain +4 links: reach, circle and hook range × (20+4L)/20, capped at ×2.5 |
| Wide Arc | weapon mod, Common | radiusPercent = 15 | swing and circle radius +15% |
| Whirlwind | weapon mod, Epic | pullSpeed = 500 | the circle drags enemies (not bosses) within 2.5 × circle radius toward you |
| Executioner | weapon mod, Epic | damagePercent = 55 | chop and rebound +55% |
| Searing Edge | weapon mod, Epic | fireStacks = 3 | every swing hit applies fire stacks |
| Riptide | perk, Epic | damagePercent = 35 | +35% damage for 4 s after the hook catches an enemy |
| Quick Draw | perk, Common | cooldownPercent = 15 | hook cooldown ÷ (1 + 15%·L), floor ⅙ |
| Undertow | perk, Common | damagePercent = 75 | the hook's own hit +75% |
| Rhythm | perk, Common | damagePercent = 30 | overhead circle +30% |
| Grappler | relic, Epic | charges = 1 | +1 hook charge |
| Kraken's Grasp | relic, Legendary | extraEnemies = 2 | yanks hard and elite enemies too; the 2 nearest come along |
| Frost Chain | relic, Epic | iceStacks = 10 | the hook's hit applies ice |

**Animation, timing, FX and HUD**
- **Body:** each swing plays a montage on the crab, on every machine, borrowed from the game's own weapons (found
  through the Spawnables DA, which loads them all: `Abilities` 0xE8, `MeleeWeapons` 0xF8).
  - The sweeps: the Katana's slash (`DA_Melee_Katana` → `AM_Crab_Melee_Katana`).
  - The chop: the Hammer's slam (`DA_Melee_Hammer` SlamMontage 0xE8 → `AM_Crab_Melee_HammerSlam`).
  - The circle: the Hammer's swing (`DA_Melee_Hammer` Montage 0xC8 → `AM_Crab_Melee_Hammer`).
  - The rebound keeps the weapon's fire montage.
  - The hook's throw: `AM_Crab_Grenade_Throw` (from `DA_Ability_Grenade`, which the game's Grappling Hook also uses),
    on the thrower's machine and on others when the rope starts flying.
  - Each is sped up or slowed (×0.5–×4) to fit the swing's time. A missing one falls back to the claw's swing.
- **Melee hit blocked:** the Katana, slam and claw montages carry the "Melee" notify, which calls
  `UCrabPlayerAnimInstance::AnimNotify_Melee` (a real melee hit). Its native exec thunk is swapped at the UFunction's
  Func (0xD8) after a check, so it does nothing for holders.
  - If the check fails, those montages are skipped (the log says so); the Hammer swing and the throw still play.
- **Per-swing timing:** each swing lasts its weight of the string: 0.20 / 0.20 / 0.20 / 0.15 / 0.35 s at the
  defaults. In OnShot the weapon's own interval (`+0x2C4`) is set to the next swing's time, from its own fire-rate
  multiplier (`+0x2C0`). Nothing shared is touched, so host and clients each time their own weapon.

**Checked statically (in the game binary and pak, 2026-10-12)**
- **No lag:** Fire (0x140d6a5d0) calls FireProjectiles (0x140d6a936, where OnShot runs) and only then re-arms its
  one-shot timer from `+0x2C4` (read 0x140d6aea8, bLoop false 0x140d6af36). The trigger-press path (0x140d6e382)
  reads it too. So the interval set in OnShot times the very next shot.
- **No lunge:** none of the borrowed montages' sequences has root motion.
- **Notifies:** "Melee" is in `AM_Crab_Melee`, `_Katana` and `_HammerSlam`; not in `_Hammer` or
  `AM_Crab_Grenade_Throw`.
- **Offsets** from the SDK dump: MeleeDA Montage 0xC8 / SlamMontage 0xE8, AbilityDA Montage 0xC8, Spawnables
  arrays above; `Montage_Play`'s parameter layout is asserted at compile time.

**Untested (check these first in game; needs installing)**
- How each borrowed montage looks at its new speed with the chain's physics.
- Swinging on the hook launches the crab every frame on its own machine, so a multiplayer client may rubber-band.
- Remote ropes trust the host's word. A rope with no "back" message clears itself after 12 s.
- Searing Edge fires on any type-0/1 hit a holder deals. Today that's only the swings.
- The impact FX may scale with the burst radius.
