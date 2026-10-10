# Changelog

Each mod has its own version. Releases are tagged `crabsync-vX.Y.Z` and `crabmods-vX.Y.Z`.

## CrabMods

### 0.2.0

- Renamed from Arsenal: Arsenal is now one item in `CrabMods.dll`.
- New: **Hoarder**, a Legendary perk. Damage +3% for each weapon mod and each perk level you have, Arsenal and Collector in one. It stacks with both.
- New: **Heavy Shot** (the game's own weapon mod) can drop for every weapon, not just Bounce weapons.
- Settings move to `CrabMods.ini`, with `[arsenal]`, `[hoarder]` and `[heavyShot]` sections. An existing `Arsenal.ini` is carried over on first launch.
- If the old `Arsenal.dll` is still installed, CrabMods leaves Arsenal to it and says so in the log.

### 0.1.0

First release, as Arsenal.

- **Arsenal**, an Epic weapon mod: damage +3% for each other weapon mod you have.

## CrabSync

### 1.0.1

- Fixed: returning to the lobby after a run could kill you over and over. The health pool carried the run's damage into the lobby, where your max health is lower, so the pool counted as empty.
- In the lobby the pool only shows its size: everyone's max includes the extra health for each player, everyone is kept at full health, and no damage is shared.
- CrabSync now waits a moment after you arrive in the lobby before starting the new team.
- An empty pool only kills the characters that were alive when the damage emptied it. A respawned or new character is never killed for an old empty pool; the pool restarts from it instead.
- The "could not fully apply" log line now says what was out of sync.

### 1.0.0

First release.

- Shared inventory for the whole lobby: weapon, ability and melee, every mod, perk and relic stack, enhancements and buffs, inventory slots and crystals.
- Lobby loadout picks are shared, and the team inventory is rebuilt from everyone's starting state when a run begins.
- One shared health pool: your max health plus 250 (`healthPerPlayer`) for each extra player. When it hits 0, everyone dies.
- Game-wide changes (crystals paid to everyone, slot resets at run start) are counted once, not once per player.
- Changes are buffered: a player's pickups are taken in once their inventory has held still for one check. Items given to teammates are queued and handed out one at a time.
- Host only. Clients need nothing.
- `xinput1_3.dll` mod loader that loads every other DLL in `Win64`.
- Turns itself off on an unsupported game build.

#### Known issue

- When two players pick up different items at the same moment, one item has been seen missing from the host's inventory screen. If it happens, press F10 while the item is missing and attach `CrabSync.log` to an issue.
