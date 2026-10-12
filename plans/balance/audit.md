# CrabMods balance audit (2026-10-11, CrabMods 0.10.x)

The vanilla baseline was studied first, without reading any mod code. It is in `vanilla-difficulty.md` (run loop and enemy scaling) and `vanilla-power.md` (how the player gains power, and what each rarity is worth). Every mod item was then measured against it.

Tags: **[C]** confirmed from code or assets. **[E]** estimate.

## 1. The vanilla yardsticks that matter

- **Run shape:**
  - 7 islands per biome, 28 per loop. Island 6 of each biome is the Shop; island 7 of biomes 1–3 is Elite; island 28 is the Boss.
  - The player gets about one choice per island (one chest per player, usually three options), plus the shop. [C]
- **Enemy HP is a step function** (1 player) [C, 0x140cf1060]:

  | Islands | HP multiplier |
  |---|---|
  | I1–3 | 1.1–1.5 |
  | I4–6 | 2.2 |
  | I8–13 | 5.7 |
  | I14–20 | 12.4 |
  | I21–27 | 22.6 |
  | I28 | 36.8 |
  | I57 | 873 |

  - Enemy damage grows much more slowly: 0.9 at I1 and 7.7 at I28.
  - Each extra player adds 45% enemy HP and 40% more enemies.
- **Player damage is three pools multiplied together** [C, 0x140d25c30, 0x140d23700]:

  ```
  W (weapon mods, additive) × ADD (perks + PS.DamageMultiplier, additive) × MUL (Greed perks, Ring of Fury)
  ```

  - Every CrabMods `DamageBonus` lands in **ADD**.
  - A flat +X% in ADD is worth a lot early and gets diluted later. A typical mid-run ADD is about 1.6–3.
- **What a rarity is worth** [C/E]:
  - **Common:** +50–100% flat, which comes to about ×1.1–1.2 mid-run.
  - **Epic:** about 2× a Common (Damage Shot +150%, ×1.33 mid-run), a 2–3% proc, or a ramping perk.
    - Ramping Epic perks use **small** rates: Tasty Orange 1% per kill (island only), Gemstone 4% per island, Collector 3% per perk level, Checklist 1% per 25 kills.
  - **Legendary:** a new damage source, more projectiles or a build-defining effect, worth about ×1.3–2.
  - **Greed:** a true ×1.25–1.75 multiplier that always has a real drawback.
  - **Relics:** never level. There are only about 3–6 per run, so vanilla relics are large: Epic relics are around 100–400 BaseBuff.
- **Rarity odds** per offered item: Epic 3.75%, Legendary 0.25% [C]. A Legendary is a rare event, so a dead Legendary hurts.
- **Slots:** 24 weapon mods, 12 ability mods, 12 melee mods, 24 perks, 10 relics [C, 0x140d29e50]. A normal 28-island run gets about 30–50 distinct items in total [E], so **slots almost never fill**.
- **Shop prices (crystals)** [C, 0x140cf1fc0]:

  | Rarity | I1 | I28 |
  |---|---|---|
  | Common | 482 | 1171 |
  | Epic | 1204 | 2927 |
  | Legendary | 3853 | 9365 |

- **Weapon baseline:** about 75–120 sustained single-target DPS. Examples: Sniper 100 per shot, 6-round clip, 2 s reload (about 75 sustained); AutoRifle 100; Rocket 68 plus a 1000u blast at 0.9 shots/s.
- **The weapon's proc roll multiplier scales every chance-based mod.** It is 1 on SMG/LMG, 2.5 on AutoRifle, 7 on Sniper/Rocket and 100 on Proc Generator.

## 2. Soul Whip and Raise Dead (highest priority)

### 2.1 Structural problems

1. **Undead get the difficulty scaling for free.**
   - Undead are real enemy spawns with `undeadHealth = 2`, so their HP is 2× the enemy's.
   - That enemy HP already includes the island multiplier (×37 at I28) **and** the player-count multiplier.
   - So the army scales with difficulty with no investment at all. Everything else the player owns has to be found to keep up.
   - Their damage (enemy damage, ×7.7 at I28) is then multiplied by the owner's full `GetDamageBuff` (ADD × MUL), on top of the tag bonus.
   - The result is the strongest late-game scaler in the mod. It rises with the same curve the game uses to make enemies hard.
