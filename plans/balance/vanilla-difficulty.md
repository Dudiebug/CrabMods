# Crab Champions (vanilla) - run loop & difficulty scaling

Source: game.exe (PE ts 0x6921D2F7), Ghidra/capstone, SDK dumps, `Content/Blueprint` assets. No mod code consulted.
Tags: **[C]** = confirmed from disassembly/asset, **[I]** = inferred / not fully traced.
Notation: I = `GS.CurrentIsland` (+0x2D4, 1-based), L = loop = floor((I-1)/28), B = floor(I/7), k = island-in-biome = I%7 (0 -> 7), N = players.

## 1. Run structure

- 7 islands per biome: `FUN_140cf1f20` returns constant 7 **[C]**. Final biome test `FUN_140cf1510`: ((I-1) mod 28) in 21..27, so 4 biomes x 7 = **28 islands per loop** **[C]**.
- Exit portals per island, from `SpawnPortals 0x140d00210` (SpawnablesDA portal slots +0x618..+0x678) **[C]**:
  - Lobby (type 1): NewRun portal, plus ContinueRun if an autosave exists.
  - k=1..4: 3 portals. P1 is Arena; when k<4 it becomes Horde 40% or Demolition 20% of the rest. P2 is Arena, or Gauntlet at 20% when k==4. P3 is Arena, or Elite at 15% when k<4 and k!=2. Each player holding Relic 10 adds one extra P1-style portal. A later 15% roll for k in {3,4,6} swaps one portal for a special one, likely Parkour **[I]**.
  - k=5: Shop portal only, so **island 6 of each biome is the Shop** (no enemies).
  - k=6: Elite portal, or **Boss** in the final biome, so island 7 of biomes 1-3 is Elite and island 28 is Boss.
  - k=7: Biome portal. After island 28 you get **Loop portal + CrabIsland portal**. Victory = clearing island 28 (Boss) then taking CrabIsland; Loop continues endlessly at I=29 with L=1 **[C portal set / I victory semantics]**.
- Island types (`ECrabIslandType`): Arena 4, Horde 5, Demolition 6, Gauntlet 7, Parkour 8, Shop 9, Elite 10, Boss 11, Loop 14, Arcade 15, Holdout 16. Arcade and Holdout are side modes; all difficulty-modifier checks return false there (`FUN_140cf1490`) **[C]**.
- Difficulty is **not one scalar**. `GS.Difficulty` (ECrabRank Bronze..Prismatic, +0x2A4) is a label. Scaling comes from `GS.DifficultyModifiers` (+0x2A8) **[C: formulas read only modifiers; I: rank = unlock tier]**.
  - Relaxed(0): enemy HP base 1.0, half action speed, +1.0 player MaxHealthMultiplier.
  - Nightmare(8): bigger HP/damage exponents, +0.5 budget.
  - BuffedEnemies(10), AmbushingElites(12), EternalPunishment(13, -0.5% max HP per hit), UnfairBosses(14), UltraChaos(16), WanderingElites(17), ImmortalSnail(20), ImpossibleModeI-IV(22-25, see section 2), LockedSlots(9, slots = 8), Lucky(5)/UltraLuck(6) rarity, DiscountedShops(3, prices x0.8), StartingGift(2, extra Epic chest on island 1).
- Challenge modifiers (`GS+0x2F0`): Twinned(10), Speedy(12, move x2.25), Jumpy(13), Explosive(17, budget x1.5, explosive spawn weight x10), StarfishParty(18, 10 starfish). Any challenge gives budget x1.2 on non-Elite/Boss islands **[C]**.

## 2. Enemy scaling

