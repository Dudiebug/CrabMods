# CrabMods

Native mods for **Crab Champions**, and the reverse-engineering notes and SDK they are built on.

| Mod | What it does | Who installs it | Download |
|---|---|---|---|
| [**CrabSync**](crabsync/README.md) | One team, one inventory, one health bar: every pickup, crystal and hit is shared by the whole lobby. | The host only | [`CrabSync-x.y.z.zip`](https://github.com/Dudiebug/CrabMods/releases?q=CrabSync) |
| [**CrabMods**](crabmods/README.md) | New items: **Arsenal** (Epic weapon mod), **Hoarder** (Legendary perk), and **Heavy Shot** for every weapon. | Every player | [`CrabMods-x.y.z.zip`](https://github.com/Dudiebug/CrabMods/releases?q=CrabMods) |

The two work together or on their own. Each mod's page has its settings, details and known issues.

## Install

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

- `build/` gets `xinput1_3.dll` (the loader), `CrabSync.dll` and `CrabMods.dll`.
- `test` runs CrabSync's ledger tests (with `wine` on Linux; set `WINE=` to choose one).
- `package` makes `build/CrabSync-<version>.zip` and `build/CrabMods-<version>.zip`.
- `install` copies the DLLs into a Steam install (set `CRAB_GAME` to the `Crab Champions` folder if it lives elsewhere).

| Path | |
|---|---|
| `crabsync/` | CrabSync: the team ledger, game inventory and health code, hooks. |
| `crabmods/` | CrabMods: runtime-made items and the damage bonus. |
| `common/crab/` | Shared by both: game addresses, minimal Unreal Engine types, logging, hooking. |
| `loader/` | The `xinput1_3.dll` mod loader. |
| `tests/` | CrabSync ledger tests. |
| `re/` | Reverse-engineering notes, SDK and scripts. |
| `third_party/minhook/` | [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause). |

Every address in `common/crab/game.hpp` comes from static analysis of the game binary. The mods read and call the game's own code; they do not modify any game files.

## Reverse engineering

[`re/`](re/README.md) has everything the mods are built on: a [report](re/REPORT.md) of the game's internals with addresses, a generated C++ SDK of every reflected class, struct and enum, native function and vtable tables, and the scripts that produced them. It is all static analysis. The game's own files, and anything decompiled or extracted from them, are not included.

## License

[MIT](LICENSE). MinHook is © Tsuda Kageyu and contributors, under its own [BSD 2-Clause license](third_party/minhook/LICENSE.txt).

These are fan-made mods. They are not affiliated with or endorsed by the developers or publishers of Crab Champions.
