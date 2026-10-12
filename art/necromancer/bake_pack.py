#!/usr/bin/env python3
"""Bakes the necromancer kit's game assets into crabmods/assets/necro.pack, which build.sh embeds in CrabMods.dll.

    python3 bake_pack.py        (needs numpy)

Input (all made in necromancer.blend, see its necro_export text block):
  export/whip_*.crabmesh   the whip's triangles per bone: "CRBM", u32 vertex count, then per vertex 6 floats
                           (position, normal; UE space, game scale) and RGBA (A = 255: emissive)
  export/held.json         the handle's bone frame -> the crab's WeaponSocket (T), the lash's anchor and direction
                           in the socket, and the lash segments' lengths
  ui/*.png, sfx/*.wav

The pack: "CRPK", u32 count, then per entry a 48-byte name, u32 offset, u32 size; then the data.
  palette.png       16 x 16: row = one of the whip's colours, column = brightness (baked shading, left dark)
  pulse_N.png       the palette at steps 0..6 of the soul glow's breathing, and 7 = a crack's flare (the glowing
                    parts in soul violet, the cord a little)
  whip_handle.mesh  u32 vertex count, then per vertex 3 floats position (WeaponSocket space) and 2 floats UV
  whip_lash_NN.mesh, whip_tip.mesh   the same, in their segment's space (the segment runs along +X from 0)
  whip.rig          anchor (3 floats), direction (3 floats), u32 segment count, the segments' lengths
  *.png             the HUD's pictures, as they are
  tag_halo.png      the ring floating around a tagged enemy (drawn here): soul violet, its alpha the shape
  raise_ring.png, raise_hex.png   the raise circle's two layers (drawn here), white on black with matching alpha:
                    the ground decal under a rising undead takes one as its shape and turns them opposite ways
  *.pcm             sounds: mono, 32 kHz, 16-bit
"""
import glob
import json
import os
import struct
import wave
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "crabmods", "assets", "necro.pack")
RATE = 32000
PAL = 16
# The glowing parts' colours as the game shows them: soul violet. And the cord's, which glows a little too.
SOUL = {(235, 224, 255): (176, 108, 255), (204, 189, 255): (214, 150, 255)}
CORD = (107, 77, 209)


