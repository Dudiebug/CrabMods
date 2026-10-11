# CrabMods

**New items for Crab Champions.** 57 items and a new weapon (the Soul Whip, with 20 items of its own): perks, relics, weapon mods, ability mods and a melee mod, plus stronger melee mods and a key totem menu that lets you choose your unlock. Each one drops from chests and rewards like the game's own.

> **Status: early (0.4.0), not yet tested in-game.** Please report what you see, with `CrabMods.log` attached.

## Items

### Damage

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Arsenal** | weapon mod | Epic | +3% damage for each other weapon mod you have. |
| **Hoarder** | perk | Legendary | +3% damage for each weapon mod **and** each perk level you have. Arsenal and Collector in one. |
| **Minimalist** | perk | Epic | +5% damage for each **empty** weapon mod slot. |
| **Underdog** | perk | Epic | +0.5% damage for every 1% of health you are missing (+25% at half health). |
| **Untouchable** | perk | Epic | +1% damage for each second without taking damage, up to +20%. Getting hit resets it. |
| **Jack of All Trades** | perk | Legendary | +3% damage for each **different** pickup tag you have (Fire, Ice, Critical, Bounce, Speed, Healing, ...). |
| **Greed Is Power** | Greed perk | Greed | +5% damage for each Greed perk level you have, this one included. Collector for Greed perks. |
| **Combo Catalyst** | perk | Epic | Your hits apply **+1 stack of each element** they already apply for every combo level above 1 (+4 at combo 5). |
| **Encore** | relic | Epic | Perks that go off by themselves have a **15% chance to go off twice**: Exploding / Freezing / Flammable / Electric / Poisonous Enemies, Explosive / Poisonous / Flammable Armor, and Grave Dodger (it can save you again that island). A failed Rigged Totems or Autoloader roll gets a second try, and Finishing Move's bonus can count twice. |
| **Autoloader** | perk | Epic | Each kill has a **10% chance to reload your weapon instantly** (more levels: 16.7%, 23.1%, ...). |
| **Purist** | relic | Epic | +10% damage while every elemental pickup you hold is the same element. |
| **Collateral Damage** | perk | Epic | +5% damage for every destructible you break (pots, barrels, rocks, crystals, nests, ...) during an island's fight. It resets each island, like Tasty Orange. |
| **Overflow** | perk | Epic | Every 100 health healed while you are already at full health gives a permanent +3% damage for the rest of the run. |
| **Martyr** | relic | Epic | While you're down, your **revive bubble hurts the enemies in it**: a third of a basic crab's max health a second (it grows every island, like the enemies). **Magnify** makes the bubble bigger, for the damage and for reviving you. |
| **Bullet Rain** | weapon mod | Epic | Your shots fall from the sky onto the spot you aim at (the way Ice Staff fires), and deal +10% damage. |
| **Hellshooter** | weapon mod | Epic | Your shots fire straight up out of the ground under the spot you aim at, and deal +10% damage. |
| **Last Crab Standing** | relic | Epic | +25% damage for every downed teammate, and another +25% when you are the only one still up. Never drops when you play alone. |

### Health and fire rate

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Thick Shell** | perk | Epic | +3% max health multiplier for each perk level you have (this one included). Collector for health. |
| **Community Support** | perk | Epic | You and every player within 20 m fire faster: one Rapid Fire level (+12%) for each player in range who has Community Support, you included. It stacks when your friends have it too. |

