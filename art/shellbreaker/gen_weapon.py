"""Shellbreaker, built from numbers: a short, stubby sword in the claw, and from its pommel a chain to a spiked mace.
The crab it's for is about 0.9 m tall and 1.5 m across.

Three parts, each in its own space (UE cm), like the whip's handle, cord pieces and tip:
  sword   the grip's middle at the origin, the blade along +X, flat faces toward ±Y, the edges toward ±Z
            -15     the pommel's eye (the chain hangs from here)
            -13..-9 the pommel; -9..9 the leather grip; 9..12 the crossguard, its ends curled like claw tips
            12..52  the blade: broad and stubby (20 cm across, 40 long), double-edged, a leaf shape, a diamond section; the
                    ridge down its middle glows (A = 255), so a Form can tint it (lightning, flame, water)
  link    one chain link, along +X from 0 (the next link starts at PITCH); the game lays them down the rope sim,
          every other one turned 90 degrees
  mace    its eye at the origin, the head along +X: a neck, then an iron ball (r 10) with 14 shell spikes

Writes export/sword.crabmesh, link.crabmesh, mace.crabmesh ("CRBM", u32 vertex count, then per vertex position,
normal, RGBA: the whip's format), export/shellbreaker.obj (all three laid out straight, to look at anywhere), and
export/held.json (sword space -> the crab's WeaponSocket, the pommel eye, the link pitch, the mace's eye).
"""
import json
import math
import os
import struct

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "export")

LEATHER = (92, 38, 30, 0)
SHELL = (214, 86, 48, 0)
STEEL = (196, 202, 210, 0)
EDGE = (236, 240, 246, 0)
GOLD = (190, 150, 70, 0)
IRON = (70, 72, 80, 0)
GLOW = (255, 255, 255, 255)

PITCH = 5.0  # chain link spacing, cm


