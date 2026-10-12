"""Builds the Parkour add-on pak from an old Crab Champions pak (build of 2025-04-03, depot 774803 manifest
3380205502637293767), for CrabMods' Parkour item. No game content lives in this repo: each player builds the pak
from their own copy of the old game.

usage: parkour_pak.py <old CrabChampions-WindowsNoEditor.pak> <out dir> [--chunk N] [--store]

Writes <out dir>/pakchunk<N>-ParkourMaps_P.pak, which goes in the current game's CrabChampions/Content/Paks/~mods.
It holds the two old Parkour maps, the 289 packages they need that the current game no longer has (re/parkour_files.txt),
and the old shader library renamed as chunk N's library, which the engine opens when it mounts a pakchunk<N> pak
(a few of the old materials' shaders are only in it).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pak import Pak  # noqa: E402
from pakwrite import write_pak  # noqa: E402

LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "parkour_files.txt")
SHADERS = "CrabChampions/Content/ShaderArchive-CrabChampions-PCD3D_SM5.ushaderbytecode"
MAPS = ("Arctic_Parkour_01.umap", "Tropical_Parkour_01.umap")


def main():
    args = sys.argv[1:]
    chunk, store = 77, False
    if "--chunk" in args:
        i = args.index("--chunk"); chunk = int(args[i + 1]); del args[i:i + 2]
    if "--store" in args:
        args.remove("--store"); store = True
    if len(args) != 2:
        sys.exit(__doc__)
    old, outdir = args
    p = Pak(old)
    want = [l.strip() for l in open(LIST) if l.strip()]
    missing = [w for w in want if w not in p.files]
    if missing or not all(any(w.endswith(m) for w in want) for m in MAPS):
        sys.exit(f"{old} isn't the 2025-04-03 build: {len(missing)} of {len(want)} files missing (first: {missing[:3]})")
    files = [(w, p.read(w)) for w in want]
    files.append((SHADERS.replace("-CrabChampions-", f"-CrabChampions_Chunk{chunk}-"), p.read(SHADERS)))
    files.sort()
    os.makedirs(outdir, exist_ok=True)
    out = os.path.join(outdir, f"pakchunk{chunk}-ParkourMaps_P.pak")
    write_pak(out, files, use_zlib=not store)
    print(f"wrote {out}: {len(files)} files, {os.path.getsize(out) / 1e6:.0f} MB")


if __name__ == "__main__":
    main()
