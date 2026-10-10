CrabMods - new items for Crab Champions.
https://github.com/Dudiebug/CrabMods

Arsenal (Epic weapon mod): damage increased by 3% for each other weapon mod
you have.
Hoarder (Legendary perk): damage increased by 3% for each weapon mod and each
perk level you have. Arsenal and Collector in one, and it stacks with both.
Heavy Shot (the game's own mod): can now drop for every weapon, not just
Bounce weapons.

EVERY PLAYER in the lobby needs it, the host and every friend.

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
