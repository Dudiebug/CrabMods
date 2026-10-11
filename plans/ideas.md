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
