# Necromancer: loot (draft)

This is the necromancy loot pool for the [necromancer kit](necromancer.md). It is a proposal for the user to cut, keep or rename. Nothing here is settled yet.

It rolls in chests **only for players holding the Soul Whip**, alongside the vanilla pool. Every number goes in `CrabMods.ini` and is a starting point to tune against game data, not a final balance.

House rules, the same as the rest of CrabMods:
- Extra copies stack: each level adds the bonus again.
- Percentages that could run away are **hyperbolic** (they approach a cap and never reach it).
- Relics don't stack.
- Rarities are the game's three: Common, Epic and Legendary (there is no Rare). Where an item says "of the base", it means the soul **base** max of 100, so upgrades never rescale costs.

Slot budget: weapon mods 24, ability mods 12, melee mods 12, perks 24, relics 10. The pool below is 51 items: 11 weapon mods, 10 ability mods, 7 melee mods, 14 perks and 9 relics.

## Essentials (build these first)

The 20 items that make the kit work. Everything after this section is the full pool, kept for later.

| Name | Kind | Rarity | Effect (per level) |
|---|---|---|---|
| **Long Lash** | Whip mod | Common | +15% bolt range. |
| **Soul Siphon** | Whip mod | Common | +1 soul for each enemy a bolt's burst hits. |
| **Branding Iron** | Whip mod | Common | +25% burst and tag radius. |
| **Death Mark** | Whip mod | Epic | Tagged enemies also take +10% damage from you. |
| **Soul Brand** | Whip mod | Legendary | A kill on a tagged enemy tags the 2 nearest enemies. |
| **Sturdy Bones** | Raise mod | Common | Undead get +15% max health. |
| **Soul Discount** | Raise mod | Common | Raise costs −8% (hyperbolic, never below half). |
| **Mass Grave** | Raise mod | Epic | Fodder raises bring +1 more. |
| **Twin Rising** | Raise mod | Legendary | 15% chance a raise brings two for one cost. |
| **Grave Claws** | Melee mod | Common | Melee kills give +100% souls. |
| **Harvester** | Melee mod | Legendary | 25% chance a melee kill on an Easy or Medium enemy raises it for free. |
| **Soul Jar** | Perk | Common | +10 max souls. |
| **Hungry Ghosts** | Perk | Common | +10% souls from kills. |
| **Last Rites** | Perk | Common | When one of your undead dies, you get back 15% of its raise cost. |
| **Army of the Dead** | Perk | Epic | +4% damage for each undead you have. |
| **Plague Bearers** | Perk | Epic | Your undead's attacks apply Poison. |
| **Grim Harvest** | Perk | Legendary | Tagged kills give +100% souls instead of +50%. |
| **Ring of Souls** | Relic | Epic | +25% souls from every source. |
| **Ring of the Grave** | Relic | Epic | Your undead have +25% max health and damage. |
| **Phylactery** | Relic | Legendary | Once per island, a death is cancelled: it spends all your souls (at least 50) and you stand back up. |

9 Common, 6 Epic, 5 Legendary. Left out for now: anything that needs an undead limit (the raise has none yet), and the ones that need new effects (wisps, pools, chains, soul debt).

## Weapon mods (Soul Whip)

The whip is the only weapon a necro holds, so its weapon mods are whip mods. They change the lash, the tag and the command over undead.

| Name | Rarity | Effect (per level) |
|---|---|---|
| **Long Lash** | Common | +15% whip reach. |
| **Soul Siphon** | Common | +1 soul for each enemy a crack hits. |
| **Lingering Mark** | Common | Tags last +3 s. |
| **Rallying Crack** | Common | The whip's undead buff is +50% stronger and lasts +1 s. |
| **Branding Iron** | Common | +25% tag radius. |
| **Death Mark** | Epic | Tagged enemies also take +10% damage from **you**, not only from your undead. |
| **Mending Lash** | Common | Cracking near your undead heals them 3% of their max health. |
| **Chain Lash** | Epic | The crack arcs from the tip to +1 enemy nearby (like Lightning Shot), tagging it. |
| **Double Crack** | Epic | Each attack cracks twice. The second crack does 50% damage and has no extra tag. |
| **Reaper's Reach** | Legendary | +5% whip reach and damage for each undead you have. |
| **Soul Brand** | Legendary | A kill on a tagged enemy puts a tag on the 2 nearest enemies (the mark spreads). |

## Ability mods (Raise Dead)

The whip forces Raise Dead, so ability mods are raise mods.

