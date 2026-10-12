"""Generate the compilable C++ SDK in ../sdk from sdk.json (ue_reflect.py), natives.json and vtables.json.

    python scripts/emit_full_sdk.py [sdk.json] [out dir]

Writes, next to the hand-written Basic.hpp/Basic.cpp:
  CrabSDK.hpp                      umbrella header
  Forward.hpp                      every class and struct, declared
  {Engine,CrabChampions}_enums.hpp
  {Engine,CrabChampions}_structs.hpp
  {Engine,CrabChampions}_classes.hpp   members at their exact offsets, function wrappers declared
  {Engine,CrabChampions}_params.hpp    each UFunction's parameter block
  {Engine,CrabChampions}_functions.cpp StaticClass(), GetDefaultObj() and the wrappers (ProcessEvent)
  LayoutChecks.cpp                 static_asserts on every member offset (compile it to verify the layout)

Every type and member sits at the offset the game's reflection tables give; anything whose C++ type cannot be
reproduced exactly becomes a byte array of the right size, so layouts are always exact.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RE = os.path.dirname(HERE)
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RE, "sdk.json")
outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(RE), "sdk")
d = json.load(open(src))
natives = json.load(open(os.path.join(RE, "natives.json")))
vtables = json.load(open(os.path.join(RE, "vtables.json")))
win_macros = set()
wm = os.path.join(HERE, "win_macros.txt")
if os.path.exists(wm):
    win_macros = {l.strip() for l in open(wm) if l.strip()}

GAME_PKG = "/Script/CrabChampions"

# --- Types ---------------------------------------------------------------------------------------------------------

CORE = {"UObject": 0x28, "UField": 0x30, "UStruct": 0xB0, "UClass": 0x230, "UFunction": 0xE0,
        "UScriptStruct": 0xC0, "UEnum": 0x60, "UInterface": 0x28, "UPackage": 0x28, "UPackageMap": 0x28}
# Names ue_reflect.py misreads (it takes a neighbouring symbol): the type they really are.
FIX = {"SuperStruct": "UStruct", "Next": "UEnum", "Surfs": "UModel", "Struct": "FProperty", "Package": "UPackage",
       "PackageMap": "UPackageMap", "Interface": "UInterface", "ClassDefaultObject": "UClass",
       "SoundfieldEncodingSettingsBase": "USoundfieldEncodingSettingsBase"}
PRIM = {"bool": (1, 1), "int8": (1, 1), "uint8": (1, 1), "int16": (2, 2), "uint16": (2, 2), "int32": (4, 4),
        "uint32": (4, 4), "int64": (8, 8), "uint64": (8, 8), "float": (4, 4), "double": (8, 8)}
BASIC = {"FName": (8, 4), "FString": (16, 8), "FText": (0x18, 8)}
WRAP = {"TSubclassOf": (8, 8), "TWeakObjectPtr": (8, 4), "TSoftObjectPtr": (0x28, 8), "TSoftClassPtr": (0x28, 8),
        "TLazyObjectPtr": (0x1C, 4), "TScriptInterface": (0x10, 8)}
DELEGATE = {"InlineMulticastDelegate": ("FMulticastInlineDelegate", 0x10, 8), "Delegate": ("FScriptDelegate", 0x10, 4),
            "SparseMulticastDelegate": ("FMulticastSparseDelegate", 1, 1),
            "MulticastDelegate": ("FMulticastInlineDelegate", 0x10, 8)}

classes = [c for c in d["classes"] if c["name"] != "??" and c["size"] and c["name"] not in CORE]
structs = [s for s in d["structs"] if s["name"] not in BASIC]
enums = d["enums"]
for e in enums:
    e["name"] = e["name"].split("::")[0]  # namespaced UENUMs: EAILockSource::Type
CLS = {c["name"]: c for c in classes}
STR = {s["name"]: s for s in structs}
ENUM = {e["name"]: e for e in enums}
CLASS_NAMES = set(CLS) | set(CORE)
TYPE_NAMES = CLASS_NAMES | set(STR) | set(ENUM) | set(BASIC) | set(PRIM) | {"FProperty", "FField"}


def enum_type(e):
    vals = [v for _, v in e["values"]] or [0]
    if min(vals) >= 0 and max(vals) <= 0xFF:
        return "uint8", 1
    if min(vals) >= -2**31 and max(vals) < 2**31:
        return "int32", 4
    return "int64", 8


def parse(s, i=0):
    """'TMap<FName, TArray<AActor*>>' -> (name, [args], is_ptr), next index."""
    m = re.compile(r"\s*([A-Za-z_][A-Za-z0-9_]*)").match(s, i)
    name, i = m.group(1), m.end()
    args = []
    if i < len(s) and s[i] == "<":
        i += 1
        while True:
            a, i = parse(s, i)
            args.append(a)
            while i < len(s) and s[i] == " ":
                i += 1
            if s[i] == ",":
                i += 1
                continue
            assert s[i] == ">", s
            i += 1
            break
    ptr = False
    while i < len(s) and s[i] in " *&":
        ptr = ptr or s[i] == "*"
        i += 1
    return (name, args, ptr), i


def obj_name(n):
    n = FIX.get(n, n)
    return n if n in CLASS_NAMES or n in STR or n in ("FProperty", "FField") else None


def resolve(node, arg=False):
    """C++ type for a parsed type: (cpp, size, align, by-value struct deps, is_enum) or None."""
    name, args, ptr = node
    if ptr:
        return ((obj_name(name) or "UObject") + "*", 8, 8, set(), False)
    if name in PRIM:
        return (name, *PRIM[name], set(), False)
    if name in BASIC:
        return (name, *BASIC[name], set(), False)
    if name == "TArray":
        inner = resolve(args[0], True)
        return (f"TArray<{inner[0] if inner else 'uint8'}>", 16, 8, set(), False)
    if name in ("TMap", "TSet"):
        inner = [resolve(a, True) for a in args]
        return (f"{name}<{', '.join(x[0] if x else 'uint8' for x in inner)}>", 0x50, 8, set(), False)
    if name in WRAP:
        inner = obj_name(args[0][0]) if args else None
        return (f"{name}<{inner or 'UObject'}>", *WRAP[name], set(), False)
    if name in DELEGATE:
        cpp, size, align = DELEGATE[name]
        return (cpp, size, align, set(), False)
    if name == "TFieldPath":
        return ("FFieldPath", 0x20, 8, set(), False)
    if name in ENUM:
        size = enum_type(ENUM[name])[1]
        return (name, size, size, set(), True)
    if name in STR:
        return (name, STR[name]["size"], STR[name]["align"], {name}, False)
    return None


def rtype(t, arg=False):
    t = t.replace("::Type", "")
    try:
        return resolve(parse(t)[0], arg)
    except (AssertionError, AttributeError, IndexError):
        return None


# --- Names ---------------------------------------------------------------------------------------------------------

KEYWORDS = set("""alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t
class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default
delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable
namespace new noexcept not not_eq nullptr operator or or_eq private protected public register reinterpret_cast requires
return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try
typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq NULL""".split())
# Methods Basic.hpp gives every object: a reflected function with one of these names would hide them.
CORE_METHODS = {"StaticClass", "GetDefaultObj", "FindObject", "GetName", "GetFullName", "IsA", "IsDefaultObject",
                "ProcessEvent", "IsChildOf", "FindFunction"}
used_win_macros = set()


def ident(n, extra=()):
    n = re.sub(r"[^A-Za-z0-9_]", "_", n)
    if not n or n[0].isdigit():
        n = "_" + n
    while n in KEYWORDS or n in TYPE_NAMES or n in extra or n in win_macros:
        if n in win_macros:
            used_win_macros.add(n)
        n += "_"
    return n


# --- Layout ------------------------------------------------------------------------------------------------------

def layout(props, start, end, bitfields=True):
    """Member lines for props placed exactly between start and end, plus [(member, offset)] for LayoutChecks
    and the by-value struct dependencies."""
    deps, entries, bits = set(), [], {}
    names = set()
    for p in props:
        off = p["offset"]
        if p["type"] == "bool" and bitfields and "bitmask" in p and p["bitmask"] != 0xFF:
            b = p["bitmask"].bit_length() - 1
            bits.setdefault(off + b // 8, {}).setdefault(b % 8, p)
        else:
            entries.append((off, p))
    starts = sorted({o for o, _ in entries} | set(bits) | {end})
    items = [(o, 0, p) for o, p in entries] + [(o, 1, g) for o, g in bits.items()]
    items.sort(key=lambda x: (x[0], x[1]))
    lines, checks, cursor, pad = [], [], start, 0

    def pad_to(o):
        nonlocal cursor
        if o > cursor:
            lines.append(f"    uint8 Pad_{cursor:X}[0x{o - cursor:X}];")
        cursor = max(cursor, o)

    def comment(o, size, p):
        fl = " ".join(f for f in p.get("flags", []) if f in (
            "Net", "RepNotify", "SaveGame", "Transient", "Config", "Edit", "BlueprintVisible", "OutParm",
            "ReturnParm", "ConstParm", "ReferenceParm"))
        extra = f" OnRep={p['repnotify']}" if p.get("repnotify") else ""
        return f" // 0x{o:04X}(0x{size:04X}) {fl}{extra}".rstrip()

    for o, kind, x in items:
        if o < cursor or o >= end:
            what = x["name"] if kind == 0 else ", ".join(p["name"] for p in x.values())
            lines.append(f"    // skipped, overlaps: {what} at 0x{o:X}")
            continue
        pad_to(o)
        if kind == 1:
            used = 0
            for bit in sorted(x):
                if bit > used:
                    lines.append(f"    uint8 : {bit - used};")
                p = x[bit]
                n = ident(p["name"], names)
                names.add(n)
                lines.append(f"    uint8 {n} : 1;".ljust(64) + comment(o, 1, p) + f" bit {bit}")
                used = bit + 1
            if used < 8:
                lines.append(f"    uint8 : {8 - used};")
            cursor = o + 1
            continue
        p = x
        gap = next(s for s in starts if s > o) - o
        r = rtype(p["type"])
        n = ident(p["name"], names)
        names.add(n)
        dim = p.get("dim") or 1
        if r and r[4] and r[1] * dim > gap:
            r = (f"TEnumAsByte<{r[0]}>", 1, 1, set(), True)
        if not r or r[1] * dim > gap or o % r[2]:
            lines.append(f"    uint8 {n}[0x{gap:X}];".ljust(64) + comment(o, gap, p) + f" {p['type']}")
            cursor = o + gap
            continue
        deps |= r[3]
        arr = f"[{dim}]" if dim > 1 else ""
        lines.append(f"    {r[0]} {n}{arr};".ljust(64) + comment(o, r[1] * dim, p))
        checks.append((n, o))
        cursor = o + r[1] * dim
    pad_to(end)
    return lines, checks, deps


def topo(names, deps_of):
    out, seen = [], set()

    def visit(n):
        if n in seen:
            return
        seen.add(n)
        for x in sorted(deps_of(n)):
            if x in names:
                visit(x)
        out.append(n)

    for n in sorted(names):
        visit(n)
    return out


# --- Enums ---------------------------------------------------------------------------------------------------------

def emit_enum(e):
    t, _ = enum_type(e)
    out = [f"// Enum {e['name']}", f"enum class {e['name']} : {t}", "{"]
    seen = set()
    for full, v in e["values"]:
        n = ident(full.split("::")[-1], seen)
        seen.add(n)
        out.append(f"    {n} = {v},")
    return out + ["};", ""]


# --- Structs -------------------------------------------------------------------------------------------------------

# UE builds with /Zp8. Under it MSVC lays a subclass's first members inside its base's alignment padding when that
# padding comes from a 16-aligned member (an FTransform): the subclass starts at the base's unaligned size. Such a
# base is emitted only up to there, without alignas, and every header is wrapped in #pragma pack(8), which makes
# both MSVC and Itanium (MinGW) compilers place the subclass the way the game does.
def own_start(t):
    return min((p["offset"] for p in t["props"]), default=None)


def layout_ends(types, size_of, super_of):
    ends = {}
    for t in types:
        sup, first = super_of(t), own_start(t)
        if sup in size_of and first is not None and first < size_of[sup]:
            ends[sup] = min(ends.get(sup, size_of[sup]), first)
    return ends


def size_assert(n, size, end):
    if end == size:
        return f"static_assert(sizeof({n}) == 0x{size:X});"
    alt = (end + 7) & ~7
    return (f"static_assert(sizeof({n}) == 0x{size:X} || sizeof({n}) == 0x{alt:X});"
            f"  // subclasses start at 0x{end:X}, inside the alignment padding")


struct_end = {n: e for n, e in layout_ends(structs, {n: s["size"] for n, s in STR.items()}, lambda s: s.get("super")).items()
              if e >= max((p["offset"] for p in STR[n]["props"]), default=0)}

struct_body, struct_deps, layout_checks = {}, {}, []
for s in structs:
    sup = s.get("super") if s.get("super") in STR else None
    start = struct_end.get(sup, STR[sup]["size"]) if sup else 0
    end = struct_end.get(s["name"], s["size"])
    lines, checks, deps = layout(s["props"], start, end)
    struct_deps[s["name"]] = deps | ({sup} if sup else set())
    align = f"alignas(0x{s['align']:X}) " if end == s["size"] else ""
    body = [f"// Struct {s['name']}  0x{s['size']:04X} bytes, align {s['align']}",
            f"struct {align}{s['name']}" + (f" : public {sup}" if sup else ""),
            "{"] + lines + ["};", size_assert(s["name"], s["size"], end), ""]
    struct_body[s["name"]] = body
    layout_checks += [(s["name"], n, o) for n, o in checks]

# Game structs: Crab-named, or holding a game struct or enum by value (so they must come after them).
game_enums = {e["name"] for e in enums if e["name"].startswith("ECrab")}
game_structs = {n for n in STR if n.startswith("FCrab")}
changed = True
while changed:
    changed = False
    for s in structs:
        n = s["name"]
        if n in game_structs:
            continue
        uses = struct_deps[n] | {rtype(p["type"])[0] for p in s["props"] if rtype(p["type"]) and rtype(p["type"])[4]}
        if uses & (game_structs | game_enums):
            game_structs.add(n)
            changed = True

# --- Classes and functions --------------------------------------------------------------------------------------

def super_of(c):
    s = c["super"]
    return FIX.get(s, s) if s else "UObject"


class_end = layout_ends(classes, {**CORE, **{n: c["size"] for n, c in CLS.items()}}, super_of)


def size_of_class(n):
    return class_end.get(n, CORE[n] if n in CORE else CLS[n]["size"])


class_body, class_funcs, params_body = {}, {}, {}
wrapper_count = 0


def param_decl(p, r):
    flags = p["flags"]
    out = "OutParm" in flags and "ReturnParm" not in flags
    const = "ConstParm" in flags
    big = r[2] and (r[0].startswith(("F", "T")) and not r[4]) and r[0] not in ("FName",)
    if out and not const:
        return f"{r[0]}&", True
    if big or (out and const):
        return f"const {r[0]}&", False
    return r[0], False


for c in classes:
    n = c["name"]
    sup = super_of(c)
    start = size_of_class(sup)
    end = class_end.get(n, c["size"])
    lines, checks, deps = layout(c["props"], start, end)
    layout_checks += [(n, m, o) for m, o in checks]
    member_names = {ident(p["name"]) for p in c["props"]}
    vt = vtables.get(n, {})
    nat = natives.get(n, {})
    head = [f"// Class {c['pkg'][len('/Script/'):]}.{n[1:]}  0x{c['size']:04X} bytes (0x{c['size'] - start:04X} own)",
            f"// StaticClass {c['staticclass_va']}" + (f"  vtable {vt['vtable']}  ctor {vt['ctor']}" if vt else "")]
    decls, impls = [], []
    for f in c["functions"]:
        fname = ident(f["name"], member_names | CORE_METHODS)
        ret = next((p for p in f["params"] if "ReturnParm" in p["flags"]), None)
        params = [p for p in f["params"] if p is not ret]
        rr = rtype(ret["type"]) if ret else None
        rs = [rtype(p["type"]) for p in params]
        if (ret and not rr) or not all(rs):
            decls.append(f"    // {f['name']}: a parameter type has no C++ equivalent; call it with ProcessEvent")
            continue
        pname = ident(f"{n}_{f['name']}")
        playout, pchecks, _ = layout(f["params"], 0, max(f["parmsize"], 1), bitfields=False)
        params_body.setdefault(n, []).extend(
            [f"// Function {f['name']}", f"struct {pname}", "{"] + playout + ["};", ""])
        layout_checks += [(f"Params::{pname}", m, o) for m, o in pchecks]
        args, copy_in, copy_out, seen = [], [], [], set()
        for p, r in zip(params, rs):
            a = ident(p["name"], seen | {fname})
            seen.add(a)
            field = ident(p["name"])
            t, is_out = param_decl(p, r)
            args.append(f"{t} {a}")
            copy_in.append(f"    Parms.{field} = {a};")
            if is_out:
                copy_out.append(f"    {a} = Parms.{field};")
        static = "Static" in f["flags"]
        rt = rr[0] if ret else "void"
        native = nat.get(f["name"])
        note = " ".join(x for x in f["flags"] if x in ("Net", "NetReliable", "NetServer", "NetClient",
                                                       "NetMulticast", "BlueprintEvent", "Static", "Native"))
        if native and native.get("thunk"):
            note += f"  exec {native['thunk']}" + (f" impl {', '.join(native['impl'])}" if native.get("impl") else "")
        decls.append(f"    {'static ' if static else ''}{rt} {fname}({', '.join(args)});".ljust(64) + f" // {note}")
        target = "GetDefaultObj()" if static else "this"
        fail = "return;" if not ret else "return {};"
        impls += [f"// {f['name']} [{note.strip()}]",
                  f"{rt} {n}::{fname}({', '.join(args)})", "{",
                  "    static UFunction* Func = nullptr;",
                  f"    if (!FindFunctionCached(Func, L\"{c['pkg']}.{n[1:]}:{f['name']}\")) {fail}",
                  f"    Params::{pname} Parms{{}};"] + copy_in + [
                  f"    {target}->ProcessEvent(Func, &Parms);"] + copy_out + (
                  [f"    return Parms.{ident(ret['name'])};"] if ret else []) + ["}", ""]
        wrapper_count += 1
    impls = [f"UClass* {n}::StaticClass()", "{",
             f"    return reinterpret_cast<UClass* (*)()>(Rebase({c['staticclass_va']}))();", "}", "",
             f"{n}* {n}::GetDefaultObj()", "{",
             f"    return static_cast<{n}*>(StaticClass()->ClassDefaultObject);", "}", ""] + impls
    body = head + [f"class {n} : public {sup}", "{", "public:"] + lines + [
        "", "public:", "    static UClass* StaticClass();", f"    static {n}* GetDefaultObj();"]
    body += [""] + decls if decls else []
    body += ["};", size_assert(n, c["size"], end), ""]
    class_body[n] = (body, deps, sup)
    class_funcs[n] = impls

# --- Write ---------------------------------------------------------------------------------------------------------

HEADER = "// Generated by re/scripts/emit_full_sdk.py from the game's reflection data. Do not edit.\n"


def write(name, lines, prelude=()):
    with open(os.path.join(outdir, name), "w", newline="\n") as fh:
        fh.write(HEADER)
        fh.write("\n".join(prelude) + "\n" if prelude else "")
        fh.write("\n".join(lines) + "\n")


def ns(lines, includes, pragma=True, pack=False):
    pre = (["#pragma once", ""] if pragma else []) + [f'#include "{i}"' for i in includes] + ["", "namespace CrabSDK {", ""]
    if pack:
        pre += ["#pragma pack(push, 0x8)", ""]
        lines = lines + ["#pragma pack(pop)", ""]
    return pre + lines + ["}  // namespace CrabSDK"]


os.makedirs(outdir, exist_ok=True)
fwd = [f"class {n};" for n in sorted(CLS)] + [""] + [f"struct {n};" for n in sorted(STR)] + [""] + [
    f"enum class {e['name']} : {enum_type(e)[0]};" for e in enums]
write("Forward.hpp", ns(fwd, ["Basic.hpp"]))

for group, pick_enum, pick_struct, pick_class in (
        ("Engine", lambda e: e not in game_enums, lambda s: s not in game_structs, lambda c: CLS[c]["pkg"] != GAME_PKG),
        ("CrabChampions", lambda e: e in game_enums, lambda s: s in game_structs, lambda c: CLS[c]["pkg"] == GAME_PKG)):
    pre = "Engine" if group == "CrabChampions" else None
    lines = []
    for e in sorted(enums, key=lambda e: e["name"]):
        if pick_enum(e["name"]):
            lines += emit_enum(e)
    write(f"{group}_enums.hpp", ns(lines, ["Forward.hpp"]))

    names = {s for s in STR if pick_struct(s)}
    lines = []
    for s in topo(names, lambda s: struct_deps[s]):
        lines += struct_body[s]
    write(f"{group}_structs.hpp", ns(lines, [f"{group}_enums.hpp"] + ([f"{pre}_structs.hpp"] if pre else []), pack=True))

    names = {c for c in CLS if pick_class(c)}
    for c in names:
        bad = class_body[c][1] - names - set(STR)
        assert not bad, (c, bad)
    lines = []
    for c in topo(names, lambda c: {class_body[c][2]}):
        lines += class_body[c][0]
    write(f"{group}_classes.hpp", ns(lines, [f"{group}_structs.hpp"] + ([f"{pre}_classes.hpp"] if pre else []), pack=True))

    lines = ["namespace Params {", ""]
    for c in sorted(names):
        lines += params_body.get(c, [])
    lines += ["}  // namespace Params", ""]
    write(f"{group}_params.hpp", ns(lines, [f"{group}_classes.hpp"], pack=True))

    lines = []
    for c in sorted(names):
        lines += class_funcs[c]
    write(f"{group}_functions.cpp", ns(lines, ["CrabSDK.hpp"], pragma=False))

write("CrabSDK.hpp", ["#pragma once", "", '#include "Basic.hpp"', '#include "Forward.hpp"',
                      '#include "Engine_enums.hpp"', '#include "CrabChampions_enums.hpp"',
                      '#include "Engine_structs.hpp"', '#include "CrabChampions_structs.hpp"',
                      '#include "Engine_classes.hpp"', '#include "CrabChampions_classes.hpp"',
                      '#include "Engine_params.hpp"', '#include "CrabChampions_params.hpp"'])

checks = ["// Compile-only: every member's offset, checked against the reflection data. Not needed in a mod.",
          '#include "CrabSDK.hpp"', "", "#ifdef __clang__", '#pragma clang diagnostic ignored "-Winvalid-offsetof"',
          "#endif", "", "namespace CrabSDK {", ""]
checks += [f"static_assert(offsetof({t}, {m}) == 0x{o:X});" for t, m, o in layout_checks]
checks += ["", "}  // namespace CrabSDK"]
write("LayoutChecks.cpp", checks)

print(f"{len(classes)} classes, {len(structs)} structs, {len(enums)} enums, {wrapper_count} functions, "
      f"{len(layout_checks)} offset checks; windows.h names renamed: {len(used_win_macros)}")
