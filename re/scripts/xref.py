"""Tiny static xref helper: rip-relative refs to strings/data, pdata function bounds, disasm."""
import sys, re, struct, bisect
_argv = sys.argv; sys.argv = ["x", "game.exe", "/dev/null"]
exec(open(__file__.replace("xref.py", "ue_reflect.py")).read().split("# ---------------------------------------------------------------- scanning")[0])
sys.argv = _argv
DATA_B = bytes(img)
pd = SECS[".pdata"]; FB = []
for a in range(pd[0], pd[1] - 12, 12):
    b, e = u32(a), u32(a + 4)
    if b and e > b: FB.append((BASE + b, BASE + e))
FB.sort(); STARTS = [b for b, _ in FB]
def func_of(va):
    i = bisect.bisect_right(STARTS, va) - 1
    return FB[i] if i >= 0 and FB[i][0] <= va < FB[i][1] else None
_T0, _T1 = TEXT[0] - BASE, TEXT[1] - BASE
_RIPREF = None
def build_riprefs():
    global _RIPREF
    if _RIPREF is not None: return _RIPREF
    _RIPREF = {}
    pat = re.compile(rb'(?:[\x48\x4c](?:\x8d|\x8b|\x89|\x3b|\x39|\x83|\xc7)|\x8b|\x89|\x80|\x83|\xc6|\xff|\x0f\xb6|\x0f\xb7|\xf3\x0f\x10|\xf3\x0f\x11)[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]')
    for m in pat.finditer(DATA_B, _T0, _T1):
        a = m.end(); disp = struct.unpack_from("<i", DATA_B, a)[0]
        # instruction length unknown for imm forms; record target for the common no-imm length
        for extra in (0, 1, 4):
            tgt = a + 4 + extra + disp
            _RIPREF.setdefault(BASE + tgt, []).append(BASE + m.start())
    return _RIPREF
def refs_to(va):
    """Exact rip-relative references to va, verified by capstone."""
    out = []
    for ia in build_riprefs().get(va, []):
        for back in (0, 1, 2):
            s = ia - back
            for ins in md.disasm(DATA_B[s - BASE:s - BASE + 16], s, 1):
                for op in ins.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and ins.address + ins.size + op.mem.disp == va:
                        out.append(ins.address)
    return sorted(set(out))
def find_wstr(s):
    b = s.encode("utf-16le") + b"\0\0"; out = []; i = DATA_B.find(b)
    while i >= 0:
        if i % 2 == 0: out.append(BASE + i)
        i = DATA_B.find(b, i + 1)
    return out
def find_cstr(s):
    b = b"\0" + s.encode() + b"\0"; i = DATA_B.find(b); return [BASE + i + 1] if i >= 0 else []
def dis(start, end=None, n=200):
    end = end or (func_of(start) or (start, start + 0x200))[1]
    for ins in md.disasm(DATA_B[start - BASE:end - BASE], start):
        extra = ""
        for op in ins.operands:
            if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                t = ins.address + ins.size + op.mem.disp
                w = wstr(t) if is_rd(t) else None; c = cstr(t) if is_rd(t) else None
                extra = f"  ; {hex(t)} " + (repr(w) if w and len(w) > 2 else repr(c) if c and len(c) > 2 else "")
        print(f"{ins.address:x}: {ins.mnemonic} {ins.op_str}{extra}")
        n -= 1
        if n <= 0: break
# chained unwind info -> root function (fragments of split functions)
_ROOT = {}
for a in range(pd[0], pd[1] - 12, 12):
    b, e, uw = u32(a), u32(a + 4), u32(a + 8)
    if not b or e <= b: continue
    ui = BASE + uw
    flags = DATA_B[uw] >> 3; ncodes = DATA_B[uw + 2]
    if flags & 4:
        ch = uw + 4 + 2 * ((ncodes + 1) & ~1)
        _ROOT[BASE + b] = BASE + struct.unpack_from("<I", DATA_B, ch)[0]
def root_of(va):
    f = func_of(va)
    if not f: return None
    s = f[0]
    while s in _ROOT: s = _ROOT[s]
    return s
