# CrabMods Manager

A small Windows app that installs, updates and configures the mods in this repo, starts the game, and when the game crashes gathers everything needed to report it. It also runs under Proton / wine on Linux and Steam Deck.

Download `CrabModsManager.exe` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=manager) and run it from anywhere. Nothing to install.

## What it does

- **Finds the game.** It looks in every Steam library, or you can **Browse...** to the `Crab Champions` folder. The folder is remembered in `CrabModsManager.ini` next to the exe. If you put the exe in the game's `Win64` folder, it uses that.
- **Lists the mods**: CrabMods and CrabSync, which version is installed, the latest release on GitHub, and whether an update is available.
- **Install / Update** downloads the mod and the `xinput1_3.dll` mod loader from the latest GitHub release, straight into the game folder.
- **Switch on/off** renames the DLL to `.dll.disabled` and back, so you can turn a mod off without losing it or its settings.
- **Remove** deletes the DLL and keeps its settings file. The mod loader goes too once no mods are left.
- **Settings**: pick a mod's `.ini`, edit it, **Save**. **Reload** throws away unsaved edits. **Open folder** opens the game's `Win64` folder.

Close the game before installing, updating or switching a mod. The game holds its DLLs open while it runs.

## Crashes

- **Launch game** starts Crab Champions through Steam.
- **While the manager is open it watches for crashes**, however the game was started. When the game crashes it saves a report, then asks whether to report it. It also offers a crash that happened while it was closed, the next time it opens (up to 3 days later).
- **Collect logs...** saves the same report by hand, for a problem that isn't a crash. It includes the newest crash report if it's from the last day.

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

On Linux the manager finds the game's crash reports in Proton's prefix (`steamapps/compatdata/774801`), even when the manager runs in another one.

## Share codes

**Copy share code** puts the mod's settings on the clipboard as one line starting with `CRAB1-`. Send it to a friend; they press **Paste share code** in their manager:
- The settings go into their editor, merged into their own file (their comments stay), and they press **Save**.
- A damaged or cut-off code is refused.
- The code holds only the settings. It can't contain a file name or anything else.

In a CrabMods game you don't need a code: **the host's CrabMods settings are sent to every player automatically** when they join, and used until they leave (see [CrabMods](../crabmods/README.md#multiplayer-the-hosts-settings)).

## Linux / Steam Deck

Run it with Proton or wine. For example, add it to Steam as a non-Steam game with Proton enabled, or:

```bash
wine CrabModsManager.exe
```

It finds Steam libraries under your home folder, Flatpak Steam included. The game itself still needs this launch option to load the mods:

```
WINEDLLOVERRIDES="xinput1_3=n,b" %command%
```

## Files

| File | |
|---|---|
| `CrabModsManager.ini` | Next to the exe: the game folder you chose, and the last crash it offered to report. |
| `CrashReports\` | Next to the exe: one folder per report (see [Crashes](#crashes)). |
| `CrabModsManager-selftest.txt` | Written by `CrabModsManager.exe --selftest`: the game folder, the releases and installed versions, and the crash reports it found. No window. `--selftest --report` also saves a report. |

The manager talks to `api.github.com` and GitHub's download servers, nothing else. Crash reports leave your PC only if you post them.

## Source

`manager/main.cpp` (the window, downloads and files) and `manager/core.hpp` (versions, release JSON, ini merging, share codes, crash report scrubbing and the issue text; tested in `tests/manager_test.cpp`). Built by `./build.sh` with the other mods.
