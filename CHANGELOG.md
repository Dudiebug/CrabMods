# Changelog

Each mod has its own version. Releases are tagged `crabsync-vX.Y.Z`, `crabmods-vX.Y.Z` and `manager-vX.Y.Z`.

## CrabMods

### Unreleased

- **New defaults now reach your `CrabMods.ini`.** Settings you never changed take the new default when an update changes it, including every change below. Settings you changed stay as you set them. The file as it was is saved as `CrabMods.old.ini`, and `CrabMods.log` lists what moved.
- **Balance pass** over every CrabMods item, against the game's own items.
  - **Shellbreaker's own damage mods multiply on top of everything else**, so they never got weaker as a build grew. They're smaller now: Serrated Links +10% (was +50%), Executioner +55% (was +75%), Momentum +8% a swing (was +15%).
  - **Undertow +300%** (was +75%). The hook's hit was too small for +75% to matter.
  - **Swing elements by rarity:** Venom Barbs (Common) 2 poison (was 3), Searing Edge and Static Chain (Epic) 3 fire / 3 lightning (were 2). Rime Edge stays at 3 ice, and Hex Links at 4 arcane.
  - **Grappler is Epic** (was Legendary): one more hook charge isn't a Legendary.
  - **Soul Siphon gives a quarter of a soul** per enemy hit (was 1). It still out-earned kills several times over.
  - **Soul Whip does 80 damage a second** (was 100). It hits an area, at range, and never reloads.
  - **Death Mark +15%** (was +10%).
  - **Underdog +1% per 1% of health missing** (was +0.5%), so +50% at half health.
  - **Untouchable +2% a second, up to +40%** (was +1%, up to +20%).
  - **Martyr never drops when you play alone**, where nobody can revive you.

### 0.14.1

- **Shellbreaker hits at range, without the white flash.** The swings now deal their damage as a melee strike where the blade lands: a 4.5 m radius, 22 m in front of you (the circle hits everything within 24 m). Before, damage only landed when you stood on top of an enemy, and each swing's burst whited out the screen. The strike breaks shields and everything a claw breaks.
- **Shellbreaker does 75 damage a second** (was 100), since every swing now hits an area.
- **Melee mods power Shellbreaker** at a quarter of their worth: damage, Big Claws' range, the Claws' elements and Vampire. They drop 3x as often while you hold it.
- **No more bullet weapon mods for Shellbreaker.** Holders only get the game's weapon mods that work for a blade: Wind Up, Aerial Shot, Uppercut, Heavy Hitter, Rapid Fire, Escalating Shot, Damage Shot, Supercharged and Juiced.
- **7 new Shellbreaker weapon mods:** Serrated Links (+50% damage), Venom Barbs (poison), Static Chain (lightning), Rime Edge (ice), Hex Links (arcane), Momentum (each swing in the string hits harder) and Backlash (sweeps also strike behind you).
- New settings in `[shellbreaker]`: `meleeModShare` (0.25) and `meleeModWeight` (3). An existing `CrabMods.ini` keeps its old `dps`, `reach`, `hitRadius` and `circleRadius` values; set them to 75, 2200, 450 and 2400 for the new tuning.

### 0.14.0

