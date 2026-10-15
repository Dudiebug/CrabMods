# Designing a new item

Seven steps from idea to shipped item. The numbers come from `vanilla-power.md`, `vanilla-difficulty.md` and `audit.md` in this folder, and the vanilla items are in `re/pickup_catalog.json`.

**Every number on a card carries a tag:**
- **[C]**: from the game's code or assets. Cite the doc and section.
- **[M]**: measured: a balance log from real runs (step 7), or something seen again and again in play.
- **[G]**: a guess.

A [G] can't decide a verdict on its own. List every [G] under **Unverified**, and measure it before or soon after the item ships.

## 1. Is it different?

Write the one-line description, then find the **two closest vanilla items** in `re/pickup_catalog.json`. The new item has to differ in at least one way:

- **What triggers it** (on kill → on reload)
- **What it changes** (damage → projectile path, loot rules)
- **How it makes you play** (stay airborne, chain kills, keep slots empty)

The same thing with bigger or smaller numbers doesn't count, and neither does turning a vanilla chance into a sure thing (a guaranteed Layered Explosion is still Layered Explosion). Rework it or drop it.

Also check the **name** isn't taken by a vanilla item (relics too) or by a planned item in `plans/`: run `python tools/check_names.py "Name"` (the list is `plans/balance/vanilla-names.md`). Do it while brainstorming, before a name goes on a card, and treat a near miss (Adrenaline vs Adrenaline Amulet) as a rename too.

## 2. Place it

Write these on the card before picking any number. All of them are [C].

- **Category and rarity.** Rarity follows the **kind** of effect, not its size:

  | Rarity | Kind of effect |
  |---|---|
  | Common | flat damage or a conditional bonus |
  | Epic | about 2× a Common, a small proc chance, or a slow ramp |
  | Legendary | a new damage source, more projectiles, or a rule change |
  | Greed | a true multiplier **with a real drawback** |
  | Relic | big, because relics never level |

- **Where its effect goes** (`vanilla-power.md` §2). This decides how much it gets diluted as the build fills:
  - the weapon-mod pool (W, additive);
  - the perk add pool (additive);
  - the Greed multiply pool;
  - crit;
  - elemental stacks;
  - or outside every pool, a true multiplier that never dilutes, like Death Mark.
- **How it levels**:
  - Linear: B × L.
  - Hyperbolic: 1 − (1 − B)^L.
  - HalfScaling, for procs: B × (1 + 0.5(L − 1)), then × the weapon's roll multiplier, 1–7.
  - Limited: no longer offered once it reaches 100%.
  - None, for relics.
- **How often it shows up.** Its spawn weight against the total for its category and rarity (`vanilla-power.md` §1). Most weapon mods and perks are 1, ability and melee mods 0.75, relics 0.3–0.5. Base rarity odds per choice are Epic 3.75%, Legendary 0.25%, more in shops (§4).
- **Rules it runs into** (§2):
  - opposite elemental rings cancel each other's bonus (Fire zeroes Ice, Ice zeroes Fire, Lightning and Poison zero each other);
  - damage types 7–10 can't crit;
  - in multiplayer each loot roll is built from one random player's items.

## 3. Pick the number and rank it against vanilla

1. **Start from the closest vanilla item's number** (e.g. Tasty Orange: 1% per kill, island only).
2. **Take 4–5 vanilla items of the same category and rarity** that do something similar. Work out each one's value in the **reference builds** below with the formulas in `vanilla-power.md` §2, and line them up from weakest to strongest. Put the new item in the line.
   - **It should land in the middle.** Strongest means nerf it. Above all of them means it belongs a rarity up. Weakest or below means buff it or move it down a rarity.
   - It should beat a typical item one rarity down and lose to a typical item one rarity up.
3. Check it at level 5 and on Proc Generator (every chance ×100) [C].

**Reference builds** are the totals of each damage pool at islands 7, 14 and 28, measured from balance logs [M] (step 7). Until there are logs, use the estimates in `vanilla-power.md` §3 and tag them [G]: mid-run weapon-mod damage ≈ 4.5 and perk damage ≈ 1.6; late, weapon-mod damage ≈ 10–20. Example: +30% perk damage mid-run is (1.6 + 0.3) / 1.6 = ×1.19, which is Common-sized.

**Conditional items:** multiply by how often the condition is on. Use a logged uptime [M] when there is one. Otherwise guess, tag it [G], and work out the item's value at both half and double your guess. If its rank changes between those, the condition decides whether it's balanced, so measure it before shipping.

**Non-damage items:** convert to something comparable.
- Crystals → how many Commons or Epics that buys at the shop price for that island ([C], `vanilla-difficulty.md` §3). Crystal income isn't traced yet, so crystals per island are [M] or [G].
- Healing → compare to Grave Dodger, Endurance and Amber Resin, against enemy damage at that island [C].
- Extra choices or loot → compare to Big Chests and Limited Loot.

## 4. Does it hold up over the whole run?

### Slots fill up
By the end of a first run a player has usually filled every slot: 10 relics, 24 perks and 24 weapon mods [M]. Once a category is full, a new item can't be taken. Only levels on items already held count (pickup check: `count < NumSlots`, [C]). So:

- **Relics** are judged against the relics a player ends up with. The question is not "is this better than an empty slot" but "would this make someone's best 10".
- **Perks and weapon mods** must be worth a slot when they're offered, mostly in biomes 1–3. From then on, an item's value comes from the levels it gains, so how it levels (step 2) is its late-run value.
- **An item that needs a full build to be good** has no late window to shine in, because by then it can't be taken. An item that counts **empty slots** dies as the run goes on.

### The game gets harder
Work out its value at **islands 3, 14, 28, 35 and 57**: picked up then, and picked up early and levelled. Put it next to the typical vanilla item of its rarity at the same point. The bar is [C] (`vanilla-difficulty.md` §2 and §5):

- **Enemy health** needs about 3.5× the starting damage by island 7, 11× by 14, 21× by 21, 34× by the island 28 boss, 87× by island 35 and 800× by island 57. Each biome boundary is a ×1.5–2.2 step, and loops multiply it again.
- **Enemy damage** grows slowly: 0.9 → 7.7 by island 28. Survival items are judged against this, not against your damage.
- **Prices** rise about 3.7% per island: a Common is 482 crystals on island 1 and 774 on island 14.
- **Level offers** grow with the island: the chance per choice is min(island, 50)% × 0.66 for Epic chests (× 0.33 for Legendary), less for each one already offered in that chest. On island 20 that is ~13% for the first choice, and it reaches its cap at island 50.

What usually happens:
- **Flat additive bonuses fade.** A Common +75% weapon mod is ×1.75 on an empty build and ×1.17 next to five Commons. That's fine for a Common, and a problem for an Epic or Legendary.
- **Multipliers, procs and crit keep up.** Check they don't make loops trivial.
- **What it counts changes.** Kills, enemies and crystals per island go up, and slots fill.
- **Fixed amounts go stale.** A flat heal, shield or crystal amount is big on island 2 and nothing on island 28. Use a percentage of max health or of the current price, or grow it per biome.

Two failure modes, both a "no" here:
- **It falls off**: strong in biome 1, dead weight by biome 3. "Great early, weak late" is OK for a Common only if that's the design and the card says so.
- **It runs away**: it grows faster than the ×1.6–3.3 per biome the game asks for, so loops stop being hard. Fix it with a cap, island-only scope, or diminishing returns.

## 5. Try to break it

Each question is a real past mistake from `audit.md`. Any "yes" needs a fix: a cap, island-only, a lower rate, or a different rarity.

- Does it count something there's **lots of** (destructibles, empty slots, kills)? *Collateral Damage, Minimalist*
- Does it **grow forever** with no cap? *Overflow*
- Does it **feed itself** (kills → resource → more kills)? Does it scale with **enemy difficulty** for free? *Raise Dead*
- Does it break the **economy** or let you skip progression? *Cursed Lottery, key totem menu*
- Is it **dead or weak** for its rarity? *Charms at Legendary, Jack of All Trades*
- Does it combo badly with Encore, Vampire, Overflow, Proc Generator, Money Is Power or the necro kit?
- Does it work with **1 and 4 players**, and with CrabSync on? With 4 players an island has about 5.2× the enemy health against 4× the damage [C, `vanilla-difficulty.md` §2]. Items that count players, teammates or "nearby" need checking at both ends. *Last Crab Standing*

## 6. Is it fun?

This one is a judgment call, not a number. At least two of these:

- You can **see** it work (popup, effect, counter).
- You can **play toward** it (a condition, not just "+X% always").
- It changes **what you pick next**.
- Its drawback changes **how you play**, not just a smaller number.

## 7. Card, build, measure, review

- Write the card into `plans/ideas.md` (template below).
- Build it with every number as a `CrabMods.ini` key, so tuning needs no rebuild.
- **Measure.** With `balanceLog=true` (not built yet, see `plans/TODO.md`), CrabMods writes one line per island to `CrabMods.log`:
  - each damage pool total (weapon-mod, perk add, Greed multiply);
  - crit chance;
  - kills;
  - crystals earned and spent;
  - items and levels held, and slots used per category;
  - for each CrabMods item, its share of the damage and how long its condition was on.

  These logs replace the [G]s: the reference builds, uptimes and crystal income.
- **Review.** After a few logged runs, add a line to `audit.md`: in line, too strong or too weak, with the logged numbers. If it was a new kind of mistake, add it to step 5.

---

## Card template

~~~markdown
## <Name> (<category>, <rarity>)

**<One-line description>**

- <Description with the real numbers>
- Spawn weight: <w> (<share of its category and rarity>)    Levels: <Linear | Hyperbolic | HalfScaling | Limited | none>
- Pool: <weapon-mod | perk add | Greed multiply | crit | elemental | outside>
- Appears in: <chests / totems>

**Different from:** <vanilla A>, <vanilla B>, because <...>
**Number:** based on <vanilla item and its number>; islands 7 / 14 / 28 ×<..>; level 5 / Proc Generator: <...>
**Vs vanilla:** <the 4–5 items, weakest to strongest, with the new item placed in the list; one rarity up and down>
**Run:** <worth a slot when offered? value at islands 3 / 14 / 28 / 35 / 57; falls off, keeps up or runs away, and the fix>
**Break it:** <any "yes" from step 5 and its fix, or "none">
**Fun:** <which of step 6>
**How:** <hooks, addresses, what's still to find>
**Unverified:** <every [G], and how it gets measured>
~~~
