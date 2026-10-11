# Changelog

Each mod has its own version. Releases are tagged `crabsync-vX.Y.Z`, `crabmods-vX.Y.Z` and `manager-vX.Y.Z`.

## CrabMods

### 0.8.1

- **Fixed a crash when opening a chest** (most likely with Lucky Charm in the game). After the game rolled a chest's loot, CrabMods looked up who opened it in a list the game had already freed, and Lucky Charm then read that leftover memory as the player. CrabMods now reads the opener before the roll, and checks it is a player.
- **Lucky Charm should now actually work**: the same bug meant it was reading garbage instead of the opener's relics, so it most likely never saw that you had it and never upgraded chest items to level 2.

### 0.8.0

- New **Epic perk, Combo Catalyst**: your hits apply **+1 stack of each element** they already apply (ice, fire, lightning, poison, arcane) **for every combo level above 1**, per Combo Catalyst level (`[comboCatalyst] stacksPerComboLevel`).
- New **Epic relic, Encore**: perks that go off by themselves have a **15% chance to go off a second time** (`[encore] chancePercent`): the on-kill explosions of Exploding, Freezing, Flammable, Electric and Poisonous Enemies, the Explosive, Poisonous and Flammable Armor explosions, and **Grave Dodger** (it can save you again that island). It also gives a failed Rigged Totems or Autoloader roll a second try, and doubles Finishing Move's bonus.
- New **Epic perk, Autoloader**: each kill has a **10% chance to reload your weapon instantly** (`[autoloader] chancePercent`; more levels add up hyperbolically).
- New **Epic perk, Finishing Move**: enemies you kill with a **critical hit** drop **50% more crystals** per level (`[finishingMove] crystalPercent`).
- New **Epic perk, Rigged Totems**: Health, Chance and Gamble totems are **25% more likely to go your way** per level (`[riggedTotems] chanceBonusPercent`): a 50% totem becomes 62.5%.

### 0.7.2

- New **Greed relic, Cursed Lottery**: **100,000 crystals** when you pick it up, but while you hold it you only get **0.01%** of the crystals you'd gain (`[cursedLottery] crystals`, `gainPercent`). It **can't be dropped**. It's as rare as the game's rarer Greed relics (spawn weight 0.15). With CrabSync, only the player who picks it up gets the crystals, not every copy.
- New **Legendary relic, Lucky Charm**: each mod and perk in the chests you open has a **25% chance to be level 2** (`[luckyCharm] chancePercent`).
- **Martyr**'s bubble now does **a third of a basic crab's max health** a second (`crabHealthPerSecond = 0.33`), so it grows with the enemies every island: 5 a second on the first islands. The old `damagePerSecond` setting is ignored.

### 0.7.1

- New **Legendary relic, Reliquary**: **+5 relic slots** (`[reliquary] slotsPerLevel`). The game allows 10 relics and won't let you pick up an 11th; with the Reliquary you can carry 15, and the inventory menu shows them all.

### 0.7.0

- **Key totem menu** now always has **every item**, CrabMods' own included (Bandolier, Hoarder, ...). Locked items say "Locked" on their card and have a **Locked** tab of their own; choosing one unlocks it. CrabMods' items are never written into your save's unlock list, and any that 0.6.0's menu put there (or empty entries) are taken out.
- **Key totem menu** cards are no longer see-through. The menu was dimming every card without gamepad focus to half opacity, as it does YES / NO. Now they are only a little darker.
- **Martyr** reworked: instead of exploding once when you go down, your **revive bubble hurts the enemies in it** for as long as you're down (`damagePerSecond`, 60 per level). **Magnify** makes the bubble bigger (`magnifyBubble`), both the area it hurts and the area a teammate can revive you from. The old `damage`, `radiusMeters` and `knockback` settings are gone.

### 0.6.0

