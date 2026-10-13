---
name: new-item
description: Design a new CrabMods perk, weapon/ability/melee mod, relic, charm or tweak, or rebalance an existing one, against the vanilla baseline. Use when the user pitches a new item idea or asks whether an item is balanced.
---

Follow `plans/balance/WORKFLOW.md` step by step. It depends on `plans/balance/vanilla-power.md`, `vanilla-difficulty.md` and `audit.md`, and on the raw data in `re/pickup_catalog.json` and `re/perk_sites.json`.

1. Run steps 1–5 yourself. Pull the nearest vanilla items from `re/pickup_catalog.json`, not from memory, and show the mid-run arithmetic.
2. If a step fails, say which one and propose the fix (a different rarity, a cap, island scope, a lower rate). Don't quietly change the user's idea.
3. Put the finished card in `plans/ideas.md` using the template in WORKFLOW.md, then give the user a short summary: how it differs from vanilla, its rarity, where it lands in the line-up of vanilla items from step 3, and any step 4 "yes" answers with their fixes.
4. Only build it (step 6) when the user asks. Every number on the card becomes a `CrabMods.ini` key.
