# Crab Champions (vanilla): how the player gains power, and what each rarity is worth

Sources: game.exe (Ghidra; addresses are image VAs), pickup_catalog.json (317 pickup DAs), and the pak's DA/BP class defaults. No mod code was read.
Tags: **[C]** = confirmed in code or assets. **[I]** = inferred (reasoned from names, descriptions or partial code, not traced end to end).

## 0. TL;DR for balancing modded items
- **Weapon-mod damage % (W pool) is additive** across all weapon mods [C, 0x140d25c30]. A Common +75% on an empty build is ×1.75 DPS. Next to 4 other Commons (+300%), the same item is only ×1.19. Its worth falls sharply as the build fills.
- **Perk damage (GetDamageBuff "add" pool) is a second, separate additive pool**, multiplied by the W pool [C, 0x140d23700]. The first +X% in each pool is worth the most.
- **Greed damage perks and Ring of Fury form a third pool** (1 + Σ), multiplied with the other two [C]. These are the only near-true multipliers. Glass Cannon +75% is ×1.75 to *everything*.
- **Rarity is mostly about kind of effect, not size**:
  - Commons are flat +50–150% damage.
  - Epics are 2–3% proc chances, +1 projectile chances, or one big flat (Damage Shot +150%, Juiced 15%×3 stats).
  - Legendaries are 3% procs, +1 projectile (Firepower), homing, splash, and build-defining perks.
- **Levels are linear stacks for most items**: L2 gives 2× the buff [C, 0x140d2e5b0]. Levels come from duplicates and upgrade offers. Max is 255.
- **Proc (HalfScaling) items scale at +50% of base per level** [C, 0x140d31f40]. The roll chance is also multiplied by the weapon's WeaponModTriggerRollMultiplier, which ranges from 1 (LMG, SMG) to 7 (Sniper, Rocket) and is 100 for the Proc Generator [C].
- **Relics never level** [C, 0x140d31c90 uses L=1]. There are 10 relic slots.
- **Base rarity odds per rolled item** (×m, the luck multiplier) [C, 0x140d31c00]:
  - Epic 3.75%, Legendary 0.25%.
  - On the shop island, 60% of rolls use m = min(1.25×island, 200), so late shops are almost all Legendary.
  - Upgrade offers (levels on held items) occur with probability min(island, 50)% × 0.66 (Epic) or × 0.33 (Legendary), divided down by how many upgrades the roll has already offered.

## 1. Item inventory by category and rarity [C, pickup_catalog.json]
BaseBuff is the raw DA value (percent, or flat damage/units). "w" is SpawnWeight; the default is 1.0, from pickup DA ctor 0x140d3f650 (+0x68 = 1.0).

| Category | Rarity | N | BaseBuff min / median / max | SpawnWeight (Σ) | Loot pools |
|---|---|---|---|---|---|
| Weapon mod | Common | 43 | 1 / 50 / 250 | 0.25–1 (41.5) | Damage 24, Elem 6, Speed 5, Skill 5, Luck 3 |
| Weapon mod | Epic | 24 | 1 / 2.5 / 150 | 1–1.5 (24.5) | Damage 12, Luck 5, Speed 3, other 4 |
| Weapon mod | Legendary | 26 | 1 / 5 / 250 | 1 (26) | Damage 10, Elem 9, Luck 5, Skill 2 |
| Ability mod | Common | 8 | 10 / 62.5 / 200 | 0.75 (6) | Damage 6, Skill 1, Speed 1 |
| Ability mod | Epic | 16 | 1 / 100 / 250 | 0.75 (12) | Damage 10, Skill 4, Luck 2 |
| Ability mod | Legendary | 19 | 1 / 100 / 100 | 0.75 (14.25) | Damage 9, Skill 5, Elem 4, Luck 1 |
| Melee mod | Common | 6 | 5 / 60 / 300 | 0.75 (4.5) | Damage 3, Skill/Speed/Health 1 |
| Melee mod | Epic | 5 | 25 / 50 / 50 | 0.75 (3.75) | Elemental 5 |
| Melee mod | Greed | 1 | 100 | 0.75 | Greed (Brawler) |
| Perk | Common | 47 | 1 / 20 / 300 | 1–1.5 (53) | Health 12, Elem 9, Damage 7, Crit 6, Econ 5, other 8 |
| Perk | Epic | 33 | 1 / 20 / 25000 | 1–1.5 (34) | Damage 10, Skill/Health/Elem 5, other 8 |
| Perk | Legendary | 15 | 1 / 25 / 200 | 1–1.5 (15.5) | Elem 6, Skill/Luck 3, Speed 2, Crit 1 |
| Perk | Greed | 14 | 25 / 60 / 125 | 1 (14) | Greed |
| Relic | Common | 14 | 3 / 26.5 / 75 | 0.3–0.5 (4.4) | Health 5, Skill 5, Speed 2, other 2 |
| Relic | Epic | 26 | 1 / 100 / 400 | 0.3–0.5 (9.2) | Skill 8, Elem 6, Damage 3, Health 3, other 6 |
| Relic | Legendary | 8 | 50 / 75 / 300 | 0.05–0.3 (2.15) | Speed 3, Skill 2, Health 2, Luck 1 |
| Relic | Greed | 7 | 33 / 625 / 2500 | 0.3 (2.1) | Greed |
| Consumable | C/E/L | 3/1/1 | n/a | 1–1.5 | Health, Economy, KeyChest |

