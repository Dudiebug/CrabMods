"""Heuristic UE4.27 cooked USkeleton -> JSON reference skeleton (names, parents, local bind transforms)."""
import json, struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
import uasset
def main(base, out):
    pk = uasset.load(base); b = open(base + ".uexp", "rb").read(); names = pk.names
    I = lambda o: struct.unpack_from("<i", b, o)[0]
    for o in range(0, len(b) - 16):
        n = I(o)
        if not (2 <= n < 1000) or I(o + 12) != -1: continue
        ok = True; bones = []
        for k in range(n):
            ni, nn, par = struct.unpack_from("<iii", b, o + 4 + 12 * k)
            if not (0 <= ni < len(names)) or nn != 0 or not (-1 <= par < k): ok = False; break
            bones.append((names[ni], par))
        if not ok: continue
        p = o + 4 + 12 * n
        if I(p) != n: continue
        poses = []
        for k in range(n):
            q = struct.unpack_from("<4f", b, p + 4 + 40 * k)
            t = struct.unpack_from("<3f", b, p + 4 + 40 * k + 16)
            s = struct.unpack_from("<3f", b, p + 4 + 40 * k + 28)
            poses.append({"q": q, "t": t, "s": s})
        json.dump([{"name": nm, "parent": pa, **ps} for (nm, pa), ps in zip(bones, poses)], open(out, "w"), indent=1)
        print("bones", n, [x[0] for x in bones][:60]); return
    sys.exit("no ref skeleton found")
if __name__ == "__main__": main(sys.argv[1], sys.argv[2])
