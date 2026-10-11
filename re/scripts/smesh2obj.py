"""Heuristic UE4.27 cooked StaticMesh LOD0 -> OBJ (positions + FRawStaticIndexBuffer)."""
import struct, sys
def main(uexp, out):
    b = open(uexp, "rb").read(); I = lambda o: struct.unpack_from("<I", b, o)[0]; best = None
    for o in range(len(b) - 16):
        if b[o:o+4] == b"\x0c\0\0\0" and b[o+8:o+12] == b"\x0c\0\0\0":
            n = I(o + 4)
            if 3 <= n < 500000 and I(o + 12) == n and o + 16 + 12 * n <= len(b) and (best is None or n > best[1]): best = (o, n)
    po, n = best; v = [struct.unpack_from("<3f", b, po + 16 + 12 * i) for i in range(n)]
    idx = None; o = po + 16 + 12 * n
    for k in range(o, min(len(b) - 12, o + 400000)):
        w = I(k)
        if w in (0, 1) and I(k + 4) == 1:
            cnt = I(k + 8); es = 4 if w else 2
            if cnt and cnt % (es * 3) == 0 and k + 12 + cnt <= len(b):
                ids = struct.unpack_from("<%d%s" % (cnt // es, "I" if w else "H"), b, k + 12)
                if max(ids) == n - 1: idx = ids; break
    with open(out, "w") as f:
        for x, y, z in v: f.write("v %f %f %f\n" % (x, z, y))
        for i in range(0, len(idx), 3): f.write("f %d %d %d\n" % (idx[i] + 1, idx[i+2] + 1, idx[i+1] + 1))
    print(uexp.split("/")[-1], n, len(idx) // 3)
if __name__ == "__main__": main(sys.argv[1], sys.argv[2])