- **Key totem menu** is now a full catalog instead of a choice of 3. It shows **every locked item**, or **every item** once everything is unlocked, as the game's own pickup cards, three to a row in a scrolling list sorted by kind, rarity and name.
  - **Category tabs** at the top (All, Weapons, Abilities, Melee, Weapon Mods, Ability Mods, Melee Mods, Perks, Relics) look like the settings menu's tabs. Only kinds that have items get a tab. The Next / Previous Submenu keys (the gamepad bumpers) switch tabs too.
  - A **search box** filters by name, type and description as you type. Several words must all match, e.g. `epic fire`.
- `choices` now only sets how many items come out of the totem in row mode (`menu = false`).

### 0.5.1

- **Key totem menu**: the item cards sit on a dark, nearly opaque plate, so they are no longer see-through over the blurred game. The plate lightens when you hover or press a card.

### 0.5.0

- **Key totem menu**: the lobby's key totem now opens a menu instead of dropping a row of items. It is the game's own confirmation prompt with the items' own pickup cards as buttons: click one (or select it with a gamepad) and it is unlocked and comes out of the totem. It works with **any host**, CrabMods or not. `menu = false` brings back the row.
- 4 new **Legendary** items, one per inventory group, each giving **+5 slots per level** in its group (`slotsPerLevel`):
  - **Bandolier** (weapon mod): weapon mod slots.
  - **Tool Belt** (ability mod): ability mod slots.
  - **Scabbard** (melee mod): melee mod slots.
  - **Backpack** (perk): perk slots.
- The **inventory menu** gets more slots when you have them, and scrolls once they don't fit.
- New ability mods (Recharge, Tool Belt) use the game's plain ability mod icon for their rarity.

### 0.4.1

- **Specialists** now also bring their pickups into your chests of **every other loot pool**: with Critical Specialist, critical pickups can come out of a Speed or Health chest too, not only Critical chests. There they are as likely as the chest's own pickups (`otherChests = 1`; `0` turns it off). This works for the regular pool chests (Damage, Critical, Elemental, Speed, Luck, Health, Economy, Skill). Random chests already offered everything, and Greed, Relic, Anvil and Spiked chests stay as they are. Rarity still applies, so an Epic critical pickup still only comes out of Epic chests.
- **Bullet Rain + Hellshooter**: with both, your shots alternate. Every other shot falls from the sky and the rest come up out of the ground (before this, Hellshooter took over every shot).

### 0.4.0

6 new items, stronger melee mods, a key totem that lets you choose, and a new Martyr.

- Perks:
  - **Collateral Damage** (Epic): +5% damage for every destructible you break during an island's fight. It resets each island, like Tasty Orange.
  - **Overflow** (Epic): every 100 health healed while at full health gives +3% damage for the rest of the run.
  - **Thick Shell** (Epic): +3% max health multiplier per perk level you have.
  - **Community Support** (Epic): you and every player within 20 m fire faster, one Rapid Fire level (+12%) for each player in range who has it, you included.
- Weapon mods:
  - **Bullet Rain** (Epic): your shots fall from the sky onto where you aim. +10% damage.
  - **Hellshooter** (Epic): your shots fire up out of the ground under where you aim. +10% damage.
- **Martyr** is new: when you go down you explode for 150 damage to the enemies around you (25 m), and **Magnify** makes it bigger. It no longer boosts teammates. Its settings are now `damage`, `radiusMeters` and `knockback`.
- **Melee mods** are stronger (`[meleeMods]`): Sharp Claws 200% (was 100), Time Claws 300% (150), Iron Claws 500% (300) with a 20% cooldown penalty (40), Big Claws 40% (20), Blender 30% (20), Vampire 15 health per kill (5), and the elemental claws apply twice the stacks.
- **Key totem** (`[keyTotem]`): the lobby's key totem offers 3 locked items to choose from, and the one you take is unlocked. Once everything is unlocked, it offers 3 of any weapon mods, ability mods, melee mods, perks and relics. It needs CrabMods on the host too.