### Health multiplier `GetEnemyHealthMultiplier 0x140cf1060` -> `LM+0x4F0` (set in `UpdateEnemyStats 0x140cfc8f0` at island start) [C]
```
base = 1.5 (Relaxed 1.0; Nightmare +0.5; UltraChaos +0.25)
per  = 0.5 (Nightmare 0.65; ImpossibleI x2)      e = 0.1L+1.33 (Nightmare 0.2L+1.5)
h = base + 1.1 * B^((L>1 ? 1.25 : 1) * e)
h *= {I=1:0.5, I=2:0.6, I=3:0.7}          h *= 0.5 if Horde
h *= (B*per + 1)  unless I == 7            h *= (0.5L + 1)
h *= 1 + (Nalive-1)*0.45   (k=0.33L+0.75 on Elite/Boss with Nightmare|UltraChaos)
h *= 1.45
```
- Per-enemy HP (`0x140ca5ca0`) = BaseMaxHealth x h x (1 + DoubleTrouble(106) buffs + 1 if Healthy buff), at least 1. Wandering elites on normal islands x0.75; bosses on normal islands x0.5 **[C]**.
- Horde: h x1.05 per wave, compounding. Gauntlet: x(0.25*wave+1). Arcade: h = 0.33. Holdout: 0.2+0.01*min(wave,50) **[C]**.
- Other stats **[C]**:
  - MoveSpeed = 1+0.25L.
  - ActionSpeed = 1+0.15L (Nightmare base 2, Relaxed 0.5).
  - **Damage mult** = (I^(1.15*f) * 0.2 + 1) * 0.75, where f = 1.1 when L>=2. Nightmare/UltraChaos use exponent 0.1L+1.25; ImpossibleII doubles the exponent.
- BuffedEnemies: 20% per non-elite enemy. Angered = damage x1.5, Frenzied = action x1.75, Speedy = move x2.25 **[C]**.

### Table (Normal, no modifiers; "HP" = h; price = crystals from `0x140cf1fc0`, see section 3)
Values are for Arena/Gauntlet/Elite/Boss islands. Horde islands are half at start. h is flat within a block of 7 islands and jumps when B increments, at I = 7, 14, 21, 28.
| I | biome.k | loop | HP x1p | HP x2p | HP x4p | HP Nightmare | rel. to I1 | enemy dmg | Common/Epic/Leg crystal price |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 1.1 | 0 | 1.09 | 1.58 | 2.56 | 1.45 | 1.0x | 0.90 | 482/1204/3853 |
| 2 | 1.2 | 0 | 1.30 | 1.89 | 3.07 | 1.74 | 1.2x | 1.08 | 498/1246/3987 |
| 3 | 1.3 | 0 | 1.52 | 2.21 | 3.58 | 2.03 | 1.4x | 1.28 | 517/1292/4135 |
| 4 | 1.4 | 0 | 2.17 | 3.15 | 5.11 | 2.90 | 2.0x | 1.49 | 537/1342/4294 |
| 5 | 1.5 | 0 | 2.17 | 3.15 | 5.11 | 2.90 | 2.0x | 1.70 | 558/1394/4460 |
| 6 | 1.6 | 0 | 2.17 | 3.15 | 5.11 | 2.90 | 2.0x | 1.93 | 579/1448/4634 |
| 7 | 1.7 | 0 | 3.77 | 5.47 | 8.86 | 4.50 | 3.5x | 2.16 | 602/1504/4814 |
| 8 | 2.1 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 2.39 | 625/1562/4999 |
| 9 | 2.2 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 2.63 | 649/1621/5188 |
| 10 | 2.3 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 2.87 | 673/1682/5382 |
| 11 | 2.4 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 3.11 | 698/1744/5580 |
| 12 | 2.5 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 3.36 | 723/1807/5781 |
| 13 | 2.6 | 0 | 5.66 | 8.20 | 13.29 | 7.42 | 5.2x | 3.62 | 748/1871/5986 |
| 14 | 2.7 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 3.87 | 774/1936/6194 |
| 15 | 3.1 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 4.13 | 801/2002/6405 |
| 16 | 3.2 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 4.39 | 827/2068/6619 |
| 17 | 3.3 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 4.65 | 854/2136/6836 |
| 18 | 3.4 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 4.92 | 882/2205/7055 |
| 19 | 3.5 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 5.18 | 910/2274/7276 |
| 20 | 3.6 | 0 | 12.37 | 17.94 | 29.07 | 17.05 | 11.4x | 5.45 | 938/2344/7500 |
| 21 | 3.7 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 5.72 | 966/2414/7726 |
| 22 | 4.1 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 6.00 | 994/2486/7954 |
| 23 | 4.2 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 6.27 | 1023/2558/8185 |
| 24 | 4.3 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 6.55 | 1052/2630/8417 |
| 25 | 4.4 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 6.83 | 1081/2704/8651 |
| 26 | 4.5 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 7.11 | 1111/2777/8888 |
| 27 | 4.6 | 0 | 22.63 | 32.81 | 53.17 | 33.00 | 20.8x | 7.39 | 1141/2852/9126 |
| 28 | 4.7 | 0 | 36.77 | 53.31 | 86.40 | 56.38 | 33.8x | 7.67 | 1171/2927/9365 |
| 29 | 1.1 | 1 | 61.90 | 89.75 | 145.46 | 106.58 | 56.9x | 7.96 | 2815/7036/22516 |
| 30 | 1.2 | 1 | 61.90 | 89.75 | 145.46 | 106.58 | 56.9x | 8.25 | 2886/7214/23086 |
| 35 | 1.7 | 1 | 95.07 | 137.84 | 223.40 | 175.34 | 87.4x | 9.70 | 3249/8122/25991 |
| 42 | 2.7 | 1 | 137.12 | 198.82 | 322.23 | 267.87 | 126.1x | 11.79 | 3775/9437/30199 |
| 49 | 3.7 | 1 | 188.68 | 273.59 | 443.41 | 387.06 | 173.5x | 13.93 | 4319/10797/34549 |
| 56 | 4.7 | 1 | 250.33 | 362.97 | 588.27 | 535.71 | 230.2x | 16.11 | 4878/12196/39026 |
| 57 | 1.1 | 2 | 872.73 | 1265.46 | 2050.92 | 2796.67 | 802.5x | 25.71 | 57132/142829/457052 |
| 70 | 2.7 | 2 | 1590.84 | 2306.72 | 3738.47 | 5717.01 | 1462.8x | 33.12 | 69566/173915/556528 |
| 84 | 4.7 | 2 | 2617.61 | 3795.53 | 6151.38 | 10315.33 | 2407.0x | 41.52 | 83484/208710/667873 |
| 85 | 1.1 | 3 | 4450.01 | 6452.52 | 10457.53 | 23943.79 | 4092.0x | 42.13 | 143763/359406/1150100 |
| 112 | 4.7 | 3 | 10242.77 | 14852.02 | 24070.52 | 65912.14 | 9418.6x | 59.41 | 191723/479308/1533786 |

