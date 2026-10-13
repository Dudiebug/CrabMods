#!/usr/bin/env python3
"""Bakes Shellbreaker's game assets into crabmods/assets/shell.pack, which build.sh embeds in CrabMods.dll
(resource 101, beside the necromancer's necro.pack at 100). Same pack format and mesh format as the necromancer's
(art/necromancer/bake_pack.py):

    uv run --no-project --with numpy python bake_pack.py      (after gen_weapon.py)

Input: export/handle.crabmesh, link.crabmesh, blade.crabmesh and export/held.json, from gen_weapon.py.

The pack:
  palette.png        16 x 16: row = one of Shellbreaker's colours, column = brightness (baked shading)
  shell_handle.mesh  u32 vertex count, then per vertex 3 floats position (the crab's WeaponSocket space) and 2
                     floats UV
  shell_link.mesh    the same, in the link's space (along +X from 0; the next link starts at the pitch)
  shell_blade.mesh   the same, in the blade's space (its eye at 0, along +X)
  A_Shell_*.pcm      sounds (gen_sounds.py): raw mono 16-bit PCM, 32 kHz
  shell.rig          the chain's anchor (the handle's eye) and direction in the socket (6 floats), the link pitch,
                     the links at rest (u32), the blade's reach from its eye (float)
"""
import glob
import json
import os
import struct
import wave
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "crabmods", "assets", "shell.pack")
PAL = 16
GLOW = (255, 196, 120)  # the blade's ridge, as the game shows it: a hot amber


def png(rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def read_crabmesh(path):
    b = open(path, "rb").read()
    assert b[:4] == b"CRBM"
    n = struct.unpack_from("<I", b, 4)[0]
    rec = np.dtype([("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
    return np.frombuffer(b, rec, n, 8)


def main():
    held = json.load(open(os.path.join(HERE, "export", "held.json")))
    T = np.array(held["T"], np.float64)
    meshes = {name: read_crabmesh(os.path.join(HERE, "export", name + ".crabmesh"))
              for name in ("handle", "link", "blade")}

    colours = sorted({tuple(c) for m in meshes.values() for c in m["c"]})
    assert len(colours) <= PAL, colours
    tex = np.zeros((PAL, PAL, 4), np.uint8)
    for row, c in enumerate(colours):
        emissive = c[3] > 127
        rgb = np.array(GLOW if emissive else c[:3], np.float64) / 255
        for col in range(PAL):
            k = col / (PAL - 1)
            light = (1.15 + 0.35 * k) if emissive else (0.42 + 0.78 * k)
            tex[row, col, :3] = np.clip(rgb * light * 255 + 0.5, 0, 255)
            tex[row, col, 3] = 255
    entries = {"palette.png": png(tex)}

    light = np.array([0.35, -0.25, 0.9])
    light /= np.linalg.norm(light)
    for name, m in meshes.items():
        p = m["p"].astype(np.float64)
        nrm = m["n"].astype(np.float64)
        if name == "handle":  # into the socket's space
            p = (np.c_[p, np.ones(len(p))] @ T.T)[:, :3]
            nrm = nrm @ T[:3, :3].T
            nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
        wrap = np.clip((nrm @ light + 0.35) / 1.35, 0, 1)
        shade = np.round(wrap * 5) / 5
        row = np.array([colours.index(tuple(c)) for c in m["c"]])
        uv = np.c_[(shade * (PAL - 1) + 0.5) / PAL, (row + 0.5) / PAL]
        data = np.c_[p, uv].astype("<f4")
        entries[f"shell_{name}.mesh"] = struct.pack("<I", len(data)) + data.tobytes()
        print(f"{name}: {len(data) // 3} triangles")

    eye = (T @ np.r_[held["eye"], 1.0])[:3]
    d = T[:3, :3] @ np.array([1.0, 0, 0])
    d /= np.linalg.norm(d)
    entries["shell.rig"] = struct.pack("<7fIf", *eye, *d, held["pitch"], held["links_rest"], held["blade_reach"])

    # The sounds (gen_sounds.py), as the mixer plays them: raw mono 16-bit PCM at 32 kHz.
    for f in sorted(glob.glob(os.path.join(HERE, "sfx", "*.wav"))):
        w = wave.open(f)
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 32000, f
        entries[os.path.basename(f)[:-4] + ".pcm"] = w.readframes(w.getnframes())

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    head = b"CRPK" + struct.pack("<I", len(entries))
    off = len(head) + len(entries) * 56
    toc, blob = b"", b""
    for name, data in entries.items():
        assert len(name) < 48, name
        toc += name.encode().ljust(48, b"\0") + struct.pack("<II", off + len(blob), len(data))
        blob += data
    open(OUT, "wb").write(head + toc + blob)
    print(f"wrote {os.path.relpath(OUT)}: {len(entries)} entries, {len(head + toc + blob) / 1e6:.2f} MB")


if __name__ == "__main__":
    main()
