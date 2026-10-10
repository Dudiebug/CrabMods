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
  Martyr (Epic relic): while you are down, each teammate deals +15% damage.
  Last Crab Standing (Epic relic): +25% damage per downed teammate, +25% more
    when you are the last one up. Never drops when you play alone.
  Double or Nothing (Greed relic): chests offer twice as many choices, or nothing.
  Fire/Ice/Lightning/Poison/Critical/Damage/Luck Specialist (Epic relics):
    that kind of pickup is offered 3x as often (Damage and Luck: islands too).
  Recharge (Epic ability mod): each kill takes 0.1 s off your ability cooldown.
  Heavy Shot (the game's own mod): drops for every weapon, not only with
    Bouncing Shot.

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