- **New weapon: Shellbreaker.** A barbed blade on a chain, on its own pedestal in the lobby (left of the Soul Whip) and in the weapon list. Left click swings a string of five (two sweeps, a chop, the rebound, a circle over the head) that hits as far as the Katana's melee; right click aims a hook that reaches 60 m, yanks small enemies to you, pulls you to big ones and lets you swing from walls and floors. It does 100 damage a second to start, and damage perks, weapon mods and fire rate grow it like any gun. It replaces your melee while you hold it. The crab plays the game's Katana, Hammer and grenade-throw animations with it, and the chain swings with real physics. Settings are in `[shellbreaker]`.
- **12 Shellbreaker items** that only drop while you hold it: Long Chain, Wide Arc, Whirlwind, Executioner and Searing Edge (weapon mods), Riptide, Quick Draw, Undertow and Rhythm (perks), and Grappler, Kraken's Grasp and Frost Chain (relics).
- **Snap Decision** (Greed perk): once each biome, a coin flip. Half your mods and perks double in level, or half of them are destroyed.
- **Wild Card** (Greed relic): each island you clear, a 20% chance your whole inventory is randomized like a Randomize totem, keeping each slot's kind and level.
- **Greed Run: every perk is a big gamble with a real price.** A Greed perk comes 2 levels higher, so it's 3x as strong. Every level you hold also makes the run harder, in a way that more items can't buy back. The curses are the game's own Greed perk rules (Juggernaut's extra enemies, Double Trouble's enemy health) or costs that only grow, never stats your other items would add back:
  - Damage perks (Damage Combo, Eagle Eye, Tasty Orange, the auras' kill effects ...): +10% more enemies spawn per level, so a level 3 perk is +30% enemies.
  - Health and Critical perks (Mango, Banana, Fortitude, Sharpshooter, Mega Crit ...): enemies get +15% health per level.
  - Speed and close-range perks (Speed Demon, Danger Close, the auras, Silver Lining): enemies hit you 12% harder per level.
  - Luck perks (Driller, Streamer Loot, Double Vision, Faulty Chests ...): better loot, but enemies get +15% health per level.
  - Economy perks: you lose 3% of your crystals each time you're hit, per level. A few more cost healing or ability cooldown.

  More enemies and tougher enemies add up over the whole team, like the game's Greed perks. Mods come 1 level higher with smaller curses of the same kinds. Every item's text shows its price before you pick it up. Settings: `perkExtraLevels` (2), `perkCurseScale` and `modCurseScale` (100%). The old per-curse settings (`maxHealthPerLevel` ...) are gone and do nothing if they're still in your `CrabMods.ini`.

### 0.13.4

- **Big Claws and Blender have caps.** Big Claws gives +3% melee range a level, up to +35% (it was +40% a level with no cap). Blender takes 15% off the melee cooldown a level, up to 75% (it was 30% a level, so level 4 had no cooldown at all). The inventory shows what each one gives you now. The new settings are `bigClawsPerLevel`, `bigClawsMax`, `blenderPerLevel` and `blenderMax`. An existing `CrabMods.ini` keeps its old `bigClawsPercent` and `blenderPercent` lines, which now do nothing.
- **Fixed the raise panel's blank white squares.** 0.13.2 and 0.13.3 shipped without the four enemy pictures, so the panel showed white squares with the tier names jammed into the key labels. The pictures are back, and each tier's name now sits in small letters under its key and cost.
- **Every whip shot cracks.** The crack plays when the simulated tip breaks the sound barrier. A stroke whose tip never got quite fast enough, or that the next shot cut off, used to stay silent. Now it cracks at its fastest point. The first 20 strokes of each session go to `CrabMods.log` with the tip's speed, to track down the rest.

### 0.13.3

- **Souls fly to you.** When you or your undead kill an enemy, its soul streaks from the body to you as a glowing violet wisp with a tail, and the pickup chime plays as it lands. Before, the soul bar just went up. The soul that fills your bar plays the full chime when it arrives.
- **New necromancy sounds.** The whip's equip and tag sounds, the soul pickups, all four raise sounds and the undead crumbling are redone to go with the new crack.

### 0.13.2

- **The Soul Whip cracks like a real whip.** The lash is a physical cord now. The crab draws the whip back over its shell and snaps it forward, and a loop rolls down the cord until the tip breaks the sound barrier. The crack plays at that moment, from the tip, instead of when you attack. It's a sharp supersonic snap with a ghostly sigh after it.
- **New swing sound.** Every lash whooshes as the crab swings it, with a faint wail.
- **Smaller download.** The necromancer's sounds are stored compressed, so CrabMods.dll is about 1.4 MB smaller.

### 0.13.1

- **Tagged enemies get a reticle on the HUD instead of marks in the world.** The rune brand on the ground and the ring floating around the enemy are gone. Now a rune reticle is drawn over each tagged enemy on your screen, sized to the enemy. It snaps in when the tag lands, turns and pulses, and has a dark rim so it reads on sand, grass and snow. Every player sees it, whether or not they hold the whip.
- **Raised undead are ghost green all over.** What an enemy carries, like the Hammer Crab's hammer and armour pieces, used to keep its normal colours.

### 0.13.0

- **Greed Run, a new game mode.** A new portal in the lobby, opposite Arcade and Holdout, starts a normal run where everything is Greed. Every island reward chest is a Greed chest. A Greed chest can offer any item at the usual rarity odds, plus the game's own Greed items, with 2 more choices than usual. Every mod and perk that appears comes 1 level higher, and each level of a non-Greed item you hold carries a curse that depends on its kind:
  - Damage items: −4% max health
  - Critical and Speed items: +4% damage taken
  - Elemental items: −3% fire rate
  - Luck and Health items: −3% damage
  - Economy items: you lose 5 crystals when hit
  - Skill items: +5% ability cooldown
  - Relics and everything else: −3% max health

  Curses never take more than 90% of your max health, damage or fire rate. Cards say "Greed" in the Greed colour, and every item's text shows its curse. You can always leave a chest's items on the ground. Coming back to the lobby ends the mode, and a continued Greed Run stays one. Change the numbers in `[greedRun]`.

### 0.12.1

- **Fixed the game crashing at the start with the Parkour maps add-on installed** ("Attempted to create a package with name containing double slashes", issue 2). Parkour gave the engine the whole map path where it wanted only the map's name. Without the add-on nothing changes.

### 0.12.0

- **Minimalist is reworked: fewer kinds of pickup, more damage.** It now gives +40% damage, minus 4% for each different pickup tag you hold (Fire, Critical, Speed, ...), so five tags leave +20% and ten leave nothing. It is Jack of All Trades the other way round. It used to give +5% per empty weapon mod slot, which was +115% at the start of a run, because there are 24 slots. The new settings are `maxPercent` and `perTagPercent`. An existing `CrabMods.ini` keeps its old `damagePercent` line, which now does nothing.
- **The key totem works like the game's own again while you still have locked items, but you get 3 choices.** It offers 3 random locked items in its menu (the game's own gives one), and the one you choose is unlocked. Closing the menu and using the totem again shows the same three. Once everything is unlocked, it opens the menu of every item as before. Set the count with `choices` in `[keyTotem]`.
- **Charms can be found again to level them up.** Chests used to leave out every charm you held (the game does that for relics), so a charm never got past level 1. Now a charm you hold can drop again, and picking it up adds a level (+5 slots).
- **Rarity changes.** Jack of All Trades, Soul Brand, Twin Rising and Grim Harvest are now Epic (they were Legendary, which was too much for what they do). Soul Siphon is now Epic (it was Common): a soul for every enemy the burst hits outearned kills several times over. Army of the Dead is now Legendary (it was Epic): with a full army it was over +100% damage per level.
- **Greed Is Power gives +3% damage per Greed perk level** (was +5%).
- **Collateral Damage gives +3% per destructible broken** (was +5%). Islands have dozens of pots, rocks and crystals, so it reached +100% and more on every island.
- **Overflow now resets each island**, like Collateral Damage. Its bonus used to be permanent and uncapped, so healing at full health kept adding damage for the whole run.
- **Community Support never drops when you play alone**, where it does nothing.
- **Purist gives +100% damage** (was +40%) while you hold no relics.
- **Cursed Lottery pays more the further the run has gone, and it turns off Money Is Power.** It gives 15,000 crystals on island 0, plus another 15,000 every 14 islands (30,000 on island 14). While you hold it, Money Is Power adds no damage, and Money Is Power's card says so. Before, the pile of crystals it gave made Money Is Power's bonus full for the rest of the run. The new settings are `crystals` and `islandsToDouble`.
- **Arsenal and Hoarder count levels, not pickups.** A level 5 weapon mod counts 5, the same way the other collector items count. Their `countStacks` setting is gone.
- **No Anvil Portals**, a new tweak that is on by default: the portals after an island never lead to an Anvil island. A portal that would have been an Anvil gets another reward instead. Turn it off with `enabled = false` in `[noAnvilPortals]`.
- **Portal Shot is gone, and Pathfinder takes Portal Ring's place.** Portal Shot replaced the wrong relic: Portal Amulet (your shots fire from the nearest enemy) drops again as normal. Pathfinder is a new Common perk that replaces Portal Ring: one more portal choice after each island for every level, where the ring gives one and never levels. Portal Ring no longer drops while it's on. A `[portalShot]` section left in `CrabMods.ini` does nothing.
- **Chance-based weapon mods trigger less often with the Soul Whip.** The whip kept the Sniper's 7× roll multiplier (Double Shot 56%, Triple Shot 49%), and every extra bolt is a whole burst. It now has 3×. Set it with `procRollMultiplier` in `[necromancer]`.
- **Fodder raises cost 10 souls** (were 5). A pack of 3 fodder had about five times the health per soul of an elite, so cheap fodder walls beat everything else.
- **Necromancer marks.** A raised undead climbs out of a turning circle of runes on the ground, which flares as it comes up and then fades. It replaces the enemy's own spawn effect, which was flat and cut into slopes. A tagged enemy has a rune brand on the ground under it and a ring of runes floating around its middle, which snaps in when the tag lands and disappears when the tag ends or the enemy dies. Every player sees them.
- **Necromancer balance.**
  - **Undead cost more to keep:** upkeep is 1% of their raise cost a second (was 0.25%), and undead still standing when the fight ends give back half their cost (was all of it).
  - **At most 20 undead per player** (was 40 in total). The new Epic perk **Legion** adds 5 per level. Set it with `maxUndead`.
  - **Hollow Hunger**, a new Common perk, halves upkeep per level, never below an eighth.
  - **Elites cost 75 souls** (were 100) and are raised at full health (were half).
  - **Undead get half your damage bonus** (they got all of it), set with `ownerDamagePercent`. Their kills give half the souls (`undeadKillSoulsPercent`), and drop crystals, but don't set off your on-kill perks and relics.
  - **Army of the Dead weighs undead by rank:** a fodder counts a quarter, a medium 1, a hard 2, an elite 4. Fodder packs gave it too much.
  - **Branding Iron** reaches 15% farther per level (was 25%). **Ring of Souls** and **Ring of the Grave** give 40% (were 25%).