### 0.3.3

- **Double or Nothing** now works on chests instead of loot choices: at the end of each island you get **twice as many reward chests, or none** (50/50, `doubleChancePercent`). It counts every chest the island gives you (bonus chests, Care Package, totems) and rolls just before High Roller, so High Roller still works on what is left. You see "Double or Nothing doubled your chests!" or "Double or Nothing took all your chests!". Chests you open now always offer their normal choices.

### 0.3.2

- With CrabSync's shared health pool (and `chestHealthCost = split`), the Spiked island's portal says "All the team's spiked chests together cost 35% of base max health", which CrabSync 1.0.3 now makes true.

### 0.3.1


- **Spiked Chest** costs **35%** of your base max health instead of the game's 25%, and the Spiked island's portal says "costs 35% of your base max health". Set it with `costPercent` in the new `[spikedChest]` section (0 to 99; 25 is the game's own). In multiplayer the host's value is used, for the cost and for everyone's portal text.

### 0.3.0


16 new items. All percentages are per level, kept low because levels stack.

- Perks:
  - **Minimalist** (Epic): +5% damage per empty weapon mod slot.
  - **Underdog** (Epic): +0.5% damage per 1% of health missing.
  - **Untouchable** (Epic): +1% damage per second without being hit, up to +20%.
  - **Jack of All Trades** (Legendary): +3% damage per different pickup tag you have.
  - **Greed Is Power** (Greed): +5% damage per Greed perk level.
- Relics:
  - **Purist** (Epic): +10% damage while all your elemental pickups are one element.
  - **Martyr** (Epic): while you are down, each teammate deals +15% damage.
  - **Last Crab Standing** (Epic): +25% damage per downed teammate, +25% more when you are the last one up. It never drops when you play alone.
  - **Double or Nothing** (Greed): every chest you open offers twice as many choices or nothing at all. It applies after every other chest perk and relic.
  - **Fire, Ice, Lightning, Poison, Critical, Damage and Luck Specialist** (Epic): pickups of that kind are offered 3× as often. Damage and Luck Specialist also make those islands 3× as likely.
- Ability mod: **Recharge** (Epic): each kill takes 0.1 s off your ability cooldown.
- **The host's settings are used by everyone** in multiplayer. They are sent to each player who joins and used until they leave; your own `CrabMods.ini` is not changed.
- New items' sections are added to an existing `CrabMods.ini`, keeping your values.
- With every item switched off, CrabMods still loads, so a host's settings can apply.
- The DLLs now carry a version number (file properties), which the manager reads.


### 0.2.0

- Renamed from Arsenal: Arsenal is now one item in `CrabMods.dll`.
- New: **Hoarder**, a Legendary perk. Damage +3% for each weapon mod and each perk level you have, Arsenal and Collector in one. It stacks with both.
- New: **Heavy Shot** (the game's own weapon mod) no longer needs Bouncing Shot to drop. It works with every weapon.
- Settings move to `CrabMods.ini`, with `[arsenal]`, `[hoarder]` and `[heavyShot]` sections. An existing `Arsenal.ini` is carried over on first launch.
- If the old `Arsenal.dll` is still installed, CrabMods leaves Arsenal to it and says so in the log.

### 0.1.0

First release, as Arsenal.

- **Arsenal**, an Epic weapon mod: damage +3% for each other weapon mod you have.

## CrabMods Manager

### 0.2.0

- **Launch game** starts Crab Champions through Steam.
- **Crash reports.** While it is open, the manager watches the game's crash folder, including Proton's prefix on Linux. When the game crashes (or crashed while the manager was closed), it saves the mod logs, the settings and the game's crash report:
  - Player names, your user and computer name, your user folder and the crash report's machine ids are taken out.
  - It saves to `CrashReports` next to the exe, as a folder and a zip.
  - It offers to open a GitHub issue already filled in with the error, the call stack, the versions and the end of the logs. Drag the zip into the issue.
- **Collect logs...** does the same by hand.
- `--selftest` lists the crash reports it finds. `--selftest --report` saves one.

### 0.1.0

First release.

- Finds the game in every Steam library (also under Proton/wine and Flatpak Steam), or lets you browse to it.
- Installs, updates, switches on/off and removes CrabMods and CrabSync from the latest GitHub release.
- Edits each mod's settings.
- Shares settings as `CRAB1-` codes: copy, then paste into another manager.
- `--selftest` writes what it found to a text file, without a window.

## CrabSync

### 1.0.4

- **Is the host syncing?** Only the host's CrabSync syncs, so playing in a friend's game you couldn't tell whether max health and deaths would be shared. The host's CrabSync now greets every player who joins. If you have CrabSync too, your `CrabSync.log` says which version the host runs and whether it shares health (`syncHealth`). If no greeting arrives within 30 seconds, you get an on-screen message: the host isn't running CrabSync 1.0.4 or newer. Shared max health needs the host on 1.0.2 or newer.
- The host's log now also records max health changes that aren't shared (in the lobby, or before the run has started), so a max health change that stays with one player can be traced.

### 1.0.3

- Max health chests (spiked) are shared out so that **every player opening one costs the team what one chest costs alone**, compounding included: with a 35% spiked chest, four players lose about 10.2% each and exactly 35% together (it was the cost divided by the number of players, which came to about 31%). Health costs are still divided by the number of players.

### 1.0.2

- Fixed: **deaths were not shared.** The game swaps a dead player's character for a spectator in the same frame, so CrabSync never saw the death. Now a player who dies empties the shared pool and everyone dies with them.
- Fixed: **max health was per player**, so players ended up with different max health and the pool was measured against the highest. Max health is now shared: losing or gaining it (Eternal Punishment hits, totems, damage areas, perks such as Fortitude, Personal Space, Bullseye, Damage Seeker and Valued Customer) changes everyone's, and everyone has the same max and the same health.
- Chests that cost health or max health (spiked and greed chests) only charge the team the cost divided by the number of players. The new `chestHealthCost` setting can be `split` (default), `full` or `free`.
- A lower max cuts health down to it instead of taking the difference off. Losing 5 max health at 100/1000 now leaves 100/995, not 95/995. This applies when a player leaves, too.

### 1.0.1


- Fixed: returning to the lobby after a run could kill you over and over. The health pool carried the run's damage into the lobby, where your max health is lower, so the pool counted as empty.
- In the lobby the pool only shows its size: everyone's max includes the extra health for each player, everyone is kept at full health, and no damage is shared.
- CrabSync now waits a moment after you arrive in the lobby before starting the new team.
- An empty pool only kills the characters that were alive when the damage emptied it. A respawned or new character is never killed for an old empty pool; the pool restarts from it instead.
- The "could not fully apply" log line now says what was out of sync.

### 1.0.0

First release.

- Shared inventory for the whole lobby: weapon, ability and melee, every mod, perk and relic stack, enhancements and buffs, inventory slots and crystals.
- Lobby loadout picks are shared, and the team inventory is rebuilt from everyone's starting state when a run begins.
- One shared health pool: your max health plus 250 (`healthPerPlayer`) for each extra player. When it hits 0, everyone dies.
- Game-wide changes (crystals paid to everyone, slot resets at run start) are counted once, not once per player.
- Changes are buffered: a player's pickups are taken in once their inventory has held still for one check. Items given to teammates are queued and handed out one at a time.
- Host only. Clients need nothing.
- `xinput1_3.dll` mod loader that loads every other DLL in `Win64`.
- Turns itself off on an unsupported game build.

#### Known issue

- When two players pick up different items at the same moment, one item has been seen missing from the host's inventory screen. If it happens, press F10 while the item is missing and attach `CrabSync.log` to an issue.
