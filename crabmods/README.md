# CrabMods

**New items for Crab Champions.** 19 items: perks, relics, a weapon mod and an ability mod. Each one drops from chests and rewards like the game's own.

> **Status: early (0.3.0), not yet tested in-game.** Please report what you see, with `CrabMods.log` attached.

## Items

### Damage

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Arsenal** | weapon mod | Epic | +3% damage for each other weapon mod you have. |
| **Hoarder** | perk | Legendary | +3% damage for each weapon mod **and** each perk level you have. Arsenal and Collector in one. |
| **Minimalist** | perk | Epic | +5% damage for each **empty** weapon mod slot. |
| **Underdog** | perk | Epic | +0.5% damage for every 1% of health you are missing (+25% at half health). |
| **Untouchable** | perk | Epic | +1% damage for each second without taking damage, up to +20%. Getting hit resets it. |
| **Jack of All Trades** | perk | Legendary | +3% damage for each **different** pickup tag you have (Fire, Ice, Critical, Bounce, Speed, Healing, ...). |
| **Greed Is Power** | Greed perk | Greed | +5% damage for each Greed perk level you have, this one included. Collector for Greed perks. |
| **Purist** | relic | Epic | +10% damage while every elemental pickup you hold is the same element. |
| **Martyr** | relic | Epic | While you are down, each teammate deals +15% damage. |
| **Last Crab Standing** | relic | Epic | +25% damage for every downed teammate, and another +25% when you are the only one still up. Never drops when you play alone. |

### Loot

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Fire / Ice / Lightning / Poison / Critical Specialist** | relics | Epic | Pickups with that tag are offered **3×** as often to you. |
| **Damage / Luck Specialist** | relics | Epic | Damage (or Luck) pickups are offered 3× as often to you, and Damage (or Luck) islands 3× as often to the team. |
| **Double or Nothing** | Greed relic | Greed | Every chest you open offers **twice as many** choices, or **nothing at all** (50/50). It applies after every other perk and relic that changes the number of choices. |
| **Heavy Shot** | the game's own weapon mod | Common | Drops **without Bouncing Shot**, for every weapon. |

### Ability

| Item | Type | Rarity | Effect |
|---|---|---|---|
| **Recharge** | ability mod | Epic | Each kill takes 0.1 s off your ability cooldown. |

- Extra copies stack: each level of an item adds its bonus again (two Minimalists give +10% per empty slot).
- Every damage bonus is added like the game's own damage perks, so they all stack with each other and with the game's items.
- The percentages are kept low on purpose, because levels stack.
- Specialists multiply the weight the game already gives each pickup, so a rare pickup stays rarer than a common one. Several Specialists that boost the same pickup multiply.

## Install

**Every player in the lobby needs CrabMods**, the host and every friend who joins.

The easy way is the [CrabMods Manager](../manager/README.md): it finds the game, installs and updates the mods, and edits their settings.

By hand:

