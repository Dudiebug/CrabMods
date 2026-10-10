# CrabMods Manager

A small Windows app that installs, updates and configures the mods in this repo. It also runs under Proton / wine on Linux and Steam Deck.

Download `CrabModsManager.exe` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=manager) and run it from anywhere. Nothing to install.

## What it does

- **Finds the game.** It looks in every Steam library, or you can **Browse...** to the `Crab Champions` folder. The folder is remembered in `CrabModsManager.ini` next to the exe. If you put the exe in the game's `Win64` folder, it uses that.
- **Lists the mods**: CrabMods and CrabSync, which version is installed, the latest release on GitHub, and whether an update is available.
- **Install / Update** downloads the mod and the `xinput1_3.dll` mod loader from the latest GitHub release, straight into the game folder.
- **Switch on/off** renames the DLL to `.dll.disabled` and back, so you can turn a mod off without losing it or its settings.
- **Remove** deletes the DLL and keeps its settings file. The mod loader goes too once no mods are left.
- **Settings**: pick a mod's `.ini`, edit it, **Save**. **Reload** throws away unsaved edits. **Open folder** opens the game's `Win64` folder.

Close the game before installing, updating or switching a mod. The game holds its DLLs open while it runs.

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
| `CrabModsManager.ini` | Next to the exe: the game folder you chose. |
| `CrabModsManager-selftest.txt` | Written by `CrabModsManager.exe --selftest`: the game folder, the releases and installed versions it found. No window. |

The manager talks to `api.github.com` and GitHub's download servers, nothing else.

## Source

`manager/main.cpp` (the window, downloads and files) and `manager/core.hpp` (versions, release JSON, ini merging, share codes; tested in `tests/manager_test.cpp`). Built by `./build.sh` with the other mods.
