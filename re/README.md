# Reverse engineering

Static analysis of `CrabChampions-Win64-Shipping.exe` (UE 4.27, PE timestamp `0x6921D2F7`), the work both mods are built on. Everything was derived from the binary on disk. Nothing was probed at runtime.

**Start with [REPORT.md](REPORT.md)**: engine globals, the inventory model, the damage pipeline, health, loot, perks and projectiles, with addresses.

## What's here

| Path | |
|---|---|
| [`REPORT.md`](REPORT.md) | The findings, with addresses and offsets. |
| [`sdk/`](sdk) | C++ headers for every reflected type: 2184 classes, 2130 structs, 1009 enums, with exact field offsets, sizes and function flags. `CrabChampions_*.hpp` is the game; `Engine_*.hpp` is Unreal. |
| `sdk.json` | The same data as JSON, plus each type's reflection-table and `StaticClass` addresses. |
| `natives.json` | Native `UFunction`s: the exec thunk and the C++ implementation it calls. |
| `vtables.json` | Each game class's vtable, constructor, and which slots hold its virtual and RPC `_Implementation` functions. |
| `perk_sites.json` | For every perk, the functions that check it (`HasPerk`, `InvArr_GetLevel`, ...). |
| [`scripts/`](scripts) | The tools that produced all of the above. |

## Not included

These stay on your machine because they are, or are extracted from, the game's copyrighted files. The scripts recreate them from your own copy of the game:

- `game.exe` and `game.pak`: links to the game's exe and pak.
- `ghidra/`, `ghidra_quick/`: Ghidra projects (they contain the binary).
- `decomp/`: decompiled game code.
- `assets/`, `pickup_catalog.json`, `pak_files.txt`: assets and text extracted from the pak.
- `strings_*.txt`: the binary's strings.

`.gitignore` keeps them out of the repo.

## Regenerating

Needs Python 3 with [capstone](https://www.capstone-engine.org/) and [pefile](https://github.com/erocarrera/pefile) (`pip install capstone pefile`). Run everything from this folder:

```bash
ln -s "<game>/CrabChampions/Binaries/Win64/CrabChampions-Win64-Shipping.exe" game.exe
python scripts/ue_reflect.py game.exe sdk     # sdk.json (reflection tables)
python scripts/emit_sdk.py sdk.json sdk       # sdk/*.hpp
python scripts/natives.py                     # natives.json: exec thunks
python scripts/natives_impl.py                #   then the implementation each thunk calls
python scripts/vtables.py                     # vtables.json (needs sdk.json and natives.json)
python scripts/perkmap.py                     # perk_sites.json (needs sdk/)
python scripts/emit_full_sdk.py               # ../sdk: CrabSDK, the compilable SDK (needs the three JSON files)
```

`sdk/` here is a readable reference. The SDK for building mods against is [`../sdk`](../sdk/README.md).

| Helper | |
|---|---|
| `scripts/xref.py`, `calls.py`, `stubs.py` | String and data references, call sites, and generated `ProcessEvent` stubs. Use them interactively: `python -i -c "import sys; sys.path.insert(0, 'scripts'); from calls import *"`. |
| `scripts/pak.py <pak> list` / `extract <dir> [filter]` | Reads the game's pak (v11). Oodle blocks need [ooz](https://github.com/powzix/ooz) as a shared library; set `LIBOOZ` to its path. |
| `scripts/uasset.py <file.uasset>...` | Prints a cooked asset's tagged properties as JSON. |
| `scripts/decomp.sh addrs.txt out.c` | Decompiles a list of functions with headless Ghidra. Set `GHIDRA` and `JAVA_HOME`. |
| `scripts/ghidra-mcp-server.sh` | Serves a Ghidra project over [ghidra-mcp](https://github.com/bethington/ghidra-mcp); `scripts/gm.py apply` names the functions listed in it. |
| `scripts/PreAnalysis.java`, `DecompileAt.java` | Ghidra scripts: analysis options for this 73 MB binary, and the decompiler used by `decomp.sh`. |

The Ghidra projects are an ordinary headless import of `game.exe`: `ghidra/CrabChampions` fully analyzed (with `-preScript PreAnalysis.java`), and `ghidra_quick/Quick` imported with `-noanalysis` for fast one-off decompiles.

All addresses are for game build `0x6921D2F7`. After a game update, rerun the scripts; the mods' addresses live in [`common/crab/game.hpp`](../common/crab/game.hpp).