### Loot

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Fire / Ice / Lightning / Poison / Critical Specialist** | relics | Epic | Pickups with that tag are offered **3×** as often to you, and can also come out of your chests of every other loot pool. |
| **Damage / Luck Specialist** | relics | Epic | Damage (or Luck) pickups are offered 3× as often to you and can also come out of your other chests, and Damage (or Luck) islands are offered 3× as often to the team. |
| **Double or Nothing** | Greed relic | Greed | At the end of each island you get **twice as many** reward chests, or **none at all** (50/50). It counts every chest the island gives you (bonus chests, Care Package, totems) and rolls before High Roller. |
| **Cursed Lottery** | Greed relic | Greed | **100,000 crystals** when you pick it up, but while you hold it you only get **0.01%** of the crystals you'd gain. It **can't be dropped**. Rare (spawn weight 0.15). |
| **Lucky Charm** | relic | Legendary | Each mod and perk in the chests you open has a **25% chance to be level 2**. |
| **Finishing Move** | perk | Epic | Enemies you kill with a **critical hit** drop **+50% crystals**. |
| **Rigged Totems** | perk | Epic | Health, Chance and Gamble totems are **25% more likely to go your way** (a 50% totem becomes 62.5%). |
| **Heavy Shot** | the game's own weapon mod | Common | Drops **without Bouncing Shot**, for every weapon. |

### Ability

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Recharge** | ability mod | Epic | Each kill takes 0.1 s off your ability cooldown. |

### Weapon

| Item | Type | Effect |
|---|---|---|
| **Soul Whip** (Necromancer) | weapon | In the lobby's weapon list, unlocked, and on its own pedestal left of the Crossbow. A lash with no ammo: each crack sends a purple **soul bolt** up to 45 m, which bursts where it hits an enemy, the floor, or at the end of its reach, hurting everything within 4 m (0.7x the Sniper's damage on a direct hit). The burst also **tags** enemies within 6 m for 8 s. While you hold it, your ability is replaced by **raising the dead**: kills fill a **soul bar** (100 max; 1 / 3 / 6 / 25 / 60 souls by the enemy's rank, +50% for tagged kills), and four keys (**V, C, X, Z**) spend souls to raise the last fodder, medium, hard or elite enemy you killed this run as an **undead** where you aim (fodder comes as a pack of 3; costs 5 / 15 / 40 / 100). Undead fight the island's enemies, go after tagged ones first and hit them 25% harder, and never hurt players. Each costs 0.25% of its raise cost a second while enemies are alive; when the souls run out, they starve, losing health until you have souls again. They all crumble when the fight ends. It has its own model (a cord lash that hangs and sways as you move, and cracks out toward your aim when you fire, sending a purple soul bolt the rest of the way), a soul glow that pulses down the cord and flares on each crack, its own sounds, and a HUD of its own: the soul bar above your health and the four raise pictures, with their keys and costs, where the ability was (left of the melee slot). |

### Soul Whip loot

These only drop for players **holding the Soul Whip**, and for them from **any chest**. Numbers are per level.

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Long Lash** | weapon mod | Common | Soul bolts fly **15%** farther. |
| **Soul Siphon** | weapon mod | Common | Each enemy your soul bolt (or its burst) hits gives **1 soul**. |
| **Branding Iron** | weapon mod | Common | The bolt's burst and its tags reach **25%** farther. |
| **Death Mark** | weapon mod | Epic | Tagged enemies take **10%** more damage from you. |
| **Soul Brand** | weapon mod | Legendary | Killing a tagged enemy tags the **2** enemies nearest it. |
| **Sturdy Bones** | ability mod | Common | Undead you raise have **15%** more health. |
| **Soul Discount** | ability mod | Common | Raising costs about **8%** fewer souls (levels add up hyperbolically; never below half). |
| **Mass Grave** | ability mod | Epic | Fodder raises bring **1** more. |
| **Twin Rising** | ability mod | Legendary | **15%** chance a raise brings twice as many for the same souls (each level rolls again). |
| **Grave Claws** | melee mod | Common | Melee kills give **100%** more souls. |
| **Harvester** | melee mod | Legendary | A melee kill on a fodder or medium enemy has a **25%** chance to rise as your undead, for free. |
| **Soul Jar** | perk | Common | **+10** max souls. |
| **Hungry Ghosts** | perk | Common | Kills give **10%** more souls. |
| **Last Rites** | perk | Common | When one of your undead falls in a fight, **15%** of its raise cost comes back. |
| **Army of the Dead** | perk | Epic | **+4%** damage for each undead you have. |
| **Plague Bearers** | perk | Epic | Your undead's attacks apply **3 poison**. |
| **Grim Harvest** | perk | Legendary | Tagged kills give **50%** more souls, on top of the tag's own +50%. |
| **Ring of Souls** | relic | Epic | **25%** more souls from everything. |
| **Ring of the Grave** | relic | Epic | Your undead have **25%** more health and damage. |
| **Phylactery** | relic | Legendary | Once per island, a hit that would down you is cancelled and you're left at half health or more. It spends all your souls, and needs at least **50**. |

### Inventory slots

One Legendary per inventory group. Each level gives that group **5 more slots**, so stacking one makes room for more. The inventory menu scrolls once the slots no longer fit.

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Bandolier** | weapon mod | Legendary | +5 weapon mod slots per level. |
| **Tool Belt** | ability mod | Legendary | +5 ability mod slots per level. |
| **Scabbard** | melee mod | Legendary | +5 melee mod slots per level. |
| **Backpack** | perk | Legendary | +5 perk slots per level. |
| **Reliquary** | relic | Legendary | +5 relic slots (the game allows 10 relics). Relics don't stack, so it's +5 once. |

### Game changes

| Change | Effect |
|---|---|
| **Melee mods** | The game's melee mods get stronger numbers (the game's own in brackets): Sharp Claws +200% melee damage [100], Time Claws +300% [150], Iron Claws +500% [300] with only +20% cooldown [40], Big Claws +40% range [20], Blender −30% cooldown [20], Vampire heals 15 per kill [5], and Ice, Fire, Lightning, Poison and Arcane Claws apply twice the stacks. |
| **Key totem** | The lobby's key totem opens a **menu of every item** (CrabMods' own included), shown as the game's own pickup cards in a scrolling list, with **category tabs** (locked items have a tab of their own; the gamepad bumpers switch tabs) and a **search box** (name, type or description). The item you click (or select with a gamepad) comes out of the totem, and is unlocked if it was locked. It works with any host. With `menu = false` a row of the items comes out of the totem instead and you pick one up (this needs CrabMods on the host too). |
| **Spiked Chest** | The Spiked island's Epic chest costs **35%** of your base max health instead of the game's 25%, and the island's portal says so. With [CrabSync](../crabsync/README.md)'s shared health pool, 35% is what the **whole team** loses once every player has opened theirs (about 10% each with four players), and the portal says "All the team's spiked chests together cost 35% of base max health". |


