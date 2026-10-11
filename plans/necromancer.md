# Necromancer: design

A lobby kit for CrabMods. You pick the **Soul Whip** as your weapon, and it locks your ability to **Raise Dead**. Kills fill a soul bar, and Raise Dead spends souls to bring back enemies you've killed as undead that fight for you. The whip commands them, Terraria-summoner style. Necromancy perks and items, which roll only for whip holders, grow the build over the run.

This file holds the design only. Technical notes from the earlier research are in [necromancer-tech-notes.md](necromancer-tech-notes.md). The user's rule (2026-10-11): settle the design first, and treat anything as buildable.

## Decided

### The kit
- **Soul Whip** (weapon) is picked in the lobby. Holding it **replaces your ability** with raising.
- **Raising** spends souls to raise undead at the crosshair, using one key per tier. It replaces the ability slot while you hold the whip.
- **Necromancy loot:** a pool of necromancy perks and items that roll in chests **only for players holding the Soul Whip**.

### Soul Whip
- A lash in an arc that **shoots out far on the crack**: the lash stretches to about 3× its rest length at the snap, then reels back. Long reach, and it pierces everything it sweeps.
- **Tagging:** a whip hit, on an enemy or **on the ground**, makes a fairly large invisible area around the impact. Every enemy inside it is tagged.
  - **Focus:** your undead go after tagged enemies. Once those are dead, they go back to hunting the nearest enemy.
  - **Tag damage:** undead deal bonus flat damage to tagged enemies.
  - **Bonus souls:** a kill on a tagged enemy, by you or your undead, gives +50% souls.
  - **Undead buff:** whip hits briefly buff your undead (e.g. attack or move speed).

### Souls
- A soul bar, like mana. **Base max 100.** Kills fill it up to the max.
- Everything is set relative to the base max: one ini number scales the whole economy.
- **The max grows** only through necromancy perks and items. Costs stay pinned to the *base* max, so upgrades make you stronger instead of rescaling everything.
- **Co-op:** each player gets souls only from their own kills and their own undead's kills.
- **Undead kills give full souls** to their owner.

Souls per kill, by the enemy's own game rank:

| Rank | Souls |
|---|---|
| Easy | 1 |
| Medium | 3 |
| Hard | 6 |
| Elite | 25 |
| Boss | 60 |
| Tagged kill | +50% |

### Raising
- **Roster:** you can raise only kinds of enemies you've killed this run.
- **Controls:** each tier has **its own raise key**. Pressing it raises that tier at the crosshair.
  - Defaults: fodder **V**, medium **C**, hard **X**, elite **Z**.
  - Each key is a setting in the kit's `CrabMods.ini` section (`raiseKeyFodder = V`, `raiseKeyMedium = C`, `raiseKeyHard = X`, `raiseKeyElite = Z`), so it also shows in CrabModsManager. Values are key names: a letter or digit, `F1`–`F12`, `Mouse4`/`Mouse5`, or names like `Tab`, `Shift`, `Ctrl`, `Alt`. The raise panel draws whatever key is set.
  - Key settings are per player: the host's settings sync must not overwrite them.
  - A tier raises the kind you killed most recently in it. Its slot shows which kind that is.
  - The Soul Whip takes the ability's place: the HUD's ability widget is replaced by the **raise panel**, with one slot per tier showing the key, the icon, the soul cost, and whether you can afford it.

| Raise | Cost |
|---|---|
| Elite (weakened, about half power) | 100 (a full starting bar) |
| Hard (big crabs and skulls) | 40 |
| Medium | 15 |
| Fodder pack of 3 | 5 |

### Undead
- They are freshly spawned enemies of the chosen kind. No ragdolls or corpses.
- They last **until killed** or until the island ends.
- **Behaviour:** they hunt the nearest enemy freely, unless the whip has tagged targets.
- **Aggro:** enemies attack undead that are closer to them than any player.
- **Power:** undead **use your stats**. Your damage perks, crit and elemental mods boost them, so your build matters for the army.
- **Upkeep:**
  - Only while enemies are alive, each undead drains 0.25% of its raise cost per second (elite 0.25 per second, about 400 seconds of fighting to eat its own cost).
  - When the bar hits 0, **the weakest undead crumbles first**.
  - Undead kills feed the bar, so a winning army sustains itself and a losing one starves.

### Expected pace
This comes from the game's spawn data, solo, loop 1:
- Your own kills give about 45 souls per combat island, plus your undead's kills.
- The first elite comes after about 2 islands of saving.
- About 1,550 kills happen by island 50, and loop 2 and later spawn 1.75 times as many.

### Art
- The Blender source is `art/necromancer/necromancer.blend`, with renders in `art/necromancer/renders/`.
- **Soul Whip:** a dark braided lash with a soul thread whose glow travels along it, bone spikes, and a three-talon claw holding a soul flame at the tip.
  - Handle: the grip glows between leather straps, with a ribcage guard and a crystal pommel.
  - Rig: a 12-bone chain.
  - Animations: Idle, Crack (the lash stretches to 3× at the snap) and Equip.
- **Effects** (all animated):
  - tag ring (the whip's impact area);
  - summon (rune circle, bone spikes, pillar, spiral ribbons, embers);
  - soul wisp;
  - tag marker over tagged enemies;
  - undead halo under your undead.
- **HUD:** built in code from the game's own widgets, like the rest of CrabMods: the key totem menu uses the game's confirmation prompt, its pickup cards (`InteractUI_SetIcon`), the game's font and tinted brushes, and items use the game's generic `T_Icon_<Kind>_<Rarity>` icons.
  - **Soul bar** and **raise panel** (in the ability's spot) use custom art from the .blend, shipped in the same mod pak as the whip mesh and effects.

## Open
- **Necromancy loot:** the draft pool of 51 items (whip mods, raise mods, claw mods, perks and relics) is in [necromancer-loot.md](necromancer-loot.md). It needs the user's cut.
- **Undead power:** how much of your stats undead get, and the exact elite weakening.
- **Soul Whip numbers:** range, arc, fire rate, damage, tag radius and how long a tag lasts, tag damage, and the buff.
- **How undead are marked:** look (tint, effects), sounds, and the HUD for the soul bar and the selected raise.
- **What happens when you swap the whip away mid-run:** the ability, the undead, the souls.
- **Edge cases:** bosses (never raised), splits (no souls), Horde and Arcade islands, death and revive with an army out.
