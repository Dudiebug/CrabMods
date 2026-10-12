CrabMods - new items for Crab Champions.
https://github.com/Dudiebug/CrabMods

Items (all bonuses are per level, and stack):
  Arsenal (Epic weapon mod): +3% damage per other weapon mod.
  Hoarder (Legendary perk): +3% damage per weapon mod and per perk level.
  Minimalist (Epic perk): +40% damage, minus 4% per different pickup tag you hold.
  Underdog (Epic perk): +0.5% damage per 1% of health missing.
  Untouchable (Epic perk): +1% damage per second without being hit, up to +20%.
  Jack of All Trades (Epic perk): +3% damage per different pickup tag.
  Greed Is Power (Greed perk): +3% damage per Greed perk level.
  Purist (Epic relic): +10% damage while all your elemental pickups are one element.
  Collateral Damage (Epic perk): +3% damage per destructible broken this island.
  Overflow (Epic perk): every 100 health healed at full health gives
    +3% damage until the island ends.
  Thick Shell (Epic perk): +3% max health multiplier per perk level.
  Community Support (Epic perk): you and players within 20 m fire faster, one
    Rapid Fire level for each player in range who has it, you included. Never
    drops when you play alone.
  Bullet Rain (Epic weapon mod): shots fall from the sky onto where you aim.
  Hellshot (Epic weapon mod): shots fire up out of the ground where you aim.
    With both, your shots alternate between the sky and the ground.
  Martyr (Epic relic): while you're down, your revive bubble hurts the
    enemies in it (a third of a basic crab's max health a second, growing
    every island). Magnify makes the bubble bigger.
  Last Crab Standing (Epic relic): +25% damage per downed teammate, +25% more
    when you are the last one up. Never drops when you play alone.
  Double or Nothing (Greed relic): twice as many island reward chests, or none.
  Cursed Lottery (Greed relic): 100,000 crystals, then only 0.01% of the
    crystals you'd gain while you hold it. Can't be dropped.
  Lucky Charm (Legendary relic): mods and perks in your chests have a 25%
    chance to be level 2.
  Combo Catalyst (Epic perk): +1 stack of each element your hits apply for
    every combo level above 1.
  Encore (Epic relic): perks that go off by themselves (on-kill and armor
    explosions, Grave Dodger, ...) have a 15% chance to go off twice.
  Autoloader (Epic perk): each kill has a 10% chance to reload instantly.
  Finishing Move (Epic perk): critical kills drop 50% more crystals.
  Rigged Totems (Epic perk): Health, Chance and Gamble totems are 25% more
    likely to go your way.
  Fire/Ice/Lightning/Poison/Critical/Damage/Luck Specialist (Epic relics):
    that kind of pickup is offered 3x as often and can come out of any of
    your chests, not only its own pool's (Damage and Luck: islands too).
  Summoner Specialist (Epic relic, Soul Whip holders only): the Soul Whip's
    pickups are offered 3x as often and can come out of any of your chests.
  Recharge (Epic ability mod): each kill takes 0.1 s off your ability cooldown.
  Enhanced Turrets (Common ability mod): the game's Enhanced Turrets perk as an
    ability mod: more turret damage per level. The perk no longer drops.
  Pathfinder (Common perk): replaces the Portal Ring relic. One more portal
    choice after each island for every level.
  Charms (Epic; Reliquary Legendary), a kind of item of their own, with their own inventory
    section: Bandolier / Tool Belt / Scabbard / Backpack / Reliquary give +5
    weapon mod / ability mod / melee mod / perk / relic slots per level.
    Charms stack and never take a relic slot. The inventory menu scrolls
    when the slots don't fit.
  Heavy Shot (the game's own mod): drops for every weapon, not only with
    Bouncing Shot.
  Melee mods (the game's own): stronger numbers, e.g. Sharp Claws +200%,
    Iron Claws +500%, Vampire 15 health per kill ([meleeMods]).
  Key totem (the lobby's): while items are still locked, choose one of 3
    random locked items from a menu, and it is unlocked. Once everything is
    unlocked, a menu of every item (category tabs, search box) lets you
    choose any item. Works with any host.
  Spiked Chest (the game's own): costs 35% of your base max health instead
    of 25%, and its island's portal says so ([spikedChest] costPercent).

Soul Whip (a new weapon, on its own pedestal left of the Crossbow in the lobby):
  Each crack sends a purple soul bolt that bursts where it lands and tags
  enemies. While you hold it, your ability becomes raising the dead: kills fill
  a soul bar, and V / C / X / Z raise the last fodder, medium, hard or elite
  enemy you killed as an undead that fights for you ([necromancer]).
  Its own loot, which only drops for whip holders:
    Long Lash, Branding Iron (Common), Soul Siphon, Death Mark, Soul
      Brand (Epic): weapon mods for the bolt and its tags.
    Sturdy Bones, Soul Discount (Common), Mass Grave, Twin Rising
      (Epic): ability mods for raising.
    Grave Claws (Common), Harvester (Legendary): melee mods for souls and free
      raises.
    Soul Jar, Hungry Ghosts, Last Rites (Common), Plague Bearers, Grim
      Harvest (Epic), Army of the Dead (Legendary): perks.
    Ring of Souls, Ring of the Grave (Epic), Phylactery (Legendary): relics.


EVERY PLAYER in the lobby needs it, the host and every friend. In multiplayer
everyone uses the host's settings (CrabMods.ini), sent when you join.

Easiest: use CrabModsManager.exe from the GitHub releases. It installs every item,
keeps CrabMods up to date, and lets you uninstall items and change settings.

INSTALL
1. In Steam, right-click Crab Champions > Manage > Browse local files.
2. Open CrabChampions\Binaries\Win64 (the folder with CrabChampions-Win64-Shipping.exe).
3. Copy xinput1_3.dll and CrabMods.dll into it. (If you already have CrabSync,
   keep its xinput1_3.dll; it is the same mod loader.)
4. Had Arsenal before? Delete Arsenal.dll. CrabMods includes Arsenal, and your
   Arsenal.ini settings are copied into CrabMods.ini on first launch.
5. Linux / Steam Deck (Proton) only: set the game's Launch Options to
       WINEDLLOVERRIDES="xinput1_3=n,b" %command%
6. Start the game. CrabMods.ini (settings) and CrabMods.log appear in the same folder.

UNINSTALL
Delete CrabMods.dll, CrabMods.ini and CrabMods.log (and xinput1_3.dll if no
other mod uses it).

If the game updates, CrabMods turns itself off (see CrabMods.log) until a new
release is out.

Licenses: LICENSE (CrabMods, MIT), MinHook-LICENSE.txt (MinHook, BSD 2-Clause).
