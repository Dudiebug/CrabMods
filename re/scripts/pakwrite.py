"""Minimal UE 4.27 pak v11 writer (uncompressed or zlib), mount ../../../, with path-hash + full directory index.

usage: pakwrite.py <out.pak> <root_dir> [--zlib]
  every file under root_dir is stored at its path relative to root_dir (e.g. CrabChampions/Content/...).
"""
import hashlib
import os
import struct
import sys
import zlib

MAGIC = 0x5A6F12E1
VERSION = 11
BLOCK = 0x10000


def fstr(s):
    if not s:
        return struct.pack("<i", 0)
    try:
        b = s.encode("ascii") + b"\0"
        return struct.pack("<i", len(b)) + b
    except UnicodeEncodeError:
        b = s.encode("utf-16le") + b"\0\0"
        return struct.pack("<i", -(len(b) // 2)) + b


def fnv64_path(path, seed):
    h = (0xCBF29CE484222325 + seed) & 0xFFFFFFFFFFFFFFFF
    for c in path.lower().encode("utf-16le"):
        h ^= c
        h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def entry_header(size, usize, method, sha, blocks, bsize):
    out = struct.pack("<qqqI", 0, size, usize, method) + sha
    if method:
        out += struct.pack("<i", len(blocks))
        for s, e in blocks:
            out += struct.pack("<qq", s, e)
    out += struct.pack("<BI", 0, bsize)
    return out


def encode_entry(off, size, usize, method, bsize, block_sizes):
    v = 0
    if bsize % 2048 == 0 and (bsize >> 11) < 0x3F:
        v |= bsize >> 11
        extra_bsize = None
    else:
        v |= 0x3F
        extra_bsize = bsize
    v |= (len(block_sizes) & 0xFFFF) << 6
    v |= (method & 0x3F) << 23
    o32, u32, s32 = off < 2**32, usize < 2**32, size < 2**32
    v |= (o32 << 31) | (u32 << 30) | (s32 << 29)
    out = struct.pack("<I", v)
    if extra_bsize is not None:
        out += struct.pack("<I", extra_bsize)
    out += struct.pack("<I" if o32 else "<Q", off)
    out += struct.pack("<I" if u32 else "<Q", usize)
    if method:
        out += struct.pack("<I" if s32 else "<Q", size)
        if not (len(block_sizes) == 1):
            out += b"".join(struct.pack("<I", b) for b in block_sizes)
    return out


def write_pak(out_path, files, use_zlib=False, mount="../../../"):
    """files: list of (pak_relative_path, bytes)."""
    methods = ["Zlib"] if use_zlib else []
    f = open(out_path, "wb")
    encoded = bytearray()
    enc_off = {}
    for path, data in files:
        off = f.tell()
        if use_zlib and len(data) > 0:
            chunks = [zlib.compress(data[i:i + BLOCK], 9) for i in range(0, len(data), BLOCK)]
            payload = b"".join(chunks)
            n = len(chunks)
            hdr_len = 53 + 4 + 16 * n
            blocks, pos = [], hdr_len
            for c in chunks:
                blocks.append((pos, pos + len(c))); pos += len(c)
            sha = hashlib.sha1(payload).digest()
            bsize = min(BLOCK, len(data))
            hdr = entry_header(len(payload), len(data), 1, sha, blocks, bsize)
            assert len(hdr) == hdr_len
            f.write(hdr + payload)
            e = encode_entry(off, len(payload), len(data), 1, bsize, [len(c) for c in chunks])
        else:
            sha = hashlib.sha1(data).digest()
            f.write(entry_header(len(data), len(data), 0, sha, [], 0) + data)
            e = encode_entry(off, len(data), len(data), 0, 0, [])
        enc_off[path] = len(encoded)
        encoded += e

    seed = zlib.crc32(os.path.basename(out_path).lower().encode("utf-32le"))
    # path hash index (+ pruned directory index = full directory index)
    dirs = {}
    for path in enc_off:
        d, fn = path.rsplit("/", 1)
        d = d + "/"
        dirs.setdefault(d, {})[fn] = enc_off[path]
        parts = d.strip("/").split("/")
        for k in range(len(parts)):
            dirs.setdefault("/".join(parts[:k]) + "/" if k else "/", {})

    def dir_blob():
        b = struct.pack("<i", len(dirs))
        for d in sorted(dirs):
            b += fstr(d) + struct.pack("<i", len(dirs[d]))
            for fn in sorted(dirs[d]):
                b += fstr(fn) + struct.pack("<i", dirs[d][fn])
        return b

    full_dir = dir_blob()
    ph = struct.pack("<i", len(enc_off))
    for path, eo in enc_off.items():
        ph += struct.pack("<Qi", fnv64_path(path, seed), eo)
    ph += full_dir

    ph_off = f.tell(); f.write(ph)
    fd_off = f.tell(); f.write(full_dir)
    index = fstr(mount) + struct.pack("<iQ", len(files), seed)
    index += struct.pack("<I", 1) + struct.pack("<QQ", ph_off, len(ph)) + hashlib.sha1(ph).digest()
    index += struct.pack("<I", 1) + struct.pack("<QQ", fd_off, len(full_dir)) + hashlib.sha1(full_dir).digest()
    index += struct.pack("<i", len(encoded)) + bytes(encoded)
    index += struct.pack("<i", 0)
    idx_off = f.tell(); f.write(index)
    footer = bytes(16) + b"\0" + struct.pack("<IiQQ", MAGIC, VERSION, idx_off, len(index)) + hashlib.sha1(index).digest()
    for k in range(5):
        footer += (methods[k].encode() if k < len(methods) else b"").ljust(32, b"\0")
    f.write(footer)
    f.close()


if __name__ == "__main__":
    out, root = sys.argv[1], sys.argv[2]
    z = "--zlib" in sys.argv
    files = []
    for dp, _, fns in os.walk(root):
        for fn in sorted(fns):
            full = os.path.join(dp, fn)
            files.append((os.path.relpath(full, root).replace(os.sep, "/"), open(full, "rb").read()))
    files.sort()
    write_pak(out, files, z)
    print("wrote", out, len(files), "files")
