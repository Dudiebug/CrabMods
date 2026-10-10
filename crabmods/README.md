# CrabMods

**New items for Crab Champions.**

| Item | Type | Effect |
|---|---|---|
| **Arsenal** | Epic weapon mod | Damage increased by **3%** for each other weapon mod you have. |
| **Hoarder** | Legendary perk | Damage increased by **3%** for each weapon mod **and** each perk level you have. Arsenal and Collector in one. |
| **Heavy Shot** | the game's own Common weapon mod | Now drops for **every weapon**. The base game only offers it with Bounce weapons. |

- Arsenal and Hoarder drop from chests and rewards of their rarity, like any other item.
- Extra copies stack: two Arsenals give +6% per mod, and two Hoarder levels give +6% per mod and per perk level.
- Arsenal, Hoarder and Collector all add up. With Arsenal, Collector and Hoarder you get every bonus at once.
- The bonuses are added like the game's own damage perks, so they stack with them normally.
- Heavy Shot is unchanged (+50% damage, heavier shots that drop). It is only allowed to show up more often.

> **Status: early (0.2.0), not yet tested in-game.** Please report what you see, with `CrabMods.log` attached.

## Install

**Every player in the lobby needs CrabMods**, the host and every friend who joins.

1. Download `CrabMods-x.y.z.zip` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=CrabMods).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Copy `xinput1_3.dll` and `CrabMods.dll` there. If you already use [CrabSync](../crabsync/README.md), keep its `xinput1_3.dll` (it is the same mod loader).
5. **Had Arsenal before?** Delete `Arsenal.dll`. CrabMods includes Arsenal, and your `Arsenal.ini` settings are copied into `CrabMods.ini` on first launch.
6. **Linux / Steam Deck (Proton) only:** set the game's Launch Options to:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```

`CrabMods.ini` (settings) and `CrabMods.log` appear in the same folder on first launch.

To uninstall, delete `CrabMods.dll`, `CrabMods.ini` and `CrabMods.log` (and `xinput1_3.dll` if no other mod uses it).

## Settings

`CrabMods.ini`. Everyone in the lobby should use the same values.

| Section | Setting | Default | |
|---|---|---|---|
| `[arsenal]` | `enabled` | `true` | |
| | `damagePerModPercent` | `3` | Bonus per other weapon mod, for each Arsenal you hold. |
| | `rarity` | `epic` | `common`, `epic` or `legendary`. |
| | `countStacks` | `false` | `true` counts a x3 stack as three mods (for Arsenal and Hoarder). |
| `[hoarder]` | `enabled` | `true` | |
| | `damagePercent` | `3` | Bonus per weapon mod and per perk level, for each Hoarder level. |
| | `rarity` | `legendary` | `common`, `epic` or `legendary`. |
| `[heavyShot]` | `anyWeapon` | `true` | Let Heavy Shot drop for every weapon. Only the host's setting matters. |

## How it works

Crab Champions has no Arsenal or Hoarder asset, so the mod makes them when a level loads:
- Arsenal is a copy of Streak Shot's data asset; Hoarder is a copy of Collector's.
- Each copy gets a new name, text, rarity and an unused type, and is added to the list that chests roll from.
- The copies have fixed names, so the game sends them between players by name. That is why every player needs the mod: without it, their game cannot find the item.

Both damage bonuses are applied where the game totals a player's damage buffs, beside the game's own damage perks:
- Arsenal: your Arsenal level × your other weapon mods × 3%.
- Hoarder: your Hoarder level × (your weapon mods + your perk levels) × 3%. It counts itself, as Collector does.

Heavy Shot's asset says "only offer me with a Bounce weapon". The mod clears that flag in memory, and the game's own Heavy Shot code already works on every projectile.

## Known limitations

- Arsenal and Hoarder use the game's plain icons for their rarity (the same ones other items without their own art use).
- Every player needs CrabMods. A player without it sees an empty slot where Arsenal or Hoarder should be.
- Heavy Shot still needs to be unlocked in your save before it can drop.
- On weapons whose shots don't fly as projectiles, Heavy Shot only adds its damage.
- If the game updates, CrabMods turns itself off (see `CrabMods.log`) until a new release is out.

## Source

See [Building](../README.md#building) in the main README. The code is in `crabmods/`: `main.cpp` (hooks and the damage bonus), `items.hpp` (making items at runtime) and `config.hpp` (`CrabMods.ini`).

## License

MIT, see [LICENSE](../LICENSE). Bundles [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause).
