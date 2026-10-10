"""Static UE 4.27 reflection extractor.

Walks the UE4CodeGen_Private param tables that UHT compiles into a monolithic
shipping exe (FClassParams / FStructParams / FEnumParams / FFunctionParams /
FPropertyParams) and reconstructs the full reflected SDK: class names, sizes,
super classes, property offsets/types/flags, UFunctions with params and flags.
No game process required.
"""
import json
import struct
import sys
from collections import defaultdict

import pefile
from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

EXE = sys.argv[1] if len(sys.argv) > 1 else "game.exe"
OUT = sys.argv[2] if len(sys.argv) > 2 else "out"

pe = pefile.PE(EXE, fast_load=True)
BASE = pe.OPTIONAL_HEADER.ImageBase
img = bytearray(pe.get_memory_mapped_image())
SECS = {s.Name.rstrip(b"\0").decode(): (BASE + s.VirtualAddress, BASE + s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)) for s in pe.sections}
TEXT = SECS[".text"]
RDATA = SECS[".rdata"]
DATA = SECS[".data"]


def in_sec(va, sec):
    return sec[0] <= va < sec[1]


def is_text(va):
    return in_sec(va, TEXT)


def is_rd(va):
    return in_sec(va, RDATA) or in_sec(va, DATA)


def u8(va):
    return img[va - BASE]


def u32(va):
    return struct.unpack_from("<I", img, va - BASE)[0]


def i32(va):
    return struct.unpack_from("<i", img, va - BASE)[0]


def u64(va):
    return struct.unpack_from("<Q", img, va - BASE)[0]


_cstr = {}


def cstr(va, maxlen=256):
    """Return printable UTF-8 C string at va, or None."""
    if va in _cstr:
        return _cstr[va]
    r = None
    if is_rd(va):
        off = va - BASE
        end = img.find(b"\0", off, off + maxlen)
        if end > off:
            b = bytes(img[off:end])
            if all(0x20 <= c < 0x7F for c in b):
                r = b.decode()
    _cstr[va] = r
    return r


def ident(va):
    s = cstr(va)
    if s and (s[0].isalpha() or s[0] == "_") and all(c.isalnum() or c in "_ -?()<>:/.'&+,%#[]!" for c in s):
        return s
    return None


def wstr(va, maxlen=256):
    if not is_rd(va):
        return None
    off = va - BASE
    out = []
    for i in range(maxlen):
        c = struct.unpack_from("<H", img, off + 2 * i)[0]
        if c == 0:
            break
        if not (0x20 <= c < 0x7F):
            return None
        out.append(chr(c))
    return "".join(out) if out else None


md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
_dis = {}


def refs(fva, max_insn=60, follow_jmp=2):
    """Disassemble a function; return (rip-relative targets, call targets, imm values)."""
    key = fva
    if key in _dis:
        return _dis[key]
    rip_t, calls, imms = [], [], []
    va = fva
    for _ in range(follow_jmp + 1):
        code = bytes(img[va - BASE: va - BASE + max_insn * 15])
        nxt = None
        n = 0
        for ins in md.disasm(code, va):
            n += 1
            for op in ins.operands:
                if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                    rip_t.append(ins.address + ins.size + op.mem.disp)
                elif op.type == X86_OP_IMM:
                    if ins.mnemonic == "call":
                        calls.append(op.imm)
                    elif ins.mnemonic == "jmp":
                        nxt = op.imm
                    else:
                        imms.append((ins.mnemonic, op.imm))
            if ins.mnemonic in ("ret", "int3") or n >= max_insn or ins.mnemonic == "jmp":
                break
        if nxt and is_text(nxt) and n <= 3:
            va = nxt
            continue
        break
    _dis[key] = (rip_t, calls, imms)
    return _dis[key]


