"""Heuristic UE4.27 cooked SkeletalMesh -> OBJ (LOD0 positions, UVs, indices).
Finds FPositionVertexBuffer (stride 12, N | bulk 12, N), the index container
(u8 2|4 then bulk elem, count) and the UV bulk following the tangent bulk."""
import struct, sys
def main(uexp, out):
    b = open(uexp, "rb").read()
    I = lambda o: struct.unpack_from("<I", b, o)[0]
    pos = []
    for o in range(0, len(b) - 16, 1):
        if b[o:o+4] == b"\x0c\0\0\0" and b[o+8:o+12] == b"\x0c\0\0\0":
            n = I(o + 4)
            if 3 <= n < 500000 and I(o + 12) == n and o + 16 + 12 * n <= len(b):
                pos.append((o, n))
    if not pos: sys.exit("no position buffer")
    po, n = max(pos, key=lambda t: t[1])          # LOD0 = most verts
    verts = [struct.unpack_from("<3f", b, po + 16 + 12 * i) for i in range(n)]
    # index container: search backwards from position buffer
    best = None
    for o in range(po - 9, max(0, po - 4000000), -1):
        ds = b[o]
        if ds in (2, 4) and I(o + 1) == ds:
            cnt = I(o + 5)
            if cnt % 3 == 0 and 3 <= cnt < 3000000 and o + 9 + ds * cnt <= po:
                fmt = "<%d%s" % (cnt, "H" if ds == 2 else "I")
                idx = struct.unpack_from(fmt, b, o + 9)
                if max(idx) < n and max(idx) > n * 0.9:
                    best = idx; break
    if best is None: sys.exit("no index buffer")
    # UVs: after position buffer -> u32 numTex, u32 n, u8 fullUV(as u32 bool), u32 hiTan bool, tangent bulk, uv bulk
    uvs = None
    o = po + 16 + 12 * n
    for k in range(o, o + 64):
        if I(k + 4) == n and I(k) in (1, 2, 3, 4):
            ntex = I(k); full = I(k + 8); hi = I(k + 12); t = k + 16
            tes, tc = I(t), I(t + 4)
            if tc == n and tes in (8, 16):
                u = t + 8 + tes * n; ues, uc = I(u), I(u + 4)
                if uc == n:
                    if ues == 4 * ntex:   # half floats
                        uvs = [struct.unpack_from("<2e", b, u + 8 + ues * i) for i in range(n)]
                    elif ues == 8 * ntex:
                        uvs = [struct.unpack_from("<2f", b, u + 8 + ues * i) for i in range(n)]
                break
    with open(out, "w") as f:
        for x, y, z in verts: f.write("v %f %f %f\n" % (x, z, y))   # UE (x fwd, y right, z up) -> Blender-ish
        if uvs:
            for u, v in uvs: f.write("vt %f %f\n" % (u, 1 - v))
        for i in range(0, len(best), 3):
            a, c, d = best[i] + 1, best[i+1] + 1, best[i+2] + 1
            f.write(("f %d/%d %d/%d %d/%d\n" % (a, a, d, d, c, c)) if uvs else ("f %d %d %d\n" % (a, d, c)))
    print(uexp.split("/")[-1], "verts", n, "tris", len(best) // 3, "uvs", bool(uvs))
if __name__ == "__main__": main(sys.argv[1], sys.argv[2])