- **Undead no longer spawn with a shield or armor plates.** An enemy can roll the Shielded or Armored buff as it spawns, and raised undead kept it.
- An existing `CrabMods.ini` keeps its old lines for the changed numbers (`rarity`, `damagePercent`, `crystals`, `costFodder`, `costElite`, `upkeepPercent`, `survivorRefundPercent`, `eliteHealth`, `radiusPercent`, `soulsPercent`, `percent` and so on), so these changes only show up with a fresh file or after you edit those lines (delete a section to get its new defaults back).

### 0.11.0

- **Parkour islands are back (experimental).** On islands 1 to 3 the Demolition portal is sometimes a Parkour portal instead. The maps come from the game's 2025-04-03 build, which every player builds into an add-on pak from their own download (`re/scripts/parkour_pak.py`, see the README). Parkour is only offered when every player has it. Settings in `[parkour]`.

### 0.10.6

- **Fixed Collateral Damage running away to huge or negative damage in multiplayer with CrabSync** (the stats screen could show Global Damage at -1073741824%). Get CrabSync 1.0.6 too; it holds the actual fix. A client now also ignores a broken value instead of using it.

### 0.10.5

- **Undead still standing when the fight ends give their souls back.** They still crumble, but you get their full raise cost back (up to your soul cap). Set it with `survivorRefundPercent` in `[necromancer]`.

