"""Draws the necro HUD's shape textures: white fills with a dark outline, so the game can tint the fill.

Run with any Python 3 that has numpy: python3 gen_ui.py. Writes pip.png, crown.png and soul.png next to itself.
"""
import os
import struct
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SS = 4  # supersampling


def png(path, rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def render(size, sdf, outline):
    """sdf(x, y) -> signed distance in pixels of the final image (negative inside). Coordinates are pixels,
    origin top left. Fill is white, the outline band is near black, outside is transparent."""
    n = size * SS
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float64)
    d = sdf((xs + 0.5) / SS, (ys + 0.5) / SS)
    fill = (d < 0).astype(np.float64)
    shape = (d < outline).astype(np.float64)
    fill = fill.reshape(size, SS, size, SS).mean((1, 3))
    shape = shape.reshape(size, SS, size, SS).mean((1, 3))
    rgb = np.zeros((size, size, 3))
    edge = np.array([0.10, 0.08, 0.14])
    a = np.maximum(shape, 1e-6)
    rgb[:] = edge
    rgb = rgb * (1 - (fill / a)[..., None]) + (fill / a)[..., None]
    out = np.concatenate([rgb, shape[..., None]], -1)
    return (np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8)


def diamond(x, y, c=32, r=24):
    return (np.abs(x - c) + np.abs(y - c)) / np.sqrt(2) - r / np.sqrt(2)


def poly_sdf(x, y, pts):
    """Signed distance to a closed polygon (pixels)."""
    d = np.full(x.shape, 1e9)
    s = np.ones(x.shape)
    n = len(pts)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[i - 1]
        ex, ey = bx - ax, by - ay
        wx, wy = x - ax, y - ay
        t = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey), 0, 1)
        dx, dy = wx - ex * t, wy - ey * t
        d = np.minimum(d, dx * dx + dy * dy)
        c1 = y >= ay
        c2 = y < by
        c3 = ex * wy > ey * wx
        flip = (c1 & c2 & c3) | (~c1 & ~c2 & ~c3)
        s = np.where(flip, -s, s)
    return s * np.sqrt(d)


def crown(x, y):
    pts = [(10, 50), (10, 18), (24, 34), (40, 10), (56, 34), (70, 18), (70, 50)]
    return poly_sdf(x, y, [(px * 0.8 + 0, py * 0.8 + 12) for px, py in pts])


def soul(x, y):
    # A soul flame: a teardrop whose tip curls, plus an eye-like hollow.
    c, cy, r = 32.0, 40.0, 15.0
    body = np.hypot(x - c, y - cy) - r
    # taper to a tip at the top, bent to the right
    t = np.clip((cy - y) / 30.0, 0, 1)
    cx = c + 8 * t * t
    width = r * (1 - t) ** 1.3
    flame = np.where(y < cy, np.abs(x - cx) - width, body)
    flame = np.maximum(flame, (8 - y))  # stop at the tip
    tip = np.hypot(x - (c + 8), y - 9) - 2.5
    shape = np.minimum(np.minimum(body, flame), tip)
    hollow = np.hypot(x - c, y - 41) - 5.0
    return np.maximum(shape, -hollow)


if __name__ == "__main__":
    png(os.path.join(HERE, "pip.png"), render(64, diamond, 4))
    png(os.path.join(HERE, "crown.png"), render(64, crown, 4))
    png(os.path.join(HERE, "soul.png"), render(64, soul, 4))
    print("wrote pip.png crown.png soul.png")