Explicit SpawnWeights [C]:
- Ability and melee mods: all 0.75.
- Relics: 0.3, except elemental rings and Time Ring at 0.5 and Portal Amulet at 0.05.
- Perks at 1.5: Banana, the crit/elemental commons, Enhanced Turrets, Mango, Critical Blast, Mega Crit, Critical Lightning.
- Weapon mods: Escalating 0.25, Orbiting 0.25, Aura Shot 1.5.

Rarity enum [C]: None 0, Common 1, Epic 2, Legendary 3, Greed 4. Greed items come only from Greed chests or shrines, plus a 5% per-item injection into shop rolls [C, LootRoll].

### Representative values (what the "unit of power" looks like)
| Rarity | Typical damage item | Value |
|---|---|---|
| Common WM | Big/Chaotic/Snake/Spiral/ZigZag/Heavy Shot | +50% W |
| Common WM | Accelerating/Aerial/BlindFire/Boomerang/Glue/HighCaliber/Recoil/Steady | +75% W (several carry a 25% drawback) |
| Common WM | Time Shot / Orbiting | +100% W |
| Common WM | Bouncing 150 (per bounce), Heavy Hitter 150 (−20% fire rate), Uppercut 250 (every 5th shot ⇒ ~+50% average) | situational |
| Common WM | Streak 5%/hit (cap 100 hits ⇒ +500%), Mag Shot 3%/bullet (cap 100), Wind Up 5%/shot (cap 40 ⇒ +200%) | ramping |
| Common WM | Elemental shots 3% chance, Double Shot 8%, Rapid Fire +12% fire rate, Sharp Shot +4 flat dmg | utility |
| Epic WM | Damage Shot +150% W; Juiced +15% damage, fire rate and reload; Supercharged +25% fire rate | flat |
| Epic WM | Triple Shot 7%, Double Tap 6%, Scatter 6%, X Shot 4%, Arc 5%; proc shots 2–3% (HalfScaling) | chance |
| Legendary WM | Firepower +1 projectile per shot; Split Shot 6%; Square 3%; Homing 10%; Dice Shot 100; Splash 250u radius; Storm/Strike 100; procs 3% | build-defining |
| Common perk | Danger Close/Eagle Eye/Equalizer 30%, Damage Combo 15%/combo level, Power Armor 15%/plate | add pool, conditional |
| Epic perk | Collector 3%/perk level, Gemstone 4%/island, Tasty Orange 1%/kill on island, Checklist 1%/25 kills, Assassin 1%/10 one-shots, Silver Lining 2%/hit taken, Money Is Power (25000), Health Is Power (2500) | scaling |
| Greed perk | Glass Cannon +75% (−50% max HP); Big Bones, Brute Force, Damage Seeker, Up The Ante +25% each | mul pool |
| Epic relic | Ring of Power +400% weapon dmg mult / −80% fire rate; Ring of Fury +50% global 5 s after a kill; Ring of Precision +150% crit dmg; elemental rings 250; Time Ring +250% DoT | slot-limited |
| Greed relic | Trigger Ring +250% fire rate / −90% weapon dmg; Ring of Tankiness +2500 HP / −60% move speed | trade-off |
| Ability mod | Most 100 (flat damage for spawners), Iron Explosion +200% (+40% cooldown), Damage Explosion +250% (Epic) | ability only |
| Melee mod | Iron Claws +300% (+40% cooldown), Time Claws 150, Sharp Claws 100 | melee only |

