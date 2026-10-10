# Changelog

## 1.0.0

First release.

- Shared inventory for the whole lobby: weapon, ability and melee, every mod, perk and relic stack, enhancements and buffs, inventory slots and crystals.
- Lobby loadout picks are shared, and the team inventory is rebuilt from everyone's starting state when a run begins.
- One shared health pool: your max health plus 250 (`healthPerPlayer`) for each extra player. When it hits 0, everyone dies.
- Game-wide changes (crystals paid to everyone, slot resets at run start) are counted once, not once per player.
- Changes are buffered: a player's pickups are taken in once their inventory has held still for one check. Items given to teammates are queued and handed out one at a time.
- Host only. Clients need nothing.
- `xinput1_3.dll` mod loader that loads every other DLL in `Win64`.
- Turns itself off on an unsupported game build.

### Known issue

- When two players pick up different items at the same moment, one item has been seen missing from the host's inventory screen. If it happens, press F10 while the item is missing and attach `CrabSync.log` to an issue.