### 0.10.4

- **Hellshooter is now Hellshot.** Its settings moved to `[hellshot]`.
- **Bullet Rain and Hellshot deal +35% damage** (was +10%). An existing `CrabMods.ini` keeps its old `damagePercent` under `[bulletRain]` until you change it.
- **Charms**, a new kind of item, now give the extra inventory slots. Bandolier, Tool Belt, Scabbard, Backpack and Reliquary are charms. Charms stack (a second one adds its levels), never take a relic slot, and have their own **CHARMS** section in the inventory and their own tab in the key totem menu.
- **Portal Shot**, a Legendary perk, replaces the Portal Amulet relic. Your shots fire from the nearest enemy, and each level adds +20% damage. Portal Amulet no longer drops while it's on.
- **Enhanced Turrets is an ability mod.** It drops like the game's perk did (only with a turret ability) and gives the perk's turret damage per level. The perk no longer drops while it's on.
- **The inventory shows what items are doing.** Every CrabMods item has a line for its current bonus. Counting items say what they count: Arsenal and Hoarder (items counted), Greed Is Power (greed perk levels), Minimalist (empty weapon mod slots), Jack of All Trades (different tags) and Purist (elements). Game items that build something up with no line of their own get a "Total gained" line.
- **Escape closes the key totem menu.** Before, it reopened right away. Your keys stay spent: use the totem again to get back to the same choice.

### 0.10.3

- **CrabMods' version shows in game**, after the game's own version in the corner of the HUD ("CrabMods 0.10.3").

### 0.10.2

- **The Soul Whip does 100 damage a second.** 0.10.1 meant to, but it was still doing 25: the bolt never hits directly, so only its 20-damage burst counted (and `whipDamage` did nothing). The burst now does all of it, 80 per crack at 1.25 cracks a second, to every enemy in it. Set it with `whipDps` in `[necromancer]`; `whipDamage` and `burstDamage` are gone.
- The log notes what the whip really does every 5 seconds you fire it (shots a second, damage a second).

### 0.10.1

- **The Soul Whip's sounds follow the game's volume.** They play through CrabMods' own mixer, so the game's sliders didn't reach them. They now go with the game's **Master** and **Gameplay** volume (times `soundVolume`).
- **Fixed souls and raisable kinds carrying over into the next run.** They were meant to clear in the lobby, but the check ran before the game knew it was the lobby. They now clear as soon as you're back there.
- New relic: **Summoner Specialist** (Epic). The Soul Whip's 20 pickups are offered 3x as often and can come out of all your chests. It only drops while you hold the whip.
- **The Soul Whip's pickups come out of their own chests.** Before, they came out of every chest a whip holder opened. Each now has a loot pool that fits it (Sturdy Bones in Health chests, Soul Jar in Economy chests, ...), plus the any-pool chests. The Ring relics no longer count as Fire pickups. Plague Bearers is a Poison pickup.
- **The game's own ability mods no longer drop for players holding the Soul Whip**, since it replaces the ability.
- **Fixed the ability still firing while holding the Soul Whip.** The block was on the host's half, which only plays the effect for the other players. Your machine fires the ability itself. It's now blocked where the key is read, for any key or controller binding.
- **Raising leaves out poor undead.** Barrel Skulls, the Immortal Snail, and enemies that blow themselves up (slugs, pufferfish) no longer join the raise list. The last other kind you killed of that rank stays.
- **The Immortal Snail stays the players' problem.** Undead don't attack it, and it doesn't turn on them.
- **Undead go for nests and spawners.** An undead with no enemy within 25 m (`nestRangeMeters`) walks to the nearest nest or spawner and breaks it (`nestDps`, 30 a second each).
- **The undead's ghostly look works on every kind.** It only set the crabs' color names. It now sets every enemy material's color and glow names, on every material of the body, and keeps reapplying it.
- **Fixed the soul bar not showing Soul Jar's bigger max** (or Soul Discount's cheaper costs) until your souls next changed.

