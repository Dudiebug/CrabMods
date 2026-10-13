# Melee and ability mods: rework

Status: proposal, 2026-10-11. Nothing built. Ideas have not been run through `plans/balance/WORKFLOW.md` yet.

## What's wrong now

**Ability mods (43 vanilla).** There are plenty of them, but they're mostly the same item:
- 24 of the 43 are "abilities explode into X": bubbles, daggers, drills, fireworks, mushrooms, turrets and so on, almost all at BaseBuff 100. They're the same effect (extra damage where the ability lands) with different visuals. Step 1 of the workflow would call most of them variants of each other.
- Only about 7 change *how or when* you use the ability: Grenadier, Heat Sink, Triple Ability, Split Ability, Glue, Chaotic and Bouncing.
- Nothing changes *when* the ability fires, and nothing links it to the weapon or melee.

**Melee mods (12 vanilla).** Too few, and all of them are numbers:
- Damage (Sharp, Iron, Time Claws), range (Big Claws), cooldown (Blender), heal on kill (Vampire), elemental stacks (5 claws) and Brawler.
- Nothing changes what a melee attack *does*. CrabMods' current "Stronger Melee Mods" only raised the numbers.

**Both are side buttons.** They don't talk to the weapon or to each other. Weapon damage keeps compounding through the run (W ≈ 15 late), while melee and ability stay on a 3–10 s cooldown with mostly flat damage. So they fall off late, and 12 slots each never fill. (Unverified: whether melee and ability damage go through the perk pool ADD × MUL and crit. That decides how badly they fall off; trace it before tuning numbers.)

## The rework: three buttons, one build

**1. Give each button a job.**
- **Weapon:** steady damage.
- **Ability:** setup and burst. Gather, mark, prime, then cash in.
- **Melee:** close-range reset and survival. Reload, refund, escape, finish off.

**2. Sort mods into families, and fill the empty ones.**

| Button | Family | What it changes | Vanilla has |
|---|---|---|---|
| Ability | **Payload** | what happens where it lands | 24, plenty: add none |
| Ability | **Cadence** | charges, cooldown, how you cast | Grenadier, Heat Sink |
| Ability | **Trigger** | when it fires (on its own, on events) | none |
| Ability | **Link** | makes the weapon or melee better | none |
| Melee | **Strike** | the move itself (lunge, slam, pull) | Big Claws |
| Melee | **On hit** | what a melee hit does to the target or to you | elemental claws, Vampire |
| Melee | **Defense** | parry, reflect, dodge | none |
| Melee | **Link** | makes the weapon or ability better | none |

**3. Make them a rotation.** The Link mods are the heart of it: shoot, cast to prime, melee to reset, shoot harder. A build that takes Link mods on all three buttons should feel like a combo, not three separate things.

**4. Scaling (needs tracing first).** If melee and ability damage don't scale with the build, give them a share of it (for example, a part of the weapon-mod pool W). Without that, every new damage mod for them is a band-aid.

**5. Loot weights.** Lower the spawn weight of the 24 vanilla payload mods a little, so the new families actually show up. CrabMods already edits loot lists.

## Ideas

First guesses. Numbers and rarities get fixed when they go through the workflow.

### Melee

| Name | Family | Rarity | Idea |
|---|---|---|---|
| Reload Strike | Link | Common | A melee hit reloads 25% of your magazine per level. |
| Marked for Death | Link | Epic | Enemies you melee take +25% weapon damage for 4 s. |
| Lunge | Strike | Epic | Melee dashes you 8 m toward your aim; hitting an enemy refunds half the cooldown. |
| Ground Pound | Strike | Common | Melee in the air slams down: more damage and radius the farther you fell. |
| Magnet Claws | Strike | Common | Melee pulls nearby enemies in to you (sets up the ability). |
| Execution | On hit | Epic | Melee kills non-boss enemies below 15% health outright; executions refund the cooldown. |
| Shatter | On hit | Legendary | Melee consumes every elemental stack on the target for bonus damage per stack. |
| Parry | Defense | Epic | Melee just as an enemy projectile reaches you reflects it at 300% damage. |
| Combo Claws | On hit | Common | Each melee within 1.5 s of the cooldown ending deals +50% more, stacking. |

Riposte, from the last batch, also belongs here (Defense).

### Ability

| Name | Family | Rarity | Idea |
|---|---|---|---|
| Primer | Link | Common | Enemies hit by your ability take +30% weapon damage for 5 s. |
| Loaded | Link | Common | Casting reloads your weapon and gives +25% fire rate for 4 s. |
| Sharpening Stone | Link | Common | Melee kills take 1 s off your ability cooldown. |
| Overcharge | Cadence | Epic | Hold to charge for up to 2 s: up to +100% damage and radius. |
| Detonator | Cadence | Epic | Press again to blow it early; hitting 3+ enemies refunds 30% of the cooldown. |
| Blood Cast | Cadence | Greed | No cooldown, but each cast costs 10% of your current health. |
| Reflex | Trigger | Legendary | Taking a big hit casts your ability at your feet for free (once per cooldown). |
| Killing Spree | Trigger | Epic | Every 25 kills, a free cast lands on the nearest group of enemies. |
| Singularity | Payload+ | Legendary | The ability pulls enemies into its blast point for 1 s before it explodes. |
| Contagion | Link | Epic | The explosion copies the biggest elemental stack in its radius onto every enemy there. |

Echo, from the last batch, is Cadence.

## Open questions

1. How far to go: add new families only, or also re-weight or merge the 24 vanilla payload mods?
2. Scaling: trace melee and ability damage first, then decide whether they get a share of the weapon pool.
3. Feasibility, to check per idea: Parry (enemy projectile hit timing), Overcharge and Detonator (held or second press on the ability key), and Reflex and Killing Spree (forcing a cast without input).
