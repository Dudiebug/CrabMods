# Shellbreaker: design

Status: framework proposal, 2026-10-12. Nothing is built. The numbers below are starting points that still have to go through `plans/balance/`.

Shellbreaker is a melee primary: a **short, stubby sword** in the claw, and from its pommel an **extending chain to a spiked mace**. Your normal attack (M1) is a fixed combo string. Your power comes from up to **six Forms**: special attacks you find as loot during the run, the way you find weapon mods now. A Form's **rarity** sets how strong and how capable it is. **Levels** from duplicate drops make it stronger.

The fantasy is a sword-art fighter: lightning-fast flash steps, flaming wheel strikes, sweeping water slashes, swinging the mace on its chain, and swinging across the island on it.

## 1. The weapon

- **The model** (`art/shellbreaker/gen_weapon.py`), to scale with the crab (0.9 m tall, 1.5 m across):
  - **Sword:** about 70 cm overall. A 40 cm blade, 20 cm wide, double-edged and leaf-shaped, with a glowing ridge a Form can tint (lightning yellow, flame orange, water blue). A shell-orange crossguard with claw-tip ends, a leather grip, and a gold pommel with a ring.
  - **Chain:** 5 cm iron links from the pommel's ring, about 1.1 m at rest. It lengthens by adding links.
  - **Mace:** an iron ball 20 cm across with 14 shell spikes.
- **M1: the sword string.** Three slashes and a finisher, about 2.5 m reach. This is the steady damage and doesn't change over the run.
- **The chain and mace.** The chain pays out up to about 25 m. Several Forms throw or whirl the mace (Mace Swing, Chain Reap, Anchor Pull). It's the whip's rope physics (`necro_lash.hpp`) with a length that changes as it pays out and reels in.
- **Damage band.** M1 alone sits at the low end of the primary band (about 75 sustained DPS). Forms bring a full kit up to the band's middle and top, never past it (see section 5).

## 2. Forms (the abilities)

- **Six slots, at most.** Each slot is a key: M2, the melee key, the ability key, and three extra keys (V, C and X by default, rebindable in `[shellbreaker]`, like the necro raise keys).
- **Forms are loot.** They drop from chests only for players holding Shellbreaker, the same way necro items only drop for whip holders.
- **Picking up a Form when all six slots are full** asks which one it replaces. This reuses the game's key totem choice UI.
- **Every Form has a cooldown.** There's no stamina bar to manage. Flow (section 4) shortens cooldowns.
- **Every Form has a tag:** Movement, Area, Single target, Control or Defense. A good kit mixes them; six Area Forms is a weak kit.

## 3. Rarity and levels

The game's own rarities are used:

| Rarity | Base power | What changes besides numbers |
|---|---|---|
| Common | ×1.0 | The basic version of the move |
| Epic | ×1.3 | One extra property (an extra hit, a status effect, a shorter cooldown) |
| Legendary | ×1.7 | The full version of the move. Some Forms only exist at Legendary. |
| Greed (Greed Run) | ×2.0 | Legendary, plus a curse like the other Greed items |

- **Levels:** a duplicate of a Form you already hold adds a level, up to level 5. Each level adds **+12% power** (damage, or the main number of a utility Form), so a maxed Form is about +50%.
- **Milestones:** levels 3 and 5 each add a small extra, for example one more hit, or a longer stun.
- **A higher-rarity copy** of a Form you hold replaces it and keeps your levels.
- **Evolutions:** a few Forms evolve into their Legendary-only big brother at level 5 Legendary (Lightning Step → Eightfold Lightning). You can also find the evolved Form directly as a Legendary drop.

## 4. Flow and Shell Break

- **Flow** is a meter that fills from hits while in the air or dashing, from using different Forms back to back, and from parries. Getting hit, or using the same Form over and over, drains it.
- **Tiers:** Ripple → Wave → Surge → Riptide. Each tier makes Form cooldowns **10% shorter** and adds **+5% damage**, so Riptide is −30% cooldowns and +15% damage at most.
- **Shell Break:** Shellbreaker's hits crack enemy shells. A broken shell leaves the enemy **exposed for 2 s**: it takes +50% damage and its attacks are interrupted. Each enemy can only be exposed again after **8 s**, so it can't be stun-locked. On bosses, exposed is a short stagger, not a stun.

## 5. No one-shots: the damage rules

Every Form follows these rules, at every rarity and level:

1. **A Form's damage per use is at most what the band allows over its cooldown.** If Lightning Step has an 8 s cooldown, one use deals at most about 8 s × 40 DPS of its share. Burst is front-loaded, but it's never extra damage on top of the band.
2. **No execute effects.** Nothing kills an enemy below a health threshold, and nothing deals a percentage of max health.
3. **Hits on the same enemy from one Form fall off:** full damage for the first 3 hits, then 50%. Eightfold Lightning's eight hits can't all land full damage on one target; it's meant for crowds.
4. **Elites take 70% and bosses 50%** of a Form's burst. This matches how the game's own big hits treat them.
5. **A full maxed kit** (six Legendary level 5 Forms at Riptide) lands around 1.3× the top of the primary band. That's the same ceiling a strong late-game gun build reaches, not a room-clearer.