2. **Undead kills are credited to the owner as player kills** (`killer = PawnOf(ps)`). Each such kill:
   - fires every vanilla on-kill effect: Tasty Orange, Checklist, Ring of Fury uptime, Exploding/Freezing/... Enemies, Vampire, crystals;
   - fires every CrabMods on-kill effect: Finishing Move, Recharge, Autoloader;
   - and pays souls.

   That makes a feedback loop: undead kill → souls → more undead → more kills → more Tasty Orange → stronger undead.
3. **Raising is close to free.**
   - `survivorRefundPercent = 100` returns the whole cost at the end of the fight.
   - Upkeep of 0.25% per second is 15 souls per minute for an Elite, and 0.25 souls per minute for a fodder.
   - So the soul bar is a loan that is always repaid, not something the player spends.
4. **Fodder is the best buy by far.** Undead HP per soul, from base enemy HP × 2:

   | Tier | Cost | Undead HP | HP per soul |
   |---|---|---|---|
   | Fodder ×3 | 5 | about 180 | **about 36** |
   | Medium | 15 | about 200 | about 13 |
   | Hard | 40 | about 480 | about 12 |
   | Elite | 100 | about 740 (eliteHealth 0.5) | **about 7** |

   - Fodder is about 5× as efficient as an Elite.
   - Fodder also makes the best aggro wall: enemies switch to an undead that is 1.5× closer than the player.
   - It is the best fuel for Army of the Dead too.
5. **The cap is 40 undead per player.** In practice upkeep never limits the army, so 40 is the real cap. A 3-player lobby can field 120 extra bodies, each with 2× enemy HP.

### 2.2 The whip itself

- **Damage:**
  - 100 DPS (80 per crack, 1.25 cracks/s) dealt as a **4 m (400u) AoE burst** on every crack, 45 m reach.
  - It has an **infinite clip**, so it never reloads.
  - Compared with the Sniper it copies (about 75 sustained DPS, single target), it is already about 1.3× the Sniper's DPS, and it hits an area.
- **Proc roll multiplier:** it keeps the **Sniper's 7×**, because the DA is copied and the multiplier is never reset.
  - That makes Double Shot 56%, Triple Shot 49% and Split Shot 42%.
  - Each extra projectile is a full extra AoE burst.
- **Tags:**
  - The tag radius (6 m) is larger than the burst (4 m), so in practice **every enemy you hit is tagged**.
  - So tags are not a choice. They are a constant +25% undead damage and +50% souls.
- **Reload mods:** Mag Shot, Autoloader and reload perks are dead picks for whip holders. This is fine, but worth listing.

**Suggested whip changes:**

| Setting | Now | Suggested | Why |
|---|---|---|---|
| whipDps | 100 | 70 | An area weapon should sit under single-target weapons (Rocket: 68 plus its blast). |
| Proc roll multiplier | 7 (Sniper) | 3–4 | Set the whip DA's `WeaponModTriggerRollMultiplier` explicitly, like Ice Staff or Crossbow. |
| burstRadiusMeters | 4 | 3 | Area grows with r², and Branding Iron already adds radius. |
| tagRadiusMeters | 6 | 3 (= burst) | Tag only what the burst hits, so aiming matters. |
| taggedSoulsPercent | 50 | 25 | The tag is near-universal. |
| Infinite clip | yes | keep, or clip 8 / reload 1.5 | Keep it only if DPS is cut as above. |

### 2.3 Suggested undead and soul changes

| Setting | Now | Suggested | Why |
|---|---|---|---|
| undeadHealth | 2 | **0.75** (and drop the elite ×0.5) | They are already scaled by island and player count. |
| Undead damage buff | owner's full GetDamageBuff | **√(GetDamageBuff)**, or ADD only (no MUL) | Stops build damage and difficulty damage both applying to the army. |
| Undead kills | count as owner kills | **souls at 50%, crystals only; no on-kill perks** | Breaks the kill feedback loop. |
| survivorRefundPercent | 100 | 50 | Raising should cost something. |
| upkeepPercent (per s) | 0.25 | 1.0 | A 60 s fight then costs about 60% of the raise. |
| costFodder / fodderPack | 5 / 3 | 9 / 3 | Brings fodder near medium on HP per soul. |
| costElite | 100 | 80 | With eliteHealth back at 1×, an Elite should not be the worst buy. |
| kMaxUndeadPerPlayer | 40 | 12 (make it an ini key) | Performance, and to bound Army of the Dead. |
| aggroBias | 1.5 | keep, but fodder doesn't draw aggro | Otherwise fodder walls are the dominant strategy. |