# ---------------------------------------------------------------- scanning
PROP_TYPES = ["Byte", "Int8", "Int16", "Int", "Int64", "UInt16", "UInt32", "UInt64", "UnsizedInt", "UnsizedUInt",
              "Float", "Double", "Bool", "SoftClass", "WeakObject", "LazyObject", "SoftObject", "Class", "Object",
              "Interface", "Name", "Str", "Array", "Map", "Set", "Struct", "Delegate", "InlineMulticastDelegate",
              "SparseMulticastDelegate", "Text", "Enum", "FieldPath"]


def parse_prop(va):
    name = ident(u64(va))
    if not name:
        return None
    rn = u64(va + 8)
    repnotify = ident(rn) if rn else None
    if rn and not repnotify:
        return None
    pflags = u64(va + 0x10)
    gen = u8(va + 0x18)
    t = gen & 0x1F
    if gen & ~0x3F or (gen & 0x20 and t != 12):
        return None
    oflags = u32(va + 0x1C)
    dim = i32(va + 0x20)
    if not (1 <= dim <= 4096):
        return None
    p = {"name": name, "type": PROP_TYPES[t], "flags": pflags, "objflags": oflags, "dim": dim}
    if repnotify:
        p["repnotify"] = repnotify
    if t == 12:  # Bool
        p["elemsize"] = u32(va + 0x24)
        p["sizeofouter"] = u64(va + 0x28)
        setbit = u64(va + 0x30)
        p["setbit"] = setbit
        p["native"] = bool(gen & 0x20)
    else:
        p["offset"] = i32(va + 0x24)
        if t in (0, 30, 13, 15, 16, 17, 18, 19, 25, 26, 27, 28, 31, 14):
            f = u64(va + 0x28)
            if f and is_text(f):
                p["typefunc"] = f
            if t == 17:
                f2 = u64(va + 0x30)
                if f2 and is_text(f2):
                    p["typefunc2"] = f2
    return p


def parse_prop_array(arr, n):
    if not (0 < n < 5000) or not is_rd(arr):
        return None
    out = []
    for i in range(n):
        pv = u64(arr + 8 * i)
        if not is_rd(pv):
            return None
        p = parse_prop(pv)
        if not p:
            return None
        p["_va"] = pv
        out.append(p)
    return out


classes, structs, enums, funcs = {}, {}, {}, {}

