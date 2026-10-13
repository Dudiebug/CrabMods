# Item ideas: backlog

Ideas waiting to be built, written up in the wiki's card format. All addresses are for game build `0x6921D2F7`. **V** means verified in a decompile; **I** means inferred.

---

## Encore (relic, Epic)

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

## Autoloader (perk, Rare)

**Chance to instantly reload after each elimination**

- 10% chance to instantly reload after each elimination
- Spawn weight: 1.0
- Buff scaling: Hyperbolic
- Appears in luck and random chests, as well as loot, chance, gamble and random totems

How: the GM damage handler `0x140ce5780` calls `0x140d26c00` to count a kill and knows the killer (**V**, REPORT.md). Hook it, roll `1 - 1/(1 + 0.1*level)` for the killer, and on a hit fill the current weapon's magazine. Still to find: the weapon's ammo field and the reload/refill function (the reload has to run on the owning client, so it may need a client RPC or a CrabSync message).

## Finishing Move (perk, Rare)

**Enemies eliminated with critical hits drop more crystals**

- Enemies eliminated with critical hits drop 50% more crystals
- Spawn weight: 1.0
- Buff scaling: Linear
- Appears in economy and random chests, as well as loot, chance, gamble and random totems

How: crits are worked out in `ACrabPS::CalcOutgoingDamage` `0x140d21480` (**V**), which fills `FCrabDamageInfo`. Note on the damage info (or a per-enemy map) that the hit was a crit, and when `0x140ce5780` reports that hit eliminated the enemy, scale its crystal drop by `1 + 0.5*level`. Still to find: where an enemy's crystal drop amount is decided (Bonus Crystals / Crystal Fertilizer read it, so their `perk_sites.json` entries are the starting point).

## Rigged Totems (perk, Rare)

**Chance based totems are more likely to roll in your favor**

- Chance based totems are 25% more likely to roll in your favor
- Spawn weight: 1.0
- Buff scaling: Linear
- Appears in luck and random chests, as well as loot, chance, gamble and random totems

How: find the chance and gamble totems' success roll (they share the totem class with the key totem `key_totem.cpp` already hooks). Multiply the success chance by `1 + 0.25*level`, capped at 100%. Sturdy Totems (vanilla) is a totem perk too, so its `perk_sites.json` entries are a good place to start looking.

## Cursed Lottery (perk, Legendary, cursed)

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

## Adrenaline (perk, Epic): PASS WITH CHANGES

(Pitched as "Second Wind"; renamed, since that name is planned for a necro item in `necromancer-loot.md`.)

**A burst of health and speed when you're nearly dead**

- Below 25% health: heal 15% max health and +50% move speed for 3 s. Once per island.
- Spawn weight: 1.0    Scaling: Linear on the heal
- Appears in: Health chests

**Different from:** Grave Dodger (20% heal at 1 HP, saves from a lethal hit), Speed Demon. Triggers earlier, adds speed, doesn't stop death. Close to Grave Dodger, but different enough.
**Number:** based on Grave Dodger 20%. At 20% it tied or beat Grave Dodger and added speed, so it was cut to 15%.
**Vs vanilla:** Amber Resin < Grim Reaper < **Adrenaline** < Grave Dodger < Survivor.
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

## Trick Shot (weapon mod, Legendary): REWORK

**Your shots ricochet into enemies**

- Reworked: shots bounce once (+1 per level), re-aim at the nearest enemy on each bounce, and bounced hits get +50% per bounce (max +150%) as a separate multiplier.
- Spawn weight: 1.0    Scaling: Linear (bounces)
- Appears in: Skill chests

**Different from:** Bouncing Shot (Common, +150% per bounce), Homing Shot (10% chance). As pitched it needed Bouncing Shot to do anything and was +50% in W (×1.11): a Common-sized, dead Legendary.
**Number:** reworked ≈ ×1.3 if ~60% of hits are bounced (a guess); best case ×1.9.
**Vs vanilla:** Square Shot < Homing Shot < **Trick Shot ≈ ×1.3** < Splash Damage < Firepower.
**Break it:** exclude Soul Whip and Flamethrower; check rockets and beams. Unverified: whether a hook can redirect the game's bounce.
**Fun:** visible arcs, shoot at walls, map-dependent.

## Momentum (perk, Common): PASS WITH CHANGES

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

