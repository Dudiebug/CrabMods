"""Checks new item names against the game's (plans/balance/vanilla-names.md) and against each other.

    python tools/check_names.py                  every idea card in plans/ and CrabMods' items (build/CrabMods.catalog.json)
    python tools/check_names.py "Name" "Name 2"  just these, e.g. while brainstorming

A clash is the same name as a vanilla item or another of ours. A near miss shares a whole word with a vanilla name
that isn't a common one (Shot, Ring, Critical, ...), or one name contains the other ("Bounty" and Bounty Hunter),
and is worth a second look.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Ours on purpose: a vanilla item CrabMods moves or replaces, under the game's name.
SAME_ON_PURPOSE = {"enhanced turrets"}

# Words too common in the game's names to mean much on their own.
COMMON = {
    "of", "the", "a", "and", "shot", "shots", "ring", "amulet", "claws", "claw", "explosion", "ability", "critical",
    "crit", "fire", "ice", "lightning", "poison", "arcane", "big", "bigger", "damage", "health", "speed", "chest",
    "chests", "mod", "melee", "weapon", "time", "double", "triple", "iron", "max", "elemental", "turret", "turrets",
}


def norm(s):
    return re.sub(r"[^a-z0-9 ]", "", s.lower()).strip()


def words(s):
    return {w for w in norm(s).split() if w not in COMMON}


def vanilla():
    names = {}
    text = open(os.path.join(ROOT, "plans", "balance", "vanilla-names.md"), encoding="utf-8").read()
    for cat, body in re.findall(r"^## (.+?) \(\d+\)\n\n(.+)$", text, re.M):
        for n in body.split(", "):
            names[n.strip()] = cat
    return names


def ours():
    """name -> every place it is used: idea cards in plans/*.md ("BUILT" ones are in the source too) and the item
    assets in CrabMods' source and catalog. Each entry is (where, key): one key per distinct item."""
    out = {}
    for dirpath, _, files in os.walk(os.path.join(ROOT, "plans")):
        for f in files:
            if not f.endswith(".md") or f == "vanilla-names.md":
                continue
            path = os.path.join(dirpath, f)
            for m in re.finditer(r"^##+ (.+?) \((?:perk|relic|weapon mod|ability mod|melee mod|consumable)[,)](.*)$",
                                 open(path, encoding="utf-8").read(), re.M | re.I):
                name = m.group(1).strip()
                built = "BUILT" in m.group(2)
                # A built card is the same item as its asset in the source.
                out.setdefault(name, []).append((os.path.relpath(path, ROOT), "built" if built else "card:" + path))
    # Every item asset in the source (the catalog misses some, e.g. Shellbreaker's loot): Kind::X, L"DA_...", L"Name".
    for dirpath, _, files in os.walk(os.path.join(ROOT, "crabmods")):
        for f in files:
            if f.endswith((".cpp", ".hpp")):
                src = open(os.path.join(dirpath, f), encoding="utf-8").read()
                for m in re.finditer(r'Kind::\w+,\s*L"(DA_\w+)",\s*L"([^"]+)"', src):
                    out.setdefault(m.group(2), []).append(("CrabMods source [%s]" % f, m.group(1)))
    return out


def distinct(places):
    """How many different items share a name: every asset is one; cards not marked BUILT are one each; a BUILT card
    is one only when no asset has the name."""
    assets = {k for _, k in places if k.startswith("DA_")}
    cards = {k for _, k in places if k.startswith("card:")}
    built = any(k == "built" for _, k in places)
    return len(assets) + len(cards) + (1 if built and not assets else 0)


def check(name, against):
    clash, near = [], []
    n, w = norm(name), words(name)
    for other, where in against.items():
        o = norm(other)
        if o == n:
            if n not in SAME_ON_PURPOSE:
                clash.append((other, where))
        elif (w & words(other)) or (len(n) > 3 and len(o) > 3 and (n in o or o in n)):
            near.append((other, where))
    return clash, near


def main():
    van = vanilla()
    mine = ours()
    asked = sys.argv[1:]
    names = asked or sorted(mine)
    bad = 0
    for name in names:
        clash, near = check(name, van)
        places = next((v for k, v in mine.items() if norm(k) == norm(name)), [])
        # A name given on the command line is new, so any use of it is a clash; otherwise two different items.
        if norm(name) not in SAME_ON_PURPOSE and (places if asked else distinct(places) > 1):
            clash += [(name, where) for where, _ in places]
        if not clash and not near:
            if asked:
                print("ok     %s" % name)
            continue
        bad += bool(clash)
        tag = "CLASH" if clash else "near"
        print("%-6s %s" % (tag, name))
        for other, where in clash + near:
            print("         %s %s: %s" % ("=" if (other, where) in clash else "~", where, other))
    print("%d name(s) checked against %d vanilla names, %d clash(es)" % (len(names), len(van), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
