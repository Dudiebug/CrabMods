# CrabSync

**One team, one inventory, one health bar.** CrabSync turns Crab Champions co-op into a true team run: every pickup, every crystal and every hit is shared by the whole lobby.

Only the host installs it. Your friends join like normal, with nothing to install.

[**Download the latest release**](https://github.com/Dudiebug/CrabSync/releases/latest)

---

## What gets shared

| | How it works |
|---|---|
| **Weapon, ability, melee** | When anyone swaps (including picking a loadout in the lobby), everyone gets the same one. |
| **Weapon / ability / melee mods, perks, relics** | One shared list. A pickup by anyone adds to everyone's stack. Salvaging a stack removes it for everyone. |
| **Crystals** | One shared wallet. Anyone can earn it, anyone can spend it. |
| **Inventory slots** | Buying a slot unlocks it for the whole team. |
| **Enhancements and buffs** | Upgrading an item upgrades it for everyone. |
| **Health** | One shared pool. Damage to anyone hurts everyone, healing heals everyone, and **when the pool hits 0, everybody dies.** |

The health pool grows with the team: its max is your normal max health **plus 250 for each extra player**. Two players start at 500, three at 750, four at 1000. Max-health pickups stack on top. Armor plates stay per-player.

Every part can be switched off in `CrabSync.ini`.

## Install

1. Download `CrabSync-x.y.z.zip` from [Releases](https://github.com/Dudiebug/CrabSync/releases/latest).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Extract the zip there. You should now have `xinput1_3.dll` and `CrabSync.dll` next to the game's exe.
5. **Linux / Steam Deck (Proton) only:** in Steam, right-click the game → **Properties** → **Launch Options**, and paste:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```
6. Launch the game and **host** a lobby. `CrabSync.ini` and `CrabSync.log` appear in the same folder on first launch.

**To uninstall,** delete `xinput1_3.dll`, `CrabSync.dll`, `CrabSync.ini` and `CrabSync.log` (and the launch option on Linux).

> [!NOTE]
> CrabSync is built for one specific build of the game. If Crab Champions updates, CrabSync detects that, writes `unsupported game build` to `CrabSync.log`, and stays off, so the game still runs normally. Watch this repo for an update.

## Settings

`CrabSync.ini` is created next to the DLL the first time the game starts. Edit it with the game closed. Only the host's settings matter.

| Setting | Default | What it does |
|---|---|---|
| `enabled` | `true` | Master switch. |
| `syncWeapon`, `syncAbility`, `syncMelee` | `true` | Share equipment swaps. |
| `syncWeaponMods`, `syncAbilityMods`, `syncMeleeMods`, `syncPerks`, `syncRelics` | `true` | Share item stacks. |
| `syncEnhancements`, `syncAccumulatedBuffs` | `true` | Share item upgrades and stacking buffs. |
| `syncSlots` | `true` | Share unlocked inventory slots. |
| `syncCrystals` | `true` | One shared crystal wallet. |
| `syncHealth` | `true` | One shared health pool. |
| `healthPerPlayer` | `250` | Max health added to the pool for each player after the first. |
| `announcePickups` | `true` | Teammates' pickups show the pickup popup and trigger perk effects, like a real pickup. |
| `settleSeconds` | `3` | How long to wait after a map loads or a run starts before syncing. |
| `pollSeconds` | `0.1` | How often the host checks for changes. |

## Hotkeys (host)

| Key | |
|---|---|
| **F9** | Force a full resync of everyone to the team inventory. |
| **F10** | Write everyone's inventory and health to `CrabSync.log` (handy for bug reports). |

## Troubleshooting

- **Nothing syncs.** Check that you are the **host**: CrabSync on a joining player's PC does nothing. Then open `CrabSync.log`. It always says what CrabSync is doing, for example `host, syncing in the lobby`.
- **No `CrabSync.log` appears.** The DLLs are in the wrong folder, or (on Linux) the launch option is missing.
- **`unsupported game build` in the log.** The game updated. Wait for a new CrabSync release.
- **Antivirus complaint.** `xinput1_3.dll` is a mod loader that sits where Windows looks for the controller DLL. Antivirus tools sometimes flag that pattern. The full source is in this repo, and every release is built from it.

When reporting a bug, press **F10** when it happens and attach the host's `CrabSync.log`.

### Known issues

- When two players pick up different items at the same moment, one item has been seen missing from the host's inventory screen. If you hit this, press **F10** while the item is missing and attach `CrabSync.log` to an [issue](https://github.com/Dudiebug/CrabSync/issues).

## Running other mods alongside

`xinput1_3.dll` is a small, general mod loader. It passes controller input through to the real Windows DLL and loads **every other `.dll` in the `Win64` folder**. To add another native mod, drop its DLL into that folder. (This also means you should not put unrelated DLLs there.)

## How it works

Crab Champions is host-authoritative: the host's game owns every player's inventory and health and replicates them to everyone. CrabSync runs inside the host's game and keeps a **team ledger**:

- A **team inventory**: what everyone should have.
- A **snapshot** of each player: what they had at the last check.

About ten times a second it compares each player to their snapshot. The difference is what that player just did (a pickup, a purchase, a hit), so it adds that to the team once. A player who is still mid-change is left alone until their inventory holds still for one check, and items going to a player are handed out one at a time, so pickups that land together are never processed half-way. Then it drives every player to the team inventory using the game's own functions, and takes fresh snapshots so its own writes are never counted as new pickups. A change the game makes to everyone at the same moment (paying out crystals, resetting slots at the start of a run) is recognised and counted once.

Health uses the same idea: the pool is stored as damage taken, so it grows when the max grows. When it runs out, every character is killed through the game's own damage path, so game over plays out exactly like a normal wipe.

Everything was built from static analysis of the game binary. CrabSync reads and calls the game's own code; it does not modify any game files.

## Building from source

Requires [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (any recent release) on Linux, macOS or Windows (with bash).

```bash
export LLVM_MINGW=/path/to/llvm-mingw   # the folder that contains bin/
./build.sh test package
```

- `build/xinput1_3.dll` is the loader, `build/CrabSync.dll` is the mod.
- `test` runs the ledger tests (with `wine` on Linux; set `WINE=` to choose one).
- `package` makes `build/CrabSync-<version>.zip`.
- `install` copies the DLLs into a Steam install (set `CRAB_GAME` to the `Crab Champions` folder if it lives elsewhere).

| Path | |
|---|---|
| `loader/` | The `xinput1_3.dll` mod loader. |
| `crabsync/ledger.*` | The team ledger: game-independent and unit-tested. |
| `crabsync/game_inv.*` | Reading and writing inventories and health in the game. |
| `crabsync/main.cpp` | Hooks, the poll loop, lobby and run handling. |
| `common/crab/` | Game addresses, minimal Unreal Engine types, logging, hooking. |
| `tests/` | Ledger tests. |
| `third_party/minhook/` | [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause). |

## License

[MIT](LICENSE). MinHook is © Tsuda Kageyu and contributors, under its own [BSD 2-Clause license](third_party/minhook/LICENSE.txt).

CrabSync is a fan-made mod. It is not affiliated with or endorsed by the developers or publishers of Crab Champions.