Notes: B keeps counting across loops, and L multiplies in three places: the exponent, (0.5L+1), and x1.25 on the exponent when L>1. So the loop-1 start (I29) is about 1.7x the I28 boss island, and loop 2 (I57) is about 14x loop 1's start **[C formula; table computed]**.

### Base HP by category (`BP_Enemy_*` HealthComponent BaseMaxHealth; `FCrabEnemySpawnSettings`) [C]
| Category | HP | SpawnCost | From island | Examples |
|---|---|---|---|---|
| Easy (1) | 15-45 | 30-50 | 1 | Crab 15, Slug 30 (explosive), MiniScorpion 30, BlockerCrab 45 (+1 plate) |
| Medium (2) | 50-150 | 125-400 | 2 | Ant/Grub/Limpet/Skull 75, Hornet/Starfish 50, element Slugs 100, Pumpkin 150 |
| Hard (3) | 175-300 | 850 | 3 | Crab variants 175 (+2 plates), Skulls 200-225, FireCrab 225, Scorpion 300 |
| Elite (4) | 650-825 | 2000 | 3 | most 825; Hornet/LightningCrab 650; LandmineLimpet 750 |
| Boss (5) | 1000-2000 | 2500 | 3 | BossAnt 1000 (+8 plates), BossSkull 2000 |

### Spawn budget & waves (`FUN_140cfef00` next wave, `FUN_140cf9720` finalise, `FUN_140cfe9c0` spawner) [C]
- mult = 1.5 (Relaxed 1.0, Nightmare 2.0).
- **Arena** (2 waves, both queued at start):
  - Wave 1: budget 40*k*mult. Easy (25% Medium when k>2), 2-4 enemy types.
  - Wave 2: budget 60*k*mult. Medium if k<3, else Hard.