1. Download `CrabMods-x.y.z.zip` from [Releases](https://github.com/Dudiebug/CrabMods/releases?q=CrabMods).
2. Open the game folder: in Steam, right-click **Crab Champions** → **Manage** → **Browse local files**.
3. Go into `CrabChampions\Binaries\Win64` (the folder with `CrabChampions-Win64-Shipping.exe`).
4. Copy `xinput1_3.dll` and `CrabMods.dll` there. If you already use [CrabSync](../crabsync/README.md), keep its `xinput1_3.dll` (it is the same mod loader).
5. **Had Arsenal before?** Delete `Arsenal.dll`. CrabMods includes Arsenal, and your `Arsenal.ini` settings are copied into `CrabMods.ini` on first launch.
6. **Linux / Steam Deck (Proton) only:** set the game's Launch Options to:
   ```
   WINEDLLOVERRIDES="xinput1_3=n,b" %command%
   ```

`CrabMods.ini` (settings) and `CrabMods.log` appear in the same folder on first launch. When an update adds items, their sections are added to the end of your `CrabMods.ini`; your own values are kept.

To uninstall, delete `CrabMods.dll`, `CrabMods.ini` and `CrabMods.log` (and `xinput1_3.dll` if no other mod uses it).

## Settings

`CrabMods.ini` has one `[section]` per item. Every item has `enabled` and `rarity` (`common`, `epic`, `legendary` or `greed`: which chests it drops from), plus:

| Section | Setting | Default | |
|---|---|---|---|
| `[arsenal]` | `damagePerModPercent` | `3` | Per other weapon mod. |
| | `countStacks` | `false` | `true` counts a x3 stack as three mods (for Arsenal and Hoarder). |
| `[hoarder]` | `damagePercent` | `3` | Per weapon mod and per perk level. |
| `[heavyShot]` | `anyWeapon` | `true` | Let Heavy Shot drop for every weapon. |
| `[minimalist]` | `damagePercent` | `5` | Per empty weapon mod slot. |
| `[underdog]` | `damagePercent` | `0.5` | Per 1% of health missing. |
| `[untouchable]` | `damagePercent` | `1` | Per second without being hit. |
| | `maxPercent` | `20` | The most it builds up to. |
| `[jackOfAllTrades]` | `damagePercent` | `3` | Per different pickup tag. |
| `[greedIsPower]` | `damagePercent` | `5` | Per Greed perk level. |
| `[purist]` | `damagePercent` | `10` | |
| `[martyr]` | `damagePercent` | `15` | For each teammate, while you are down. |
| `[lastCrabStanding]` | `perDownedPercent` | `25` | Per downed teammate. |
| | `lastStandingPercent` | `25` | Extra when you are the only one up. |
| `[doubleOrNothing]` | `doubleChancePercent` | `50` | The chance of twice as many choices; otherwise the chest is empty. |
| `[recharge]` | `secondsPerKill` | `0.1` | |
| `[fireSpecialist]` ... `[luckSpecialist]` | `multiplier` | `3` | How many times as likely. |

All percentages and amounts are per level of the item.

### Multiplayer: the host's settings

**In multiplayer everyone plays with the host's settings.** When you join, the host's CrabMods sends its `CrabMods.ini` settings to you, and your game uses them until you leave. Your log says `using the host's settings`, followed by every item's values. When you host again, or play alone, your own settings are back. Your `CrabMods.ini` file is never changed.

To share settings outside a game, use **Copy share code** and **Paste share code** in the [Manager](../manager/README.md).

## How it works

Crab Champions has none of these assets, so CrabMods makes them when a level loads:
- Each new item is a copy of a similar item's data asset (Arsenal of Streak Shot, Hoarder of Collector, Recharge of Heat Sink, the relics of rings, ...). The copy gets a new name, text, rarity and an unused type, and is added to the list that chests roll from.
- The copies have fixed names, so the game sends them between players by name. That is why every player needs the mod: without it, their game cannot find the item.

The effects:
- **Damage bonuses** are added where the game totals a player's damage buffs, beside its own damage perks.
- **Specialists** change each pickup's spawn weight during every loot roll made for the holder, then put the weights back. Damage and Luck Specialist also re-pick each island's reward with those pools' weights tripled.
- **Double or Nothing** changes the number of choices the chest has already worked out from every other perk and relic, just before the chest rolls its loot.
- **Recharge** reads each player's kill counter (which the game sends to everyone) whenever the ability cooldown ticks, and takes the kills since the last tick off the cooldown.
- **Heavy Shot**'s asset says "only offer me to a player who holds Bouncing Shot". CrabMods clears that flag in memory. Heavy Shot's effect was never tied to a weapon, so nothing else needs changing:
  - The game applies Heavy Shot to every projectile: gravity = (the bullet's own gravity + 2) × (1 + its 15% weight), plus its +50% damage.
  - So even bullets that normally fly dead straight (most guns) start to drop.
- **Host settings** go out over the game's own `ClientMessage` call (the one that prints to the console), sent to each player as they join. CrabMods picks it up before it reaches the console.

## Known limitations

- The new items use the game's plain icons for their rarity (the same ones other items without their own art use).
- Every player needs CrabMods. A player without it sees an empty slot where a CrabMods item should be.
- Heavy Shot still needs to be unlocked in your save before it can drop.
- On very fast guns (Sniper, Marksman Rifle, Laser Cannons) the bullet reaches its target before it can drop much, so Heavy Shot is mostly its damage there.
- Recharge only counts kills made while your ability is recharging, so kills while it is ready are not saved up.
- If the game updates, CrabMods turns itself off (see `CrabMods.log`) until a new release is out.

## Source

See [Building](../README.md#building) in the main README. The code is in `crabmods/`:
- `main.cpp`: the hooks, which pass each game event to every item.
- `item.hpp`: the item base classes; `items/*.cpp`: one file per item.
- `world.hpp`: reading players, inventories and health; `config.hpp`: `CrabMods.ini`.

## License

MIT, see [LICENSE](../LICENSE). Bundles [MinHook](https://github.com/TsudaKageyu/minhook) (BSD 2-Clause).
