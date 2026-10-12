# Shellbreaker: design

Status: proposal, 2026-10-11. Nothing is built, and the numbers haven't been through `plans/balance/WORKFLOW.md`.

Shellbreaker is a melee-primary combat system for CrabMods. It isn't one fixed kit: **you build your combo out of loot over the run**, the same way the rest of Crab Champions works. The lobby weapon decides the *body* (a Frame), and Technique drops decide the *moves*. Movement and variety feed a Flow meter. Enemies have shells that melee breaks open.

## Pillar 1: Frames (the lobby weapon)

A Frame sets speed, reach, weight and one signature trait. It does **not** decide your moves.

| Frame | Feel | Signature |
|---|---|---|
| **Blade** | fast, 3 m | Hits that land in rhythm with your swing speed get faster |
| **Maul** | slow, 4 m, huge hits | Every hit staggers |
| **Reach** (spear or chain) | medium, 6 m | Tip hits do more damage |
| **Tether** (glaive or flail) | thrown, about 12 m | It comes back, so you can hit on the way out and the way back |

Every Technique works with every Frame and changes to suit it. For example, Lunge is:
- **Blade:** a quick stab.
- **Maul:** a body slam.
- **Reach:** a flying javelin.
- **Tether:** a throw, then pulling yourself along to it.

## Pillar 2: Techniques (moves are loot)

The M1 string has **slots**, and Technique drops fill them. They roll from chests the same way weapon mods do.

| Slot | When it fires | Example Techniques |
|---|---|---|
| **Opener** | first hit | Lunge, Hook Pull, Leap Strike |
| **Chain** ×2 | middle hits | Cleave (wide), Flurry (3 fast hits), Launcher (knocks up) |
| **Finisher** | last hit | Quake, Execute, Shockwave, Detonate |
| **Aerial** | M1 in the air | Meteor, Air Spin, Dive Kick |
| **Charged** | hold M1 | Pierce, Whirlwind, Big Throw |
| **Melee key** | the old melee button | Parry, Grapple, Dash Strike, Counter |

- Every run starts with a basic string. By island 30 the string is the player's own, and no two runs play the same.
- **Duplicate drops level a Technique up**, the same as charms do now (merging levels on the host).
- **Swapping:** picking up a new Technique asks which slot it replaces. This reuses the key totem / inventory UI.

## Pillar 3: Flow (the meter, built from movement and variety)

Crab is a movement game, so movement should feed damage.

- **Gaining Flow:** hits while **in the air or dashing**, using a **different Technique** from the last one, and parries.
- **Losing Flow:** getting hit, or spamming the same move. Doing nothing slowly drains it.
- **Tiers:** Ripple → Wave → Surge → **Riptide**. Each tier adds damage. At Riptide, finishers **echo**, meaning they hit twice.

This pushes players to play stylishly without hard-coded combos.

## Pillar 4: Shell Break (enemies)

- Every enemy has a **shell bar**. Melee hits crack it, finishers crack it a lot, and the right element cracks it even more.
- When the shell breaks, the enemy is **exposed** for 3 s: it's stunned and takes +50% damage.
- During that window, the next finisher becomes a **Shell Crack** execute: a big hit, a burst of Flow, and loot shards.
- On bosses, the shell becomes **break phases** instead of a stun-lock.

This gives melee a job that guns don't have: you break things open.

## How a fight plays

1. Dash in with Lunge, then Cleave, Flurry, and Quake as the finisher.
2. A shell breaks, so jump, Meteor onto the exposed enemy, and Shell Crack it.
3. That's enough Flow for Riptide.
4. A sniper is shooting you, so Grapple to it with the melee key, and the echoed finisher deletes it.

## How it fits what CrabMods already has

- **Weapon mods still count:** every hit fires an invisible hit projectile, the same trick as the Soul Whip bolt. Elements, on-hit effects and the weapon damage pool all still apply. Elements also crack shells.
- **The ability slot stays vanilla.** The Link ability mods in `melee-ability-rework.md` can interact with Flow and shells.
- **Loot:** Techniques are a new item category. They only roll for players holding a Frame, the same as necro items for whip holders. Existing perks can get melee versions.
- **Multiplayer:** the host owns shell bars and breaks. One player breaks the shell and another cashes in the Shell Crack, so co-op combos come naturally.
- **Reuse:** the necro HUD pattern (meter bar plus custom panel), `necro_lash.hpp` for chain or tether physics, and the Blender pipeline from the whip.

## Cost and scope

- The full version is 4 Frames × about 15 Techniques.
- Each Technique needs its own animation per Frame. That is the expensive part.
- **Cheaper first version:** 1–2 Frames and about 10 Techniques.

## Open questions

1. **Loot vs. fixed kit:** Techniques as loot, or a fixed kit per weapon (like the earlier Chainfall idea: sword and mace on a chain)?
2. **Flow:** only damage scaling, or should Riptide also unlock a mode?
3. **Shell Break:** does it apply to all damage (guns too), or only melee? Melee-only gives melee a clear reason to exist.
4. **RE:** can we move enemies cleanly (Hook Pull, Launcher)? This isn't confirmed. Launch velocity is the first thing to try.
5. **Scaling:** trace whether melee damage goes through the perk pool and crit before tuning (see `melee-ability-rework.md`).