## 2. Damage formula [C unless marked]
Projectile damage is fixed **at projectile setup** (0x140d61a70 writes proj+0x280) and **re-scaled on hit** by CalcOutgoingDamage.

```
Hit = ProjDamage(DA) × WM(0x140d25c30) + SharpShot flat (proj+0x284)
    × GetDamageBuff(0x140d23700) = ADD × MUL
    × situational add-ons inside CalcOutgoingDamage(0x140d21480)
    × crit term
    clamp multiplier ≥ 0.01
```

**WM, the weapon-mod multiplier (0x140d25c30)** [C]:
- Start at W = 1 and add each weapon mod's buff (all linear, B×0.01×L):
  - Accelerating(3), ZigZag(4), Spiral(5), Snake(6), Chaotic(7), Boomerang(8), Orbiting(9), Recoil(10), HighCaliber(14).
  - WindUp(15): min(round(buff), shots) × 0.05.
  - Steady(16) when aiming, minus Trick's debuff; Trick(17) when hip-firing, minus Steady's debuff.
  - Aerial(18) when airborne.
  - BlindFire(20), TimeShot(21), Glue(25), BigShot(26).
  - Streak(27) × min(streak, 100).
  - MagShot(28): min(round(buff), ammo) × 0.03.
  - Uppercut(29) when shot# % 5 == 0.
  - Heavy(30), HeavyHitter(31), DamageShot(54), Juiced(56).
- Then WM = (1 − Brawler melee debuff) × W × (1 + RingOfPower relic 25) × (1 − TriggerRing debuff 61).

**ADD, the perk additive pool (0x140d23700)** [C]. ADD = PS.DamageMultiplier (PS+0x3A0, base 1) plus:

| Perk | Contribution |
|---|---|
| Vitality(12) | if at high health (0x140cbaf10) |
| DamageCombo(13) | × combo level (PS+0x354) |
| AntiCrit(18) | buff |
| PowerArmor(27) | × armor plates |
| Equalizer(28) | if the perk-array count is even |
| TastyOrange(68) | × island kills (PS+0x35C) |
| Collector(72) | × Σ perk levels |
| MoneyIsPower(73) | min(crystals, 25000·L) × 4e-5 (= +100%/level at the cap) |
| HealthIsPower(74) | min(2500·L, health) × 4e-4 |
| SpeedIsPower(75) | (SpeedDemon(29) buff + Ring of Swiftness relic 49) × buff75. Speed Demon 0.2 gives +20%; Swiftness 3.0 gives +300% |

Gemstone, Checklist, Assassin and Silver Lining accumulate into PS.DamageMultiplier or AccumulatedBuff [I].
REPORT.md labels perk 74 as SpeedIsPower; the code shows 74 = HealthIsPower and 75 = SpeedIsPower [C].

**MUL, the multiplicative pool** = 1 + BigBones(108) + GlassCannon(109) + DamageSeeker(110) + BruteForce(111) + UpTheAnte(114) + RingOfFury relic(27) if a kill happened within the last 5 s (PS+0x360) [C]. **GetDamageBuff = ADD × MUL.**

**Add-ons inside CalcOutgoingDamage (0x140d21480)**, added to B before crit [C]:
- ElementalSpecialist(14) if the target has exactly one debuff type.
- ElementalExpert(15) × the number of debuff types present (ice/fire/lightning/poison/arcane).
- DangerClose(25) when within 2500u; EagleEye(26) when beyond 5500u.
- Full Metal Jacket (relic 23) adds an armor-penetration tag.
- Elemental stack scaling:

| Element | Stack multiplier | Zeroed by |
|---|---|---|
| Ice | 1 + IceCold(40) + IceRing(30) | FireRing |
| Fire | 1 + Firestarter(41) + FireRing(31) | IceRing |
| Lightning | 1 + HighVoltage(42) + LightningRing(32) | PoisonRing |
| Poison | 1 + PoisonRing(33) | LightningRing |
| Arcane | 1 + PotentMagic(44) + ArcaneRing(34) | none |

