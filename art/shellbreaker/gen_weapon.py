"""Shellbreaker, built from numbers: a handle in the claw, a chain from it, and a blade on the chain's end.
The crab it's for is about 0.9 m tall and 1.5 m across.

Three parts, each in its own space (UE cm), like the whip's handle, cord pieces and tip:
  handle  the grip's middle at the origin, along +X toward the chain
            -13..-9 a shell-orange end knob with three claw tips; -9..9 the leather grip; 9..12 a gold cap
            14      the eye (the chain hangs from here)
  link    one heavy chain link, along +X from 0 (the next link starts at PITCH); the game lays them down the rope sim,
          every other one turned 90 degrees
  blade   its eye at the origin, along +X: a gold cap, a shell-orange socket with two back-swept barbs, then a leaf
          blade 48 cm long, double-edged, a diamond section; the ridge down its middle glows (A = 255), so a Form can
          tint it (lightning, flame, water)

Writes export/handle.crabmesh, link.crabmesh, blade.crabmesh ("CRBM", u32 vertex count, then per vertex position,
normal, RGBA: the whip's format), export/shellbreaker.obj (all three laid out straight, to look at anywhere), and
export/held.json (handle space -> the crab's WeaponSocket, the handle's eye, the link pitch, the blade's eye).
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

PITCH = 7.5  # chain link spacing, cm


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



# --- the handle -------------------------------------------------------------------------------------------------------
handle = Mesh()
handle.lathe(-13.01, -13, 0.0, 2.0, SHELL, sides=12)  # the end knob, with three claw tips swept back
handle.lathe(-13, -11, 2.0, 4.0, SHELL, sides=12)
handle.lathe(-11, -9, 4.0, 2.8, SHELL, sides=12)
for k in range(3):
    a = 2 * math.pi * k / 3
    handle.spike(np.array([-11.0, 3.5 * math.cos(a), 3.5 * math.sin(a)]), np.array([-0.6, math.cos(a), math.sin(a)]),
                 5.0, 1.4, SHELL, SHELL)
for k in range(6):  # the grip: leather wraps
    x = -9 + 3 * k
    handle.lathe(x, x + 1.5, 2.6, 2.95, LEATHER, twist=0.25 * k)
    handle.lathe(x + 1.5, x + 3, 2.95, 2.6, LEATHER, twist=0.25 * k)
handle.lathe(9, 10.5, 2.6, 3.6, GOLD, sides=12)  # the gold cap
handle.lathe(10.5, 12, 3.6, 1.8, GOLD, sides=12)
handle.lathe(12, 12.01, 1.8, 0.0, GOLD, sides=12)
handle.torus([14.1, 0, 0], 1.9, 0.7, GOLD)  # the eye, in the XZ plane

# --- one chain link: a stadium of heavy iron wire, lying in the XZ plane, along +X from 0 --------------------------
link = Mesh()
link.torus([PITCH / 2, 0, 0], 2.4, 0.95, IRON, stretch=2.9, n1=20, n2=8)

# --- the blade: its eye at the origin, along +X -----------------------------------------------------------------------
NT, NU = 30, 8
ts = np.linspace(0, 1, NT + 1)
X0, X1, WS = 11.0, 59.0, 0.85


def half_w(t):
    # 7 cm at the socket, swelling to 8.5 past halfway, then a short point
    body = (8.5 + 1.5 * math.sin(math.pi * min(t / 0.7, 1.0) * 0.9)) * WS
    return body * (1 - max(0.0, (t - 0.78) / 0.22) ** 1.2) if t < 1 else 0.0


def half_t(t, u):  # u in -1..1 across the blade: a diamond with a raised ridge
    return (2.0 * (1 - 0.4 * t)) * (1 - abs(u) ** 1.2) + 0.08


blade = Mesh()
blade.torus([0, 0, 0], 1.9, 0.7, GOLD, ax=(1, 0, 0), ay=(0, 1, 0))  # the eye, turned 90 degrees to the last link
blade.lathe(1.79, 1.8, 0.0, 1.8, GOLD, sides=12)
blade.lathe(1.8, 4.8, 1.8, 2.6, GOLD, sides=12)
blade.lathe(4.8, 11.8, 2.6, 4.2, SHELL, sides=12)  # the socket, flaring to the blade
blade.lathe(11.8, 11.81, 4.2, 0.0, SHELL, sides=12)
for s in (-1, 1):  # barbs, swept back toward the chain
    blade.spike(np.array([11.0, 0, s * 3.0]), np.array([-0.8, 0, s * 0.6]), 8.0, 1.8, SHELL, EDGE)
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
        blade.quad(B[i, j, 1], B[i + 1, j, 1], B[i + 1, j + 1, 1], B[i, j + 1, 1], col)
        blade.quad(B[i, j, 0], B[i, j + 1, 0], B[i + 1, j + 1, 0], B[i + 1, j, 0], col)
# the ridge's glow: a thin strip down both faces
for s in (-1, 1):
    for i in range(1, int(NT * 0.8)):
        q = []
        for t in (ts[i], ts[i + 1]):
            x = X0 + (X1 - X0) * t
            for u in (-0.09, 0.09):
                q.append(np.array([x, s * (half_t(t, u) + 0.04), u * half_w(t)]))
        blade.quad(*((q[0], q[2], q[3], q[1]) if s > 0 else (q[0], q[1], q[3], q[2])), GLOW)

# --- write -----------------------------------------------------------------------------------------------------------
os.makedirs(OUT, exist_ok=True)
parts = {"handle": handle, "link": link, "blade": blade}
arrays = {}
for name, m in parts.items():
    a = m.array()
    arrays[name] = a
    with open(os.path.join(OUT, name + ".crabmesh"), "wb") as f:
        f.write(b"CRBM" + struct.pack("<I", len(a)) + a.tobytes())
    print(f"{name}: {len(a) // 3} triangles, extent {a['p'].min(0).round(1)} .. {a['p'].max(0).round(1)} cm")

# the whole thing laid out straight for viewing: the handle, then 20 links along +X from its eye, then the blade
EYE = np.array([14.1 + 1.9, 0, 0])
NLINKS = 20
layout = [(arrays["handle"], np.eye(3), np.zeros(3))]
for k in range(NLINKS):
    R = np.eye(3) if k % 2 == 0 else np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0.0]])
    layout.append((arrays["link"], R, EYE + np.array([k * PITCH, 0, 0])))
layout.append((arrays["blade"], np.eye(3), EYE + np.array([NLINKS * PITCH + 1.5, 0, 0])))
cols = sorted({tuple(c) for a in arrays.values() for c in a["c"]})
with open(os.path.join(OUT, "shellbreaker.mtl"), "w") as f:
    for k, c in enumerate(cols):
        f.write(f"newmtl c{k}\nKd {c[0]/255:.3f} {c[1]/255:.3f} {c[2]/255:.3f}\n")
        if c[3] > 127:
            f.write(f"Ke {c[0]/255:.3f} {c[1]/255:.3f} {c[2]/255:.3f}\n")
with open(os.path.join(OUT, "shellbreaker.obj"), "w") as f:
    f.write("# Shellbreaker: handle, chain, blade laid out straight. Metres, Y up (from UE cm, Z up)\n")
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

# handle space -> WeaponSocket: the handle along the whip handle's direction, the grip in the claw
held = json.load(open(os.path.join(HERE, "..", "necromancer", "export", "held.json")))
d = np.array(held["dir"], float)
d /= np.linalg.norm(d)
Tw = np.array(held["T"], float)
up = Tw[:3, 2] - d * (Tw[:3, 2] @ d)
up /= np.linalg.norm(up)
T = np.eye(4)
T[:3, 0], T[:3, 1], T[:3, 2] = d, np.cross(up, d), up
json.dump({"T": T.tolist(), "eye": EYE.tolist(), "pitch": PITCH, "links_rest": NLINKS,
           "blade_eye": [0, 0, 0], "blade_reach": X1},
          open(os.path.join(OUT, "held.json"), "w"), indent=1)