- **Horde**: Easy only (10% max Medium), 1-3 types, budget x15 per wave. The next wave starts when alive < 1.5x the last wave size.
- **Gauntlet**: categories 1-3, 3-6 types, budget (0.25*wave+1)*mult*65, 5 waves.
- **Elite/Boss** (`0x140d04910`): count = 1 + min(L,2) + UnfairBosses + WanderingElites + 2*XL + greed term **[C]**.
- Finalise: x1.75 when looping; x(1+greed perks); x2 per XL level; ImpossibleIII x2; challenge x1.2/x1.5; **x(1+0.4(N-1))** x0.7. Points = round(budget)*10, then enemies are drawn by SpawnWeight until SpawnCost exceeds the points. Looping/XL doubles the number of types; WanderingElites raises max category to 5 from wave 2.
- Spawning: one at a time while alive < cap (LM+0x524; 100 in Arcade/Holdout). Interval 0.2s Easy / 0.6s Medium / 2.5s Hard+; 0.15s on Horde or when looping.
- **Player count**: per-enemy HP x(1+0.45(N-1)) **and** budget x(1+0.4(N-1)). Total HP to chew through per island grows about 1.45*1.4 = 2.03x at 2p and 2.35*2.2 = 5.2x at 4p, versus N x player DPS.
- Example: Arena I=5, 1p: wave 2 = 60*5*1.5 = 450 -> x0.7 -> 315 -> 3150 points, about 3 Hard enemies of 175-300 HP x h 2.17, i.e. 380-650 HP each **[I composition, C numbers]**.

## 3. Rewards cadence

- **End of island** (`0x140d01610`, host, **one chest per player**) **[C]**:
  - Normal islands: the class comes from the reward pool the portal showed, at SpawnablesDA +0x518..+0x578, e.g. Damage/Crit/Elemental/Speed/Luck/Health/Economy/Skill/Greed/Upgrade/Random/Relic.
  - Parkour: Secret/Random/Epic/Legendary chest by medal.
  - Elite island: Epic chest (+0x588). Boss island: Legendary chest (+0x590).
  - Island 1 also gives a Regeneration chest (+0x598); StartingGift adds an Epic chest.
  - Perks 0x29/0x2A give extra chests on no-damage clears.
- Portal reward-pool weights (`SpawnPortals`, no repeats per set): Damage/Health/Random 1.25; Crit/Elemental/Speed/Luck/Economy/Skill 0.75; Relic 0.5; Upgrade 0.4; Greed 0.2 **[C, REPORT]**.
- Choices per chest = `ACrabChest.NumPickupsToSpawn` (+0x2AC), then perks/relics, at least 1 (`0x140caf1c0`). From the BP assets: Legendary 4, Regeneration 2, Upgrade 2, Greed 1, Key 1, Secret 1. Pool chests (Damage etc.) use the native default, not overridden in BP; likely 3 **[I]**. Spiked: Epic, costs 25% base max HP.
- **Rarity roll** (`LootRoll 0x140d33240`, per choice slot; base % from `0x140d31c00`) **[C]**:
  - Legendary 0.25% x m, Epic 3.75% x m. Otherwise the chest's own rarity (Common for pool chests).
  - m = 1; Lucky -> 2; UltraLuck +50; UltraChaos +0.5.
  - **Shop**: 60% chance that m = 1.25 x I (cap 200). At I=6 that gives Epic 28% / Legendary 1.9%; at I=20, Epic 94% / Legendary 6%.
  - Luck pickup buff 50: forced Epic at Buff%, forced Legendary at 0.15 x Buff%.
  - **Rarity does NOT scale with island number except in shops** **[C]**.
- **Prices** `0x140cf1fc0(rarity, currency)` **[C]**:
  - V = (1+(0.05I)^1.2) x c x (0.25L+1)^2 x [0.8 DiscountedShops]. c = 2.34375L+4.6875 for L<=1, else 14.0625L+28.125.
  - Crystals: Common 100V, Epic/Greed 250V, Legendary 800V (see table).
  - Keys: 2 / 5 / 16, flat.
  - Health currency: 15I / 38I / 120I HP.
  - Shop totems use multiples of the Common price: x1.5, x0.3*I, x1.6 (Epic), x1, and (0.08+0.012(I-1))*I* (`kt2.c` ~1800).
  - The crystal income formula was not traced **[open]**.
- Totems (`0x140cb5860`): Health totem gives +base max HP or -50%; Loot -8% max HP; Gold -33% **[C, REPORT]**.
- Expected haul at island N (1p, normal path): about N-1 chest picks (one per cleared island; shop islands buy instead), plus 3 Epic picks (I7/14/21 elites) and 1 Legendary (I28), plus roughly 1 shop purchase per biome. **Items scale linearly with N; enemy HP scales by about 1.4-1.9x per biome** **[I]**.

