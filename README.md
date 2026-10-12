# CrabMods

Native mods for **Crab Champions**, and the reverse-engineering notes and SDK they are built on.

| Mod | What it does | Who installs it | Download |
|---|---|---|---|
| [**CrabSync**](crabsync/README.md) | One team, one inventory, one health bar: every pickup, crystal and hit is shared by the whole lobby. | The host only | [`CrabSync-x.y.z.zip`](https://github.com/Dudiebug/CrabMods/releases?q=CrabSync) |
| [**CrabMods**](crabmods/README.md) | The **Soul Whip**, a necromancer weapon that raises the enemies you kill as undead, with 20 items of its own; and 39 more new items: damage perks and relics (Hoarder, Collateral Damage, Overflow, Last Crab Standing, ...), Thick Shell, Community Support, Bullet Rain and Hellshot, a Martyr relic whose revive bubble hurts enemies, Specialist relics that make one kind of loot 3× as likely, Double or Nothing, Cursed Lottery, Lucky Charm (chest items can be level 2), Encore (perks go off twice), Combo Catalyst, Autoloader, Finishing Move, Rigged Totems, Recharge, Pathfinder (more portal choices), Enhanced Turrets as an ability mod, charms that add 5 inventory slots per level, stronger melee mods, a key totem menu to choose your unlock, Heavy Shot for every weapon, and Spiked Chests that cost 35% max health (35% for the whole team with CrabSync). In multiplayer the host's settings are used by everyone. | Every player | [`CrabMods-x.y.z.zip`](https://github.com/Dudiebug/CrabMods/releases?q=CrabMods) |
| [**CrabMods Manager**](manager/README.md) | **The easiest way to play with these mods.** Every item is a mod you install or uninstall, with its settings beside it; keeps the mods and itself up to date; shares your whole setup as one short code; starts the game and turns a crash into a ready-made GitHub issue. Windows, or Proton/wine on Linux and Steam Deck. | You | [`CrabModsManager.exe`](https://github.com/Dudiebug/CrabMods/releases?q=manager) |

The two mods work together or on their own. Each one's page has its settings, details and known issues.

## Install

**The easy way:** download [`CrabModsManager.exe`](https://github.com/Dudiebug/CrabMods/releases?q=manager) and run it. It finds the game, installs every mod, and keeps them up to date. Uninstall whatever you don't want. On Linux / Steam Deck it shows the launch option from step 5 below to paste into Steam.

By hand:

1. Download the zip for the mod you want from [Releases](https://github.com/Dudiebug/CrabMods/releases).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Extract the zip there. Both zips contain the same `xinput1_3.dll`, so overwriting it is fine.
5. **Linux / Steam Deck (Proton) only:** in Steam, right-click the game → **Properties** → **Launch Options**, and paste:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```

Each mod writes its own `.ini` (settings) and `.log` next to its DLL on first launch.

`xinput1_3.dll` is a small mod loader. The game loads it as its controller DLL; it passes controller input through to the real Windows DLL and loads every other `.dll` in the `Win64` folder.

> [!NOTE]
> The mods are built for one build of the game. After a game update they detect the change, write `unsupported game build` to their log and stay off, so the game still runs normally. Watch this repo for an update.

## Building

Requires [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (any recent ucrt release) on Linux, macOS or Windows (with bash).

```bash
export LLVM_MINGW=/path/to/llvm-mingw   # the folder that contains bin/
./build.sh test package
```

- `build/` gets `xinput1_3.dll` (the loader), `CrabSync.dll`, `CrabMods.dll` and `CrabModsManager.exe`.
- `test` runs CrabSync's ledger tests and the manager's tests (with `wine` on Linux; set `WINE=` to choose one).
- `package` makes `build/CrabSync-<version>.zip` and `build/CrabMods-<version>.zip`.
- `install` copies the DLLs into a Steam install (set `CRAB_GAME` to the `Crab Champions` folder if it lives elsewhere).

| Path | |
|---|---|
| `crabsync/` | CrabSync: the team ledger, game inventory and health code, hooks. |
| `crabmods/` | CrabMods: the hooks (`main.cpp`) and one file per item (`items/`). |
| `manager/` | CrabMods Manager. |
| `common/crab/` | Shared by both: game addresses, minimal Unreal Engine types, logging, hooking. |
| `loader/` | The `xinput1_3.dll` mod loader. |
| `tests/` | CrabSync ledger tests and manager tests. |
| `re/` | Reverse-engineering notes, SDK and scripts. |
| `sdk/` | CrabSDK: compilable C++ headers for every game and engine type, with function wrappers, for writing your own DLL mods. |
| `third_party/minhook/` | [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause). |
| `third_party/imgui/` | [Dear ImGui](https://github.com/ocornut/imgui) 1.91.9b (MIT), the manager's UI. |

Every address in `common/crab/game.hpp` comes from static analysis of the game binary. The mods read and call the game's own code; they do not modify any game files.

## Reverse engineering

[`re/`](re/README.md) has everything the mods are built on: a [report](re/REPORT.md) of the game's internals with addresses, a generated C++ SDK of every reflected class, struct and enum, native function and vtable tables, and the scripts that produced them. It is all static analysis. The game's own files, and anything decompiled or extracted from them, are not included.

To write your own native mod, start with [CrabSDK](sdk/README.md): compilable headers with exact layouts for every class, struct and enum, and C++ wrappers for 6209 game and engine functions.

## License

[MIT](LICENSE). MinHook is © Tsuda Kageyu and contributors, under its own [BSD 2-Clause license](third_party/minhook/LICENSE.txt). Dear ImGui is © Omar Cornut, [MIT](third_party/imgui/LICENSE.txt). The manager's font, Roboto, is © Google, under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).

These are fan-made mods. They are not affiliated with or endorsed by the developers or publishers of Crab Champions.
