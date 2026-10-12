# Greed Run: design

A new game mode for CrabMods. A **Greed Run portal** sits in the lobby, opposite the Arcade and Holdout portals. Taking it starts a **normal run** (same islands, same difficulty) where **everything is Greed**. Every chest is a Greed chest, and every item offered anywhere (chests, totems, shop, shrines) is a Greed item.

User's decisions (2026-10-11): everything is Greed, with **real trade-offs** (most pickups are worth taking, but sometimes you skip one). Players **can skip chests**. Greed versions exist **only in this mode**: never in normal runs, the key totem menu or the catalog.

## Greed versions of every item

The game has only a handful of Greed items, so the mode makes a Greed version of every item, vanilla and CrabMods alike.

**Automatic Greed version** (every item that has no hand-made one):
- **Upside:** a Greed pickup comes at **one level higher** than it would normally (a level 1 drop is level 2). Level is what every item's effect counts, so this works the same for vanilla and CrabMods items.
- **Curse:** each level you hold carries a curse picked by the item's original loot pool:

| Item's pool | Curse per level held |
|---|---|
| Damage | −4% max health |
| Critical | +4% damage taken |
| Elemental | −3% fire rate |
| Speed | +4% damage taken |
| Luck | −3% damage |
| Health | −3% damage |
| Economy | lose 5 crystals each time you're hit |
| Skill | +5% ability cooldown |
| Relic, Upgrade, anything else | −3% max health |

- The item's text in the game gets a Greed line ("Greed: −4% max health per level") so you can see the price before you pick it up.
- Curses of the same kind add up. Every curse has a floor: max health ≥ 10%, damage ≥ 10%, fire rate ≥ 10%, so a run can get very hard but never breaks.

**Hand-made Greed versions** replace the automatic one for an item with their own twist. They come in batches over later releases. First ideas:
- **Glass Cannon:** +2 levels instead of +1, but your max health is 1 until the island ends.
- **Banana:** gives no max health; heals you fully at every island start instead.
- **Collector:** counts double, but every pickup costs 10 crystals.
- **Money Is Power:** doubled, but you lose 1% of your crystals per second in fights.

**Vanilla Greed items** and CrabMods' own Greed items keep their own effects and get no extra curse.

## Chests

- Every chest (island reward chests, bonus chests, Care Package, totem chests, key chests) is a Greed chest, and every item in it is a Greed version.
- **Skipping:** you can always leave a chest unopened or walk away from its ring of pickups. If the game ever forces a pick, the mode adds a way out.
- Shops, shrines and totems offer Greed versions too.

## The portal

- Spawned by the host in the lobby, opposite the minigame portals, labelled **Greed Run**, in Greed colours.
- Taking it starts a normal run with the mode on. Coming back to the lobby turns it off.
- The mode flag is sent to every player's CrabMods, since every machine shows the Greed item text.
- A Greed Run continued from the main menu stays a Greed Run (the flag is saved with the run).

## Built (first version, `crabmods/items/greed_run.cpp`, `[greedRun]`)

- **Portal:** after the lobby's own portals spawn, the host spawns a copy of `BP_Portal_NewRun` at (-5757, 5000, 1000), the mirror of the minigame portals, and gives it the Greed reward. That reward replicates, so every player sees it in Greed colours with the Greed icon. On machines with CrabMods it's renamed "Greed Run".
- **Starting:** `ACrabGM_EnterChosenPortal`. The Greed Run portal turns the mode on, the normal New Run portal turns it off, and Continue Run reads `GreedRun.state`. The host tells each player's CrabMods.
- **Items keep their real rarity**, because rewriting every item to Greed rarity breaks the game's rolls, the Random totem and the achievements. Reward chests become Greed chests instead. A Greed chest's roll becomes Random pool at Common, which keeps the usual Epic and Legendary odds; the game's Greed items are lowered to Common for that roll. It also gets +2 choices.
- **Greed version** = +1 level when the pickup appears (host, at the card), plus a curse per level by the item's original pool. Cards and item texts show it on every machine. Dropped items don't get the extra level again.
- The first island's reward is Greed (the portal's reward carries over). Fine for this mode.

## Next

- Hand-made Greed versions (Glass Cannon, Banana, Collector, Money Is Power ...).
- A "GREED RUN" sign over the portal like the "MINIGAMES" one.
- In-game checks: the portal's spot (on the ground?), the shop's pickups get the level, the Random totem.
