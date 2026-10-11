CrabMods - new items for Crab Champions.
https://github.com/Dudiebug/CrabMods

Items (all bonuses are per level, and stack):
  Arsenal (Epic weapon mod): +3% damage per other weapon mod.
  Hoarder (Legendary perk): +3% damage per weapon mod and per perk level.
  Minimalist (Epic perk): +5% damage per empty weapon mod slot.
  Underdog (Epic perk): +0.5% damage per 1% of health missing.
  Untouchable (Epic perk): +1% damage per second without being hit, up to +20%.
  Jack of All Trades (Legendary perk): +3% damage per different pickup tag.
  Greed Is Power (Greed perk): +5% damage per Greed perk level.
  Purist (Epic relic): +10% damage while all your elemental pickups are one element.
  Collateral Damage (Epic perk): +5% damage per destructible broken this island.
  Overflow (Epic perk): every 100 health healed at full health gives a
    permanent +3% damage.
  Thick Shell (Epic perk): +3% max health multiplier per perk level.
  Community Support (Epic perk): you and players within 20 m fire faster, one
    Rapid Fire level for each player in range who has it, you included.
  Bullet Rain (Epic weapon mod): shots fall from the sky onto where you aim.
  Hellshooter (Epic weapon mod): shots fire up out of the ground where you aim.
    With both, your shots alternate between the sky and the ground.
  Martyr (Epic relic): while you're down, your revive bubble hurts the
    enemies in it (60 damage a second). Magnify makes the bubble bigger.
  Last Crab Standing (Epic relic): +25% damage per downed teammate, +25% more
    when you are the last one up. Never drops when you play alone.
  Double or Nothing (Greed relic): twice as many island reward chests, or none.
  Fire/Ice/Lightning/Poison/Critical/Damage/Luck Specialist (Epic relics):
    that kind of pickup is offered 3x as often and can come out of any of
    your chests, not only its own pool's (Damage and Luck: islands too).
  Recharge (Epic ability mod): each kill takes 0.1 s off your ability cooldown.
  Bandolier / Tool Belt / Scabbard / Backpack (Legendary weapon mod / ability
    mod / melee mod / perk): +5 slots in that inventory group per level. The
    inventory menu scrolls when they don't fit.
  Reliquary (Legendary relic): +5 relic slots on top of the game's 10.
  Heavy Shot (the game's own mod): drops for every weapon, not only with
    Bouncing Shot.
  Melee mods (the game's own): stronger numbers, e.g. Sharp Claws +200%,
    Iron Claws +500%, Vampire 15 health per kill ([meleeMods]).
  Key totem (the lobby's): a menu of every item, with category tabs (locked
    items have their own) and a search box, lets you choose any item; a
    locked one is unlocked. Works with any host.
  Spiked Chest (the game's own): costs 35% of your base max health instead
    of 25%, and its island's portal says so ([spikedChest] costPercent).


EVERY PLAYER in the lobby needs it, the host and every friend. In multiplayer
everyone uses the host's settings (CrabMods.ini), sent when you join.

Easiest: use CrabModsManager.exe from the GitHub releases. It installs, updates
and edits settings for you.

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
