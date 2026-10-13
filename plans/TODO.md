# TODO

## Run the new items through the balance workflow

None of these went through `plans/balance/WORKFLOW.md` (the new-item skill) before release. Run each one through steps 1–5, put its card in `plans/ideas.md`, and fix the numbers in code and the ini defaults where a step fails.

- [ ] Shellbreaker itself: 75 DPS, area swings (4.5 m radius, 22 m out; circle 24 m), melee mods at `meleeModShare` 0.25 and dropping ×3 (`meleeModWeight`), and the blade-only vanilla weapon-mod whitelist (`kBladeMods` in `crabmods/items/shellbreaker.cpp`).
- [ ] The 7 weapon mods new in 0.14.1: Serrated Links, Venom Barbs, Static Chain, Rime Edge, Hex Links, Momentum, Backlash (`crabmods/items/shell_loot.cpp`).
- [ ] The 12 Shellbreaker items from 0.14.0 (its card in `ideas.md` says "BUILT (beta, untested)": check it covers all of them).
- [ ] Snap Decision and Wild Card (both "BUILT (beta, untested)" in `ideas.md`).
- [ ] Name clash: the new Momentum weapon mod has the same name as the Momentum perk idea in `ideas.md`. Rename one.

## A new, less invasive mod manager

CrabModsManager (0.3.0) does too much to the player's setup. Build a new one that is less invasive. To decide before starting:

- What "invasive" means today: list what the current manager touches (the game folder, the loader `xinput1_3.dll`, `CrabMods.ini`, auto/self-updates, crash reports, CRAB2 setup codes) and what it changes without asking.
- What the new one does by default: as few files as possible, nothing changed without the player's say, easy to undo (an uninstall that puts back exactly what was there).
- Whether it replaces CrabModsManager or ships next to it, and how existing installs move over.
