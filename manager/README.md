# CrabMods Manager

The easiest way to get and keep the mods in this repo. Every CrabMods item and CrabSync is listed as a mod you can install or uninstall, and each one's settings are right next to it. It keeps the mods and itself up to date, shares your whole setup as one short code, starts the game, and when the game crashes gathers everything needed to report it. Windows, or Proton / wine on Linux and Steam Deck.

Download `CrabModsManager.exe` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=manager) and run it from anywhere. Nothing to install.

## Mods

- **On first start it installs everything**: CrabMods with every item, CrabSync, and the `xinput1_3.dll` mod loader. Uninstall what you don't want. (If you already had mods installed, it leaves them as they are.)
- **The Mods tab** lists CrabMods, CrabSync and every CrabMods item, with its type and rarity. Search it, or show only installed or not-installed mods, or one type.
- **Install / Uninstall** on a CrabMods item puts it in or takes it out of the game's loot (its `enabled` setting in `CrabMods.ini`). On CrabMods or CrabSync it installs or removes the DLL. Installing an item without CrabMods installs CrabMods with just that item.
- **Click a mod** to see what it does and its settings: switches, sliders, numbers and lists, with each setting's explanation. Changes are saved at once and used the next time the game starts. **Reset** puts a setting back to its default.
- New items in a CrabMods update arrive installed.

The settings are still the mods' own `CrabMods.ini` and `CrabSync.ini`, so editing them by hand works too. The manager keeps their comments.

In multiplayer the host's CrabMods items and settings are used by everyone (see [CrabMods](../crabmods/README.md#multiplayer-the-hosts-settings)), and only the host needs CrabSync.

## Updates

- **Mods**: at start and every 30 minutes it checks GitHub, and installs a new CrabMods or CrabSync release by itself. While the game is running, the update waits until you close it.
- **The manager itself**: when a newer manager is released, it downloads it, swaps it in and restarts. The status bar then says it was updated.
- **Play** waits for running updates to finish, then starts the game through Steam.

Both can be switched off in the **Settings** tab, which also shows the installed and latest versions.

Managers before 0.3.0 can't update themselves: download 0.3.0 once by hand.

## Setup codes

The **Share** tab has your setup code: which mods are installed and every setting you changed, in one line like `CRAB2-S;M-1a2b`. Everything at its default is just `CRAB2-S;M-` and a 4-character check. A friend pastes it in **Load a code**, sees what it installs, uninstalls and changes, and presses **Load**.

- A damaged or cut-off code is refused.
- Parts of a code from a newer CrabMods (items or settings this version doesn't have) are skipped, and it says how many.
- Old `CRAB1-` codes from managers 0.1 and 0.2 still load.

**Reset to defaults** installs everything again with every setting at its default.

## Crashes

- **While the manager is open it watches for crashes**, however the game was started. When the game crashes it saves a report, then asks whether to report it. It also offers a crash that happened while it was closed, the next time it opens (up to 3 days later).
- **Collect logs...** (Settings tab) saves the same report by hand, for a problem that isn't a crash. It includes the newest crash report if it's from the last day.

A report is a folder in `CrashReports` next to the exe. It holds a `.zip` with:
- `CrabMods.log`, `CrabSync.log`, `CrabMods.ini` and `CrabSync.ini`;
- the game's crash report from `Saved\Crashes` (`CrashContext.runtime-xml`, the minidump when it has one);
- `report.md`, a summary: the error, the call stack, the versions and the end of the logs.

**What's taken out** of every file (except the minidump, which is memory from the game):
- player names (they become `Player 1`, `Player 2`, ...);
- your user name and computer name, and the user folder in paths;
- the crash report's machine and account ids.

**Reporting it**:
- **Yes** opens a new GitHub issue in your browser, already filled in with the summary, and opens the report folder. Describe what you were doing, drag the `.zip` into the issue, and submit.
- **No** only opens the folder. Post `report.md` and the zip anywhere else, such as Discord.
- Nothing is sent unless you submit the issue yourself.

Opening an issue needs a GitHub account. GitHub has no way to file issues without one, and a key built into the app would let anyone post as the repo.

## Linux / Steam Deck

Run it with Proton or wine. For example, add it to Steam as a non-Steam game with Proton enabled, or:

```bash
wine CrabModsManager.exe
```

It finds Steam libraries under your home folder, Flatpak Steam included, and the game's crash reports in Proton's prefix. The game itself needs this launch option to load the mods; the manager shows it until Steam has it, with a **Copy** button:

```
WINEDLLOVERRIDES="xinput1_3=n,b" %command%
```

## Files

| File | |
|---|---|
| `CrabModsManager.ini` | Next to the exe: the game folder, the update switches, and the last crash it offered to report. |
| `cache\` | Next to the exe: the latest mod DLLs, downloaded ahead of installing (and to list a mod's items before it's installed). |
| `CrashReports\` | Next to the exe: one folder per report (see [Crashes](#crashes)). |
| `CrabModsManager-selftest.txt` | Written by `CrabModsManager.exe --selftest`: the game folder, the releases, installed versions, every item and setting, the setup code and the crash reports it found. No window. `--selftest --report` also saves a report. |

The manager talks to `api.github.com` and GitHub's download servers, nothing else. Crash reports leave your PC only if you post them.

## Source

- `manager/main.cpp`: the window ([Dear ImGui](https://github.com/ocornut/imgui) on Direct3D 11), installs, updates and setups.
- `manager/platform.hpp`: Windows plumbing (finding the game, downloads, file versions, crash reports).
- `manager/core.hpp`: versions, release JSON, ini merging, the item catalog and its settings, setup codes, crash report scrubbing and the issue text. Tested in `tests/manager_test.cpp`.

The mod DLLs describe themselves to the manager: CrabMods exports its item list with every default setting (`CrabModsCatalog`), and CrabSync its default settings (`CrabSyncDefaultIni`). Outside the game their `DllMain` does nothing, so the manager can load them to read this. Built by `./build.sh` with the other mods. The font is [Roboto](https://fonts.google.com/specimen/Roboto) (Apache 2.0).