**Crit** [C]:
- Allowed for damage types other than 7–10. Type 6 uses crit chance × 0.15.
- GetCritChance (0x140d22f20), in %: (1 + Sharpshooter(16) + HotShot(17) − AntiCrit debuff + RingOfWisdom accumulated) × (1 + RingOfLuck(51)). Halved with Overspill(60). **Base crit chance is 1%.** Roll is 0x140d360a0.
- On crit: B ×= (2.0 + PowerPunch(19)) × (1 + CriticalThinking(20) + RingOfPrecision(29)).
  - With Overspill: × (1 + crit% × 0.01).
  - MegaCrit(76): chance of an extra ×3.
- Crit procs:
  - CriticalArrow(21).
  - CriticalBlast(77): radial 40×L damage, 2000u radius.
  - CriticalLightning(98): 10000u radius.
- Non-crit with CriticalThinking: B ×= (1 − debuff).
- ModifyOutgoingDamage is 0x140d11a10.

**Buff getters** [C]:
- Perk/mod buff 0x140d2e5b0(DA, L):
  - Linear: B·0.01·L.
  - Hyperbolic (DA+0xCC): 1 − ((100−B)·0.01)^L.
  - ×100 when bGetBuffAsMultiplier (DA+0xCE) is false, i.e. raw units.
- Debuff 0x140d2dfa0 works the same way (BaseDebuff 0xD0, bHyperbolicDebuff 0xD4).
- Weapon mod 0x140d31f40: if TriggerChanceType (DA+0xD9) == 2 (HalfScaling), buff = B + (L−1)·0.5·B.
- Relics 0x140d31c90 / 0x140d31cd0 always use L = 1.
- Inventory array buff 0x140d2df60.

## 3. Benchmarks: what a rarity is worth (% effective DPS)
All values are relative DPS against one target and ignore procs' own damage, which isn't extracted [I where noted]. "Empty" means no other item in the same pool.

| Item (L1) | Empty build | Typical mid-run build* | Notes |
|---|---|---|---|
| Common WM +50% | ×1.50 | ×1.11 | W pool |
| Common WM +75% | ×1.75 | ×1.17 | W pool |
| Common WM +100% (Time/Orbiting) | ×2.00 | ×1.22 | W pool |
| Epic Damage Shot +150% | ×2.50 | ×1.33 | W pool |
| Epic Juiced 15% (dmg + fire rate + reload) | ≈×1.32 | ≈×1.18 | assumes fire rate is multiplicative [I] |
| Epic Supercharged +25% fire rate | ≈×1.25 | ≈×1.25 | fire rate is a separate axis [I, UpdateFireRate 0x140d69f70 not traced] |
| Epic Triple Shot 7% (×roll mult) | AutoRifle 2.5 ⇒ 17.5% chance of +2 shots ≈ ×1.35 | same | chance is multiplied by WeaponModTriggerRollMultiplier [C]; effect size [I] |
| Legendary Firepower +1 projectile | ×2.0 on single-shot weapons, ×1.33 on Triple formation | same | [I] |
| Legendary 3% proc, e.g. Torpedo/Beam/Spark | AutoRifle: 7.5% of shots; Sniper: 21% | same | SecondaryBuff damage per proc not extracted [I] |
| Epic perk Collector 3%/perk level | +3%/level ⇒ ~+60–90% at 20–30 perk levels | | ADD pool |
| Epic perk Gemstone 4%/island | +4%/island ⇒ +120% by island 30 | | ADD pool [I accumulation] |
| Greed Glass Cannon +75% | ×1.75 | ×1.75 | MUL pool; nearly undiluted |
| Greed Big Bones/Brute/Seeker/Ante +25% | ×1.25 | ×1.20 (next to Glass Cannon) | MUL pool |
| Relic Ring of Power +400% / −80% fire rate | ×5 per shot × 0.2 rate ⇒ ×1.0 sustained DPS | | favours burst and per-hit flats; costs a relic slot |
| Relic Trigger Ring +250% rate / −90% dmg | ×3.5 × 0.1 ⇒ ×0.35 DPS | | only good for proc and on-hit builds [I] |
| Relic Ring of Fury +50% (5 s after a kill) | ×1.5 MUL | | near-permanent while killing |
| Relic Ring of Precision +150% crit dmg | crit hits ×(2)·(2.5) = 5 instead of 2 | | worth little until crit chance is high |

\*Mid-run W ≈ 1 + 3.5 (e.g. five +50–100% commons); ADD ≈ 1.6; MUL ≈ 1.