### 0.10.0

- New **weapon, the Soul Whip** (the Necromancer), in the lobby's weapon list. A lash with no ammo: each crack sends a purple soul bolt up to 45 m that bursts on an enemy, the floor or at its reach, hurting everything within 4 m and **tagging** enemies. While you hold it your ability becomes **raising the dead**: kills fill a soul bar, and **V / C / X / Z** spend souls to raise the last fodder (a pack of 3), medium, hard or elite enemy you killed this run as an **undead** that fights for you. Undead go after tagged enemies first, never hurt players, cost a trickle of souls while they fight, and crumble when the island's fight ends. It stands on its **own pedestal left of the Crossbow** (the whip itself turning there in a violet glow), and has its own **model** (a cord lash that hangs and sways as you move, and cracks out toward your aim when you fire, with a soul glow pulsing down it), **sounds**, and **HUD**: a soul bar above your health and the four raise pictures, with keys and costs, where the ability was, left of the melee slot. Every number is in `[necromancer]`. The raise keys (`raiseKeyFodder` ...) and `soundVolume` are **your own**: joining someone's game never changes them.
- **20 Soul Whip items**, which only drop for players holding the whip, from any of their chests: weapon mods **Long Lash** (bolt reach), **Soul Siphon** (souls per bolt hit), **Branding Iron** (burst and tag radius), **Death Mark** (tagged enemies take more of your damage), **Soul Brand** (tagged kills spread the tag); ability mods **Sturdy Bones** (undead health), **Soul Discount** (cheaper raises), **Mass Grave** (bigger fodder packs), **Twin Rising** (raises can double); melee mods **Grave Claws** (melee kill souls), **Harvester** (melee kills can rise free); perks **Soul Jar** (max souls), **Hungry Ghosts** (kill souls), **Last Rites** (fallen undead refund souls), **Army of the Dead** (damage per undead), **Plague Bearers** (undead poison), **Grim Harvest** (tagged kill souls); relics **Ring of Souls** (all souls), **Ring of the Grave** (undead health and damage), **Phylactery** (souls cancel a death once per island). Each has its own `[section]` with one number per level.
- **Fixed Finishing Move and Rigged Totems most likely doing nothing.** Both recognised the game's call they change by its address, but compared it with the address from before Windows moves the game in memory, so it could never match. They now compare with where the game actually is.

### 0.9.0

- **Works with CrabMods Manager 0.3.0**, which shows every item as a mod to install or uninstall, with its settings. CrabMods tells the manager its items and their default settings, and does nothing when it's loaded outside the game.
- **Spiked Chest** and **Heavy Shot** can be switched off like every other item (`[spikedChest] enabled`, `[heavyShot] enabled`; the old `anyWeapon` still works). With Spiked Chest off, the chest costs the game's own 25%.

### 0.8.2

- **Pickups show their level.** The game's pickup card never showed one, so you couldn't see Lucky Charm work. A pickup above level 1 now has its level after its name on the card, like **"Vitality (Lv 2)"**. This shows for every player.

### 0.8.1

- **Fixed a crash when opening a chest** (most likely with Lucky Charm in the game). After the game rolled a chest's loot, CrabMods looked up who opened it in a list the game had already freed, and Lucky Charm then read that leftover memory as the player. CrabMods now reads the opener before the roll, and checks it is a player.
- **Lucky Charm should now actually work**: the same bug meant it was reading garbage instead of the opener's relics, so it most likely never saw that you had it and never upgraded chest items to level 2.

### 0.8.0

- New **Epic perk, Combo Catalyst**: your hits apply **+1 stack of each element** they already apply (ice, fire, lightning, poison, arcane) **for every combo level above 1**, per Combo Catalyst level (`[comboCatalyst] stacksPerComboLevel`).
- New **Epic relic, Encore**: perks that go off by themselves have a **15% chance to go off a second time** (`[encore] chancePercent`): the on-kill explosions of Exploding, Freezing, Flammable, Electric and Poisonous Enemies, the Explosive, Poisonous and Flammable Armor explosions, and **Grave Dodger** (it can save you again that island). It also gives a failed Rigged Totems or Autoloader roll a second try, and doubles Finishing Move's bonus.
- New **Epic perk, Autoloader**: each kill has a **10% chance to reload your weapon instantly** (`[autoloader] chancePercent`; more levels add up hyperbolically).
- New **Epic perk, Finishing Move**: enemies you kill with a **critical hit** drop **50% more crystals** per level (`[finishingMove] crystalPercent`).
- New **Epic perk, Rigged Totems**: Health, Chance and Gamble totems are **25% more likely to go your way** per level (`[riggedTotems] chanceBonusPercent`): a 50% totem becomes 62.5%.