- Extra copies stack: each level of an item adds its bonus again (two Minimalists give +10% per empty slot).
- Every damage bonus is added like the game's own damage perks, so they all stack with each other and with the game's items.
- The percentages are kept low on purpose, because levels stack.
- Specialists multiply the weight the game already gives each pickup, so a rare pickup stays rarer than a common one. Several Specialists that boost the same pickup multiply.

## Install

**Every player in the lobby needs CrabMods**, the host and every friend who joins.

The easy way is the [CrabMods Manager](../manager/README.md): it installs CrabMods with every item, keeps it up to date, and lets you uninstall single items and change their settings.

By hand:

1. Download `CrabMods-x.y.z.zip` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=CrabMods).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Copy `xinput1_3.dll` and `CrabMods.dll` there. If you already use [CrabSync](../crabsync/README.md), keep its `xinput1_3.dll` (it is the same mod loader).
5. **Had Arsenal before?** Delete `Arsenal.dll`. CrabMods includes Arsenal, and your `Arsenal.ini` settings are copied into `CrabMods.ini` on first launch.
6. **Linux / Steam Deck (Proton) only:** set the game's Launch Options to:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```

`CrabMods.ini` (settings) and `CrabMods.log` appear in the same folder on first launch. When an update adds items, their sections are added to the end of your `CrabMods.ini`; your own values are kept.

To uninstall, delete `CrabMods.dll`, `CrabMods.ini` and `CrabMods.log` (and `xinput1_3.dll` if no other mod uses it).