lo, hi = RDATA[0], DATA[1]
print("scanning", hex(lo), hex(hi), file=sys.stderr)
for va in range(lo, hi - 0x60, 8):
    q0 = u64(va)
    if not is_text(q0):
        continue
    q1, q2, q3 = u64(va + 8), u64(va + 16), u64(va + 24)
    # FClassParams: NoRegisterFunc, ConfigName, CppClassInfo, Deps, FuncLinks, Props, Ifaces, 4x int32, flags
    if (q1 == 0 or (is_rd(q1) and ident(q1))) and is_rd(q2):
        deps, fl, pr, it = q3, u64(va + 32), u64(va + 40), u64(va + 48)
        nd, nf, np_, ni = i32(va + 56), i32(va + 60), i32(va + 64), i32(va + 68)
        if (0 <= nd < 64 and u32(va + 72) & 0x80 and (q1 or nd > 0) and 0 <= nf < 2000 and 0 <= np_ < 2000 and 0 <= ni < 64
                and (nd == 0) == (deps == 0) and (nf == 0) == (fl == 0) and (np_ == 0) == (pr == 0)
                and (ni == 0) == (it == 0) and all(x == 0 or is_rd(x) for x in (deps, fl, pr, it))):
            props = parse_prop_array(pr, np_) if np_ else []
            links = []
            ok = props is not None
            for i in range(nf if ok else 0):
                f, n = u64(fl + 16 * i), u64(fl + 16 * i + 8)
                if not (is_text(f) and ident(n)):
                    ok = False
                    break
                links.append((f, ident(n)))
            if ok:
                classes[va] = {"noreg": q0, "config": ident(q1) if q1 else None, "flags": u32(va + 72), "props": props, "funclinks": links,
                               "deps": [u64(deps + 8 * i) for i in range(nd)],
                               "ifaces": [u64(it + 16 * i) for i in range(ni)]}
                continue
    # FStructParams: Outer, Super, StructOps, Name, SizeOf, AlignOf, Props, NumProps, ObjFlags, StructFlags
    if (q1 == 0 or is_text(q1)) and (q2 == 0 or is_text(q2)) and is_rd(q3) and ident(q3):
        size, align, pr, np_ = u64(va + 32), u64(va + 40), u64(va + 48), i32(va + 56)
        if 0 < size < 0x100000 and align in (1, 2, 4, 8, 16, 32, 64) and 0 <= np_ < 2000 and (np_ == 0) == (pr == 0):
            props = parse_prop_array(pr, np_) if np_ else []
            if props is not None:
                structs[va] = {"name": ident(q3), "outer": q0, "super": q1, "size": size, "align": align,
                               "props": props, "flags": u32(va + 64)}
                continue
    # FEnumParams: Outer, DisplayNameFunc, Name, CppType, Enumerators, Num, ObjFlags, Flags(u8), CppForm(u8)
    if (q1 == 0 or is_text(q1)) and ident(q2) and ident(q3):
        ep, ne = u64(va + 32), i32(va + 40)
        if is_rd(ep) and 0 < ne < 2000:
            vals = []
            for i in range(ne):
                n = ident(u64(ep + 16 * i))
                if not n:
                    vals = None
                    break
                vals.append((n, struct.unpack_from("<q", img, ep + 16 * i + 8 - BASE)[0]))
            if vals:
                enums[va] = {"name": ident(q2), "cpp": ident(q3), "outer": q0, "values": vals}
                continue
    # FFunctionParams: Outer, Super, Name, OwningClassName, DelegateName, StructSize, Props, NumProps,
    #                  ObjFlags, FuncFlags, RPCId, RPCResponseId
    if (q1 == 0 or is_text(q1)) and ident(q2):
        oc, dn = q3, u64(va + 32)
        if (oc == 0 or ident(oc)) and (dn == 0 or ident(dn)):
            ssize, pr, np_ = u64(va + 40), u64(va + 48), i32(va + 56)
            if ssize < 0x10000 and 0 <= np_ < 200 and (np_ == 0) == (pr == 0) and (ssize == 0) <= (np_ == 0):
                props = parse_prop_array(pr, np_) if np_ else []
                if props is not None:
                    funcs[va] = {"name": ident(q2), "outer": q0, "super": q1, "owning": ident(oc) if oc else None,
                                 "delegate": ident(dn) if dn else None, "parmsize": ssize, "props": props,
                                 "objflags": u32(va + 60), "flags": u32(va + 64), "rpcid": struct.unpack_from("<H", img, va + 68 - BASE)[0]}

print(f"raw: classes={len(classes)} structs={len(structs)} enums={len(enums)} funcs={len(funcs)}", file=sys.stderr)

# ---------------------------------------------------------------- name resolution
params_at = {}
for d, kind in ((classes, "class"), (structs, "struct"), (enums, "enum"), (funcs, "func")):
    for va in d:
        params_at[va] = kind


def static_class_info(fva):
    """Resolve X::StaticClass -> (pkg, name, size, superfunc)."""
    rip_t, calls, imms = refs(fva, max_insn=80, follow_jmp=3)
    ws = [w for w in (wstr(t) for t in rip_t) if w]
    pkg = next((w for w in ws if w.startswith("/Script/")), None)
    name = next((w for w in ws if not w.startswith("/") and w[:1].isalnum()), None)
    # sizeof is passed as an immediate (mov r9d/ dword [rsp+..], imm)
    sizes = [v for m, v in imms if m == "mov" and 0x20 <= v < 0x200000]
    supers = [t for t in rip_t if is_text(t)]
    return pkg, name, (sizes[0] if sizes else None), supers


def construct_target(fva):
    """Given a Z_Construct_* function, find the params struct it constructs."""
    rip_t, calls, imms = refs(fva, max_insn=40, follow_jmp=3)
    for t in rip_t:
        if t in params_at:
            return t
    return None