## 4. Player baseline

- New run (`0x140d29e50`) **[C]**:
  - BaseMaxHealth = **250**; MaxHealthMultiplier = 1.0; DamageMultiplier = 1.0; ScaleMultiplier = 1.0.
  - Slots: 24 weapon mods / 12 ability / 12 melee / 24 perks (LockedSlots: 8). Relics capped at 10.
- Max HP = max(0.01, 1 - buff0x6D - buff0x76) x (MaxHealthMultiplier + perk 6, 0x68-0x6C) x (BaseMaxHealth + perk1 + relic 0x3B) (`0x140d28a30`) **[C]**.
- **Damage** (`GetDamageBuff 0x140d23700`) **[C]**:
  - add = DamageMultiplier + Buff13 x Combo + Buff12 + Buff18 + ... + TastyOrange 1%/kill this island + Collector 3%/perk level + MoneyIsPower + SpeedIsPower.
  - mul = 1 + Buff108..111 + Buff114 (+Relic27 at low health). Result = add x mul.
  - Weapon mods multiply separately per projectile (`0x140d61a70`, `0x140d25c30`).
  - Combo only matters through perk 13 (Damage Combo, 15%/combo level).
  - Stacking perks (Assassin, Checklist, Gemstone 4%/island, Silver Lining) permanently add to DamageMultiplier.
- **Crit** (`CalcOutgoingDamage 0x140d21480`; chance `0x140d22f20`) **[C]**:
  - Chance% = 1 + perk16 + perk17 - perk18 (+relic terms), x0.15 for damage type 6. **Base crit = 1%**.
  - Crit damage = (2 + perk19) x (1 + perk20 + relic29) [x(1+0.01*chance) with relic 0x3C]; x3 with relic 0x4C.
- **Pickups**: picking up a duplicate adds levels (Level = min(255, L+new)). Buff = BaseBuff% x Level, or hyperbolic 1-(1-b)^Level when `bHyperbolicBuff`.
- Typical BaseBuff values **[C, pickup_catalog]**:
  - Common damage weapon mods: +50..150% per level, multiplicative flag. Examples: Damage Shot (Epic) 150, Heavy Hitter 150, Uppercut 250 every 5th shot.
  - Damage perks: Danger Close/Eagle Eye/Equalizer 30%; Damage Combo 15%/combo; Collector 3%/perk level; Gemstone 4%/island.

## 5. Needed vs available damage (estimate) [I]
"Needed" = h(I)/h(1), the factor by which DPS must grow to keep the same time-to-kill per enemy (1p, Normal). "Available" assumes one pick per cleared non-shop island.
- **Pessimistic**: only half the picks are damage, +0.75 additive each.
- **Optimistic**: every pick is damage at +1.0 additive, plus multiplicative perks at about x1.2 per biome.

| I | needed | pessimistic avail | optimistic avail | verdict |
|---|---|---|---|---|
| 5 | 2.0x | 2.5x | 5x | ahead |
| 7 (Elite) | 3.5x | 3.3x | 6.5x | even |
| 14 (Elite) | 11.4x | 5.5x | 16x | behind unless focused |
| 21 (Elite) | 20.8x | 8x | 30x | needs multiplicative stacking |
| 28 (Boss) | 33.8x | 10x | 50x | build-dependent |
| 35 (loop 1) | 87x | 12x | 80x | requires synergies (proc mods, crit, elemental) |
| 57 (loop 2) | 800x | - | - | effectively a wall for linear builds |

Takeaway: enemy HP is a step function, flat for 6 islands then x1.5-2.2 at each biome boundary, compounded in loops. Player power grows about linearly per island. Biome 1 is generous. Biomes 2-3 require choosing Damage/Crit/Elemental pools consistently, and loops require multiplicative and proc stacking. Enemy damage grows much more slowly (0.9 -> 7.7 at I28) and is mostly countered by max-HP items (base 250).

## Open items
- Crystal drop amounts per kill or island.
- Native default NumPickupsToSpawn for pool chests.
- Exact Parkour/special portal roll at the 15% branch.
- How a Horde island ends (timer).
- ECrabRank effect, if any, beyond unlocks.