**Rule of thumb for vanilla value per rarity at L1**:
- Common ≈ +50–100% of base (×1.1–1.2 marginal mid-run).
- Epic ≈ 2× a Common in the W pool, or a 2–3% (×roll mult) proc.
- Legendary ≈ a new damage source or a multiplier on projectile count (×1.3–2).
- Greed ≈ a true multiplier (×1.25–1.75) with a real drawback.

**Level compounding** [C formulas]:
- Linear items: L levels give L×B in their pool. Five dupes of +75% (+375%) on an otherwise-empty W pool ⇒ ×4.75, a straight line. Each extra level is worth less as a ratio (L→L+1 is ×(1+B(L+1))/(1+BL)).
- HalfScaling procs: chance = B·(1 + 0.5(L−1)). A 3% proc reaches 4.5% at L2, 9% at L5 and 16.5% at L10, then × roll mult. Procs therefore scale at about half the rate of linear items, but they multiply with projectile count and fire rate.
- Hyperbolic items (40 in the catalog, e.g. 3% elemental shot chances): 1 − (1−B)^L. 3% gives 5.9% at L2, 26.3% at L10 and 45.6% at L20.
- Limited items (bLimited, DA+0xCD): offered only until buff × roll mult ≥ 100% [C, 0x140d23e90].
- Most stacking happens in the add pools, so a late run with 10–20 levels across many Commons sits at W ≈ 10–20. One new Common then adds under 10% DPS, while a Greed MUL item still adds 25–75%. This is the main power curve: additive dilution, broken only by MUL-pool Greed items, crit, projectile count and procs.

## 4. Loot roll mechanics [C unless marked]
**LootRoll 0x140d33240**:
- Picks a random player and reads their held items.
- Rarity multiplier m:
  - m = 1 by default; m = 0 for the Lesser pool (secret chest).
  - Shop island (GS+0x2D8 == 9): with 60% chance, m = min(CurrentIsland(GS+0x2D4) × 1.25, 200).
  - Otherwise (not the key chest), difficulty modifiers (GS+0x2A8) apply: Lucky(5) sets m = 2; UltraLuck(6) adds 50; UltraChaos(16) adds 0.5. These are skipped on Arcade and Holdout.
- Upgrade chances (0x140d31c00): Epic 3.75%·m, Legendary 0.25%·m.
  - With 3 choices, P(≥1 Epic) ≈ 11% and P(≥1 Legendary) ≈ 0.75% at m = 1. Assumes an independent roll per choice [I].
  - An upgrade only happens if an eligible item of that rarity exists in the pool.
- Streamer Loot perk (50): if the base rarity is < Legendary, it forces Legendary with chance buff × 0.15%; if < Epic, it forces Epic with chance buff%.
- **Upgrade offers** (a held item offered again to add a level):
  - Chance = 1/(n+1) × (0.66 Epic | 0.33 Legendary) × param_6 %, where n = upgrades already offered in this roll.
  - param_6 = min(CurrentIsland, 50). Upgrade Ring sets it to 1e9, i.e. always.
  - So upgrade offers scale with island number up to island 50.
- Shop rolls: each Greed-rarity item has a 5% chance to enter.
- Final pick is weighted by SpawnWeight (0x140d32450).

**Chest roll 0x140caf1c0**:
- Choices = NumPickupsToSpawn + round(BigChests(80) buff − LimitedLoot debuff(120)) + 2 (Coral Amulet relic 13) − 2 (Upgrade Ring relic 65), minimum 1.
- Chest ctor 0x140cae770 defaults: Common, 3 choices, cost 0.

| Chest BP | Pool | Base rarity | Choices | Cost |
|---|---|---|---|---|
| Critical/Damage/Economy/Elemental/Health/Luck/Random/Skill/Speed | that pool | Common | 3 | 0 |
| Epic | EpicChest | Epic | 3 | 0 |
| Legendary | Legendary | Legendary | 4 | 0 |
| Greed | Greed | Greed | 1 | 0 |
| Key | KeyChest | | 1 | keys |
| Regeneration | | | 2 | |
| Upgrade | | | 2 | |
| Secret | Lesser (m = 0) | | 1 | |
| Spiked | SpikedChest(14) | Epic | | 25% base max HP (CurrencyType MaxHealth) |
| Relic | RelicChest | | | |

Currencies: Crystal 0, Key 1, Health 2, MaxHealth 3 (pay 0x140caf9f0).

