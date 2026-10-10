"""Locate generated UFunction call stubs (FindFunctionChecked(NAME_X) + ProcessEvent) and their callers."""
from calls import *
import xref
def name_globals(fname):
    """FName globals initialised from L"fname" (store after the FName ctor call)."""
    out = []
    for s in find_wstr(fname):
        for r in refs_to(s):
            f = func_of(r)
            seen = False
            for ins in md.disasm(DATA_B[r - BASE:r - BASE + 0x40], r):
                if ins.mnemonic == "call": seen = True
                for op in ins.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                        t = ins.address + ins.size + op.mem.disp
                        if t != s: out.append(t)
                if seen and out: break
            # also look backwards: lea rcx,[G] ; lea rdx,[str] ; call FName::FName
            for ins in md.disasm(DATA_B[r - 0x10 - BASE:r - BASE], r - 0x10):
                for op in ins.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and ins.mnemonic == "lea":
                        out.append(ins.address + ins.size + op.mem.disp)
    return sorted(set(t for t in out if not is_text(t)))
def stubs(fname):
    res = []
    for g in name_globals(fname):
        for r in refs_to(g):
            f = func_of(r)
            if f and f not in res: res.append(f)
    return res
