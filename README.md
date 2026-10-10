# Arsenal

**A new weapon mod for Crab Champions.** The more weapon mods you collect, the harder you hit.

> **Arsenal** (Epic weapon mod): Damage increased by **3%** for each other weapon mod you have.

- Drops from Epic chests and rewards, like any other weapon mod.
- Each extra Arsenal stacks the bonus: two Arsenals give +6% per mod.
- The bonus is added like the game's own damage perks, so it stacks with them normally.
- Nothing is replaced. Streak Shot and every other mod stay as they are.

> **Status: early (0.1.0), not yet tested in-game.** Please report what you see, with `Arsenal.log` attached.

## Install

**Every player in the lobby needs Arsenal**, the host and every friend who joins.

1. Download `Arsenal-x.y.z.zip` from [Releases](https://github.com/Dudiebug/CrabArsenal/releases/latest).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Copy `xinput1_3.dll` and `Arsenal.dll` there. If you already use [CrabSync](https://github.com/Dudiebug/CrabSync), keep its `xinput1_3.dll` (it is the same mod loader).
5. **Linux / Steam Deck (Proton) only:** set the game's Launch Options to:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```

`Arsenal.ini` (settings) and `Arsenal.log` appear in the same folder on first launch.

To uninstall, delete `Arsenal.dll`, `Arsenal.ini` and `Arsenal.log` (and `xinput1_3.dll` if no other mod uses it).

## Settings

`Arsenal.ini`. Everyone in the lobby should use the same values.

| Setting | Default | |
|---|---|---|
| `enabled` | `true` | |
| `damagePerModPercent` | `3` | Bonus per other weapon mod, for each Arsenal you hold. |
| `rarity` | `epic` | `common`, `epic` or `legendary`. |
| `countStacks` | `false` | `true` counts a x3 stack as three mods. |

## How it works

Crab Champions has no Arsenal asset, so the mod makes one when a level loads. It copies Streak Shot's data asset, gives the copy a new name, text, rarity and an unused weapon mod type, and adds it to the list that chests roll from. The copy has a fixed name, so the game sends it between players by name. That is why every player needs the mod: without it, their game cannot find the item.

The damage bonus is applied where the game totals a player's damage buffs. Your Arsenal level times your other weapon mods, times 3%, is added beside the game's own damage perks.

## Known limitations

- It uses the game's plain bullet icon for its rarity (the same one other Epic mods without their own art use).
- Every player needs it. A player without it sees an empty slot where Arsenal should be.
- If the game updates, Arsenal turns itself off (see `Arsenal.log`) until a new release is out.

## Building

Needs [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) (ucrt). On Linux:

```bash
./build.sh            # build/xinput1_3.dll and build/Arsenal.dll
./build.sh install    # also copy them into the Steam game folder (or set CRAB_GAME)
./build.sh package    # build/Arsenal-<version>.zip
```

## License

MIT, see [LICENSE](LICENSE). Bundles [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause).