## 6. Starting roster (12 Forms)

Names are our own. The style is inspired by sword-art anime; nothing is copied from one.

### Lightning

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Lightning Step** | Movement / Single target | A blinding flash step about 10 m forward. You can't be hit during it, and it cuts everything you pass through. | It shocks what it cuts. | 2 steps in a row, the second toward the nearest enemy. | 8 s |
| **Eightfold Lightning** (Legendary only, or evolved) | Area | — | — | 8 flash steps zig-zagging between the nearest enemies, each a cut, then a thunderclap where you finish. | 18 s |

### Flame

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Fire Wheel** | Area | A forward flip that carves a burning vertical arc in front of you. | It leaves a burning trail on the ground. | The wheel rolls on for 6 m after you land. | 9 s |
| **Rising Sun Spiral** | Control | A spinning uppercut that launches enemies around you and sets them burning. | Bigger radius. | You rise with them and can follow up in the air. | 12 s |

### Water

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Flowing Cut** | Single target | A long smooth combo of 5 slashes that steers toward your aim. | Each slash hops to a nearby enemy. | 8 slashes, and the last one breaks shells outright. | 10 s |
| **Whirlpool** | Control | A spin that pulls nearby enemies in. | It slows them. | It holds them in place for 1.5 s. | 14 s |

### Wind

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Gale Crescent** | Area | A crescent blade of wind that flies 15 m. | It pierces. | 3 crescents in a fan. | 7 s |

### The chain

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Mace Swing** | Movement | Throw the mace into a tree, rock, wall or enemy and swing on the chain (pendulum physics, reel in with M1, release to fling). 15 m chain. | 25 m chain, and hitting an enemy on the way out stuns it briefly. | You can re-throw once mid-air to chain a second swing. | 6 s |
| **Chain Reap** | Area | The mace whirls around you on the full chain once. | Twice. | It drags hit enemies toward you. | 11 s |
| **Anchor Pull** | Control | Hook an enemy and yank it to you. Elites and bosses pull you to them instead. | It exposes the enemy (section 4). | Hook up to 3 enemies at once. | 9 s |

### Shell

| Form | Tag | Common | Epic | Legendary | Cooldown |
|---|---|---|---|---|---|
| **Shellquake** | Area | Leap up and slam down: a shockwave that cracks shells. | Bigger shockwave. | An aftershock 0.5 s later. | 12 s |
| **Iron Shell** | Defense | A 0.4 s parry stance. Blocking a hit counters it. | The counter exposes the attacker. | A successful parry resets your other cooldowns by 2 s. | 6 s |

All damage Forms deal the game's own element status where they have one (Lightning: shock, Flame: burn), so the elemental items you already find work with them.

## 7. How it's built: the whip's way

Everything is made the way the Soul Whip was: authored in Blender and Python, baked into CrabMods.dll's pack, and played from code. There's no Unreal Editor and no extra pak.

- **Crab animation.** Each Form is posed on the real SK_Crab skeleton in Blender (`crabrig.py` already loads, poses and skins it) at the game's real speeds. It's baked to a `*.crabanim` file in the pack: bone rotations per frame, plus timing marks for the hits. In game the mod gives the crab a `PoseableMeshComponent` with the same mesh during the move. It copies the crab's current pose, plays our frames on top, then hands back.
- **The sword, chain and mace.** Meshes are built from triangle lists like the whip's (`sword`, `link` and `mace` `.crabmesh`). The links are laid along the whip's rope physics, every other one turned a quarter, with a variable length.
- **Effects** (lightning, flame arcs, water ribbons, wind crescents) are meshes and trails baked from Blender, glowing like the whip's soul light.
- **Sounds** are made by the same generator as the necromancy sounds and preview-mixed the same way before they ship.
- **Hits** fire the same invisible hit projectile as the Soul Whip bolt, so weapon mods, elements, crits and on-hit effects still apply.
- **Multiplayer:** the host owns cooldowns, Flow, shells and damage. Every machine plays each player's Form animation and effects when told it started, as with the whip's rope.

## 8. Order of work

1. **Two tests in game:**
   - Play one baked animation on the crab through a `PoseableMeshComponent`.
   - Move the crab along a swing (and launch an enemy).
2. **Preview videos in Blender** at real speeds, with sound: Mace Swing, Lightning Step, Fire Wheel.
3. **First version:**
   - M1 string, Flow and Shell Break.
   - 6 Forms: Lightning Step, Fire Wheel, Flowing Cut, Mace Swing, Shellquake, Iron Shell.
   - Rarity and levels.
4. **Second version:** the other 6 Forms, Eightfold Lightning's evolution, and Shellbreaker items (like the whip's 20).

## Open questions

1. **Keys:** are six keys too many on a controller? A radial menu on one button could be an alternative.
2. **Greed rarity:** does Shellbreaker get Greed Forms only in Greed Run, like the other Greed items?
3. **Shell Break:** melee only, or do guns crack shells too? Melee only gives Shellbreaker a clear job.
4. **RE:** does moving the crab ourselves every frame (Mace Swing) work smoothly in multiplayer? That's test 1.
5. **Scaling:** check whether melee damage goes through the perk pool and crits before tuning (see `balance/vanilla-power.md`).
