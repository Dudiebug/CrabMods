"""Minimal UE 4.27 cooked .uasset/.uexp tagged-property reader (no unversioned props)."""
import json
import struct
import sys

from pak import R


def fname(r, names):
    i, n = r.i32(), r.i32()
    s = names[i] if 0 <= i < len(names) else f"<bad{i}>"
    return s if n == 0 else f"{s}_{n - 1}"


class Package:
    def __init__(self, uasset, uexp):
        r = R(uasset)
        assert r.u32() == 0x9E2A83C1
        legacy = r.i32()
        if legacy != -4:
            r.i32()
        self.ue4ver = r.i32(); r.i32()
        for _ in range(r.i32()):
            r.raw(20)
        self.header_size = r.i32()
        r.fstr()
        self.pkgflags = r.u32()
        name_count, name_off = r.i32(), r.i32()
        if not self.pkgflags & 0x80000000:
            r.fstr()
        r.i32(); r.i32()
        export_count, export_off, import_count, import_off = r.i32(), r.i32(), r.i32(), r.i32()
        nr = R(uasset, name_off)
        self.names = []
        for _ in range(name_count):
            self.names.append(nr.fstr()); nr.u32()
        ir = R(uasset, import_off)
        self.imports = []
        for _ in range(import_count):
            cp, cn = fname(ir, self.names), fname(ir, self.names)
            outer = ir.i32()
            on = fname(ir, self.names)
            self.imports.append({"class": cn, "outer": outer, "name": on})
        er = R(uasset, export_off)
        self.exports = []
        for _ in range(export_count):
            ci, si, ti, oi = er.i32(), er.i32(), er.i32(), er.i32()
            on = fname(er, self.names)
            er.u32()
            ssize, soff = er.u64(), er.u64()
            er.raw(4 * 3 + 16 + 4 + 4 + 4 + 4 * 5)
            self.exports.append({"class": ci, "outer": oi, "name": on, "size": ssize, "off": soff})
        self.uexp = uexp

    def ref(self, idx):
        if idx < 0:
            imp = self.imports[-idx - 1]
            outer = self.ref(imp["outer"]) if imp["outer"] else ""
            return (outer + "." if outer else "") + imp["name"]
        if idx > 0:
            return self.exports[idx - 1]["name"]
        return None

    def export_props(self, i):
        e = self.exports[i]
        r = R(self.uexp, e["off"] - self.header_size)
        return self.props(r, r.o + e["size"])

    # ------------------------------------------------------------------ tagged props
    def props(self, r, end):
        out = {}
        while r.o < end:
            name = fname(r, self.names)
            if name == "None":
                break
            typ = fname(r, self.names)
            size, idx = r.i32(), r.i32()
            tag = {"type": typ}
            if typ == "StructProperty":
                tag["struct"] = fname(r, self.names); r.raw(16)
            elif typ == "BoolProperty":
                tag["bool"] = r.raw(1)[0]
            elif typ in ("ByteProperty", "EnumProperty"):
                tag["enum"] = fname(r, self.names)
            elif typ in ("ArrayProperty", "SetProperty"):
                tag["inner"] = fname(r, self.names)
            elif typ == "MapProperty":
                tag["inner"] = fname(r, self.names); tag["value"] = fname(r, self.names)
            if r.raw(1)[0]:
                r.raw(16)
            start = r.o
            try:
                v = self.value(r, tag, size, top=True)
            except Exception as ex:  # keep going on unknown encodings
                v = f"<{typ} {size}b: {ex}>"
            r.o = start + size
            key = name if idx == 0 else f"{name}[{idx}]"
            out[key] = v
        return out

    def value(self, r, tag, size, top=False):
        t = tag["type"]
        if t == "BoolProperty":
            return bool(tag["bool"]) if top else bool(r.raw(1)[0])
        if t == "IntProperty":
            return r.i32()
        if t == "UInt32Property":
            return r.u32()
        if t == "Int64Property":
            return struct.unpack("<q", r.raw(8))[0]
        if t == "FloatProperty":
            return round(struct.unpack("<f", r.raw(4))[0], 6)
        if t == "DoubleProperty":
            return struct.unpack("<d", r.raw(8))[0]
        if t == "ByteProperty":
            if tag.get("enum") and tag["enum"] != "None":
                return fname(r, self.names)
            return r.raw(1)[0]
        if t in ("EnumProperty", "NameProperty"):
            return fname(r, self.names)
        if t == "StrProperty":
            return r.fstr()
        if t == "TextProperty":
            return self.text(r)
        if t in ("ObjectProperty", "ClassProperty", "InterfaceProperty", "WeakObjectProperty"):
            return self.ref(r.i32())
        if t in ("SoftObjectProperty", "SoftClassProperty"):
            p = fname(r, self.names); s = r.fstr()
            return p + (":" + s if s else "")
        if t == "StructProperty":
            return self.struct(r, tag.get("struct"), size)
        if t in ("ArrayProperty", "SetProperty"):
            if t == "SetProperty":
                r.i32()
            n = r.i32()
            inner = tag["inner"]
            if inner == "StructProperty":
                fname(r, self.names); fname(r, self.names)
                isz = r.i32(); r.i32()
                sname = fname(r, self.names); r.raw(16)
                if r.raw(1)[0]:
                    r.raw(16)
                return [self.struct(r, sname, isz // max(n, 1)) for _ in range(n)]
            return [self.value(r, {"type": inner}, 0) for _ in range(n)]
        if t == "MapProperty":
            r.i32()
            n = r.i32()
            return {str(self.value(r, {"type": tag["inner"]}, 0)): self.value(r, {"type": tag["value"]}, 0) for _ in range(n)}
        return f"<{t}>"

    NATIVE = {"Vector": "<3f", "Vector2D": "<2f", "Rotator": "<3f", "LinearColor": "<4f", "Color": "<4B",
              "IntPoint": "<2i", "Quat": "<4f", "Vector4": "<4f"}

    def struct(self, r, sname, size):
        if sname in self.NATIVE:
            fmt = self.NATIVE[sname]
            return [round(x, 4) if isinstance(x, float) else x for x in struct.unpack(fmt, r.raw(struct.calcsize(fmt)))]
        if sname == "GameplayTagContainer":
            return [fname(r, self.names) for _ in range(r.i32())]
        if sname == "GameplayTag":
            return fname(r, self.names)
        if sname == "Guid":
            return r.raw(16).hex()
        if sname == "SoftObjectPath":
            p = fname(r, self.names); s = r.fstr(); return p + (":" + s if s else "")
        return self.props(r, len(self.uexp))

    def text(self, r):
        r.u32()
        h = struct.unpack("<b", r.raw(1))[0]
        if h == -1:
            return r.fstr() if r.u32() else ""
        if h == 0:
            ns, key, src = r.fstr(), r.fstr(), r.fstr()
            return src
        return f"<text history {h}>"


def load(path_noext):
    return Package(open(path_noext + ".uasset", "rb").read(), open(path_noext + ".uexp", "rb").read())


if __name__ == "__main__":
    for p in sys.argv[1:]:
        pk = load(p[:-7] if p.endswith(".uasset") else p)
        res = {e["name"]: pk.export_props(i) for i, e in enumerate(pk.exports)}
        print(json.dumps({p.split("/")[-1]: res}, indent=1, default=str))
