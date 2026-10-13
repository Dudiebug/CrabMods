# CrabMods Manager

The easiest way to get and keep the mods in this repo. Every CrabMods item and CrabSync is listed as a mod you can install or uninstall, with its settings right next to it. It tells you when updates are out, shares your whole setup as one short code, starts the game, and when the game crashes gathers everything needed to report it.

It asks before it changes anything. Nothing goes into your game folder until you press Install. Every change is shown to you first. Every file it adds is checked against the checksum GitHub publishes for it. Remove all mods puts the folder back exactly as it was.

| | Download | |
|---|---|---|
| Windows | [`CrabModsManager.exe`](https://github.com/Dudiebug/CrabMods/releases?q=manager) | Run it from anywhere. Nothing to install. |
| Linux, Steam Deck | [`CrabModsManager-x86_64.AppImage`](https://github.com/Dudiebug/CrabMods/releases?q=manager) | Make it executable (`chmod +x`, or Properties → Allow executing), then run it. On a Steam Deck, use Desktop Mode. |

## First start

The first screen shows:
- the game folder it found (**Change...** to pick another);
- what to install: CrabMods and CrabSync, each with a checkbox;
- two questions, which you answer: should mod updates install automatically, and should the manager update itself automatically;
- every file it will add or replace.

**Install** does it. **Just browse** looks around first; install from the Mods tab whenever you like. Until you choose one of them, nothing is written anywhere, not even the manager's own settings.

If your game folder already has an `xinput1_3.dll` from something else, it is kept in the manager's backups, and Remove all mods puts it back.

## Mods

- **The Mods tab** lists CrabMods, CrabSync and every CrabMods item, with its type and rarity.
  - **Search** it, or show only installed or not-installed mods, or one type.
  - **Sort** it by type, name, rarity, installed first, or newest (the newest items first). You can also click a column header. CrabMods and CrabSync stay on top.
  - Items that came with the latest CrabMods update have a **NEW** badge.
- **Install / Uninstall** on a CrabMods item puts it in or takes it out of the game's loot (its `enabled` setting in `CrabMods.ini`). On CrabMods or CrabSync it adds or removes the DLL. Installing an item without CrabMods installs CrabMods with just that item.
- **Click a mod** to see what it does, and its settings: switches, sliders, numbers and lists, each with its explanation. Changes are saved at once and used the next time the game starts. **Reset** puts a setting back to its default.

The settings are still the mods' own `CrabMods.ini` and `CrabSync.ini`, so editing them by hand works too. The manager keeps their comments.

In multiplayer the host's CrabMods items and settings are used by everyone (see [CrabMods](../crabmods/README.md#multiplayer-the-hosts-settings)), and only the host needs CrabSync.

### Every change is shown first

Before it installs, uninstalls, updates, rolls back, loads a setup code or resets to defaults, a **Review changes** window lists:
- every file it will add, replace or remove;
- what it backs up;
- which settings change.

**Apply** does it; **Cancel** changes nothing.

**While the game is running**, changes to the game folder wait. The bar at the top says what's waiting, and the change is made as soon as you close the game.

## Updates

The manager checks GitHub for new releases every 5 minutes (Settings: every minute, 5, 15, or only when you press **Check now**). An unchanged list costs nothing against GitHub's hourly limit.

What happens with a new release is up to you. You answer this on the first screen, and can change it in Settings.

| | Automatic | Not automatic (ask me first) |
|---|---|---|
| A new CrabMods or CrabSync | It's installed (once the game is closed), and a note says so: *CrabMods updated 0.14.1 → 0.14.2*, with **What's new**. | A bar says *CrabMods 0.14.2 is available* with **Update**, **Skip this version** and **What's new**. |
| A new manager | It's downloaded and checked, and goes in when you close the manager. The next start says *CrabMods Manager updated to 0.5.0*. It never restarts while you're using it. | A bar offers **Update**, which downloads it and restarts the manager. **Skip** and **What's new** too. |

The taskbar entry flashes when it installed an update, or has a new manager ready.

- **Roll back to 0.14.0** on a mod's page puts the previous version back. The version you left is then skipped, so an automatic update doesn't bring it straight back. **Settings → Versions** lists the installed and latest versions, and undoes a skip.
- **Offline** (Settings): no update checks and no downloads at all, unless you press **Check now** or install something.
- **Play** waits for running changes to finish, then starts the game through Steam.

### Coming from manager 0.3

0.3 updates itself to 0.4 as it always did, and 0.4 keeps your setup: the mods you have, your settings and your game folder. It asks once how updates should work from now on. Until you answer, nothing updates on its own.

Managers before 0.3.0 can't update themselves: download the newest manager by hand once.

## Setup codes

The **Share** tab has your setup code: which mods are installed and every setting you changed, in one line like `CRAB2-S;M-1a2b`. Everything at its default is just `CRAB2-S;M-` and a 4-character check. A friend pastes it in **Load a code**, reviews what it installs, uninstalls and changes, and presses **Apply**.

- A damaged or cut-off code is refused.
- Parts of a code from a newer CrabMods (items or settings this version doesn't have) are skipped, and it says how many.
- Old `CRAB1-` codes from managers 0.1 and 0.2 still load.

**Reset to defaults** installs everything again with every setting at its default.

## Removing everything

**Settings → Remove all mods...** removes exactly the files the manager put into the game folder, and puts back any file it had replaced. A checkbox also deletes the mods' own settings and logs (`CrabMods.ini`, `CrabMods.log`, ...). Like every change, it is shown to you first.

## Why does my antivirus flag this?

`xinput1_3.dll`, the mod loader, works the way many malware loaders do: the game loads it as its controller DLL, and it loads the mods into the game. Some antivirus programs flag that pattern, and new files that few people have downloaded. The manager isn't code-signed.

You can check that your files are the ones built here:
- **Help** lists each installed file's SHA-256 checksum next to the one GitHub publishes for the release, and says whether they match.
- Every release has a `SHA256SUMS.txt` to compare by hand: `certutil -hashfile CrabMods.dll SHA256` on Windows, `sha256sum CrabMods.dll` on Linux.
- The source is all here, and `./build.sh` builds the same files.

The manager never runs or loads a mod DLL. What each mod contains (its items and their settings) comes from the `CrabMods.catalog.json` and `CrabSync.catalog.json` files published with each release.

## Crashes

- **While the manager is open it watches for crashes**, however the game was started. When the game crashes it saves a report, then asks whether to report it. It also offers a crash that happened while it was closed, the next time it opens (up to 3 days later).
- **Collect logs...** (Settings tab) saves the same report by hand, for a problem that isn't a crash. It includes the newest crash report if it's from the last day.

A report is a folder in `CrashReports` (see [Files](#files)). It holds a `.zip` with:
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

Use the AppImage. It finds Steam libraries under your home folder (Flatpak Steam included), the game's crash reports in its Proton prefix, and whether the game is running. Folder pickers use `zenity` or `kdialog` when you have one; otherwise type the path.

The game runs under Proton, so it needs this launch option to load the mods. The manager shows it, with a **Copy** button, until Steam has it:

```
WINEDLLOVERRIDES="xinput1_3=n,b" %command%
```

The Windows `CrabModsManager.exe` also still runs under Proton or wine, if you'd rather use that.

## Files

| | Windows | Linux |
|---|---|---|
| Settings: `CrabModsManager.ini` (the game folder, the update choices, skipped versions) | next to the exe | `~/.config/CrabModsManager/` |
| `CrabModsManager.log`: what it changed, downloaded and checked | next to the exe | `~/.config/CrabModsManager/` |
| `state/installed.json`: every file it put in the game folder, with its checksum | next to the exe | `~/.config/CrabModsManager/` |
| `state/backups/`: files it replaced, to put back | next to the exe | `~/.config/CrabModsManager/` |
| `state/seen.json`: which version each item first appeared in (for NEW) | next to the exe | `~/.config/CrabModsManager/` |
| `CrashReports/` | next to the exe | `~/.config/CrabModsManager/` |
| `cache/`: the last two versions of each mod, the catalogs and the release list | `cache\` next to the exe | `~/.cache/CrabModsManager/` |
| A downloaded manager update, until it goes in | `update\` next to the exe | `CrabModsManager-x86_64.AppImage.new` next to the AppImage |

On Linux, `$XDG_CONFIG_HOME` and `$XDG_CACHE_HOME` are used when set.

`--selftest` writes `CrabModsManager-selftest.txt` with the settings: the game folder, the releases and their checksums, installed versions, every item and setting, the setup code and the crash reports it found. It opens no window. `--selftest --report` also saves a report.

The manager talks to `api.github.com` and GitHub's download servers, nothing else. Crash reports leave your computer only if you post them.

## Source

- `manager/app.cpp`: the pages, installs, updates, setups and crash reports. The same on Windows and Linux.
- `manager/platform.hpp`: what differs between them, in `platform_win.cpp` (WinHTTP, the registry, the shell) and `platform_linux.cpp` (libcurl, XDG folders, `/proc`, `xdg-open`).
- `manager/win_main.cpp` and `manager/linux_main.cpp`: the window, with [Dear ImGui](https://github.com/ocornut/imgui) on Direct3D 11 or SDL2 with OpenGL 3.
- `manager/files.hpp`: the install record, backups, change plans and the cache.
- `manager/core.hpp`, `sha256.hpp`, `pe_version.hpp`: versions, release JSON, checksums, ini merging, the item catalog, setup codes, crash report scrubbing.

Tests:
- `tests/manager_core_test.cpp` covers the core.
- `tests/manager_app_test.cpp` drives the real pages without a window against a stand-in GitHub and game folder: first start, install, the game running, rollback, a tampered download, automatic updates, Remove all, a manager update, and a player coming from 0.3.
- `tests/platform_linux_test.cpp` covers the Linux side.

`./build.sh test` builds and runs them on Windows; `manager/linux/build.sh test appimage` builds and runs them on Linux and makes the AppImage. The font is [Roboto](https://fonts.google.com/specimen/Roboto) (Apache 2.0).
