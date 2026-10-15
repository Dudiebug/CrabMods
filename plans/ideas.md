# Item ideas: backlog

Ideas waiting to be built, written up in the wiki's card format. All addresses are for game build `0x6921D2F7`. **V** means verified in a decompile; **I** means inferred.

---

## Encore (relic, Epic): BUILT

**Activated perks have a chance to activate a second time**

- 15% chance for an activated perk to activate a second time
- Spawn weight: 0.5
- Buff scaling: none (relics don't stack)
- Appears in luck and random chests, as well as loot, chance, gamble and random totems

### What counts as "activated"

Only perks that *do something at a moment* (a kill, a hit, a roll). Always-on stat perks (Collector, Banana, Magnify, ...) have nothing to repeat, so Encore ignores them. The repeat can never repeat itself: one Encore roll per activation, no chains.

| Kind | Examples | What the second activation does |
|---|---|---|
| On elimination | Exploding / Freezing / Flammable / Electric / Poisonous Enemies, Scavenger | The death effect happens again on the same spot (a second explosion, a second drop) |
| Chance on elimination | Autoloader, Finishing Move (new perks, below) | Autoloader: no effect if it already reloaded, so Encore instead **re-rolls a failed proc**. Finishing Move: the bonus crystals drop twice |
| On being hit | Explosive / Poisonous / Flammable Armor, Crimson Haze | The armor effect fires twice |
| Once per island | Grave Dodger | Not a double heal: the once-per-island charge is **refunded**, so it can save you again this island (shows "Encore!") |
| Chance totems | Rigged Totems (new, below) | A losing totem roll gets one free re-roll |
| Instant one-shots | Cursed Lottery (new, below) | **Excluded.** A second 99% max-health hit would kill you, and a second 100000 crystals breaks the economy |

The rule in two lines: if the perk *failed*, Encore gives it a second roll; if it *succeeded* and doing it twice makes sense, it does it twice; one-shot pickups are excluded.

### How it would work

The game has no single "a perk activated" event. Each perk is checked inline in the function where it happens (`re/perk_sites.json`):

- `0x140ca4020` checks Scavenger and all five `*Enemies` perks: the per-enemy elimination handler (**I**).
- `0x140ca7fe0` checks Snatcher, Crimson Haze and the `*Armor` perks: the player-hit handler (**I**).

Plan:

1. Decompile both. If they only dispatch perk effects, hook each one and, on a 15% roll, call the original a second time behind a reentrancy flag (the cheap version covering most perks at once). If they do other work too (scoring, combo, elimination count), hook the per-perk effect call sites inside them instead.
2. Grave Dodger, Autoloader, Rigged Totems and Finishing Move each get their own small hook, since "again" means something different for each (see the table).
3. Roll on the host only. The effects are server side, so clients see the result through normal replication; CrabSync needs nothing new.
4. Feedback: a short "Encore!" popup through the `UpdatePerkBuff` popup path, so players can tell the relic fired.
5. New relic built the usual way: copy a ring from `SpawnablesDA.Relics`, a free type id from 200 up.

`CrabMods.ini`:

```ini
[encore]
enabled = true
# Chance for an activated perk to activate again, in percent.
chance = 15
```

### Open questions

- Is 15% too strong on the elemental `*Enemies` perks in big waves? A second explosion can kill more enemies, which can activate more perks. The no-chain rule stops it repeating forever, but a lower chance (10%) for area effects may be needed.

---

The four perks below come from the same batch of cards. They are new: the game has none of them (`re/perk_sites.json` lists every vanilla perk). Grave Dodger, which was in the batch too, is vanilla (`DA_Perk_GraveDodger`, Epic, Health pool, hyperbolic) and only shows how the cards are laid out. New perks are built the usual way: copy a perk from `SpawnablesDA.Perks`, a free type id from 200 up.

## Autoloader (perk, Rare): BUILT

**Chance to instantly reload after each elimination**

- 10% chance to instantly reload after each elimination
- Spawn weight: 1.0
- Buff scaling: Hyperbolic
- Appears in luck and random chests, as well as loot, chance, gamble and random totems

How: the GM damage handler `0x140ce5780` calls `0x140d26c00` to count a kill and knows the killer (**V**, REPORT.md). Hook it, roll `1 - 1/(1 + 0.1*level)` for the killer, and on a hit fill the current weapon's magazine. Still to find: the weapon's ammo field and the reload/refill function (the reload has to run on the owning client, so it may need a client RPC or a CrabSync message).

## Finishing Move (perk, Rare): BUILT

**Enemies eliminated with critical hits drop more crystals**

- Enemies eliminated with critical hits drop 50% more crystals
- Spawn weight: 1.0
- Buff scaling: Linear
- Appears in economy and random chests, as well as loot, chance, gamble and random totems

How: crits are worked out in `ACrabPS::CalcOutgoingDamage` `0x140d21480` (**V**), which fills `FCrabDamageInfo`. Note on the damage info (or a per-enemy map) that the hit was a crit, and when `0x140ce5780` reports that hit eliminated the enemy, scale its crystal drop by `1 + 0.5*level`. Still to find: where an enemy's crystal drop amount is decided (Bonus Crystals / Crystal Fertilizer read it, so their `perk_sites.json` entries are the starting point).

## Rigged Totems (perk, Rare): BUILT

**Chance based totems are more likely to roll in your favor**

- Chance based totems are 25% more likely to roll in your favor
- Spawn weight: 1.0
- Buff scaling: Linear
- Appears in luck and random chests, as well as loot, chance, gamble and random totems

How: find the chance and gamble totems' success roll (they share the totem class with the key totem `key_totem.cpp` already hooks). Multiply the success chance by `1 + 0.25*level`, capped at 100%. Sturdy Totems (vanilla) is a totem perk too, so its `perk_sites.json` entries are a good place to start looking.

## Cursed Lottery (perk, Legendary, cursed): BUILT

**Gain a gigantic amount of crystals but lose most of your max health**

- Gain 100000 crystals but lose 99% max health
- Spawn weight: 0.15
- Buff scaling: Linear or Exponential (crystals)
- Debuff scaling: Hyperbolic (health)
- Appears in greed chests, as well as greed and random totems

How:
- Crystals: once, on pickup (hook `ACrabPS::AddPerk` `0x140d20520`): add `100000 * level`, or `100000 * 2^(level-1)` for exponential. One-shot, so a second copy pays again but nothing pays twice for one pickup.
- Health: `UpdateMaxHealth` already subtracts penalty perk buffs (`(1 - buff 0x6d - buff 0x76) * ...`, clamped to 0.01, **V**). Add Cursed Lottery's penalty to that call's result the same way CrabMods adds max-health multipliers (return address `0x140d28ad3`). Hyperbolic: `0.99 * level / (1 + ...)`, tuned so it never reaches 100%. The game's `max(0.01, ...)` floor and minimum of 1 health keep you alive.
- Open question: is the max-health loss permanent for the run, or only while you hold the perk? It's a perk, so holding it is the natural rule (selling/removing it gives the health back).

---

# Batch 2026-10-11: run through `plans/balance/WORKFLOW.md`

Twelve ideas, each checked against vanilla by the workflow (steps 1–5). Numbers are the **fixed** ones, not the first pitch. Mid-run means weapon-mod pool W ≈ 4.5, perk pool ADD ≈ 1.6. Verdicts: 8 pass with changes, 4 need rework.

## Vendetta (perk, Epic): PASS WITH CHANGES

**The enemy that last hurt you takes more damage from you**

- +50% damage (ADD) to the last Medium-or-larger enemy that damaged you, until it dies. A halo marks it.
- Spawn weight: 1.0    Scaling: Linear
- Appears in: Damage, Epic and Random chests

**Different from:** Elemental Specialist (+50% vs one-debuff targets), Silver Lining (rewards being hit). New trigger: *who* hit you picks your target.
**Number:** based on Elemental Specialist +50%. On the target (1.6+0.5)/1.6 = ×1.31; in a horde ≈ ×1.05, vs bosses and Elites ≈ ×1.2–1.3. Pitched +100% was ×1.63 on bosses, too strong.
**Vs vanilla:** Assassin < Silver Lining < **Vendetta** < Gemstone (×1.35) < Collector (×1.23–1.47). Beats Danger Close on bosses only.
**Break it:** fodder could steal the mark → Medium+ attackers only.
**Fun:** visible halo, play toward it (let the Elite hit you, then burst it), pairs with tank builds.
**How:** instigator from TakeDamage; bonus in CalcOutgoingDamage. Unverified: instigator for projectiles and AoE.

## Echo (ability mod, Epic): PASS WITH CHANGES

**Your ability strikes again a moment later**

- Your ability fires a second time 1.5 s later, **at wherever you aim then**, for 50% damage.
- Spawn weight: 0.75    Scaling: none, or +25% echo damage per level
- Appears in: Damage and Skill chests

**Different from:** Layered Explosion (25% chance to explode twice), Clone Explosion. "Same spot" was just a guaranteed Layered Explosion; re-aiming the second hit makes it new.
**Number:** ×1.5 ability damage, the same as Triple Ability's expected value. Too small for Legendary (≈ ×2), so it was moved down to Epic.
**Vs vanilla:** Layered Explosion ×1.25 < Time Explosion ≈ ×1.4 < **Echo ×1.5** = Triple Ability < Split Ability ≈ ×1.9 < Clone Explosion ≈ ×2.
**Break it:** echo re-triggering Clone, Split or Targeting copies, or a second turret → damage-only, no copies, no turrets, never echoes itself. Doesn't drop for Soul Whip holders.
**Fun:** visible second blast; you place the second hit.
**How:** hook ability fire, schedule a second cast. Unverified: forcing a cast without input; ability base damage.

## Most Wanted (perk, Epic): PASS WITH CHANGES

(Pitched as "Bounty"; renamed, since vanilla has Bounty Hunter.)

**Hunt a marked enemy each island for permanent damage**

- Each island one Medium-or-larger enemy is marked. Killing it gives +3% damage (ADD), permanently.
- Spawn weight: 1.0    Scaling: Linear
- Appears in: Damage and Epic chests

**Different from:** Gemstone (+4% per island, automatic), Checklist. New: you have to hunt one particular enemy.
**Number:** based on Gemstone 4%. Island 14: +42% = ×1.26; island 28: ≈ ×1.5. Pitched 5% beat Gemstone while being conditional.
**Vs vanilla:** Assassin < Silver Lining < **Most Wanted ×1.26** < Gemstone ×1.35 < Collector.
**Break it:** fodder mark = free → Medium+ only. Undead kills count as yours → only your own non-undead kills pay.
**Fun:** halo, hunt, focus fire.
**How:** pick from the spawned list at island start; add to DamageMultiplier like Gemstone. Unverified: how Gemstone stores its value.

## Overclock (relic, Greed): PASS WITH CHANGES

**Much faster fire rate, but your weapon overheats**

- +75% fire rate. After 3 s of continuous firing it overheats and can't fire for 2 s. Heat only cools while idle; reloading doesn't cool it. Heat bar on screen.
- Spawn weight: 0.3    Scaling: none
- Appears in: Greed chests and shrines, shop Greed injection

**Different from:** Trigger Ring (+250% rate, −90% damage), Rapid Fire / Supercharged. New: the drawback is a heat game, not a damage tax.
**Number:** AutoRifle ×1.46, LMG ≈ ×1.25, Minigun ≈ ×1.15 (big clips hit the heat). Pitched +100% ≈ ×1.6, near Glass Cannon with a weaker drawback.
**Vs vanilla:** Trigger Ring ×0.35 < Big Bones etc. ×1.25 < **Overclock ×1.15–1.46** < Glass Cannon ×1.75.
**Break it:** Ring of Power (−80% fire rate, ×5 damage) if fire rate adds: 0.2 + 0.75 → ×4.75. Apply Overclock multiplicatively or make them exclusive. Small-clip guns barely feel the heat; consider a 2.5 s window.
**Fun:** heat bar, burst-fire play, the drawback changes how you shoot.
**How:** fire-rate buff + a can't-fire gate. **Trace UpdateFireRate 0x140d69f70 first.**

## Pack Tactics (perk, Common): PASS WITH CHANGES

**Deal more damage near your teammates**

- +10% damage (ADD) per living teammate within 3000u. Doesn't drop in solo games. A counter shows the count.
- Spawn weight: 1.0    Scaling: Linear
- Appears in: Damage chests

**Different from:** Community Support (fire rate per nearby holder), Last Crab Standing. New: rewards sticking together.
**Number:** based on Danger Close +30%. 4 players ×1.19, 3 players ×1.125, 2 players ×1.06 (~70% uptime knocks each down a bit). Pitched 15% was ×1.28 at 4 players, Epic-sized.
**Vs vanilla:** Equalizer ×1.09 < **Pack Tactics ×1.06–1.19** ≈ Danger Close ×1.19 ≈ Eagle Eye.
**Break it:** dead solo → no solo drop. Downed teammates excluded.
**Fun:** play toward it (stay together); needs the counter to be visible.
**How:** teammate distances from the player array, added in the ADD step.

## Last Round (weapon mod, Common): REWORK, maybe drop

**The last shot in your magazine hits much harder**

- Reworked: +50% damage per round of clip size on the final shot, capped at +1000%.
- Spawn weight: 1.0    Scaling: Linear
- Appears in: Damage chests

**Different from:** Uppercut (+250% every 5th shot), Mag Shot. It's Uppercut with the counter set to the clip size, so it's borderline on step 1.
**Number:** pitched +5% per round averages +5% per shot on *every* gun = ×1.01, a dead pick. Reworked: Sniper average +50%, AutoRifle +29%, LMG +16%, Minigun +5%.
**Vs vanilla:** **pitched** < every Common; reworked ≈ Chaotic Shot / Uppercut ×1.11, and only on small clips.
**Break it:** dead as pitched. All You Can Eat / Efficient Shot never reach the last round. Overkill wastes the spike.
**Fun:** big popup, hold the last round for the Elite.
**Decision needed:** keep the rework, or drop it as too close to Uppercut.

## Last Gasp (perk, Epic): PASS WITH CHANGES

(Pitched as "Second Wind", then "Adrenaline"; renamed, since Second Wind is planned for a necro item in `necromancer-loot.md` and Adrenaline Amulet is a vanilla relic.)

**A burst of health and speed when you're nearly dead**

- Below 25% health: heal 15% max health and +50% move speed for 3 s. Once per island.
- Spawn weight: 1.0    Scaling: Linear on the heal
- Appears in: Health chests

**Different from:** Grave Dodger (20% heal at 1 HP, saves from a lethal hit), Speed Demon. Triggers earlier, adds speed, doesn't stop death. Close to Grave Dodger, but different enough.
**Number:** based on Grave Dodger 20%. At 20% it tied or beat Grave Dodger and added speed, so it was cut to 15%.
**Vs vanilla:** Amber Resin < Grim Reaper < **Last Gasp** < Grave Dodger < Survivor.
**Break it:** Unverified: with CrabSync's health pool, is 25% measured on the shared pool or your own?
**Fun:** visible speed burst; pulls toward health builds.

## Riposte (melee mod, Epic): PASS WITH CHANGES

**Strike back hard right after you get hit**

- A melee attack within 1.5 s of taking damage deals 3× damage and refunds the melee cooldown. One use per hit; the multiplier caps at ×7 with levels.
- Spawn weight: 0.75    Scaling: Linear (×1+2L, max ×7)
- Appears in: Damage chests

**Different from:** Silver Lining (a ramp from hits), Vampire. No vanilla item triggers on "I was just hit".
**Number:** Claw at ADD 1.6: ≈ 48–80 DPS for one hit taken every 10–6 s. Sharp Claws ≈ 53, Iron Claws ≈ 133.
**Vs vanilla:** Fire/Poison Claws < Sharp Claws < Ice/Lightning/Arcane Claws < **Riposte** < Iron Claws.
**Break it:** the refunded swing must not reopen the window. Level 5 would be ×11 → cap ×7. Vampire + Overflow combo already capped. Unverified: which pool melee damage goes through; whether blocked hits count (suggest damage > 0 only).
**Fun:** cooldown-refund flash, play toward the window.

## Ricochet (weapon mod, Legendary): BUILT (beta, untested), flat 50% chosen
(Pitched as "Trick Shot", a vanilla name, then reworked from "bounce off a wall, then home in". This is the user's
version, 2026-10-14.)

**Your shots ricochet from enemy to enemy**

- A shot that hits an enemy ricochets to the nearest other enemy within 15 m, then on from there: 1 ricochet, +1 per
  level, at most 4. Each ricochet deals 50% of the shot's damage (chosen 2026-10-14 over halving per bounce; see below).
- Spawn weight: 1.0    Levels: Limited (not offered once you have 4 ricochets)
- Pool: outside (extra hits, after every multiplier, so it doesn't dilute)
- Appears in: Damage and Skill chests

**Different from:** Targeting Shot (Epic, a *chance* to re-aim one shot on impact), Piercing Shot (Epic, full damage,
but only enemies in a line), Bouncing Shot (Common, bounces off walls), Homing Shot (Legendary, a chance to curve, hits
once). Nothing vanilla guarantees a hit jumps to a new enemy.
**Number:** a multiplier on hits, like Firepower. p = chance a ricochet finds an enemy within 15 m [G]: horde 0.8,
mixed fight 0.5, lone boss ≈ 0. Value = 1 + Σ (0.5·p)^k for k = 1..ricochets:

| Ricochets | Horde (p 0.8) | Mixed (p 0.5) | Boss |
|---|---|---|---|
| 1 | ×1.40 | ×1.25 | ×1.0 |
| 2 | ×1.56 | ×1.31 | ×1.0 |
| 3 | ×1.62 | ×1.33 | ×1.0 |
| 4 | ×1.65 | ×1.33 | ×1.0 |

**Vs vanilla:** Legendary bar is ×1.3–2 (a new damage source or more projectiles). Homing Shot ≈ ×1.1 < **Ricochet L1
×1.25–1.4** < Firepower ×1.33–2. One down: Piercing Shot / Targeting Shot (Epic) are about the same at L1 in a line or a
crowd, so L1 is a weak Legendary. One up: n/a.
**Decision open: halving per bounce makes levels 3 and 4 almost worthless** (+3% and +1% in a horde), which step 2
counts as a dead level. Options, all with the 4-ricochet cap:
  - *As pitched* (each ricochet half the last): L4 ×1.33–1.65. Safe, but only level 2 is worth taking.
  - *Each ricochet 50% of the shot* (flat, not compounding): ×1.25/1.38/1.44/1.47 mixed, ×1.40/1.72/1.98/2.18 horde. Every
    level matters; L4 in a horde is top of the Legendary band. **Recommended.**
  - *Each ricochet 75% of the last* (`falloffPercent = 25`): ×1.38/1.52/1.57/1.59 mixed, L4 ×2.30 horde. Strongest.
**Run:** worth a slot when offered, best in biomes 1–3 hordes. It's a multiplier on hits, so it keeps up with enemy
health, and the cap stops it running away. Weak on bosses by design (unless they have adds).
**Break it:**
  - *Piercing Shot*: every pierced enemy starting its own chain multiplies → one chain per shot, from the first enemy hit.
  - *Procs*: if ricochet hits roll the weapon's procs, Proc Generator and elemental chances multiply by up to ×2 → ricochets
    carry damage and the shot's elements at the ricochet's share, but don't roll procs or count as a "shot".
  - *Explosions and ticking damage*: Rocket / Cluster splash, Flamethrower and Lightning Scepter hit many times → direct
    projectile hits only (`DamageInfo_Type` not Radial, not melee); exclude Flamethrower; check the Scepter.
  - Soul Whip (necro) excluded. Thermal Shock: a ricochet can shock (fine; its cooldown is per enemy).
  - 4 players: more enemies → p goes up a little; fine.
**Fun:** you see the arcs (needs a visual); you play toward packed enemies; it changes picks toward Firepower/Split Shot
(more shots → more chains).
**How (built, `crabmods/items/ricochet.cpp`):** host. A direct hit (`DamageInfo_Type` 0) from the holder's character
starts a chain, unless they hold the Flamethrower. Its pre-buff damage comes from `OnOutgoingDamageBase`. Every
`delay` (0.08 s) the chain jumps to the nearest live enemy within `range` that it hasn't hit, skipping undead, and hits
it through `UCrabHC_TakeDamage` from the player's character, as type 4 (Lesser, not throttled per enemy), for
`damagePercent` of that base. The game then applies the player's buffs and crit again, as for any hit, and the kill is
theirs. Ricochets start no chains and carry no elements.
**Differs from the card:** every direct hit starts a chain, pierced ones too. It's a per-hit multiplier, like a damage
bonus, so Piercing Shot gains the same ×1.25–2.2 from it as from any other hit. There's no tracer yet: the damage
numbers on each enemy are the only visual.
**Unverified:** that `TakeDamage` runs the player's buffs for a ricochet (the log's first 5 ricochets show the damage
before and after); whether clients see the damage numbers; p per fight type; ricochet range (15 m [G]).

## Inertia (perk, Common): PASS WITH CHANGES (renamed from Momentum: Shellbreaker's loot has a Momentum)

**Deal more damage while you keep moving**

- After 2 s of moving without stopping: +25% damage (ADD) until you stand still.
- Spawn weight: 1.0    Scaling: Linear
- Appears in: Damage and Speed chests

**Different from:** Speed Is Power (damage per bonus speed), Untouchable. New trigger: movement state.
**Number:** based on Danger Close / Vitality 30%. You move most of the time, so 25%: ×1.156 peak, ≈ ×1.125 at ~80% uptime.
**Vs vanilla:** Equalizer ×1.09 ≈ Danger Close < **Momentum ×1.125** < Vitality < Power Armor.
**Break it:** high uptime → 25% not 30%. Unverified: what counts as moving (dash, slide, airborne).
**Fun:** counter, kiting.

## Gambler's Tray (perk, Greed): REWORK

(Pitched as "High Roller"; renamed, since that's a vanilla Greed relic.)

**Greed items sneak into your chests**

- Reworked: each chest has a 15% chance per level (max 60%) that one choice is **replaced** by a Greed item. At most one per island.
- Spawn weight: 1.0    Scaling: Linear, capped
- Appears in: Greed chests and shrines

**Different from:** Limited Loot (−1 choice for crystals), Big Chests (+1 choice). Gives access to Greed items, which vanilla offers ~0.5 times a run.
**Number:** pitched "+1 choice and a Greed item" had no drawback, which breaks the Greed rule. Reworked: ≈ 4 Greed offers per run at L1; the cost is a lost normal choice.
**Vs vanilla:** Double-Edged Sword < Slippery Slope < Limited Loot < **Gambler's Tray** < Glass Cannon.
**Break it:** Double or Nothing / Lucky Charm extra chests → one per island cap. Host-side only. Unverified: whether a Greed item can be put into a normal chest roll (`key_totem.cpp` spawns pickups by hand).
**Fun:** a visible Greed card in the chest; a real choice.

## Salvage Rights (perk, Common): PASS WITH CHANGES, trace first

**Salvaging items pays out crystals**

- When you salvage an item: crystals equal to 25% of its shop price per item level, capped at 75%. Greed and key-chest items excluded.
- Spawn weight: 1.0    Scaling: Linear, capped
- Appears in: Economy chests

**Different from:** Paycheck (20% of a Common's price per island), Snatcher. New trigger: salvage.
**Number:** based on Paycheck. One salvage per island ≈ 1.25× Paycheck. Without the cap, buying, levelling and salvaging an item makes a profit.
**Vs vanilla:** Tony's Black Card < Crystal Combo < **Salvage Rights** < Paycheck < Snatcher.
**Break it:** buy-salvage loop → cap. Must not pay on Cursed Lottery buys. Faulty Chests leftovers.
**Do not build until** vanilla salvage is traced: what it refunds (`PickupInfo_SalvageReward`) and whether owned items can be salvaged.

---

## Snap Decision (perk, Greed): BUILT (beta, untested)

**Each biome, a coin flip: half your items double in level, or half of them vanish**

- Once each biome (at the first island you clear in it): 50% chance that 50% of your mods and perks (rounded up) double in level; otherwise 50% of them (rounded down) are destroyed. Relics, charms and Snap Decision itself are never touched.
- Spawn weight: Glass Cannon's    Scaling: none (more levels do nothing)
- Appears in: Greed chests and shrines

**Different from:** High Roller (a 33/33/33 flip, but on chests), Level Up (Legendary: random items +1 level every 3rd island). Neither gambles with what you already hold. The trigger is new (a biome), and so is the effect (your inventory is the stake).
**Number:** based on High Roller's coin flip and Level Up's level-ups. Mid-run (weapon-mod damage ≈ 4.5 = 1 + 3.5 from mods, perk damage ≈ 1.6 = 1 + 0.6):
- heads: (1 + 3.5 × 1.5) / 4.5 = ×1.39 and (1 + 0.6 × 1.5) / 1.6 = ×1.19, so **×1.65**
- tails: (1 + 3.5 × 0.5) / 4.5 = ×0.61 and (1 + 0.3) / 1.6 = ×0.81, so **×0.50** (but the freed slots refill)
- average ×1.08; geometric √(1.65 × 0.5) = ×0.91. A true gamble, a little worse than even before the refill.
- level 5: same (no scaling). Proc Generator: no chance to scale. Island 28: about 3 flips if picked up in biome 1–2; levels cap at 255.

**Vs vanilla:** Up The Ante (×1.25, more enemies) < Double Trouble < **Snap Decision** (×0.5 or ×1.65) < Glass Cannon (×1.75, little health). One rarity down: no Legendary does this; Level Up (Legendary) gives about +9 levels a run for free, and a single heads mid-run gives about +15.
**Break it:**
- Grows forever? A run of heads compounds: 3 heads in a row (12.5%) is ×8 on the items that stay picked. That's the gamble Greed is for, so no fix for now. If it's too strong, add a cap (`maxDoublesPerRun`).
- Drop it right after heads to dodge the next tails. That costs you the perk, so it's fair.
- Solo or 4 players: each holder flips on their own.
- CrabSync's shared-pickup copies go through GivePickup, and this uses the adders and removers, so no copies. Unverified with CrabSync on.
**Fun:** you see it (a message, and a note in the inventory menu); it changes what you pick next (pile up levels before the next biome or not); the drawback changes how you play (do you keep it for another flip?).
**How:** host, `OnRewardChests` at the first clear in each biome (`LM+0x358` BiomeDA changes; the island number going down is a new run). Doubling re-adds the item at its own level (the adder merges, V `ACrabPS_Add*`); destroying uses `ACrabPS_Remove*` by type. `items/snap_decision.cpp`, `items/inventory_edit.hpp`.
**Unverified:** that the first island cleared is the right moment, rather than arriving in the biome. The pickup popups (one per doubled item, from the adders).

## Wild Card (relic, Greed): BUILT (beta, untested)

**Each island, a 20% chance to randomize your entire inventory, like a random totem**

- 20% chance when clearing each island: every mod, perk and relic becomes a random other one of the same kind, at the same level. Wild Card and charms stay; no Greed items roll in (the game's Randomize totem leaves them out too).
- Spawn weight: 0.3 (High Roller's)    Scaling: none (a relic)
- Appears in: Greed chests and shrines

**Different from:** the Randomize totem (once, your choice, where it stands; this is random and keeps happening), High Roller (gambles chests, not your build). You stop building synergies and play what you're given.
**Number:** based on High Roller's chance (33%). Lowered to 20% because a reroll is a much bigger swing than a missing chest.
- About 28 × 0.2 ≈ 5.6 rerolls a run from island 0, or about 3 if found at island 14.
- **The upside is rarity.** A pick is uniform over the game's lists, not weighted the way chests are: 26 of 93 weapon mods are Legendary (28%), 15 of 95 non-Greed perks (16%), 19 of 43 ability mods (44%). A 10-weapon-mod build rerolls into about 3 Legendaries. A Legendary chest offers one about 0.25% of the time (vanilla-power).
- **The downside is synergy and Greed.** The Greed perks you hold become non-Greed ones, and an elemental or crit build loses its core.
- Mid-run, guessed: about ×1.3 from the rarity boost, about ×0.7 from the lost synergy, so roughly even on average with a huge spread. Level 5, Proc Generator: n/a. Island 28: the same.

**Vs vanilla (Greed relics):** Upgrade Ring < Ring of Favoritism < High Roller < **Wild Card** < Trigger Ring (guessed; relic values aren't worked out in vanilla-power).
**Break it:**
- **Skips progression?** Likely yes. The uniform pick makes every reroll a rarity jackpot (step 4: economy). Proposed fix: pick weighted by `SpawnWeight` and the chest rarity odds instead of uniformly. That isn't "like a random totem" any more, so it's your call. The beta does what the totem does: uniform.
- Combos: Snap Decision then Wild Card keeps the doubled levels on new items. That's fine, since both are Greed.
- The necromancer's items only roll for Soul Whip holders.
- Solo or 4 players: each holder rolls on their own.
**Fun:** you see it (a message and every slot changing); it changes what you pick next (why invest in synergy?); the drawback changes how you play (adapt every island).
**How:** host, `OnRewardChests` on each clear. Remove each item, then add a random one not held at the same level (`ACrabPS_Remove*` / `Add*`). It doesn't call the game's `ACrabPS 0x140d27a70` (the Randomize totem's function, V): that one deletes charms and Greed items (this relic too) and lets two slots roll the same item. `items/wild_card.cpp`.
**Unverified:** the stateful perks (accumulated buffs, collectors) when removed by code; and the many pickup popups.

## Shellbreaker's loot (12 items: weapon mods, perks, relics): BUILT (beta, untested)

All of them drop only for players holding Shellbreaker. Full table: plans/shellbreaker.md.

**Different from:** vanilla has no chain, no hook and no swing string, so each item tunes one of Shellbreaker's own
parts. (The perk was first called Momentum; it's Riptide now, so the unbuilt "Momentum (perk, Common)" card above
keeps its name.)

**Numbers.** A swing's share of the string's damage is its weight ÷ 5. Vanilla anchors are from
re/pickup_catalog.json.

*Damage items*
- **Executioner** (Epic, +75% on swings 3 and 4). Those swings are (0.91 + 0.68) / 5 = 31.8% of the string, so
  the string gets ×1.24 per level, multiplicatively.
  - First tried at 40% (×1.13), below Supercharged's ×1.25, so it was raised to 75%. Epic, beside **Supercharged**
    (Epic, +25% fire rate = ×1.25).
- **Rhythm** (Common, +30% on the circle). The circle is 1.59 / 5 = 31.8% of the string, so ×1.095 per level.
  - Next to **Sharpshooter** (Common, +6% crit ≈ ×1.06) and under **Heavy Hitter** (Common, +150% but slower fire).
- **Riptide** (Epic, +35% for 4 s after a landing). It's an additive damage bonus. Mid-run perk damage ≈ 1.6, so
  1.95 / 1.6 = ×1.22 while it's up.
  - Uptime is about 4/6 with one charge and about 100% with Grappler, so ×1.15 to ×1.22.
  - First tried at 20% (×1.08 average), too weak for Epic, so it was raised to 35%.
  - Under **Glass Cannon** (Greed, +75% global).

*Area and control*
- **Wide Arc** (Common, +15% radius) is ×1.32 area per level. That only means more damage with several enemies in
  range. Common.
- **Long Chain** (Common, +4 links) gives +20% reach and +20% hook range, and the circle covers ×1.44 the area.
  Common.
- **Whirlwind** (Epic) is control, not damage: it groups enemies for the next swings. Bosses are left out.

*Elements*
- **Searing Edge** (Epic, 2 fire stacks on every swing hit). Swings come about 4.5 times a second. **Fire Shot** is
  Common and only a 3% chance per level, so this is far more reliable. That is why it's Epic, not Common.
- **Frost Chain** (Epic, 10 ice per hook hit, about every 6 s) is a burst of control on one target.

*Hook*
- **Undertow** (Common, +75%) and **Quick Draw** (Common, ×1.15 rate, floor ⅙) only touch the hook. Hook damage is
  20 per 6 s, about 3 DPS, so they're utility; Common.
- **Grappler** (Legendary, +1 charge): two hooks in a row means two yanks or two mobility moves. Legendary, like
  vanilla's extra charges.
- **Kraken's Grasp** (Legendary): yanks reach elites, and 2 more enemies come along. It's a crowd pull, so
  Legendary.

**Break it**
- Long Chain grows without limit: 10 levels would be a 3× reach. Fixed: the factor is capped at ×2.5
  (8 levels at the default), which also bounds Whirlwind's drag radius.
- Solo or 4 players: everything is per holder, and the host applies the hook's effects.

---

## Thermal Shock (weapon mod, Epic): BUILT (beta, untested)

**Hitting a burning enemy with ice, or a frozen one with fire, shatters both for a burst of damage**

- If a weapon hit applies fire to an enemy that already has ice stacks, or ice to an enemy that already has fire
  stacks, the hit deals +250% damage and all of that enemy's fire and ice stacks are removed. Each enemy can be
  shocked once every 1.5 s. Explosion and splash hits get +100% instead.
- Spawn weight: 1 (1 of 24.5 Epic weapon-mod weight)    Levels: the cooldown shrinks, 1.5 s / (1 + 0.25(L − 1)),
  Limited at 0.5 s (level 9)
- Pool: outside (a multiplier on the finished hit, after crit, like Death Mark)
- Appears in: Elemental and Epic chests

**Different from:** Elemental Expert (+X per debuff type the target holds) and Elemental Specialist (+50% against
one debuff type). Both are passive bonuses for *holding* statuses. Thermal Shock *spends* them: the trigger (the
opposite element landing) and the effect (a burst that removes the stacks) are new. Vanilla treats fire and ice as
opposites (Fire Ring zeroes ice, Ice Ring zeroes fire [C, vanilla-power §2]); this is the first item that rewards
using both.
**Number:** pitched at +300%, with no cooldown, triggering on any hit while both elements were present. Mid-run
(W ≈ 4.5 [G]) that's AutoRifle ≈ ×1.15, Flamethrower ≈ ×1.26 and Sniper ≈ ×1.43, but **Proc Generator ≈ ×4**:
every hit applies both elements (roll ×100 [C]), so every hit shocks. Fixed:
- a 1.5 s cooldown per enemy (shared by all players);
- the stacks must already be on the enemy *before* the hit, so one hit applying both elements can't trigger it;
- 300% → 250%, and splash and explosion hits get only +100%, so one Fire Explosion can't shock a whole iced crowd
  at full strength.

Value = 1 + 2.5 × (share of hits that shock). Elemental shots are 3% × the weapon's roll multiplier [C], so
reapplying both elements after a shock takes:

| Weapon (Fire Shot + Ice Shot, L1) | Hits per shock | Value |
|---|---|---|
| AutoRifle (7.5% each, 10/s) | ≈ 20 (reapplying), 15 at L2 elements (cooldown) | ×1.11–1.17 |
| Flamethrower (fire built in; ice 8.6%, 12/s) | ≈ 18 (cooldown) | ×1.14 |
| Proc Generator (both every hit, 8/s) | 12 (cooldown) | ×1.21; L5 ×1.42, L9 ×1.63 |
| Sniper (21% each, 1/s) | ≈ 7–8 (reapplying) | ×1.33–1.36 |

The value doesn't drop as the build fills (it's outside the W pool), so islands 7 / 14 / 28 are the same:
×1.11–1.36.
**Vs vanilla (Epic weapon mods, mid-run, [C] vanilla-power §3):** Juiced ×1.18 ≈ **Thermal Shock ×1.11–1.36** <
Supercharged ×1.25 < Damage Shot ×1.33 < Triple Shot ×1.35. Above the Commons (×1.11–1.22), below Firepower (×1.33–2.0).
On Sniper it's at the top of the Epics, like every chance item there (roll ×7).
**Run:**
- Dead without an element source. Island 3: only with Flamethrower, Ice Staff or an elemental shot already held.
  That's fine for an Epic in Elemental chests, which are where the elements come from.
- Island 14 and later it keeps up, while W-pool Epics fade: Damage Shot at W ≈ 15 is ×1.10, this is still ×1.11–1.36.
  It doesn't run away: the cooldown fixes the shock rate, and levels only shorten the cooldown, to a 0.5 s floor.
  Islands 35 / 57: the same multiplier; the enemy-health bar is met by the rest of the build.
**Break it:**
- Proc Generator: fixed (cooldown, stacks must already be there). It's ×1.63 at level 9, which is in line with every
  other proc item on that gun.
- AoE: Fire / Ice Explosion on a pre-iced crowd shocks every enemy at once → splash and explosions +100%. [G]
- 4 players: one cooldown per enemy, not per player, so 4 holders can't shock one target 4×. Any player's stacks
  count, which makes a fire player and an ice player a combo.
- Encore: a weapon mod, not a perk, so Encore doesn't repeat it. Necro undead and turret hits don't trigger (the
  hit must come from the holder's own weapon).
- Fire Ring or Ice Ring zeroes one element's damage, but the stacks still trigger the shock. That's fine: it's
  still a choice between the ring and the burst.
**Fun:**
- You see it: a steam burst and a "Shock!" popup.
- You play toward it: hit with one element, then the other.
- It changes what you pick next: the opposite element.
- Its drawback changes how you play: the shock removes the burn's damage over time and ends freezes early, so you
  choose when to cash in.
**How:** host, hook `ACrabPS::CalcOutgoingDamage` `0x140d21480`. Read the hit's `DamageInfo_Debuffs` (+0x20,
FCrabDebuff: Type 1 Ice / 2 Fire) and the target character's `DebuffState` (`ACrabC+0x6E8`: IceStack +0,
FireStack +4, replicated). If the hit adds one element and the target already holds the other, and the target's
cooldown is up: Damage (+0x08) ×3.5 (×2 if `DamageInfo_Type` is 1 Radial), zero IceStack and FireStack, drop the
hit's fire and ice debuffs, and call `OnRep_DebuffState` (`0x140cbbae0`) for the host's own screen. Melee hits are
excluded, and so are undead and turrets (`DamageInfo_DamagingC` +0x40 must be the holder's character). Not offered
once the cooldown is at its floor (LootWeight 0).
`items/thermal_shock.cpp`; ini keys `bonusPercent = 250`, `splashPercent = 100`, `cooldown = 1.5`,
`cooldownPerLevel = 25`, `minCooldown = 0.5`.
**Unverified:**
- Whether `DebuffState` is updated before or after CalcOutgoingDamage. This decides whether "already there" can be
  read directly.
- How many stacks each element proc adds, and what removing them costs (burn damage lost, freeze shortened).
- Elemental uptime per weapon: the hits-per-shock table assumes L1 Fire Shot + Ice Shot [G].
- Whether burn ticks (damage types 7–10) go through CalcOutgoingDamage. They must not trigger the shock.
- The splash +100% [G]. All of these go to `balanceLog` (the share of damage, shocks per island).


## Scrapper (perk, Epic): BUILT (untested)

**Skip your reward chests to upgrade a later one**

- Leaving an island without opening your reward chest (or opening it and taking nothing) gives 1 scrap. At 3 scrap, your
  next reward chest goes up a tier and the scrap is spent: a pool chest (Damage, Crit, ...) becomes an Epic chest, an Epic
  chest (elite islands) a Legendary chest. A Legendary chest keeps the scrap for the next one.
- The skips must be in a row: taking anything from a reward chest puts scrap back to 0.
- Level 2: 2 scrap per upgrade. Limited: not offered after level 2 (1 scrap would make every other chest Epic).
- Spawn weight: 1 (like most perks)    Levels: Limited (2)
- Pool: outside (loot)
- Appears in: luck, economy and random chests; loot, chance, gamble and random totems

**Different from:** Upgrade Ring (every choice becomes a level-up, 2 fewer choices), Streamer Loot (a % chance at a better
roll), because it's a trade you choose: a pick now for a better chest later. Nothing in vanilla rewards skipping.
**Number:** based on the game's chest tiers [C, `vanilla-difficulty.md` §3]: 3 Common picks for one Epic chest (3 Epic
choices). An Epic is worth about 2× a Common [C, WORKFLOW §2], so early it's a loss (3 → about 2), and it only pays when
the picks you skip are worth little. Elite islands (7, 14, 21, ...) are the jackpot: 3 skips turn their Epic chest into a
Legendary one (4 choices) [C].
**Vs vanilla:** extra-choice items are the closest: Limited Loot (fewer choices) < Big Chests < **Scrapper** < Coral Amulet
(+2 choices, relic). One rarity down: no Common changes chest tiers. One up: Streamer Loot's forced rolls.
**Run:** weak when offered in biome 1 (every pick still fills a slot). It grows as slots fill: once a category is full,
a chest of new items you can't take is a free scrap [C, `count < NumSlots`]. So it rises late and never runs away: at most
one upgrade per 2–3 islands.
**Break it:** economy: an Epic chest every 2 islands late is about +1 Epic pick per 2 islands; fine at Epic [G].
Multiplayer: each player has their own chest, scrap is per player [C, 0x140d01610]. Double or Nothing: zero chests isn't a
skip (no chest to leave); doubled chests give one scrap per untaken chest, so cap it at 1 scrap per island.
Bonus chests (Care Package, totems, Lucky Charm): not reward chests, don't count.
Faulty Chests (Legendary perk: 15% per level that each unchosen item stays [C, DA]): it only acts once you take
something, which already uses the chest, so it can't give a pick and a scrap. Leftovers it keeps are still the chest's.
**Fun:** you play toward it (skip chests on purpose), it changes what you pick, and a counter shows the scrap
("Scrapper 2/3").
**How:** host. `OnRewardChests` (0x140d01610, as Double or Nothing) notes each player's reward chest; the chest roll
(0x140caf1c0, opener) notes the pickups it spawns (`SpawnPickup` 0x140d00060 calls during the roll), and only taking
one of those (0x140d3cf00) marks the chest used. Pickups from anywhere else (totems, shop, the ground) don't count. At the next `OnRewardChests`, an unused chest from
the last island gives a scrap; with enough scrap, swap the main chest class for SpawnablesDA +0x588 (Epic) or +0x590
(Legendary).
**Unverified:** whether a chest left on the island is still alive (and readable) when the next island's rewards spawn,
or needs noting at portal time; how often slots are full by biome 3 [M, balanceLog].

## Connoisseur (relic, Epic): BUILT (untested)

**Items you already hold come with an extra level**

- Taking an item you already hold (from a chest, shop or totem) gives it 2 levels instead of 1.
- Chosen Epic by the user (2026-10-14).
- Spawn weight: 0.3 (like most relics)    Levels: none (relic)
- Pool: outside (levels)
- Appears in: relic chests

**Different from:** Upgrade Ring (Greed: every choice is a level-up, 2 fewer choices), Level Up (Legendary perk: random
items +1 level every 3rd island), because it doubles levels you choose, not random ones, and costs no choices.
**Number:** late in a run most picks are levels on items you hold (slots are full) [C, `count < NumSlots`]. Guess about
10–20 such picks a run [G], so +10–20 levels, against Level Up's ~+9 (Legendary) [G, `ideas.md` Snap Decision].
**Vs vanilla (Epic relics):** Ring of Precision (+150% crit damage, needs crit) < **Connoisseur** ≈ Ring of Fury (×1.5
MUL) < Ring of Power [C, `vanilla-power.md` §3]. It fits Epic only because relics are judged big: as a perk it would be
Legendary.
**Run:** weak in biome 1 (you hold little), strong from biome 2 on, best late when slots are full. Keeps up, since
levels are what late runs are made of [WORKFLOW §4]; it doesn't run away, it at most doubles the rate.
**Break it:** Upgrade Ring + Connoisseur: every choice is +2 levels; two relics plus a Greed drawback, fine. Limited items
(offered until 100%): the extra level may overshoot 100%, so clamp. Level 255 cap [C]. Not for levels the game gives
itself (Level Up, the Enhancement totem, the Key totem menu).
**Fun:** it changes what you pick (you favour items you hold), and a popup shows "+2".
**How:** host. A pickup taken (`ACrabInteractPickup` Interact 0x140d3cf00) by a player who already holds its DA: after the
game adds its level, add one more through the inventory arrays (as `inventory_edit.hpp` does).
**Unverified:** picks a run on held items [G, balanceLog]; whether shop and totem pickups go through the same Interact.

## Appraiser (perk, Epic): BUILT (untested)

**Chests get luckier the further you get**

- Chest choices roll Epic and Legendary more often the further into the run you are: the game's luck multiplier m
  (Epic 3.75% × m, Legendary 0.25% × m) gets +0.12 × island × level, counting islands up to 50.
- Epic chosen by the user (2026-10-14); pitched as Common.
- Spawn weight: 1 (like most perks)    Levels: Linear
- Pool: outside (loot)
- Appears in: luck and random chests; loot, chance, gamble and random totems

**Different from:** Streamer Loot (Common, flat 4% per level to force Epic, 0.6% Legendary [C, DA_Perk_StreamerLoot]),
Ring of Luck (Legendary relic: chance perks and totems +75%), because it grows with the run: it gives normal chests the
island scaling only shops have [C, `vanilla-difficulty.md` §3]. A slow ramp is Epic's kind of effect [WORKFLOW §2].
**Number:** at L1, per choice [C formula, value G]:

| Island | m | Epic | Legendary |
|---|---|---|---|
| 7 | 1.84 | 6.9% | 0.46% |
| 14 | 2.7 | 10% | 0.67% |
| 28 | 4.4 | 16.5% | 1.1% |
| 50+ | 7 | 26% | 1.75% |

  Averaged over islands 1–35 that's about +8% Epic per choice, 2× Streamer Loot L1 (+4%), as an Epic should be.
  Level 3 at island 50: m = 19, Epic 71%, Legendary 4.75% per choice; shops already reach m = 62 there [C], so fine.
**Vs vanilla:** Streamer Loot L1 < **Appraiser** (early) < Streamer Loot L2 < **Appraiser** (biome 3+). One rarity up:
Ring of Luck. It loses to Streamer Loot when picked late at low island, beats it from island ~14.
**Run:** weak in biome 1 (m ≈ 1.5), worth it from biome 2 on. Picked early it ramps the whole run; picked late it's
strong at once. The island-50 cap stops loops from making every choice Epic.
**Break it:** chests only: not shops (they have their own scaling), not the secret chest (m = 0), not key chests. Stacks
with the Lucky modifier (m = 2) by adding. Multiplayer: the roll uses one random player's items [C, LootRoll], so it
counts the holder's level only when the roll picks them [C]; fine, like Streamer Loot. Ultra Luck already maxes it.
**Fun:** you can see it (more purple and gold choices as the run goes on); it changes what you pick next (better rarity
makes chests worth more than shops); a HUD line could show the current odds.
**How:** host. Hook the rarity chance function (0x140d31c00, Epic 3.75% / Legendary 0.25% × m) or LootRoll
(0x140d33240) and add the bonus to m when the roll is a chest roll and the rolled player holds Appraiser. Island from
`GS+0x2D4`, island type `GS+0x2D8` (skip Shop 9).
**Unverified:** that 0x140d31c00 gets m as an argument (or where m lives) and that the chest/shop source can be told
apart there; the +0.12 per island [G], tuned from balanceLog's Epic picks per island.

## Fixation (relic, Greed): BUILT (untested)

**Chests offer one choice, but it's much more likely to be rare**

- Every chest offers 1 choice, but its rarity rolls at ×8 luck (m × 8): Epic 30%, Legendary 2% per chest instead of
  3.75% / 0.25% per choice [C base odds, `vanilla-difficulty.md` §3].
- Spawn weight: 0.3 (like Greed relics [C])    Levels: none (relic)
- Pool: outside (loot)
- Appears in: greed chests; greed and random totems

**Different from:** Coral Amulet (Common relic: +2 choices), Upgrade Ring (Greed: −2 choices, all upgrades), Streamer
Loot (flat upgrade chance), because it trades choice for rarity: you stop building around synergies and take what comes.
**Number:** per chest, vanilla is best-of-3 Commons with an 11% chance of at least one Epic [C]. Fixation is one item
with 30% Epic and 2% Legendary. An Epic is worth about 2× a Common [C, WORKFLOW §2], so about 1.35 Commons' worth
against best-of-3 Commons (≈1.2 with the choice [G]): a small gain in power, a big loss in control. That is Greed's
trade.
**Vs vanilla (Greed relics):** Upgrade Ring < **Fixation** ≈ Ring of Favoritism < High Roller < Trigger Ring (guessed;
relic values aren't worked out [G]).
**Run:** fine early (any item fills a slot). Worse late: one forced choice is often a new item you have no slot for
[C, `count < NumSlots`], so it's a run-start gamble. Elite and boss chests: their 3–4 choices drop to 1 too (Epic and
Legendary chests already have their rarity, so ×8 only helps them to Legendary).
**Break it:** Coral Amulet, Big Chests, Upgrade Ring: set the count to 1 after every modifier (REPORT "Chest choices":
rewrite `Choices` on the first `LootRoll`). Hoarder Backpack (take all): Fixation cuts it to 1, a bad combo by design.
Appraiser / Clean Sweep: their luck adds first, then ×8; cap m at 50 (UltraLuck's level).
**Fun:** its drawback changes how you play (no picking, adapt to what you get); every chest is a reveal.
**How:** host. Chest roll (0x140caf1c0) notes the opener; on its first `LootRoll` (0x140d33240) set `Choices = 1` and
multiply m for that roll. Same luck hook as Appraiser.
**Unverified:** where m lives in the roll (shared with Appraiser); ×8 [G], tuned so a chest's worth ≈ vanilla's.

## Clean Sweep (perk, Epic): BUILT (untested)

**Flawless islands give luckier reward chests**

- Clearing an island without taking damage (the game's own "flawless") makes that island's reward chest roll at
  +5 luck per level (m + 5L): at level 1, Epic 22.5% and Legendary 1.5% per choice instead of 3.75% / 0.25% [C base].
- Limited: not offered after level 4 (m = 21: Epic 79% per choice).
- Spawn weight: 1 (like most perks)    Levels: Linear, Limited (4)
- Pool: outside (loot)
- Appears in: luck and random chests; loot, chance, gamble and random totems

**Different from:** Skill Ring (Epic relic: a flawless clear upgrades an Epic chest to a Legendary chest), Duplication
Ring (Epic relic: an extra chest for a flawless elite or boss island), because it works on every island's chest, as a
perk that levels, and raises the odds instead of the chest. Stacks with both.
**Number:** flawless islands are about 40% [G]. 40% × 22.5% − 3.75% ≈ +7.5% Epic per choice averaged, 2× Streamer Loot L1
(+4%, Common [C, DA]), as an Epic should be.
**Vs vanilla:** Streamer Loot L1 < **Clean Sweep** (40% flawless) < Skill Ring (relic). At 20% flawless it drops to
about Streamer Loot; at 80% it beats Appraiser. The flawless rate decides it, so measure it.
**Run:** worth it in biome 1 (early islands are easy to clear clean). Falls off when islands get hard to flawless
(biome 4+) [G]: that's the design, skill keeps it going.
**Break it:** chests only (not shops). Multiplayer: flawless is per player [unverified]; each player's own chest.
Double or Nothing / XL totem: every reward chest of that island gets it. Fixation: adds before ×8, m capped at 50.
**Fun:** you play toward it (dodge to keep flawless), you see it work (a "Clean Sweep!" popup on a flawless clear, then
better choices).
**How:** host. Find the flawless flag Skill Ring (relic 41) and Duplication Ring (relic 42) read at the island end
(0x140d01610); on reward-chest rolls that island, add 5L to m (same luck hook as Appraiser).
**Unverified:** the flawless flag and whether it's per player; flawless rate per biome [G, balanceLog].

## Speed Loader (melee mod, Common): BUILT (untested)

**Melee hits reload your weapon**

- A melee swing that hits at least one enemy refills 25% of your weapon's magazine per level (once per swing, not per
  enemy). Limited: not offered after level 4 (100%).
- Spawn weight: 0.75 (like melee mods [C])    Levels: Linear, Limited (4)
- Pool: outside (uptime)
- Appears in: skill and random chests; loot, chance, gamble and random totems

**Different from:** Ammo Ring (Epic relic: dashing reloads fully [C, DA]), Ring of Reloading (Common relic: reload speed
+75% [C]), because the trigger is a melee hit: you weave melee into your reloads.
**Number:** melee cooldown is 3–6 s (Claw 3, Katana 4, Pickaxe 5, Hammer 6 [C, DA]). If a magazine lasts 2–3 s [G],
25% per 4 s swing is about 0.6 s more firing per 4 s, so about +15% DPS at L1 and +60% at L4 [G]. Common-sized at L1
(Sharp Claws: +100% melee damage, Common).
**Vs vanilla:** Ring of Reloading < **Speed Loader** L1 < Ammo Ring (a full reload per dash). One rarity up: Ammo Ring.
**Run:** good early and late (uptime scales with every damage item). Blender (−20% melee cooldown per level) raises it.
Weapons that never reload get nothing: the description says so.
**Break it:** Blender stacks → at most one refill per 1.5 s. Dagger (fast melee) is the best case; the limit covers it.
**Fun:** you play toward it (melee between bursts); the magazine counter jumps.
**How:** ammo may be client-predicted: find what Ammo Ring (relic) calls on a dash and call the same with a fraction.
Melee hits: `OnOutgoingDamage` with Type 3 (Melee).
**Unverified:** where ammo lives and who owns it (client or host); magazine time per weapon [G].

## Culling Blow (melee mod, Epic): BUILT (untested) (renamed from Executioner: Shellbreaker's loot has one)

**Melee finishes off weak enemies**

- A melee hit on an enemy under 10% health per level kills it outright. Elites: half the threshold. Bosses: never.
  Limited: not offered after level 3 (30%).
- Spawn weight: 0.75 [C]    Levels: Linear, Limited (3)
- Pool: outside (a % of health: a true multiplier on enemies you melee)
- Appears in: damage and random chests; loot, chance, gamble and random totems

**Different from:** Sharp Claws and Iron Claws (more melee damage), Grim Reaper (heals on kills), because it's a
threshold: melee becomes the finisher for enemies your gun has worn down.
**Number:** killing at 10% skips the last 10% of health: ×1.11 damage on enemies you melee; ×1.25 at 20%, ×1.43 at 30%.
The value is in the role (no chasing the last sliver), not raw DPS. Epic, since a percentage never dilutes.
**Vs vanilla:** Sharp Claws (+100% melee, but melee is a small share of damage late) < **Executioner** < Arcane Claws.
**Run:** keeps up: a % of health scales with enemy health for free. That's a "yes" in step 5, so it's capped at 30% and
elites get half.
**Break it:** Blender + Dagger spam still needs the enemy under 30%. Big Claws reaches more enemies per swing: fine.
Multiplayer: kills go to the hitter.
**Fun:** you see it (an "Executed!" popup), you play toward it (shoot, then finish in melee).
**How:** host. `OnOutgoingDamage` with Type 3 (Melee): after the hit, if the enemy's health / max is under the
threshold and it isn't a boss, deal its remaining health through `UCrabHC_TakeDamage` (as Ricochet does).
**Unverified:** the elite and boss flags.

## Feedback Loop (ability mod, Common): BUILT (untested)

**Ability kills cut its cooldown**

- Each enemy your ability kills takes 0.1 s per level off its cooldown (the user's number, 2026-10-14), at most 50% of
  the cooldown per use.
- Spawn weight: 0.75 (like ability mods [C])    Levels: Linear
- Pool: outside (uptime)
- Appears in: skill and random chests; loot, chance, gamble and random totems

**Different from:** Heat Sink (Common: −10% ability cooldown per level [C, DA]), because it pays for kills: packed rooms
chain abilities, empty ones don't.
**Number:** cooldowns are 6–10 s (Ice Blast, Laser Beam 6; Black Hole 9; Air Strike, Electro Globe 10 [C]). An ability
that kills 10 enemies at L1 saves 1 s, −10 to −17%: about one Heat Sink, as a Common should be. Weak where enemies are
few (bosses, elites).
**Vs vanilla:** **Feedback Loop** (few kills) < Heat Sink < **Feedback Loop** (horde).
**Run:** falls off where abilities stop killing (tanky late enemies), keeps up on hordes. It feeds itself (kills →
ability → kills), so it's capped at 50% per use.
**Break it:** Triple / Split Ability and explosion mods give more kills per use; the 50% cap holds. Stacks with Heat
Sink: fine.
**Fun:** you play toward it (aim abilities into crowds); the cooldown visibly snaps.
**How:** find the ability cooldown (PS or ability component) and how a kill is tagged as the ability's (DamageInfo tags
or the damage causer).
**Unverified:** both of those; kills per use [G, balanceLog].

## Shrapnel (ability mod, Legendary): BUILT (untested)

**Your ability's blast fires your weapon's projectiles**

- Each ability use fires a ring of 12 of your weapon's projectiles (+6 per level, at most 36) from where it hits, at
  your weapon's damage, with your weapon mods. One ring per use, not per explosion.
- Legendary chosen by the user (2026-10-14): a new damage source [WORKFLOW §2].
- Spawn weight: 0.75 [C]    Levels: Linear, capped at 36
- Pool: weapon (each projectile is a weapon shot)
- Appears in: skill and random chests; loot, chance, gamble and random totems

**Different from:** Reload Arc (Common weapon mod: reloading fires an arc of projectiles [C]), Dagger Blast and the
explosion mods (the ability's own damage), because it turns every ability use into a weapon volley with all your weapon
mods: weapon and ability builds help each other.
**Number:** 12 shots per 6–10 s use is 1.2–2 extra shots a second; against a weapon's 2–10 shots a second that's
×1.15–1.9 [G], plus every on-hit mod. Heat Sink and Feedback Loop raise it.
**Vs vanilla (Legendary):** in line with new damage sources; Ricochet ×1.25–2.2 [ideas.md].
**Run:** keeps up: it scales with every weapon mod you collect.
**Break it:** one ring per use (Triple / Split Ability, Layered Explosion don't multiply it). Split Shot and Ricochet
apply to each projectile: allowed, they're weapon mods; the 36 cap and the use rate hold it. Shotguns: one projectile
per ring slot, not a full spread.
**Fun:** you see it (a ring of your bullets), it changes what you pick (weapon mods help your ability).
**How:** host. Find what Reload Arc calls to fire its arc (weapon projectiles with mods) and call it in a ring where the
ability hits.
**Unverified:** Reload Arc's spawn function; the ability's hit location; how projectiles carry their owner's mods.

## Follow Through (melee mod, Epic): BUILT (untested)

**Using your ability empowers your next melee**

- After you use your ability, your next melee within 3 s deals +200% damage (+100% per level after the first) and
  applies the elements of your elemental explosion mods (Fire, Ice, Lightning, Poison, Arcane Explosion).
- Spawn weight: 0.75 [C]    Levels: Linear
- Pool: melee damage
- Appears in: skill and random chests; loot, chance, gamble and random totems

**Different from:** Sharp Claws and Iron Claws (always more melee damage), the elemental claws (always an element),
because it's a combo: ability, then melee.
**Number:** ability every 6–10 s, melee every 3–6 s: about every other melee is empowered, so about ×2 melee damage
on average at L1 [G]. Iron Claws is +300% with a cooldown penalty at Common [C]; Follow Through is Epic because it also
carries elements (the elemental claws are Epic [C]).
**Vs vanilla:** Sharp Claws < **Follow Through** < Arcane Claws (Epic, always on).
**Run:** melee is a small share of damage late, like every melee mod.
**Break it:** none (one empowered melee per ability use).
**Fun:** you play toward it (ability, then dive in); you see it (a glow or popup for 3 s).
**How:** host. Note the ability use; on the next Type 3 (Melee) `OnOutgoingDamage` within 3 s, raise the damage and add
the element debuffs (DamageInfo +0x20 Debuffs, as the elemental claws do).
**Unverified:** how to see an ability use on the host; how the claws add their element.