## Settings

`CrabMods.ini` has one `[section]` per item. Every item has `enabled` and `rarity` (`common`, `epic`, `legendary` or `greed`: which chests it drops from), plus:

| Section | Setting | Default | |
|---|---|---|---|
| `[arsenal]` | `damagePerModPercent` | `3` | Per other weapon mod. |
| | `countStacks` | `false` | `true` counts a x3 stack as three mods (for Arsenal and Hoarder). |
| `[hoarder]` | `damagePercent` | `3` | Per weapon mod and per perk level. |
| `[heavyShot]` | `enabled` | `true` | Let Heavy Shot drop for every weapon (called `anyWeapon` before 0.9.0, still read). |
| `[minimalist]` | `damagePercent` | `5` | Per empty weapon mod slot. |
| `[underdog]` | `damagePercent` | `0.5` | Per 1% of health missing. |
| `[untouchable]` | `damagePercent` | `1` | Per second without being hit. |
| | `maxPercent` | `20` | The most it builds up to. |
| `[jackOfAllTrades]` | `damagePercent` | `3` | Per different pickup tag. |
| `[greedIsPower]` | `damagePercent` | `5` | Per Greed perk level. |
| `[purist]` | `damagePercent` | `10` | |
| `[collateralDamage]` | `damagePercent` | `5` | Per destructible broken this island. |
| `[overflow]` | `damagePercent` | `3` | Per stack. |
| | `healthPerStack` | `100` | Healing past max health one stack takes. |
| `[martyr]` | `crabHealthPerSecond` | `0.33` | Damage a second to each enemy in the bubble, per level, as a share of a basic crab's max health (15 times this island's enemy health multiplier). It falls off to 20% at the edge. |
| | `magnifyBubble` | `true` | The holder's revive bubble grows with their Magnify (+20% a level). |
| `[lastCrabStanding]` | `perDownedPercent` | `25` | Per downed teammate. |
| | `lastStandingPercent` | `25` | Extra when you are the only one up. |
| `[doubleOrNothing]` | `doubleChancePercent` | `50` | The chance of twice as many island reward chests; otherwise none. |
| `[cursedLottery]` | `crystals` | `100000` | Crystals when you pick it up. |
| | `gainPercent` | `0.01` | The share of crystal gains you still get while holding it. |
| `[luckyCharm]` | `chancePercent` | `25` | The chance for each mod or perk in your chests to be level 2. |
| `[comboCatalyst]` | `stacksPerComboLevel` | `1` | Extra stacks of each element per combo level above 1 (rounded per hit). |
| `[finishingMove]` | `crystalPercent` | `50` | More crystals from critical kills. |
| `[autoloader]` | `chancePercent` | `10` | Per kill; levels add up hyperbolically. |
| `[riggedTotems]` | `chanceBonusPercent` | `25` | How much more likely totems go your way (capped at certain). |
| `[encore]` | `chancePercent` | `15` | The chance for a perk to go off a second time. |
| `[recharge]` | `secondsPerKill` | `0.1` | |
| `[spikedChest]` | `enabled` | `true` | Off: the Spiked Chest costs the game's own 25%. |
| | `costPercent` | `35` | What the Spiked Chest costs, in percent of base max health (0 to 99; the game's own is 25). The portal text follows it. |
| `[thickShell]` | `healthPercent` | `3` | Max health multiplier per perk level. |
| `[communitySupport]` | `fireRatePercent` | `12` | Per Community Support in range (Rapid Fire's is 12). |
| | `rangeMeters` | `20` | |
| `[bulletRain]`, `[hellshooter]` | `damagePercent` | `10` | |
| `[meleeMods]` | `sharpClawsPercent` | `200` | Game: 100. |
| | `timeClawsPercent` | `300` | Game: 150. |
| | `ironClawsPercent` | `500` | Game: 300. |
| | `ironClawsCooldownPercent` | `20` | Game: 40. |
| | `bigClawsPercent` | `40` | Game: 20. |
| | `blenderPercent` | `30` | Game: 20. |
| | `vampireHealth` | `15` | Game: 5. |
| | `elementalClawsMultiplier` | `2` | Stacks per hit of Ice, Fire, Lightning, Poison and Arcane Claws. |
| `[keyTotem]` | `menu` | `true` | `false`: a row of random locked pickups comes out of the totem instead of the menu. |
| | `choices` | `3` | Without the menu: how many items are in the row (1 to 8). |
| `[necromancer]` | `whipDamage` | `0.7` | Soul Whip damage on a direct hit, as a multiple of the Sniper's. |
| | `whipFireRate`, `whipRangeMeters` | `1.25`, `45` | Cracks a second; how far the bolt flies. |
| | `burstRadiusMeters`, `burstDamage` | `4`, `20` | The bolt's purple burst: its reach and its damage. |
| | `tagRadiusMeters`, `tagSeconds` | `6`, `8` | Who a crack tags, and for how long. |
| | `tagDamagePercent`, `taggedSoulsPercent` | `25`, `50` | More undead damage to tagged enemies; more souls for tagged kills. |
| | `maxSouls` | `100` | |
| | `soulsEasy` ... `soulsBoss` | `1`, `3`, `6`, `25`, `60` | Souls a kill gives, by the enemy's rank. |
| | `costFodder`, `costMedium`, `costHard`, `costElite` | `5`, `15`, `40`, `100` | Souls a raise costs. |
| | `fodderPack` | `3` | Fodder raised at once. |
| | `raiseRangeMeters` | `40` | |
| | `upkeepPercent` | `0.25` | Of an undead's raise cost, each second while enemies are alive. |
| | `starvePercent` | `4` | Out of souls, each undead loses this percent of its health a second. |
| | `undeadHealth`, `undeadDamage` | `2`, `1` | Undead health and damage, as a multiple of the enemy's own (your damage bonuses multiply the damage too). |
| | `eliteHealth`, `eliteDamage` | `0.5`, `0.5` | Raised elites' health and damage, on top of that, as a multiple of a real one's. |
| | `aggroBias` | `1.5` | Enemies turn on an undead when it is this many times closer than the nearest player. |
| | `raiseKeyFodder`, `raiseKeyMedium`, `raiseKeyHard`, `raiseKeyElite` | `V`, `C`, `X`, `Z` | **Your own keys**: the host's settings never change them. A letter or digit, `F1`-`F24`, `Mouse3`-`Mouse5`, `Num0`-`Num9`, `Tab`, `Shift`, `Ctrl`, `Alt`, `Space`, `CapsLock`, `Enter`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, or the arrows (`Up`, ...). |
| | `soundVolume` | `80` | **Your own**: the Soul Whip kit's sounds, in percent (0 is silent). They don't follow the game's volume slider. |
| `[longLash]` ... `[phylactery]` (the Soul Whip loot) | one number each | | `rangePercent`, `souls`, `radiusPercent`, `damagePercent`, `enemies`, `healthPercent`, `discountPercent`, `extraFodder`, `chancePercent`, `soulsPercent`, `refundPercent`, `poisonStacks`, `percent`, `minSouls`: the bold numbers in the table above, per level. Plus `enabled` and `rarity`. |
| `[bandolier]`, `[toolBelt]`, `[scabbard]`, `[backpack]`, `[reliquary]` | `slotsPerLevel` | `5` | Slots per level (1 to 50). |
| `[fireSpecialist]` ... `[luckSpecialist]` | `multiplier` | `3` | How many times as likely. |
| | `otherChests` | `1` | How likely their pickups are in your chests of other loot pools (Speed, Health, ...), compared with that chest's own pickups. `0` keeps them in their own chests. |

All percentages and amounts are per level of the item. `[meleeMods]` and `[keyTotem]` have only `enabled` besides the settings above; switching `meleeMods` off puts the game's values back.

### Multiplayer: the host's settings

**In multiplayer everyone plays with the host's settings.** When you join, the host's CrabMods sends its `CrabMods.ini` settings to you, and your game uses them until you leave. The Soul Whip's raise keys and sound volume are the exception: those stay your own. Your log says `using the host's settings`, followed by every item's values. When you host again, or play alone, your own settings are back. Your `CrabMods.ini` file is never changed.

To play someone's setup yourself, or host it, use their setup code in the [Manager](../manager/README.md#setup-codes)'s **Share** tab.

## How it works

Crab Champions has none of these assets, so CrabMods makes them when a level loads:
- Each new item is a copy of a similar item's data asset (Arsenal of Streak Shot, Hoarder of Collector, Recharge of Heat Sink, the relics of rings, ...). The copy gets a new name, text, rarity and an unused type, and is added to the list that chests roll from.
- The copies have fixed names, so the game sends them between players by name. That is why every player needs the mod: without it, their game cannot find the item.

The effects:
- **Damage bonuses** are added where the game totals a player's damage buffs, beside its own damage perks.
- **Specialists** change each pickup's spawn weight during every loot roll made for the holder, then put the weights back. A chest only offers pickups of its own loot pool (read from each pickup's asset during the roll), so for the holder's rolls of a regular pool chest (Damage to Skill) CrabMods also sets the boosted pickups' pool to the chest's, and puts it back after. Damage and Luck Specialist also re-pick each island's reward with those pools' weights tripled.
- **Double or Nothing**: at the end of an island the host builds each player's list of reward chests, then checks for High Roller. CrabMods hooks that check: for a player with Double or Nothing it doubles or empties the list first (and, like High Roller, opens the portals straight away when no chests are left).
- **Cursed Lottery**: the host gives every pickup through one function; when it gives the Cursed Lottery to someone who didn't have it, they get the crystals. CrabSync hands out copies through the same function, so only the game's own call pays. Every crystal gain goes through one function too, where a holder's gain is cut to the share (fractions are carried over, so lots of small gains still add up). The host refuses to drop it.
- **Lucky Charm**: when a chest is opened, the host rolls its loot for the opener and spawns it straight after. In between, CrabMods turns level-1 mods and perks into level 2 at the chance (weapons, abilities and relics stay level 1).
- **Combo Catalyst**: the host works out every hit a player lands in one function (their damage buffs, crits, and the elemental perks scaling the hit's element stacks). After it, CrabMods adds the extra stacks to each element the hit applies, from the player's combo level.
- **Encore**: on the host, the on-kill and armor perks make their explosions through one function, and CrabMods tells their calls apart from all other explosions by where they come from; a lucky one is made again at the same spot. After Grave Dodger saves a player, a lucky roll marks it unused for the island again.
- **Autoloader**: ammo is counted only by the machine of the player who holds the weapon, so that machine watches the player's kill counter (four times a second) and rolls for each new kill. A success fills the clip of every weapon that isn't full or reloading, like a finished reload, and updates the ammo on the HUD.
- **Finishing Move**: the host remembers whether each player's last hit on an enemy was critical. When the enemy dies, it sets up its crystal drop, and CrabMods raises the amount if the killing blow was critical and the killer has the perk.
- **Rigged Totems**: the Health, Chance and Gamble totems decide with one roll (a percent chance of the good outcome) when a player uses them. For a player with the perk, CrabMods raises that chance before the roll.
- **Recharge** reads each player's kill counter (which the game sends to everyone) whenever the ability cooldown ticks, and takes the kills since the last tick off the cooldown.
- **Spiked Chest**: every spiked chest copies its cost from the chest's class defaults when it spawns, so CrabMods changes that one number. The host spawns the chests and takes the payment, so the host's setting is what you pay. The portal text is one fixed sentence in the game's own strings, which CrabMods rewrites in memory on every player's machine (padded to the same length, because the game copies exactly that many characters).
- **Heavy Shot**'s asset says
 "only offer me to a player who holds Bouncing Shot". CrabMods clears that flag in memory. Heavy Shot's effect was never tied to a weapon, so nothing else needs changing:
  - The game applies Heavy Shot to every projectile: gravity = (the bullet's own gravity + 2) × (1 + its 15% weight), plus its +50% damage.
  - So even bullets that normally fly dead straight (most guns) start to drop.
- **Collateral Damage**: on the host, every destructible reports the hit that breaks it, with the player who dealt it (or the last player who hit it, for explosions). Breaks only count during an island's fight, and not the landmines, homing barrels and mushrooms players' own items spawn. The bonus is kept in the perk's own counter, which the game sends to every player.
- **Overflow**: all healing happens on the host. CrabMods watches the two places where healing is capped at max health and counts what was lost. The bonus is kept in the perk's own counter, which the game carries from island to island.
- **Thick Shell** and **Community Support** add their bonus where the game totals Banana's max health and Rapid Fire's fire rate. The game only recomputes fire rate when your items change, so CrabMods checks who is in range four times a second and recomputes it when that changes.
- **Bullet Rain** turns on the game's own "shots come from above" mode (Ice Staff's) for the holder's weapon. **Hellshooter** moves each shot's start to the ground under where you aim and points it straight up. The machine that fires the shot sends the changed start and direction to everyone, as the game does for every shot. With both, Hellshooter leaves every other shot alone, so they alternate between the sky and the ground.
- **Martyr**: the revive bubble is the game's revival harvest area. Every machine sets a holder's bubble radius (which the host checks for reviving teammates) and its effect's size to the base radius times their Magnify. Four times a second the host fires the game's radial damage (Explosive Armor's) from the bubble's centre, with the downed character as its source, so the enemy-only filter, Magnify and kill credit work as they do for the game's explosions. The damage is the basic crab's base health (15) times the enemy health multiplier the game sets for each wave.
- **Melee mods**: each melee mod's numbers live in its data asset, so CrabMods changes those when a level loads, and the game's descriptions follow.
- **Key totem**: the game picks the unlock on your machine, unlocks it in your save and asks the host to spawn it. CrabMods opens a menu first: the game's own confirmation prompt (YES / NO hidden) with tabs, a search box and the items' own pickup cards as buttons in a scroll box, over a blurred game. Escape can't close it, since the keys are already spent. Your choice is unlocked and sent to the host exactly as the game sends its own pick, so any host spawns it. With `menu = false`, CrabMods sends several picks instead, the host spawns them in a row that only you can take (and removes the rest when you take one), then tells your machine which one you took.
- **Bandolier, Tool Belt, Scabbard, Backpack**: the host raises the player's slot count for that group (the same number the game sets to 24 / 12 / 12 / 24 at the start of a run) by 5 per level, and lowers it again if the item goes, never below what you hold. The count is sent to everyone and kept between islands by the game. The inventory menu only has widgets for the game's own slots, so CrabMods adds more like them (same look, hover and gamepad focus) before the menu lays them out, and puts the slots part of the menu in a scroll box.
- **Reliquary**: relics have no slot count; the game allows 10, both when checking whether you can take a relic and in the inventory menu. On every machine, CrabMods raises both by the Reliquary's slots (worked out from the inventory, which everyone sees).
- **Soul Whip**: a copy of the Sniper's weapon asset (with its own projectile: the Arcane Wand's purple orb, slowed so you can see it, which bursts on a hit or at the whip's reach) given the Lightning Scepter's swing, crosshair and aiming, added to the lobby's weapon list before the game builds it. The whip's damage multiplies the hit where the host totals a player's damage. The machine of the player who cracks it sends where the crack landed to the host, which tags the enemies there. Raise keys are read on your machine (only with the game window in front, no menu open and the chat closed) and sent to the host in the same hidden way, over the game's chat call. The host keeps everyone's souls and the kinds they can raise, spawns the undead through the game's own enemy spawn (outside the island's enemy count, so the island can still end), and tells every player where, so their machines tint them too. Who can hurt whom is one game function, which CrabMods answers for undead (enemies and undead: yes; players and undead: no); who an enemy goes after is another, which CrabMods follows with its own pick. An undead's death drops nothing, and its kills count as its owner's. The ability key does nothing while you hold the whip. Its pedestal is a copy of the Crossbow's stone, placed by every machine one gap left of it each time the lobby loads (and taken away on islands, which load into the lobby's world); the host spawns the pickup on it, which the game sends to everyone. Its looks and sounds come inside `CrabMods.dll`: on start, every machine builds the whip's meshes and the HUD's pictures from them through the engine's own run-time mesh and texture functions. The lash is a chain of mesh pieces drawn on each machine (no physics: a droop, a sway and a trail behind as you move), which cracks whenever the weapon's shot count (sent by the game) goes up: it unrolls out toward where the shot lands, holds, and falls back. The glow is the whip's palette at eight brightnesses, a material each, swapped on the pieces as a wave. The sounds play through a little mixer of CrabMods' own, since the engine can't load a sound at run time.
- **Host settings** go out over the game's own `ClientMessage` call (the one that prints to the console), sent to each player as they join. CrabMods picks it up before it reaches the console.

- **Soul Whip loot**: ordinary new pickups (like the other items) whose loot weight is 0 for anyone not holding the whip, and who can turn up in any of a whip holder's chests. What they do is all inside the whip's own code on the host: souls, raise costs and counts, undead health, damage and poison, tags. Long Lash and Branding Iron set the bolt's reach and burst on the shooter's machine as it fires. Army of the Dead's undead count comes from the host with the souls.

## Known limitations

- The new items use the game's plain icons for their rarity (the same ones other items without their own art use).
- Every player needs CrabMods. A player without it sees an empty slot where a CrabMods item should be.
- Heavy Shot still needs to be unlocked in your save before it can drop.
- On very fast guns (Sniper, Marksman Rifle, Laser Cannons) the bullet reaches its target before it can drop much, so Heavy Shot is mostly its damage there.
- Recharge only counts kills made while your ability is recharging, so kills while it is ready are not saved up.
- With [CrabSync](../crabsync/README.md)'s shared health pool, the whole team goes down together, so **Martyr**'s bubble only appears when the whole team goes down, and **Last Crab Standing** rarely does anything. Turn off `syncHealth` in `CrabSync.ini` if you want them.
- **Key totem**: the menu stays open until you choose (the keys are already spent). With `menu = false`, the unlock happens when you take one of the items, and walking away without taking one unlocks nothing. A new unlock joins the loot on the next level, as in the game.
- **Scabbard** uses Big Claws' icon: the game has no plain melee mod icon.
- **Slot items** are tested only by reading the game's code, not in play: if the inventory menu looks wrong with extra slots, please open an issue with your `CrabMods.log`.
- **Hellshooter** puts every bullet of a spread shot under your aim, with a little scatter; the game's own formation (side by side, ...) is lost.
- **Community Support** counts players by their character's position, so a downed player neither gives nor gets it.
- **Soul Whip** is tested only by reading the game's code, not in play yet: its pedestal, model, lash, HUD and sounds are all new and unchecked in game. If its art can't be built on your machine, it falls back to the Sniper's look (your log says why). Its sounds don't follow the game's volume settings (`soundVolume` sets them). It isn't kept as your weapon between runs. If something looks wrong, please open an issue with your `CrabMods.log`.
- Everyone should run the same CrabMods version: a player on an older one can't see the newer items.
- If the game updates, CrabMods turns itself off (see `CrabMods.log`) until a new release is out.

## Source

See [Building](../README.md#building) in the main README. The code is in `crabmods/`:
- `main.cpp`: the hooks, which pass each game event to every item.
- `item.hpp`: the item base classes; `items/*.cpp`: one file per item.
- `world.hpp`: reading players, inventories and health; `config.hpp`: `CrabMods.ini`.

## License

MIT, see [LICENSE](../LICENSE). Bundles [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause).
