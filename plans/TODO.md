# TODO

## Run the new items through the balance workflow

None of these went through `plans/balance/WORKFLOW.md` (the new-item skill) before release. Run each one through steps 1–6, put its card in `plans/ideas.md`, and fix the numbers in code and the ini defaults where a step fails.

- [ ] Shellbreaker itself: 75 DPS, area swings (4.5 m radius, 22 m out; circle 24 m), melee mods at `meleeModShare` 0.25 and dropping ×3 (`meleeModWeight`), and the blade-only vanilla weapon-mod whitelist (`kBladeMods` in `crabmods/items/shellbreaker.cpp`).
- [ ] The 7 weapon mods new in 0.14.1: Serrated Links, Venom Barbs, Static Chain, Rime Edge, Hex Links, Momentum, Backlash (`crabmods/items/shell_loot.cpp`).
- [ ] The 12 Shellbreaker items from 0.14.0 (its card in `ideas.md` says "BUILT (beta, untested)": check it covers all of them).
- [ ] Snap Decision and Wild Card (both "BUILT (beta, untested)" in `ideas.md`).
- [ ] Name clash: the new Momentum weapon mod has the same name as the Momentum perk idea in `ideas.md`. Rename one.

## CrabMods Manager 0.4.0 release

Built: asks before installing, change review, opt-in updates, checksums, catalogs, rollback, Remove all, sorting, the Linux AppImage (see the CHANGELOG entry).

- [x] Upload `CrabMods.catalog.json`, `CrabSync.catalog.json` and `SHA256SUMS.txt` to `crabmods-v0.14.1` and `crabsync-v1.0.8`, so 0.4.0 can list the items of the current releases. `--selftest` says "published" for both once done.
- [x] Publish `manager-v0.4.0` with `CrabModsManager.exe` and `SHA256SUMS.txt`; the workflow adds the AppImage.
- [ ] Try the AppImage on a Steam Deck: install, the launch option hint, an update while the game runs.