func_name_cache = {}


def type_name(fva):
    """Resolve the name of the type a Z_Construct / StaticClass function returns."""
    if fva in func_name_cache:
        return func_name_cache[fva]
    func_name_cache[fva] = None
    r = None
    t = construct_target(fva)
    if t is not None:
        k = params_at[t]
        if k == "class":
            r = class_name(t)
        elif k == "struct":
            r = "F" + structs[t]["name"]
        elif k == "enum":
            r = enums[t]["cpp"] or enums[t]["name"]
        elif k == "func":
            r = funcs[t]["name"]
    else:
        pkg, name, _, _ = static_class_info(fva)
        if name:
            r = name
        else:
            # NoRegister wrappers: call StaticClass
            rip_t, calls, imms = refs(fva, max_insn=12, follow_jmp=3)
            for c in calls:
                pkg, name, _, _ = static_class_info(c)
                if name:
                    r = name
                    break
    func_name_cache[fva] = r
    return r


_cn = {}


def class_name(cva):
    if cva in _cn:
        return _cn[cva]
    c = classes[cva]
    pkg, name, size, supers = static_class_info(c["noreg"])
    c["pkg"], c["size"] = pkg, size
    full = None
    if name:
        # StaticClass passes TEXT("AFoo")+1 → the string we resolved starts after the prefix char.
        full = name
    _cn[cva] = full
    c["name"] = full
    c["_supers"] = supers
    return full


for cva in classes:
    class_name(cva)

# Resolve class name prefix from the wide string (UHT passes name+1, so look one char back).
for cva, c in classes.items():
    rip_t, _, _ = refs(c["noreg"], max_insn=80, follow_jmp=3)
    for t in rip_t:
        w = wstr(t)
        if w and c["name"] and w == c["name"]:
            pre = wstr(t - 2)
            if pre and len(pre) == len(w) + 1 and pre[0] in "AUI":
                c["cpp"] = pre
            break
    c.setdefault("cpp", ("U" if c["name"] else "?") + (c["name"] or "?"))

CPP_BY_NAME = {c["name"]: c["cpp"] for c in classes.values() if c.get("name")}

# Super class: ClassParams deps[0] is Super's Z_Construct_UClass.
for cva, c in classes.items():
    sn = type_name(c["deps"][0]) if c["deps"] else None
    c["super"] = CPP_BY_NAME.get(sn, sn)

# Function owner: FFunctionParams.Outer is the owning class's Z_Construct_UClass.
link_by_func = {}
for cva, c in classes.items():
    for f, n in c["funclinks"]:
        t = construct_target(f)
        if t in funcs:
            link_by_func[t] = (cva, n)


def prop_type(p):
    t = p["type"]
    tf = p.get("typefunc")
    tn = type_name(tf) if tf else None
    tn = CPP_BY_NAME.get(tn, tn)
    if t == "Struct":
        return tn or "F?"
    if t in ("Object", "WeakObject", "LazyObject", "SoftObject", "Interface"):
        base = (("A" if False else "") + (tn or "UObject"))
        return {"Object": f"{base}*", "WeakObject": f"TWeakObjectPtr<{base}>", "LazyObject": f"TLazyObjectPtr<{base}>",
                "SoftObject": f"TSoftObjectPtr<{base}>", "Interface": f"TScriptInterface<{base}>"}[t]
    if t == "Class":
        return f"TSubclassOf<{tn or 'UObject'}>"
    if t == "SoftClass":
        return f"TSoftClassPtr<{tn or 'UObject'}>"
    if t in ("Byte", "Enum"):
        return tn or ("uint8" if t == "Byte" else "enum?")
    if t in ("Delegate", "InlineMulticastDelegate", "SparseMulticastDelegate"):
        return f"{t}<{tn}>"
    return {"Int8": "int8", "Int16": "int16", "Int": "int32", "Int64": "int64", "UInt16": "uint16", "UInt32": "uint32",
            "UInt64": "uint64", "Float": "float", "Double": "double", "Bool": "bool", "Name": "FName", "Str": "FString",
            "Text": "FText", "Array": "TArray", "Map": "TMap", "Set": "TSet", "FieldPath": "TFieldPath"}.get(t, t)