def png(rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def smooth(d, w):
    """1 inside a line of half width w (distance d), fading over a pixel or so."""
    return np.clip((w - d) / 0.004 + 0.5, 0, 1)


def seg_dist(x, y, a, b):
    ax, ay = a
    bx, by = b
    vx, vy = bx - ax, by - ay
    t = np.clip(((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy), 0, 1)
    return np.hypot(x - ax - t * vx, y - ay - t * vy)


def raise_layers(size=512):
    """The raise circle: an outer ring of runes, and a hexagram inside. Coordinates run -1..1 across."""
    c = (np.arange(size) + 0.5) / size * 2 - 1
    x, y = np.meshgrid(c, c)
    r, a = np.hypot(x, y), np.arctan2(y, x)

    ring = smooth(np.abs(r - 0.94), 0.022) + smooth(np.abs(r - 0.80), 0.008)
    # 24 runes between the rings: a radial stroke each, and a short arc or a dot by turns.
    n = 24
    sector = (a + np.pi) / (2 * np.pi) * n
    k, f = np.floor(sector), sector - np.floor(sector)
    width = 2 * np.pi * r / n  # a sector's width at this radius, in the same units
    band = (r > 0.825) & (r < 0.905)
    ring += band * smooth(np.abs(f - 0.5) * width, 0.008)
    arc = band & (k % 2 == 0) & (np.abs(f - 0.5) < 0.32)
    ring += arc * smooth(np.abs(r - np.where(k % 4 == 0, 0.845, 0.885)), 0.007)
    for i in range(1, n, 2):
        ang = (i + 0.5) / n * 2 * np.pi - np.pi
        rr = 0.865 + (0.02 if i % 4 == 1 else -0.02)
        ring += smooth(np.hypot(x - rr * np.cos(ang + 0.07), y - rr * np.sin(ang + 0.07)), 0.011)
    # The inner ring: six dashes.
    ring += smooth(np.abs(r - 0.64), 0.009) * (np.cos(6 * a) > -0.6)
    ring = np.clip(ring, 0, 1)
    ring = np.maximum(ring, 0.22 * np.clip(1 - r / 0.95, 0, 1) ** 1.5)  # a soft glow inside

    hexa = smooth(np.abs(r - 0.56), 0.007)
    pts = [(0.56 * np.cos(np.pi / 2 + i * np.pi / 3), 0.56 * np.sin(np.pi / 2 + i * np.pi / 3)) for i in range(6)]
    for tri in (pts[0::2], pts[1::2]):
        for i in range(3):
            hexa = np.maximum(hexa, smooth(seg_dist(x, y, tri[i], tri[(i + 1) % 3]), 0.011))
    for px, py in pts:
        d = np.hypot(x - px, y - py)
        hexa = np.maximum(hexa, smooth(np.abs(d - 0.05), 0.008))
        hexa = np.maximum(hexa, smooth(d, 0.018))
    hexa = np.maximum(hexa, smooth(np.abs(r - 0.2), 0.008))
    hexa = np.maximum(hexa, 0.6 * np.clip(1 - r / 0.16, 0, 1))

    def rgba(v):
        g = np.clip(v * 255 + 0.5, 0, 255).astype(np.uint8)
        return png(np.dstack([g, g, g, g]))

    return rgba(ring), rgba(hexa)


def tag_halo(size=256):
    """Three lock-on brackets on an outer ring, a thin ring inside, and twelve diamond runes between."""
    c = (np.arange(size) + 0.5) / size * 2 - 1
    x, y = np.meshgrid(c, c)
    r, a = np.hypot(x, y), np.arctan2(y, x)
    px = 2 / size  # a pixel
    sm = lambda d, w: np.clip((w - d) / px + 0.5, 0, 1)
    v = sm(np.abs(r - 0.92), 0.035) * (np.cos(3 * a) > -0.35)  # the brackets
    v = np.maximum(v, sm(np.abs(r - 0.66), 0.012))
    for i in range(12):
        ang = i / 12 * 2 * np.pi
        dx, dy = x - 0.79 * np.cos(ang), y - 0.79 * np.sin(ang)
        u = dx * np.cos(ang) + dy * np.sin(ang)  # along the radius
        w = -dx * np.sin(ang) + dy * np.cos(ang)
        v = np.maximum(v, sm(np.abs(u) / 0.06 + np.abs(w) / 0.035, 1.0) * (0.7 if i % 2 else 1.0))
    glow = 0.35 * np.exp(-((r - 0.79) / 0.16) ** 2) * (r < 1)
    alpha = np.clip(np.maximum(v, glow), 0, 1)
    core = v  # the lines run white-hot in the middle, violet at the edges and in the glow
    rgb = np.array([176, 108, 255], np.float64) / 255
    col = rgb[None, None, :] * (1 - 0.55 * core[..., None]) + 0.55 * core[..., None]
    out = np.dstack([np.clip(col * 255 + 0.5, 0, 255), alpha * 255 + 0.5]).astype(np.uint8)
    return png(out)


def read_crabmesh(path):
    b = open(path, "rb").read()
    assert b[:4] == b"CRBM"
    n = struct.unpack_from("<I", b, 4)[0]
    rec = np.dtype([("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
    return np.frombuffer(b, rec, n, 8)


def main():
    held = json.load(open(os.path.join(HERE, "export", "held.json")))
    T = np.array(held["T"], np.float64)
    meshes = {}
    for f in sorted(glob.glob(os.path.join(HERE, "export", "whip_*.crabmesh"))):
        name = os.path.basename(f)[:-9]
        meshes["whip_handle" if name == "whip_root" else name] = read_crabmesh(f)

    # The palette: every colour the whip uses, one row each.
    colours = sorted({tuple(c) for m in meshes.values() for c in m["c"]})
    assert len(colours) <= PAL, colours

    def palette(glow, cord):
        tex = np.zeros((PAL, PAL, 4), np.uint8)
        for row, c in enumerate(colours):
            rgb = np.array(SOUL.get(c[:3], c[:3]), np.float64) / 255
            emissive = c[3] > 127
            for col in range(PAL):
                k = col / (PAL - 1)
                light = (1.15 + 0.35 * k) * glow if emissive else (0.42 + 0.78 * k)
                if c[:3] == CORD:
                    light *= cord
                tex[row, col, :3] = np.clip(rgb * light * 255 + 0.5, 0, 255)
                tex[row, col, 3] = 255
        return png(tex)

    entries = {"palette.png": palette(1.0, 1.0)}
    for k in range(7):  # the breathing, dim to bright
        entries[f"pulse_{k}.png"] = palette(0.45 + 0.75 * k / 6, 0.92 + 0.16 * k / 6)
    entries["pulse_7.png"] = palette(1.9, 1.3)  # a crack's flare

    # Baked light: from above, a little in front. Crab Champions' look is soft cel shading, so the light is
    # wrapped and stepped gently.
    light = np.array([0.35, -0.25, 0.9])
    light /= np.linalg.norm(light)
    for name, m in meshes.items():
        p = m["p"].astype(np.float64)
        nrm = m["n"].astype(np.float64)
        if name == "whip_handle":
            p = (np.c_[p, np.ones(len(p))] @ T.T)[:, :3]
            nrm = nrm @ T[:3, :3].T
            nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
        wrap = np.clip((nrm @ light + 0.35) / 1.35, 0, 1)
        shade = np.round(wrap * 5) / 5  # five soft bands
        row = np.array([colours.index(tuple(c)) for c in m["c"]])
        uv = np.c_[(shade * (PAL - 1) + 0.5) / PAL, (row + 0.5) / PAL]
        data = np.c_[p, uv].astype("<f4")
        entries[name + ".mesh"] = struct.pack("<I", len(data)) + data.tobytes()
        print(f"{name}: {len(data) // 3} triangles")

    rig = struct.pack("<6fI", *held["anchor"], *held["dir"], len(held["seg"])) + struct.pack(
        f"<{len(held['seg'])}f", *held["seg"])
    entries["whip.rig"] = rig

    entries["raise_ring.png"], entries["raise_hex.png"] = raise_layers()
    entries["tag_halo.png"] = tag_halo()

    for f in sorted(glob.glob(os.path.join(HERE, "ui", "*.png"))):
        entries[os.path.basename(f)] = open(f, "rb").read()

    for f in sorted(glob.glob(os.path.join(HERE, "sfx", "*.wav"))):
        w = wave.open(f)
        a = np.frombuffer(w.readframes(w.getnframes()), "<i2").reshape(-1, w.getnchannels()).astype(np.float64)
        mono = a.mean(1)
        n = int(round(len(mono) * RATE / w.getframerate()))
        spec = np.fft.rfft(mono)[: n // 2 + 1]  # band-limited resample
        out = np.fft.irfft(spec, n) * (n / len(mono))
        entries[os.path.basename(f)[:-4] + ".pcm"] = np.clip(np.round(out), -32768, 32767).astype("<i2").tobytes()

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