### 0.7.2

- New **Greed relic, Cursed Lottery**: **100,000 crystals** when you pick it up, but while you hold it you only get **0.01%** of the crystals you'd gain (`[cursedLottery] crystals`, `gainPercent`). It **can't be dropped**. It's as rare as the game's rarer Greed relics (spawn weight 0.15). With CrabSync, only the player who picks it up gets the crystals, not every copy.
- New **Legendary relic, Lucky Charm**: each mod and perk in the chests you open has a **25% chance to be level 2** (`[luckyCharm] chancePercent`).
- **Martyr**'s bubble now does **a third of a basic crab's max health** a second (`crabHealthPerSecond = 0.33`), so it grows with the enemies every island: 5 a second on the first islands. The old `damagePerSecond` setting is ignored.

### 0.7.1

- New **Legendary relic, Reliquary**: **+5 relic slots** (`[reliquary] slotsPerLevel`). The game allows 10 relics and won't let you pick up an 11th; with the Reliquary you can carry 15, and the inventory menu shows them all.

### 0.7.0

- **Key totem menu** now always has **every item**, CrabMods' own included (Bandolier, Hoarder, ...). Locked items say "Locked" on their card and have a **Locked** tab of their own; choosing one unlocks it. CrabMods' items are never written into your save's unlock list, and any that 0.6.0's menu put there (or empty entries) are taken out.
- **Key totem menu** cards are no longer see-through. The menu was dimming every card without gamepad focus to half opacity, as it does YES / NO. Now they are only a little darker.
- **Martyr** reworked: instead of exploding once when you go down, your **revive bubble hurts the enemies in it** for as long as you're down (`damagePerSecond`, 60 per level). **Magnify** makes the bubble bigger (`magnifyBubble`), both the area it hurts and the area a teammate can revive you from. The old `damage`, `radiusMeters` and `knockback` settings are gone.

### 0.6.0

- **Key totem menu** is now a full catalog instead of a choice of 3. It shows **every locked item**, or **every item** once everything is unlocked, as the game's own pickup cards, three to a row in a scrolling list sorted by kind, rarity and name.
  - **Category tabs** at the top (All, Weapons, Abilities, Melee, Weapon Mods, Ability Mods, Melee Mods, Perks, Relics) look like the settings menu's tabs. Only kinds that have items get a tab. The Next / Previous Submenu keys (the gamepad bumpers) switch tabs too.
  - A **search box** filters by name, type and description as you type. Several words must all match, e.g. `epic fire`.
- `choices` now only sets how many items come out of the totem in row mode (`menu = false`).

### 0.5.1

- **Key totem menu**: the item cards sit on a dark, nearly opaque plate, so they are no longer see-through over the blurred game. The plate lightens when you hover or press a card.

### 0.5.0

- **Key totem menu**: the lobby's key totem now opens a menu instead of dropping a row of items. It is the game's own confirmation prompt with the items' own pickup cards as buttons: click one (or select it with a gamepad) and it is unlocked and comes out of the totem. It works with **any host**, CrabMods or not. `menu = false` brings back the row.
- 4 new **Legendary** items, one per inventory group, each giving **+5 slots per level** in its group (`slotsPerLevel`):
  - **Bandolier** (weapon mod): weapon mod slots.
  - **Tool Belt** (ability mod): ability mod slots.
  - **Scabbard** (melee mod): melee mod slots.
  - **Backpack** (perk): perk slots.
- The **inventory menu** gets more slots when you have them, and scrolls once they don't fit.
- New ability mods (Recharge, Tool Belt) use the game's plain ability mod icon for their rarity.

### 0.4.1

- **Specialists** now also bring their pickups into your chests of **every other loot pool**: with Critical Specialist, critical pickups can come out of a Speed or Health chest too, not only Critical chests. There they are as likely as the chest's own pickups (`otherChests = 1`; `0` turns it off). This works for the regular pool chests (Damage, Critical, Elemental, Speed, Luck, Health, Economy, Skill). Random chests already offered everything, and Greed, Relic, Anvil and Spiked chests stay as they are. Rarity still applies, so an Epic critical pickup still only comes out of Epic chests.
- **Bullet Rain + Hellshooter**: with both, your shots alternate. Every other shot falls from the sky and the rest come up out of the ground (before this, Hellshooter took over every shot).

### 0.4.0

6 new items, stronger melee mods, a key totem that lets you choose, and a new Martyr.