def fold_props(props):
    """UHT emits container inner/key/value and enum underlying props immediately
    before their owner, so a stack recovers the tree."""
    stack = []
    for p in props:
        p = dict(p)
        p["ctype"] = prop_type(p)
        t = p["type"]
        if t in ("Array", "Set") and stack:
            inner = stack.pop()
            p["ctype"] = f"{'TArray' if t == 'Array' else 'TSet'}<{inner['ctype']}>"
        elif t == "Map" and len(stack) >= 2:
            v, k = stack.pop(), stack.pop()
            p["ctype"] = f"TMap<{k['ctype']}, {v['ctype']}>"
        elif t == "Enum" and stack and stack[-1]["name"] == "UnderlyingType":
            stack.pop()
        if t == "Bool":
            p["offset"] = None
        stack.append(p)
    return stack


def bool_offset(p):
    """Recover a bitfield/native bool's byte offset+mask from its SetBit thunk."""
    sb = p.get("setbit")
    if not sb:
        return None
    rip_t, calls, imms = refs(sb, max_insn=8, follow_jmp=1)
    code = bytes(img[sb - BASE: sb - BASE + 40])
    for ins in md.disasm(code, sb):
        if ins.mnemonic in ("or", "mov") and ins.operands and ins.operands[0].type == X86_OP_MEM:
            m = ins.operands[0].mem
            mask = ins.operands[1].imm if len(ins.operands) > 1 and ins.operands[1].type == X86_OP_IMM else None
            return m.disp, mask
        if ins.mnemonic == "ret":
            break
    return None


CPF = {0x1: "Edit", 0x2: "ConstParm", 0x4: "BlueprintVisible", 0x8: "ExportObject", 0x10: "BlueprintReadOnly",
       0x20: "Net", 0x40: "EditFixedSize", 0x80: "Parm", 0x100: "OutParm", 0x200: "ZeroConstructor",
       0x400: "ReturnParm", 0x800: "DisableEditOnTemplate", 0x2000: "Transient", 0x4000: "Config",
       0x10000: "DisableEditOnInstance", 0x20000: "EditConst", 0x40000: "GlobalConfig", 0x80000: "InstancedReference",
       0x200000: "DuplicateTransient", 0x1000000: "SaveGame", 0x2000000: "NoClear", 0x8000000: "ReferenceParm",
       0x10000000: "BlueprintAssignable", 0x20000000: "Deprecated", 0x100000000: "RepSkip", 0x200000000: "RepNotify",
       0x400000000: "Interp", 0x800000000: "NonTransactional", 0x1000000000: "EditorOnly",
       0x4000000000: "NoDestructor", 0x10000000000: "AutoWeak", 0x20000000000: "ContainsInstancedReference",
       0x40000000000: "AssetRegistrySearchable", 0x80000000000: "SimpleDisplay", 0x100000000000: "AdvancedDisplay",
       0x200000000000: "Protected", 0x400000000000: "BlueprintCallable", 0x800000000000: "BlueprintAuthorityOnly",
       0x1000000000000: "TextExportTransient", 0x2000000000000: "NonPIEDuplicateTransient",
       0x4000000000000: "ExposeOnSpawn", 0x8000000000000: "PersistentInstance", 0x10000000000000: "UObjectWrapper",
       0x20000000000000: "HasGetValueTypeHash", 0x40000000000000: "NativeAccessSpecifierPublic",
       0x80000000000000: "NativeAccessSpecifierProtected", 0x100000000000000: "NativeAccessSpecifierPrivate",
       0x200000000000000: "SkipSerialization"}
