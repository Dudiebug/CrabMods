# Designing a new item

Six steps from idea to shipped item. The numbers come from `vanilla-power.md`, `vanilla-difficulty.md` and `audit.md` in this folder, and the vanilla items are in `re/pickup_catalog.json`.

## 1. Is it different?

Write the one-line description, then find the **two closest vanilla items** in `re/pickup_catalog.json`. The new item has to differ in at least one way:

- **What triggers it** (on kill → on reload)
- **What it changes** (damage → projectile path, loot rules)
- **How it makes you play** (stay airborne, chain kills, keep slots empty)

The same thing with bigger or smaller numbers doesn't count, and neither does turning a vanilla chance into a sure thing (a guaranteed Layered Explosion is still Layered Explosion). Rework it or drop it.

Also check the **name** isn't taken by a vanilla item (relics too) or by a planned item in `plans/`.

## 2. Pick the rarity, then the number

Rarity follows the **kind** of effect, not its size:

| Rarity | Kind of effect | How much stronger mid-run |
|---|---|---|
| Common | flat damage or a conditional bonus | ×1.1–1.2 |
| Epic | about 2× a Common, a small proc chance, or a slow ramp | ×1.25–1.35 |
| Legendary | a new damage source, more projectiles, or a rule change | ×1.3–2 |
| Greed | a true multiplier **with a real drawback** | ×1.25–1.75 |
| Relic | big, because relics never level and you only get 3–6 | like a level-3 perk |

To pick the number:

1. **Copy the closest vanilla item's number** as a starting point (e.g. Tasty Orange: 1% per kill, island only).
2. Check it mid-run: a typical build has weapon-mod damage ≈ 4.5 and perk damage ≈ 1.6. So +30% perk damage is (1.6 + 0.3) / 1.6 = ×1.19, which is Common-sized.
3. Check it at level 5, on Proc Generator (every chance ×100), and at island 28.

**Conditional items:** multiply by how often the condition is on. Rough uptimes: always-easy conditions (moving, high health) ~80%, positional ones (close range, near teammates) ~50–70%, one specific target ~15% in a horde but most of a boss fight. Write the guess on the card.

**Non-damage items:** convert to something comparable. Crystals → how many Commons/Epics that buys at the current shop price (`audit.md` §1). Healing → compare to Grave Dodger, Endurance, Amber Resin. Extra choices or loot → compare to Big Chests and Limited Loot.

## 3. Compare it to vanilla

This is the balance check. Take **4–5 vanilla items of the same category and rarity** from `re/pickup_catalog.json`, the ones closest in what they do. Work out each one's mid-run value with the same sum as step 2, and line them up from weakest to strongest. Then put the new item in the line.

Example: for a new Common perk, line it up against Danger Close (+30% close up), Eagle Eye (+30% far away), Equalizer (+30% with an even perk count), Damage Combo (+15% per combo level) and Power Armor (+15% per armor plate).

- **It should land in the middle.** The strongest spot means nerf it. Above all of them means it belongs a rarity up. The weakest spot or below means buff it or move it down a rarity.
- **Check the rarities next to it.** It should beat a typical item one rarity down and lose to a typical item one rarity up.
- **The side-by-side test.** Imagine it in a chest next to those vanilla items. If you would always take it, it's too strong. If you would never take it, it's too weak. "It depends on my build" is the goal.

## 4. Try to break it

Each question is a real past mistake. Any "yes" needs a fix: a cap, island-only, a lower rate, or a different rarity.

- Does it count something there's **lots of** (destructibles, empty slots, kills)? *Collateral Damage, Minimalist*
- Does it **grow forever** with no cap? *Overflow*
- Does it **feed itself** (kills → resource → more kills)? Does it scale with **enemy difficulty** for free? *Raise Dead*
- Does it break the **economy** or let you skip progression? *Cursed Lottery, key totem menu*
- Is it **dead or weak** for its rarity? *Charms at Legendary, Jack of All Trades*
- Does it combo badly with Encore, Vampire, Overflow, Proc Generator, Money Is Power or the necro kit?
- Does it work the same **solo and with 4 players**, and with CrabSync on? *Last Crab Standing*

## 5. Is it fun?

At least two of these:

- You can **see** it work (popup, effect, counter).
- You can **play toward** it (a condition, not just "+X% always").
- It changes **what you pick next**.
- Its drawback changes **how you play**, not just a smaller number.

## 6. Card, build, review

- Write the card into `plans/ideas.md` (template below).
- Build it with every number as a `CrabMods.ini` key, so tuning needs no rebuild.
- After people have played it, add a line to `audit.md`: in line, too strong, or too weak. If it was a new kind of mistake, add it to step 4.

---

## Card template

~~~markdown
## <Name> (<category>, <rarity>)

**<One-line description>**

- <Description with the real numbers>
- Spawn weight: <w>    Scaling: <Linear | Hyperbolic | HalfScaling | none>
- Appears in: <chests / totems>

**Different from:** <vanilla A>, <vanilla B>, because <...>
**Number:** based on <vanilla item and its number>; mid-run ×<..>; level 5 / Proc Generator / island 28: <...>
**Vs vanilla:** <the 4–5 items, weakest to strongest, with the new item placed in the list; one rarity up and down>
**Break it:** <any "yes" from step 4 and its fix, or "none">
**Fun:** <which of step 5>
**How:** <hooks, addresses, what's still to find>
**Unverified:** <anything guessed, to check in game or in the exe before building>
~~~