class Mesh:
    def __init__(self):
        self.tris = []

    def add(self, p, n, c):
        self.tris.append((np.asarray(p, float), np.asarray(n, float), c))

    def flat(self, a, b, c, col):
        n = np.cross(np.subtract(b, a), np.subtract(c, a))
        n = n / max(np.linalg.norm(n), 1e-9)
        self.add([a, b, c], [n, n, n], col)

    def quad(self, a, b, c, d, col):
        self.flat(a, b, c, col)
        self.flat(a, c, d, col)

    def lathe(self, x0, x1, r0, r1, col, sides=12, twist=0.0, c=(0.0, 0.0)):
        slope = (r0 - r1) / max(abs(x1 - x0), 1e-6) * (1 if x1 > x0 else -1)
        for i in range(sides):
            a0 = 2 * math.pi * i / sides + twist
            a1 = 2 * math.pi * (i + 1) / sides + twist

            def pt(x, r, a):
                return np.array([x, c[0] + r * math.cos(a), c[1] + r * math.sin(a)])

            def nm(a):
                v = np.array([slope, math.cos(a), math.sin(a)])
                return v / np.linalg.norm(v)

            p = [pt(x0, r0, a0), pt(x1, r1, a0), pt(x1, r1, a1), pt(x0, r0, a1)]
            n = [nm(a0), nm(a0), nm(a1), nm(a1)]
            self.add([p[0], p[2], p[1]], [n[0], n[2], n[1]], col)
            self.add([p[0], p[3], p[2]], [n[0], n[3], n[2]], col)

    def ring(self, center, ax, ay, r, col, n1=16, n2=8, stretch=0.0):
        """A ring around center in the plane of ax, ay; stretch > 0 makes it a stadium (a chain link) along ax."""
        c, ax, ay = (np.asarray(v, float) for v in (center, ax, ay))
        az = np.cross(ax, ay)
        # the wire's path: a half circle around +stretch, then one around -stretch (straight sides between)
        half = n1 // 2
        path = []
        for side, a0 in ((1, -math.pi / 2), (-1, math.pi / 2)):
            for k in range(half + 1):
                a = a0 + math.pi * k / half
                d = ax * math.cos(a) + ay * math.sin(a)
                path.append((c + ax * stretch * side + d * self.R, d))
        path.append(path[0])

        def pt(u, v):
            mid, d = path[u]
            vv = 2 * math.pi * v / n2
            return mid + (d * math.cos(vv) + az * math.sin(vv)) * r, d * math.cos(vv) + az * math.sin(vv)

        for i in range(len(path) - 1):
            for j in range(n2):
                q = [pt(i, j), pt(i + 1, j), pt(i + 1, j + 1), pt(i, j + 1)]
                self.add([q[0][0], q[1][0], q[2][0]], [q[0][1], q[1][1], q[2][1]], col)
                self.add([q[0][0], q[2][0], q[3][0]], [q[0][1], q[2][1], q[3][1]], col)

    def torus(self, center, R, r, col, ax=(1, 0, 0), ay=(0, 0, 1), stretch=0.0, n1=16, n2=8):
        self.R = R
        self.ring(center, ax, ay, r, col, n1, n2, stretch)

    def sphere(self, c, r, col, n=16):
        c = np.asarray(c, float)
        for i in range(n):
            for j in range(n // 2):
                def pt(u, v):
                    th, ph = 2 * math.pi * u / n, math.pi * v / (n // 2)
                    d = np.array([math.cos(ph), math.sin(ph) * math.cos(th), math.sin(ph) * math.sin(th)])
                    return c + d * r, d
                q = [pt(i, j), pt(i, j + 1), pt(i + 1, j + 1), pt(i + 1, j)]
                self.add([q[0][0], q[1][0], q[2][0]], [q[0][1], q[1][1], q[2][1]], col)
                self.add([q[0][0], q[2][0], q[3][0]], [q[0][1], q[2][1], q[3][1]], col)

    def spike(self, base, d, length, r, col, tip_col, sides=6):
        d = d / np.linalg.norm(d)
        u = np.cross(d, [0, 0, 1.0] if abs(d[2]) < 0.9 else [0, 1.0, 0])
        u /= np.linalg.norm(u)
        v = np.cross(d, u)
        tip = base + d * length
        mid = base + d * length * 0.55
        for i in range(sides):
            a0, a1 = 2 * math.pi * i / sides, 2 * math.pi * (i + 1) / sides
            p0 = base + (u * math.cos(a0) + v * math.sin(a0)) * r
            p1 = base + (u * math.cos(a1) + v * math.sin(a1)) * r
            m0 = mid + (u * math.cos(a0) + v * math.sin(a0)) * r * 0.45
            m1 = mid + (u * math.cos(a1) + v * math.sin(a1)) * r * 0.45
            self.quad(p0, p1, m1, m0, col)
            self.flat(m0, m1, tip, tip_col)

    def array(self):
        rec = np.dtype([("p", "<f4", 3), ("n", "<f4", 3), ("c", "u1", 4)])
        a = np.zeros(len(self.tris) * 3, rec)
        for k, (p, n, c) in enumerate(self.tris):
            a["p"][3 * k:3 * k + 3] = p
            a["n"][3 * k:3 * k + 3] = n
            a["c"][3 * k:3 * k + 3] = c
        return a


# --- the sword -------------------------------------------------------------------------------------------------------
sword = Mesh()
sword.lathe(-13, -11, 2.0, 3.8, GOLD)  # the pommel: a gold knob
sword.lathe(-11, -9, 3.8, 2.9, GOLD)
sword.lathe(-13.01, -13, 0.0, 2.0, GOLD)
sword.torus([-15.2, 0, 0], 1.9, 0.7, GOLD)  # the eye, in the XZ plane
for k in range(6):  # the grip: leather wraps
    x = -9 + 3 * k
    sword.lathe(x, x + 1.5, 2.6, 2.95, LEATHER, twist=0.25 * k)
    sword.lathe(x + 1.5, x + 3, 2.95, 2.6, LEATHER, twist=0.25 * k)

# the crossguard: a thick bar, its ends curling forward like claw tips
gy, gx0, gx1 = 2.6, 9.0, 12.5
for z0, z1 in ((-12.5, 12.5),):
    P = [np.array(p, float) for p in ((gx0, -gy, z0), (gx1, -gy, z0), (gx1, gy, z0), (gx0, gy, z0),
                                     (gx0, -gy, z1), (gx1, -gy, z1), (gx1, gy, z1), (gx0, gy, z1))]
    for f in ((0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
        sword.quad(*[P[i] for i in f], SHELL)
for s in (-1, 1):
    sword.spike(np.array([10.75, 0, s * 12.0]), np.array([0.9, 0, s * 0.45]), 9.0, 2.2, SHELL, SHELL)

# the blade: a leaf, broad and short
NT, NU = 30, 8
X0, X1 = 12.5, 52.0
ts = np.linspace(0, 1, NT + 1)


def half_w(t):
    # 8.5 cm at the guard, swelling to 10 past halfway, then a short, blunt point
    body = 8.5 + 1.5 * math.sin(math.pi * min(t / 0.7, 1.0) * 0.9)
    return body * (1 - max(0.0, (t - 0.78) / 0.22) ** 1.2) if t < 1 else 0.0


def half_t(t, u):  # u in -1..1 across the blade: a diamond with a raised ridge
    return (2.0 * (1 - 0.4 * t)) * (1 - abs(u) ** 1.2) + 0.08


us = np.linspace(-1, 1, 2 * NU + 1)
B = np.zeros((NT + 1, len(us), 2, 3))
for i, t in enumerate(ts):
    x = X0 + (X1 - X0) * t
    for j, u in enumerate(us):
        h = half_t(t, u) if t < 1 else 0.0
        B[i, j, 0] = [x, -h, u * half_w(t)]
        B[i, j, 1] = [x, h, u * half_w(t)]
for i in range(NT):
    for j in range(len(us) - 1):
        col = EDGE if j == 0 or j == len(us) - 2 else STEEL
        a, b, c, d = B[i, j, 1], B[i + 1, j, 1], B[i + 1, j + 1, 1], B[i, j + 1, 1]
        sword.quad(a, b, c, d, col)
        a, b, c, d = B[i, j, 0], B[i, j + 1, 0], B[i + 1, j + 1, 0], B[i + 1, j, 0]
        sword.quad(a, b, c, d, col)
# the ridge's glow: a thin strip down both faces
for s in (-1, 1):
    for i in range(1, int(NT * 0.8)):
        q = []
        for t in (ts[i], ts[i + 1]):
            x = X0 + (X1 - X0) * t
            for u in (-0.09, 0.09):
                q.append(np.array([x, s * (half_t(t, u) + 0.04), u * half_w(t)]))
        if s > 0:
            sword.quad(q[0], q[2], q[3], q[1], GLOW)
        else:
            sword.quad(q[0], q[1], q[3], q[2], GLOW)

# --- one chain link: a stadium of iron wire, lying in the XZ plane, along +X from 0 --------------------------------
link = Mesh()
link.torus([PITCH / 2, 0, 0], 1.6, 0.6, IRON, stretch=1.9, n1=20, n2=6)

# --- the mace: eye at the origin, head along +X -----------------------------------------------------------------------
mace = Mesh()
mace.torus([0, 0, 0], 1.9, 0.7, GOLD, ax=(1, 0, 0), ay=(0, 1, 0))  # the eye, turned 90 degrees to the last link
mace.lathe(1.8, 4.0, 1.4, 2.6, GOLD, sides=10)
mace.lathe(4.0, 8.5, 2.6, 3.4, IRON, sides=10)
mace.lathe(8.5, 10.0, 4.4, 4.4, SHELL, sides=10)
BALL = np.array([19.0, 0, 0])
mace.sphere(BALL, 10.0, IRON, n=28)
dirs = [np.array([1.0, 0, 0])]  # one spike straight on, then two rings of six, tilted
for k in range(6):
    a = 2 * math.pi * k / 6
    dirs.append(np.array([0.45, math.cos(a), math.sin(a)]))
    dirs.append(np.array([-0.35, math.cos(a + math.pi / 6), math.sin(a + math.pi / 6)]))
dirs.append(np.array([-0.2, 0, 1.0]))
for d in dirs[:14]:
    d = d / np.linalg.norm(d)
    mace.spike(BALL + d * 9.2, d, 8.0, 2.4, SHELL, EDGE)

# --- write -----------------------------------------------------------------------------------------------------------
os.makedirs(OUT, exist_ok=True)
parts = {"sword": sword, "link": link, "mace": mace}
arrays = {}
for name, m in parts.items():
    a = m.array()
    arrays[name] = a
    with open(os.path.join(OUT, name + ".crabmesh"), "wb") as f:
        f.write(b"CRBM" + struct.pack("<I", len(a)) + a.tobytes())
    print(f"{name}: {len(a) // 3} triangles, extent {a['p'].min(0).round(1)} .. {a['p'].max(0).round(1)} cm")

# the whole thing laid out straight for viewing: the sword, then 24 links down -X from the eye, then the mace
EYE = np.array([-15.2 - 1.9, 0, 0])
NLINKS = 24
layout = [(arrays["sword"], np.eye(3), np.zeros(3))]
for k in range(NLINKS):
    R = np.array([[-1, 0, 0], [0, 1, 0], [0, 0, -1.0]])  # along -X
    if k % 2:
        R = R @ np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0.0]])
    layout.append((arrays["link"], R, EYE + np.array([-k * PITCH, 0, 0])))
layout.append((arrays["mace"], np.array([[-1, 0, 0], [0, 1, 0], [0, 0, -1.0]]),
               EYE + np.array([-NLINKS * PITCH - 1.0, 0, 0])))
cols = sorted({tuple(c) for a in arrays.values() for c in a["c"]})
with open(os.path.join(OUT, "shellbreaker.mtl"), "w") as f:
    for k, c in enumerate(cols):
        f.write(f"newmtl c{k}\nKd {c[0]/255:.3f} {c[1]/255:.3f} {c[2]/255:.3f}\n")
        if c[3] > 127:
            f.write(f"Ke {c[0]/255:.3f} {c[1]/255:.3f} {c[2]/255:.3f}\n")
with open(os.path.join(OUT, "shellbreaker.obj"), "w") as f:
    f.write("# Shellbreaker: sword, chain, mace laid out straight. Metres, Y up (from UE cm, Z up)\n")
    f.write("mtllib shellbreaker.mtl\n")
    base = 1
    for a, R, t in layout:
        p = a["p"].astype(float) @ R.T + t
        n = a["n"].astype(float) @ R.T
        for v in p:
            f.write(f"v {v[0]*0.01:.5f} {v[2]*0.01:.5f} {-v[1]*0.01:.5f}\n")
        for v in n:
            f.write(f"vn {v[0]:.4f} {v[2]:.4f} {-v[1]:.4f}\n")
        for k in range(len(a) // 3):
            f.write(f"usemtl c{cols.index(tuple(a['c'][3 * k]))}\n")
            i = base + 3 * k
            f.write(f"f {i}//{i} {i+1}//{i+1} {i+2}//{i+2}\n")
        base += len(a)

# sword space -> WeaponSocket: the blade along the whip handle's direction, the grip in the claw
held = json.load(open(os.path.join(HERE, "..", "necromancer", "export", "held.json")))
d = np.array(held["dir"], float)
d /= np.linalg.norm(d)
Tw = np.array(held["T"], float)
up = Tw[:3, 2] - d * (Tw[:3, 2] @ d)
up /= np.linalg.norm(up)
T = np.eye(4)
T[:3, 0], T[:3, 1], T[:3, 2] = d, np.cross(up, d), up
json.dump({"T": T.tolist(), "eye": EYE.tolist(), "pitch": PITCH, "links_rest": NLINKS,
           "mace_eye": [0, 0, 0], "mace_reach": 19.0 + 10.0 + 8.0},
          open(os.path.join(OUT, "held.json"), "w"), indent=1)