- Perks:
  - **Collateral Damage** (Epic): +5% damage for every destructible you break during an island's fight. It resets each island, like Tasty Orange.
  - **Overflow** (Epic): every 100 health healed while at full health gives +3% damage for the rest of the run.
  - **Thick Shell** (Epic): +3% max health multiplier per perk level you have.
  - **Community Support** (Epic): you and every player within 20 m fire faster, one Rapid Fire level (+12%) for each player in range who has it, you included.
- Weapon mods:
  - **Bullet Rain** (Epic): your shots fall from the sky onto where you aim. +10% damage.
  - **Hellshooter** (Epic): your shots fire up out of the ground under where you aim. +10% damage.
- **Martyr** is new: when you go down you explode for 150 damage to the enemies around you (25 m), and **Magnify** makes it bigger. It no longer boosts teammates. Its settings are now `damage`, `radiusMeters` and `knockback`.
- **Melee mods** are stronger (`[meleeMods]`): Sharp Claws 200% (was 100), Time Claws 300% (150), Iron Claws 500% (300) with a 20% cooldown penalty (40), Big Claws 40% (20), Blender 30% (20), Vampire 15 health per kill (5), and the elemental claws apply twice the stacks.
- **Key totem** (`[keyTotem]`): the lobby's key totem offers 3 locked items to choose from, and the one you take is unlocked. Once everything is unlocked, it offers 3 of any weapon mods, ability mods, melee mods, perks and relics. It needs CrabMods on the host too.

### 0.3.3

- **Double or Nothing** now works on chests instead of loot choices: at the end of each island you get **twice as many reward chests, or none** (50/50, `doubleChancePercent`). It counts every chest the island gives you (bonus chests, Care Package, totems) and rolls just before High Roller, so High Roller still works on what is left. You see "Double or Nothing doubled your chests!" or "Double or Nothing took all your chests!". Chests you open now always offer their normal choices.

### 0.3.2

- With CrabSync's shared health pool (and `chestHealthCost = split`), the Spiked island's portal says "All the team's spiked chests together cost 35% of base max health", which CrabSync 1.0.3 now makes true.

### 0.3.1