FUNC = {0x1: "Final", 0x2: "RequiredAPI", 0x4: "BlueprintAuthorityOnly", 0x8: "BlueprintCosmetic", 0x40: "Net",
        0x80: "NetReliable", 0x100: "NetRequest", 0x200: "Exec", 0x400: "Native", 0x800: "Event", 0x1000: "NetResponse",
        0x2000: "Static", 0x4000: "NetMulticast", 0x8000: "UbergraphFunction", 0x10000: "MulticastDelegate",
        0x20000: "Public", 0x40000: "Private", 0x80000: "Protected", 0x100000: "Delegate", 0x200000: "NetServer",
        0x400000: "HasOutParms", 0x800000: "HasDefaults", 0x1000000: "NetClient", 0x2000000: "DLLImport",
        0x4000000: "BlueprintCallable", 0x8000000: "BlueprintEvent", 0x10000000: "BlueprintPure",
        0x20000000: "EditorOnly", 0x40000000: "Const", 0x80000000: "NetValidate"}


def flagstr(v, table, keep=None):
    return [n for b, n in table.items() if v & b and (keep is None or n in keep)]


PROP_KEEP = {"Net", "RepNotify", "Parm", "OutParm", "ReturnParm", "Transient", "SaveGame", "BlueprintVisible",
             "BlueprintReadOnly", "Edit", "Config", "ConstParm", "ReferenceParm", "RepSkip", "InstancedReference",
             "ExportObject"}

# ---------------------------------------------------------------- emit
result = {"classes": [], "structs": [], "enums": []}
funcs_by_class = defaultdict(list)
for fva, f in funcs.items():
    owner = link_by_func.get(fva)
    if owner:
        funcs_by_class[owner[0]].append(fva)


def emit_props(props):
    out = []
    for p in fold_props(props):
        d = {"name": p["name"], "type": p["ctype"], "flags": flagstr(p["flags"], CPF, PROP_KEEP)}
        if p["type"] == "Bool":
            bo = bool_offset(p)
            d["size"] = p["elemsize"]
            if bo:
                d["offset"], d["bitmask"] = bo
        else:
            d["offset"] = p["offset"]
        if p["dim"] > 1:
            d["dim"] = p["dim"]
        if p.get("repnotify"):
            d["repnotify"] = p["repnotify"]
        out.append(d)
    out.sort(key=lambda d: (d.get("offset") if d.get("offset") is not None else 1 << 30))
    return out


for cva, c in classes.items():
    fl = []
    for fva in funcs_by_class.get(cva, []):
        f = funcs[fva]
        fl.append({"name": f["name"], "flags": flagstr(f["flags"], FUNC), "parmsize": f["parmsize"],
                   "params": emit_props(f["props"]), "rpcid": f["rpcid"] or None, "va": hex(fva)})
    fl.sort(key=lambda x: x["name"])
    result["classes"].append({"name": c.get("cpp"), "pkg": c.get("pkg"), "size": c.get("size"), "super": c.get("super"),
                              "config": c["config"], "flags": hex(c["flags"]), "props": emit_props(c["props"]),
                              "functions": fl, "params_va": hex(cva), "staticclass_va": hex(c["noreg"])})
for sva, s in structs.items():
    result["structs"].append({"name": "F" + s["name"], "size": s["size"], "align": s["align"],
                              "super": type_name(s["super"]) if s["super"] else None, "props": emit_props(s["props"])})
for eva, e in enums.items():
    result["enums"].append({"name": e["cpp"] or e["name"], "values": e["values"]})

for k in result:
    result[k].sort(key=lambda x: x["name"] or "")
json.dump(result, open(OUT + ".json", "w"), indent=1)
print(f"classes={len(result['classes'])} structs={len(result['structs'])} enums={len(result['enums'])} "
      f"funcs_linked={len(link_by_func)}/{len(funcs)} unnamed_classes={sum(1 for c in result['classes'] if not c['name'] or c['name'].startswith('?'))}",
      file=sys.stderr)
