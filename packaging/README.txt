Arsenal - a new weapon mod for Crab Champions.
https://github.com/Dudiebug/CrabArsenal

Arsenal (Epic weapon mod): damage increased by 3% for each other weapon mod
you have. Each extra Arsenal stacks the bonus.

EVERY PLAYER in the lobby needs it, the host and every friend.

INSTALL
1. In Steam, right-click Crab Champions > Manage > Browse local files.
2. Open CrabChampions\Binaries\Win64 (the folder with CrabChampions-Win64-Shipping.exe).
3. Copy xinput1_3.dll and Arsenal.dll into it. (If you already have CrabSync,
   keep its xinput1_3.dll; it is the same mod loader.)
4. Linux / Steam Deck (Proton) only: set the game's Launch Options to
       WINEDLLOVERRIDES="xinput1_3=n,b" %command%
5. Start the game. Arsenal.ini (settings) and Arsenal.log appear in the same folder.

UNINSTALL
Delete Arsenal.dll, Arsenal.ini and Arsenal.log (and xinput1_3.dll if no other
mod uses it).

If the game updates, Arsenal turns itself off (see Arsenal.log) until a new
release is out.

Licenses: LICENSE (Arsenal, MIT), MinHook-LICENSE.txt (MinHook, BSD 2-Clause).