- **Spiked Chest** costs **35%** of your base max health instead of the game's 25%, and the Spiked island's portal says "costs 35% of your base max health". Set it with `costPercent` in the new `[spikedChest]` section (0 to 99; 25 is the game's own). In multiplayer the host's value is used, for the cost and for everyone's portal text.

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

### 0.4.0

The manager asks first, and runs on Linux.

- **Nothing changes until you say so.** The first start shows the game folder, what it would install, every file it would add, and two questions about updates. Nothing is written until you press **Install**, or **Just browse** to look around first.
- **Every change is shown before it's made.** Installing, uninstalling, updating, rolling back, loading a setup code and resetting to defaults all open a **Review changes** window. It lists each file it adds, replaces or removes, what it backs up, and which settings change.
- **Updates are your choice**, for the mods and for the manager separately. You can install them automatically, or be told about them, with **Update**, **Skip this version** and **What's new**. After an update a note says what changed, for example *CrabMods updated 0.14.1 → 0.14.2*. An automatic manager update goes in when you close the manager; it never restarts while you're using it.
- **Checks every 5 minutes** (or every minute, 15 minutes, or only when you press Check now). An unchanged release list doesn't count against GitHub's limit. **Offline** turns off all network use except Check now.
- **Every download is checked** against the SHA-256 checksum GitHub publishes for it, and refused if it doesn't match. Releases also have a `SHA256SUMS.txt`, and the new **Help** page compares your installed files with it. It also explains why antivirus programs may flag the mod loader.
- **It never runs a mod DLL.** Each release now publishes its item list as a catalog file, and the manager reads that.
- **Remove all mods** takes out exactly the files the manager added and puts back any file it replaced, such as another `xinput1_3.dll`. It can also delete the mods' settings and logs.
- **Roll back** to the previous version of CrabMods or CrabSync from its page. The newer version is then skipped until you undo it in Settings.
- **While the game is running**, changes wait and go in when it closes. The bar at the top says what's waiting.
- **Sort the Mods list** by type, name, rarity, installed first or newest, or click a column header. Items from the latest CrabMods update have a **NEW** badge.
- **A native Linux version**, `CrabModsManager-x86_64.AppImage`, for Linux and Steam Deck. It keeps its files in `~/.config/CrabModsManager` and `~/.cache/CrabModsManager`, and updates itself like the Windows one. The Windows exe still runs under Proton or wine.
- **Coming from 0.3**, it updates itself as before and keeps your mods, settings and game folder. It asks once how updates should work, and updates nothing on its own until you answer.

### 0.3.0

A new manager: a real window instead of a settings editor.

- **Every CrabMods item is a mod** you install or uninstall, listed with CrabMods and CrabSync in one table you can search and filter by type. Click one to see what it does and change its settings with switches, sliders and lists (saved at once; Reset puts a setting back).
- **Everything is installed by default.** On first start it installs CrabMods with every item, CrabSync and the mod loader. New items in an update arrive installed.
- **Always up to date.** It checks GitHub at start and every 30 minutes and installs mod updates by itself (waiting for the game to close if it's running). **The manager updates itself too**: it downloads the new version, swaps it in and restarts. Both can be switched off. Managers before 0.3.0 can't do this: download 0.3.0 by hand once.
- **Setup codes** (`CRAB2-`) hold which mods are installed and only the settings you changed, so they're short: everything at its default is `CRAB2-S;M-` plus a 4-character check. Loading one shows what it installs, uninstalls and changes first. Old `CRAB1-` codes still load.
- **Play** waits for running updates, then starts the game.
- On Linux / Steam Deck it shows the Steam launch option with a Copy button until Steam has it.
- Crash reports work as in 0.2.0 (Settings tab: Collect logs, Open reports folder).

### 0.2.0

- **Launch game** starts Crab Champions through Steam.
- **Crash reports.** While it is open, the manager watches the game's crash folder, including Proton's prefix on Linux. When the game crashes (or crashed while the manager was closed), it saves the mod logs, the settings and the game's crash report:
  - Player names, your user and computer name, your user folder and the crash report's machine ids are taken out.
  - It saves to `CrashReports` next to the exe, as a folder and a zip.
  - It offers to open a GitHub issue already filled in with the error, the call stack, the versions and the end of the logs. Drag the zip into the issue.
- **Collect logs...** does the same by hand.
- `--selftest` lists the crash reports it finds. `--selftest --report` saves one.

### 0.1.0

First release.

- Finds the game in every Steam library (also under Proton/wine and Flatpak Steam), or lets you browse to it.
- Installs, updates, switches on/off and removes CrabMods and CrabSync from the latest GitHub release.
- Edits each mod's settings.
- Shares settings as `CRAB1-` codes: copy, then paste into another manager.
- `--selftest` writes what it found to a text file, without a window.

## CrabSync

### 1.0.8

- **Fixed continuing a saved run losing your whole inventory.** Loading the save in the lobby restored your items, then CrabSync's next check wrote the lobby's empty team over them before the run started. A save restored in the lobby now starts the team from the saved items.

### 1.0.7

- **Arcade and Holdout work in multiplayer.** The lobby's minigame portals were solo only; now the whole lobby can vote for them and play together, with one team score. Every player who votes for one needs CrabSync 1.0.7. Nothing is shared during a minigame: everyone has their own health, so in Holdout one hit only takes out the crab that was hit. Turn it off with `minigames = false`.

### 1.0.6

- **Fixed syncing stopping partway through a run.** CrabSync adds up every player's changes to an item's built-up buff. CrabMods' Collateral Damage keeps each player's own count there, so the two fought on every poll. The buff swung further each time (to huge or negative damage), and a player whose items never stopped changing was never synced again. Buffs of items added by other mods (CrabMods) are now left to those mods, per player.

### 1.0.5

- **Works with CrabMods Manager 0.3.0**: CrabSync tells the manager its default settings, so they show up there as controls, and does nothing when it's loaded outside the game.

### 1.0.4

- **Is the host syncing?** Only the host's CrabSync syncs, so playing in a friend's game you couldn't tell whether max health and deaths would be shared. The host's CrabSync now greets every player who joins. If you have CrabSync too, your `CrabSync.log` says which version the host runs and whether it shares health (`syncHealth`). If no greeting arrives within 30 seconds, you get an on-screen message: the host isn't running CrabSync 1.0.4 or newer. Shared max health needs the host on 1.0.2 or newer.
- The host's log now also records max health changes that aren't shared (in the lobby, or before the run has started), so a max health change that stays with one player can be traced.

### 1.0.3

- Max health chests (spiked) are shared out so that **every player opening one costs the team what one chest costs alone**, compounding included: with a 35% spiked chest, four players lose about 10.2% each and exactly 35% together (it was the cost divided by the number of players, which came to about 31%). Health costs are still divided by the number of players.

### 1.0.2

- Fixed: **deaths were not shared.** The game swaps a dead player's character for a spectator in the same frame, so CrabSync never saw the death. Now a player who dies empties the shared pool and everyone dies with them.
- Fixed: **max health was per player**, so players ended up with different max health and the pool was measured against the highest. Max health is now shared: losing or gaining it (Eternal Punishment hits, totems, damage areas, perks such as Fortitude, Personal Space, Bullseye, Damage Seeker and Valued Customer) changes everyone's, and everyone has the same max and the same health.
- Chests that cost health or max health (spiked and greed chests) only charge the team the cost divided by the number of players. The new `chestHealthCost` setting can be `split` (default), `full` or `free`.
- A lower max cuts health down to it instead of taking the difference off. Losing 5 max health at 100/1000 now leaves 100/995, not 95/995. This applies when a player leaves, too.

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
