"""Minimal UE 4.27 (pak v11) reader: list + extract with Oodle (libooz) / zlib."""
import ctypes
import os
import struct
import sys
import zlib

OOZ = ctypes.CDLL(os.path.expanduser(os.environ.get("LIBOOZ", "~/.local/opt/ooz-linux/libooz.so")))
OOZ.ooz_decompress.restype = ctypes.c_int
OOZ.ooz_decompress.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_size_t]


class R:
    def __init__(self, b, o=0):
        self.b, self.o = b, o

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.o)[0]; self.o += 4; return v

    def i32(self):
        v = struct.unpack_from("<i", self.b, self.o)[0]; self.o += 4; return v

    def u64(self):
        v = struct.unpack_from("<Q", self.b, self.o)[0]; self.o += 8; return v

    def raw(self, n):
        v = self.b[self.o:self.o + n]; self.o += n; return v

    def fstr(self):
        n = self.i32()
        if n == 0:
            return ""
        if n < 0:
            s = self.raw(-n * 2).decode("utf-16le"); return s.rstrip("\0")
        return self.raw(n).rstrip(b"\0").decode("utf-8", "replace")


class Pak:
    def __init__(self, path):
        self.f = open(path, "rb")
        sz = os.path.getsize(path)
        self.f.seek(sz - 0x200)
        tail = self.f.read(0x200)
        i = tail.rfind(struct.pack("<I", 0x5A6F12E1))
        _, self.ver, ioff, isz = struct.unpack_from("<IIQQ", tail, i)
        names = tail[i + 44:i + 44 + 32 * 5]
        self.methods = [None] + [names[k * 32:(k + 1) * 32].rstrip(b"\0").decode() for k in range(5)]
        self.f.seek(ioff)
        r = R(self.f.read(isz))
        self.mount = r.fstr()
        self.count = r.i32()
        r.u64()
        if r.u32():
            r.u64(); r.u64(); r.raw(20)
        if not r.u32():
            raise RuntimeError("no full directory index")
        fdi_off, fdi_size = r.u64(), r.u64(); r.raw(20)
        self.enc = r.raw(r.i32())
        self.f.seek(fdi_off)
        d = R(self.f.read(fdi_size))
        self.files = {}
        for _ in range(d.i32()):
            dname = d.fstr()
            for _ in range(d.i32()):
                fname = d.fstr()
                off = d.i32()
                self.files[(self.mount + dname + fname).replace("../../../", "")] = off

    def entry(self, encoff):
        r = R(self.enc, encoff)
        v = r.u32()
        bsize = r.u32() if (v & 0x3F) == 0x3F else (v & 0x3F) << 11
        method = (v >> 23) & 0x3F
        off = r.u32() if v & (1 << 31) else r.u64()
        usize = r.u32() if v & (1 << 30) else r.u64()
        size = (r.u32() if v & (1 << 29) else r.u64()) if method else usize
        enc = bool(v & (1 << 22))
        nblocks = (v >> 6) & 0xFFFF
        hdr = 53 + (4 + 16 * nblocks if method else 0)
        blocks = []
        if nblocks:
            if nblocks == 1 and not enc:
                blocks = [size]
            else:
                blocks = [r.u32() for _ in range(nblocks)]
        return dict(off=off, size=size, usize=usize, method=method, bsize=bsize, blocks=blocks, hdr=hdr, enc=enc)

    def read(self, path):
        e = self.entry(self.files[path])
        assert not e["enc"]
        self.f.seek(e["off"] + e["hdr"])
        if not e["method"]:
            return self.f.read(e["usize"])
        out = bytearray()
        left = e["usize"]
        for bs in e["blocks"]:
            src = self.f.read(bs)
            n = min(e["bsize"], left)
            m = self.methods[e["method"]]
            if m == "Oodle":
                dst = ctypes.create_string_buffer(n + 64)
                got = OOZ.ooz_decompress(src, len(src), dst, n)
                if got != n:
                    raise RuntimeError(f"oodle failed {path}")
                out += dst.raw[:n]
            else:
                out += zlib.decompress(src)
            left -= n
        return bytes(out)


if __name__ == "__main__":
    p = Pak(sys.argv[1])
    cmd = sys.argv[2]
    if cmd == "list":
        for k in sorted(p.files):
            print(k)
    elif cmd == "extract":
        outdir, pat = sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else ""
        n = 0
        for k in p.files:
            if pat in k:
                dst = os.path.join(outdir, k)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                open(dst, "wb").write(p.read(k))
                n += 1
        print("extracted", n)
