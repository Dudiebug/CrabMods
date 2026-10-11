CrabSync - one team, one inventory, one health bar for Crab Champions co-op.
https://github.com/Dudiebug/CrabMods

Only the HOST needs this. Friends join like normal.
Things are only shared when the host runs CrabSync (max health and deaths need
1.0.2 or newer). If you install it too, you get a message in a friend's game
when their CrabSync is missing or older than 1.0.4.

INSTALL
1. In Steam, right-click Crab Champions > Manage > Browse local files.
2. Open CrabChampions\Binaries\Win64 (the folder with CrabChampions-Win64-Shipping.exe).
3. Copy xinput1_3.dll and CrabSync.dll into it.
4. Linux / Steam Deck (Proton) only: set the game's Launch Options to
       WINEDLLOVERRIDES="xinput1_3=n,b" %command%
5. Start the game and host a lobby. CrabSync.ini (settings) and CrabSync.log
   appear in the same folder.

HOTKEYS (host)
  F9   force everyone back in sync
  F10  write everyone's inventory to CrabSync.log (attach it to bug reports)

UNINSTALL
Delete CrabSync.dll, CrabSync.ini and CrabSync.log (and xinput1_3.dll if no
other mod uses it).

If the game updates, CrabSync turns itself off (see CrabSync.log) until a new
release is out.

Licenses: LICENSE (CrabSync, MIT), MinHook-LICENSE.txt (MinHook, BSD 2-Clause).
