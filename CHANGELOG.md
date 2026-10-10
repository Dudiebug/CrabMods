# Changelog

Each mod has its own version. Releases are tagged `crabsync-vX.Y.Z`, `crabmods-vX.Y.Z` and `manager-vX.Y.Z`.

## CrabMods

### 0.3.0

16 new items. All percentages are per level, kept low because levels stack.

- Perks:
  - **Minimalist** (Epic): +5% damage per empty weapon mod slot.
  - **Underdog** (Epic): +0.5% damage per 1% of health missing.
  - **Untouchable** (Epic): +1% damage per second without being hit, up to +20%.
  - **Jack of All Trades** (Legendary): +3% damage per different pickup tag you have.
  - **Greed Is Power** (Greed): +5% damage per Greed perk level.
- Relics:
  - **Purist** (Epic): +10% damage while all your elemental pickups are one element.
  - **Martyr** (Epic): while you are down, each teammate deals +15% damage.
  - **Last Crab Standing** (Epic): +25% damage per downed teammate, +25% more when you are the last one up. It never drops when you play alone.
  - **Double or Nothing** (Greed): every chest you open offers twice as many choices or nothing at all. It applies after every other chest perk and relic.
  - **Fire, Ice, Lightning, Poison, Critical, Damage and Luck Specialist** (Epic): pickups of that kind are offered 3× as often. Damage and Luck Specialist also make those islands 3× as likely.
- Ability mod: **Recharge** (Epic): each kill takes 0.1 s off your ability cooldown.
- **The host's settings are used by everyone** in multiplayer. They are sent to each player who joins and used until they leave; your own `CrabMods.ini` is not changed.
- New items' sections are added to an existing `CrabMods.ini`, keeping your values.
- With every item switched off, CrabMods still loads, so a host's settings can apply.
- The DLLs now carry a version number (file properties), which the manager reads.


### 0.2.0

- Renamed from Arsenal: Arsenal is now one item in `CrabMods.dll`.
- New: **Hoarder**, a Legendary perk. Damage +3% for each weapon mod and each perk level you have, Arsenal and Collector in one. It stacks with both.
- New: **Heavy Shot** (the game's own weapon mod) no longer needs Bouncing Shot to drop. It works with every weapon.
- Settings move to `CrabMods.ini`, with `[arsenal]`, `[hoarder]` and `[heavyShot]` sections. An existing `Arsenal.ini` is carried over on first launch.
- If the old `Arsenal.dll` is still installed, CrabMods leaves Arsenal to it and says so in the log.

### 0.1.0

First release, as Arsenal.

- **Arsenal**, an Epic weapon mod: damage +3% for each other weapon mod you have.

## CrabMods Manager

### 0.1.0

First release.

- Finds the game in every Steam library (also under Proton/wine and Flatpak Steam), or lets you browse to it.
- Installs, updates, switches on/off and removes CrabMods and CrabSync from the latest GitHub release.
- Edits each mod's settings.
- Shares settings as `CRAB1-` codes: copy, then paste into another manager.
- `--selftest` writes what it found to a text file, without a window.

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