Portal reward weights (REPORT.md):
- Damage, Health, Random: 1.25.
- Critical, Elemental, Speed, Luck, Economy, Skill: 0.75.
- Relic: 0.5. Upgrade: 0.4. Greed: 0.2.

So the expected chance of a Relic chest per island is ≈ 0.5/9.5 ≈ 5%, and of a Greed chest ≈ 2% [I: assumes all are eligible].

**Levels, duplicates and slots**:
- An inventory element is 0x28 bytes. Level = stack count.
- A duplicate pickup adds levels to the existing entry, capped at 255.
- Can-pickup check (e.g. weapon mods 0x140d22c70):
  - If held: L < 255, and for limited items, not maxed.
  - If new: count < NumSlots.
- Default slots (0x140d29e50): 24 weapon mods, 12 ability mods, 12 melee mods, 24 perks. Relics are capped at 10 (CanPickupRelic 0x140d22c30).
- Relics never level.

## 5. Weapons, abilities, melee (class defaults) [C]
Weapon DA ctor 0x140d6ed80 defaults: fire rate 10, burst 3 @0.06, roll mult 1.0, clip 24, reload 1.0, Automatic. Projectile DA defaults (0x140d698f0): lifetime 2.5, speed 6500, explosion radius 1000 / damage 100, collision radius 10.

Base DPS = dmg × rate × projectiles. Pellet counts for cluster, arc and triple formations are not extracted [I].

| Weapon | Dmg/proj | Fire rate | Formation | Roll mult | Clip | Reload | ≈DPS (1 proj) | Extras |
|---|---|---|---|---|---|---|---|---|
| Sniper | 100 | 1 | Single | 7 | 6 | 2.0 | 100 | pierce 4 |
| Seagle | 25 | 4 | Single | 4 | 10 | 1.25 | 100 | |
| AutoRifle | 10 | 10 | Single | 2.5 | 35 | 1.25 | 100 | |
| OrbLauncher | 8 | 15 | Single | 2 | 42 | 1.0 | 120 | |
| IceStaff | 12 | 10 | Single | 3.5 | 25 | 1.25 | 120 | radius 250, Ice Shot, 25% area |
| LMG | 12 | 6.5 | Single | 1 | 64 | 1.75 | 78 | |
| Minigun | 3 | 25 | Single | 1.5 | 200 | 2.25 | 75 | Escalating Shot |
| RocketLauncher | 75 | 0.9 | Single | 7 | 5 | 1.75 | 68 + explosion | radius 1000 |
| ArcaneWand | 25 | 2 | Single | 5.5 | 8 | 1.25 | 50 | radius 1500, Arcane Shot L2 |
| LaserCannons | 3 | 16 | dual | 1.5 | 50 | 1.25 | 48 (×2?) | |
| MarksmanRifle | 15 | 2.75 | Triple | 5 | 16 | 1.25 | 41 (×3 = 124) | |
| DualPistols | 6 | 6 | dual | 4 | 20 | 1.0 | 36 (×2?) | |
| SMG | 2 | 16 | Single | 1 | 40 | 0.75 | 32 | |
| BurstPistol | 8 | 4 | Burst 5 @0.05 | 4 | 30 | 1.0 | 32 (×5 = 160 if rate = bursts) | |
| Flamethrower | 2 | 12 | Single | 2.875 | 40 | 1.25 | 24 | pierce 3, Fire Shot, 15% area |
| Crossbow | 12 | 2 | LargeArc | 5 | 12 | 1.0 | 24 × pellets | |
| BladeLauncher | 9 | 3 | Single | 5 | 9 | 1.25 | 27 | pierce 3, prox 250 |
| ClusterLauncher | 8 | 2.25 | MediumCluster | 4 | 8 | 1.75 | 18 × pellets | radius 750 |
| AutoShotgun | 4 | 4 | MediumCluster | 3.5 | 12 | 1.25 | 16 × pellets | |
| LightningScepter | 2 | 7 | SmallArc | 3 | 25 | 2.0 | 14 × pellets | radius 325, 1 bounce, Lightning Shot |
| DualShotguns | 8 | 1.2 | MediumCluster, dual | 4 | 5 | 1.0 | 9.6 × pellets | |
| BurstRifle | 3 | 3 | Burst 3 @0.075 | 2.5 | 30 | 1.0 | 9 (×3) | |
| PoisonCannon | 2 | 0.5 | Burst 3 @0.1 | 5 | 6 | 2.5 | 1 (×3) + poison | radius 1250, Poison Shot |
| ProcGenerator | 1 | 8 | Single | **100** | 22 | 1.25 | 8 | every proc mod ×100 ⇒ near-guaranteed |