**Rough soul income** [E, from spawn budgets ÷ spawn costs]:
- Arena kill souls run about 25–40 at I1 and about 50–90 by k = 4, counting tags.
- So the default costs are sound as long as raising really spends souls.
- Most of the problem is refund, upkeep and kill credit, not the prices.

### 2.4 Necro loot

| Item | Now | Verdict | Suggestion |
|---|---|---|---|
| **Soul Siphon** (C weapon mod) | 1 soul per enemy hit per crack | **Broken.** About 3 enemies × 1.25/s × 60 s is about 225 souls a fight, 4–6× all kill income, from one Common. | 1 soul **per crack** if it hits anything (not per enemy), or 0.25 per enemy capped at 1 per crack. Keep it Common. |
| **Army of the Dead** (E perk) | +4% ADD per undead | **OP.** 40 fodder gives +160% per level. A vanilla Epic ramp is 1–4%, capped by something real. | Count by tier (fodder 0.25, medium 1, hard 2, elite 4) and cap the count at 10 per level. Or move it to Legendary. |
| Twin Rising (L ability mod) | 15% to double | Weak for a Legendary (expected +15%). | 30%, or move to Epic. |
| Soul Brand (L weapon mod) | tagged kill tags the 2 nearest | Weak: tags are already near-universal. If the tag radius is cut, it is fine as an Epic. | Epic, 3 enemies. |
| Grim Harvest (L perk) | +50% souls from tagged kills | That is about +33% souls with a near-universal tag, which is Epic territory. | Epic, or make it +1 soul per tagged kill per level. |
| Death Mark (E weapon mod) | +10% to tagged | A true multiplier outside the pools, so it never dilutes. Weak at L1, fine stacked. | OK. |
| Branding Iron (C) | +25% radius | That is +56% area per level, on a weapon whose damage is area. | 15%. |
| Mass Grave (E) | +1 fodder per pack | +33% fodder. Fine after the fodder price fix. | OK. |
| Harvester (L melee mod) | 25% free raise on claw kill | Fine. Melee cooldown limits it. | OK. |
| Phylactery (L relic) | Once per island, cancel death, heal to 50% | Fine. Grave Dodger-like and costs all souls. | OK. |
| Sturdy Bones, Soul Discount, Grave Claws, Soul Jar, Hungry Ghosts, Last Rites, Long Lash | Commons | Fine. | OK. |
| Ring of Souls, Ring of the Grave (E relics) | +25% | On the low side for relics, which never level. | 40%. |
| Plague Bearers (E perk) | 3 poison per undead hit | Fine. | OK. |

## 3. Other CrabMods items

### Too strong

| Item | Problem | Suggestion |
|---|---|---|
| **Cursed Lottery** (Greed relic) | +100,000 crystals. The shop is island 6 of each biome. There, 60% of rolls use a luck multiplier of 1.25 × island, so a choice is Legendary about 1.9% of the time at I6 and about 7% at I27 (no Legendaries at I1). That is still roughly 20–30 Epics or Commons at biome-1 to biome-2 prices (Epic about 1,500–2,300), so it buys out every shop. It also maxes Money Is Power instantly (it caps at 25,000 per level) and keeps it maxed, because you can't spend 100k. The curse can't cost anything comparable. | 15,000 × (1 + island/14), about 2–3 Legendaries. Or pay it as the price of N Legendaries at the current island. Have Money Is Power ignore Lottery crystals, or let the curse also zero Money Is Power. |
| **Collateral Damage** (E perk) | +5% ADD per destructible broken (island only). Tasty Orange, the vanilla analogue, is 1% per kill. Islands have dozens of pots, rocks and crystals, so this is about +100–250% per island. | **1%** per break, or 2% capped at +50% per level. |
| **Minimalist** (E perk) | +5% per empty weapon-mod slot, and there are 24 slots. That is +115% at I1 with one mod, still about +70% mid-run, and Bandolier adds 25% more. | 2% per empty slot, counted out of 12 (cap 12 × 2% = +24% per level). Or compute it from 8 slots. |
| **Key totem menu** | Picks *any* item, so a run can start with Firepower or Glass Cannon. Vanilla only lets you unlock locked items. (Needs a check: does a lobby pick carry into the run?) | Limit it to locked items, or price by rarity (vanilla key prices: 2 / 5 / 16), or Commons only once everything is unlocked. |
| **Portal Shot** (L perk) | Portal Amulet is a 0.05-weight relic (the rarest drop), because perfect delivery is strong. As a Legendary perk at weight 1 it's about 20× more common, and it levels for +20% each. | SpawnWeight 0.25, and +10% per level. |
| **Overflow** (E perk) | +3% permanent ADD per 100 overheal, with no cap. Buffed Vampire (15 HP per kill) at full health makes it +0.45% per kill, forever. | 1% per 100 overheal, capped at +50% per level. Or make it island-only, like Tasty Orange. |
| **Melee mod rework** | Elemental Claws ×2 stacks and Iron Claws +500% with only a 20% cooldown penalty are big. Melee is a minor vanilla damage source, so this is defensible, but Vampire 15 combined with Overflow is the abuse case. | Vampire 10. Keep the rest. |