| Name | Rarity | Effect (per level) |
|---|---|---|
| **Sturdy Bones** | Common | Undead you raise get +15% max health. |
| **Thrifty Dead** | Common | Undead upkeep −20% (hyperbolic). |
| **Grave Burst** | Common | The raise lands like an airstrike: damage in its circle, scaled by tier cost. |
| **Haunted Ground** | Epic | The raise circle leaves a soul pool for 5 s that slows enemies and tags everything in it. |
| **Soul Discount** | Common | Raise costs −8% (hyperbolic, never below half). |
| **Corpse Bomb** | Epic | When one of your undead dies, it explodes for damage scaled by its raise cost. |
| **Second Wind** | Epic | The first time each undead would die, it stands back up at 30% health. |
| **Mass Grave** | Epic | Fodder raises bring +1 more (a pack of 4, then 5, and so on). |
| **Elite Pact** | Epic | Raised elites are +25% stronger, up to full power (they start at half). |
| **Twin Rising** | Legendary | 15% chance that a raise brings two of that kind for one cost (a second level gives 27.8%, and so on). |

**Vanilla ability mods:** see "Open" below.

## Melee mods (claw)

The claw becomes the necro's close-up tool for harvesting souls and commanding.

| Name | Rarity | Effect (per level) |
|---|---|---|
| **Grave Claws** | Common | Melee kills give +100% souls. |
| **Rending Claws** | Common | Melee hits tag the enemy. |
| **Commanding Claws** | Common | A melee hit sends all your undead at that enemy. |
| **Lifebinder** | Common | Melee kills heal all your undead 10%. |
| **Bonebreaker** | Epic | Melee hits on tagged enemies do +100% damage and give 2 souls. |
| **Soul Rip** | Epic | Melee kills pull out a soul wisp that seeks the nearest enemy and tags it. |
| **Harvester** | Legendary | A melee kill on an Easy or Medium enemy has a 25% chance to raise it for free (counts toward the undead limit). |

## Perks

| Name | Rarity | Effect (per level) |
|---|---|---|
| **Soul Jar** | Common | +10 max souls. Costs are pinned to the base, so this is pure headroom. |
| **Hungry Ghosts** | Common | +10% souls from kills. |
| **Pack Leader** | Common | Your undead move and attack +10% faster. |
| **Last Rites** | Common | When one of your undead dies, you get back 15% of its raise cost. |
| **Soul Battery** | Common | +1% fire rate for every 10 souls you are holding. |
| **Grave Robber** | Common | Opening a chest gives +10 souls. |
| **Undying Loyalty** | Common | Your undead take −10% damage (hyperbolic). |
| **Plague Bearers** | Epic | Your undead's attacks apply Poison. |
| **Bigger Army** | Epic | +2 undead limit (the base is 12, with Easy 1, Medium 2, Hard 3, Elite 6). |
| **Army of the Dead** | Epic | +4% damage for each undead you have. Collector for armies. |
| **Shared Pain** | Epic | 15% of the damage you take goes to your nearest undead instead. |
| **Soul Ward** | Epic | When you're hit, souls absorb part of it: 1 soul blocks 5 damage, up to 30% of the hit. |
| **Grim Harvest** | Legendary | Tagged kills give +100% souls instead of +50%. |
| **Overlord** | Legendary | Your undead get +20% more of your stat bonuses (hyperbolic toward all of them). |

## Relics

There is one of each and they don't stack. The names follow the game's rings and amulets.

| Name | Rarity | Effect |
|---|---|---|
| **Ring of Souls** | Epic | +25% souls from every source. |
| **Ring of Command** | Epic | +50% tag radius and duration. |
| **Ring of the Grave** | Epic | Your undead have +25% max health and damage. |
| **Lantern Amulet** | Epic | Co-op: a kill by a teammate within 15 m gives you 25% of its souls. Solo, it's +10% souls. |
| **Gravebound Amulet** | Epic | No upkeep while your soul bar is above half. |
| **Bone Crown** | Legendary | +6 undead limit (room for one more elite). |
| **Phylactery** | Legendary | Once per island, a death is cancelled. It spends all your souls (at least 50) and you stand back up. |
| **Deathless Amulet** | Legendary | Up to 3 of your undead (the strongest) follow you to the next island. |
| **Soul Debt** | Legendary, cursed | Raises may take you to −50 souls. While you're below 0 you take +25% damage and kills pay the debt first. |

## Open

- **Vanilla loot for whip holders.**
  - Many vanilla weapon mods are projectile mods (Homing, Split, Bouncing Shot, ...) that mean nothing on a whip. Options:
    - (a) filter them out of a whip holder's chests;
    - (b) map them to the crack (e.g. Split Shot = Chain Lash);
    - (c) let them roll and do nothing.
  - Damage, fire rate and element mods probably work as they are.
- **Vanilla ability mods on Raise Dead.** Explosion mods (Fire, Ice, Lightning Explosion, ...) could go off in the raise circle, which fits naturally if the raise is built on the airstrike. Turret and grenade mods don't fit, so filter them out.
- **Which elements undead carry.** Should your element mods and perks put their elements on your undead's attacks? "Undead use your stats" suggests yes.
- **How many** to build first: the 20 in Essentials above.
- **Art:** icons follow the game's per-type and per-rarity look (`T_Icon_<Kind>_<Rarity>`, as the other CrabMods items do), or get custom necro icons.