Every weapon DA has Rarity Greed. **Roll mult is the main hidden scaler for chance items**: a 3% Legendary proc is 3% on LMG/SMG, 21% on Sniper/Rocket, and effectively 100% on Proc Generator.

**Abilities** [C, DA_Ability_*]:
- Cooldowns: Air Strike 10 s (armor-piercing), Black Hole 9 s, Electro Globe 10 s, Ice Blast 6 s, Laser Beam 6 s.
- Grenade and Grappling Hook use the native default cooldown, which was not extracted.
- Ability damage comes from projectile DAs; the projectile default explosion is 100 dmg in 1000u [C default, I that abilities use it].

**Melee** [C, DA_Melee_*]:

| Melee | Dmg | Cooldown | Range | Mod debuff mult | Notes |
|---|---|---|---|---|---|
| Claw | 100 | 3 s | 2000 | 4 | 360°, knockback 7500 |
| Hammer | 125 | 6 s | 2500 | 5 | slam |
| Pickaxe | 50 | 5 s | 750 | 3 | +8 line strikes @0.1 s over 6000u |
| Katana | 33 | 4 s | 1850 | | dash |
| Dagger | 40 | native default | 1250 | | fast |

Melee-mod Iron Claws (+300%, +40% cooldown) is the biggest single Common buff in the game, but it applies only to melee.

## 6. Economy
**Confirmed:**
- Crystals live at PS+0x470 and are added via 0x140d20d00 (saturating). Keys live at PS+0x3B0.
- Chest payment uses `Cost`, an int at +0x2B4. Only the Spiked chest sets a non-zero BP cost (25% of base max HP); normal chests are free.
- Money Is Power turns crystals into damage: +1% per 250 crystals, up to +100% per level at 25000.
- Greed economy perks: Limited Loot, Rising Star and Workaholic give +125% crystal drops, each with a drawback. Bribe costs 5% of crystals.
- Key totem (0x140cb5860) spends keys to permanently unlock one locked pickup (weapons first). That is meta-progression, not in-run power.

**Totems (BP defaults)**:

| Totem | SpawnWeight | ChanceToExplode | Interact cooldown |
|---|---|---|---|
| Enhancement | 2.5 | 75% | |
| Upgrade | 2.5 | 50% | |
| Health | 0.5 | 75% | |
| Greed | 0.4 | 100% | |
| Gold | 0.3 | 100% | |
| Random | 0.1 | 100% | |
| Reroll | 1.0 | 25% | 1 s |
| Crystal | 1.0 | 60% | |
| XL | 1.0 | 75% | 1 s |
| Chance, Fuse, Gamble, Glass, Loot | 1.0 | 100% (single use) | |
| Contract | 1.0 | | 1 s |

"ChanceToExplode" is the % chance the totem breaks after each use [I]. Totem crystal prices are not in BP defaults, so they come from native code or the game state and are **not extracted**.

**Not extracted [I]**: crystal drop per kill, shop pedestal prices (BP_ShopPedestal has no price override), and Paycheck's "price of a common pickup".
- Acquisition-rate estimate [I]: one chest per island with 3 choices, so about 1 item per island plus shop purchases.
- Perks and weapon mods dominate (Σ weights ~120 Perk / ~92 WM vs Relic ~18 inside mixed pools).
- Relics come mostly from Relic portals (≈5% of islands) and the 0.3-weight relic entries in normal pools. Expect about 3–6 relics per 30-island run, which is why relics are deliberately large (100–400%) and non-levelling.
- Upgrade offers rise linearly with island (min(island, 50)%). After about island 20, most chests offer at least one level-up of a held item, which is the main late-game compounding source.

## Open items / caveats
- Not traced:
  - Fire-rate formula (UpdateFireRate 0x140d69f70).
  - Formation pellet counts.
  - Proc projectile SecondaryBuff damages.
  - Ability base damages.
  - Shop and totem prices; crystal drop rates.
  - Whether rarity rolls happen per choice or per chest.
- Enemy-side scaling (health per island, difficulty) is outside this report. A separate agent was studying difficulty scaling.