### Weak or dead picks (wrong rarity)

| Item | Problem | Suggestion |
|---|---|---|
| **Bandolier, Tool Belt, Scabbard, Backpack** (L charms) | The slots (24/12/12/24) basically never fill in a normal run. A Legendary roll (0.25%) spent on an empty slot is the worst outcome in the mod. | Make them **Common**, or put them in the shop or key totem only. Better still, make them do something now (see note below). Reliquary is different: 10 relic slots can fill in long or loop runs, so it can stay Epic. |
| **Purist** (E relic) | +10%, relics never level, and the requirement is tight. Vanilla Epic relics are 100–400. | +40–50%. |
| **Jack of All Trades** (L perk) | 3% per tag, about 10 tags, so about +30% ADD. That is Common value at Legendary rarity. | Epic. Or 6% per tag. |
| **Greed Is Power** (Greed perk) | It goes into ADD at 5% per Greed level with no drawback. Vanilla Greed perks are +25% **MUL** with a drawback. | Move it to the MUL pool, +15% per Greed level, with a drawback such as −15% max HP per level. |
| **Arsenal** (E weapon mod) | +3% per other mod, in ADD, so about +30% mid-run. An Epic weapon mod is about +150% W (Damage Shot). | 6% per mod, or move it into W. |
| **Community Support** (E perk) | Solo it's just Rapid Fire, a Common, at Epic rarity. | Common, or 20% per holder. |
| **Bullet Rain / Hellshot** (E weapon mods) | +35% in ADD plus a delivery change. Fine as is. | OK. |
| **Untouchable, Underdog, Thick Shell, Hoarder, Recharge, Finishing Move, Autoloader, Encore, Rigged Totems, Lucky Charm, Specialists, Double or Nothing, Martyr, Spiked Chest, Enhanced Turrets, Heavy Shot** | In line with vanilla, or niche. Last Crab Standing does little with CrabSync health sync on. | OK. |

**Charm note.** If the charms stay Legendary, they should do something now: "+5 slots and +3% damage per empty slot". Or make slots scarce on purpose, for example a host option for vanilla-challenge-style 8 starting slots, so the charms matter.

## 4. Suggested order of work

1. Necromancer kill credit, refund and upkeep, plus the undead HP/damage scaling (2.3). These are the structural fixes.
2. Soul Siphon, Army of the Dead, and the whip's roll multiplier and DPS.
3. Cursed Lottery, Collateral Damage, Minimalist.
4. Rarity moves: charms, Jack of All Trades, Twin Rising, Grim Harvest, Soul Brand, Community Support.
5. Small number bumps: Purist, Arsenal, the necro rings, Greed Is Power.

## 5. Unverified, worth checking in game

- Pool-chest choice count (assumed 3), crystals per kill, and the destructible count per island. These fix the size of the Collateral Damage and Overflow problems.
- Whether the bolt's explosion damage goes through W (weapon mods) or only through GetDamageBuff.
- Whether key-totem picks carry into the run.
- Real fight lengths. Upkeep math assumes about 60 s.
