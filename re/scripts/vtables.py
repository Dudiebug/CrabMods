"""Game-class vtables: StaticClass -> GetPrivateStaticClassBody InternalConstructor arg -> T::T(FObjectInitializer&) -> own vtable store."""
import sys, json, struct
from capstone.x86 import *
sys.argv = ["x", "game.exe", "/dev/null"]
exec(open("scripts/ue_reflect.py").read().split("classes, structs, enums, funcs = {}, {}, {}, {}")[0])

def vt_len(va):
    n = 0
    while is_rd(va) and is_text(u64(va + 8 * n)): n += 1
    return n

def insns(va, n=120):
    out = []
    for ins in md.disasm(bytes(img[va - BASE: va - BASE + n * 15]), va):
        out.append(ins)
        if ins.mnemonic in ("ret", "int3") or len(out) >= n: break
    return out

def rip(ins, op):
    return ins.address + ins.size + op.mem.disp

def internal_ctor(sc):
    """InternalConstructor fn ptr: stack arg 10 of GetPrivateStaticClassBody (r11-0x40 / rsp+0x48)."""
    leas = {}
    for ins in insns(sc, 60):
        if ins.mnemonic == "lea" and ins.operands[1].type == X86_OP_MEM and ins.operands[1].mem.base == X86_REG_RIP:
            leas[ins.operands[0].reg] = rip(ins, ins.operands[1])
        elif ins.mnemonic == "mov" and ins.operands[0].type == X86_OP_MEM and ins.operands[1].type == X86_OP_REG:
            m = ins.operands[0].mem
            if (m.base == X86_REG_R11 and m.disp == -0x40) or (m.base == X86_REG_RSP and m.disp == 0x48):
                return leas.get(ins.operands[1].reg)
        if ins.mnemonic == "call": break
    return None

def ctor_of(ic):
    """InternalConstructor: mov rcx,[rcx]; test; jne T::T  (or a call)."""
    for ins in insns(ic, 12):
        if ins.mnemonic in ("jne", "jmp", "call") and ins.operands[0].type == X86_OP_IMM:
            return ins.operands[0].imm
    return None

def own_vtable(ctor):
    regs = {}
    this = {X86_REG_RCX}
    for ins in insns(ctor, 80):
        if ins.mnemonic == "mov" and ins.operands[0].type == X86_OP_REG and ins.operands[1].type == X86_OP_REG \
                and ins.operands[1].reg in this:
            this.add(ins.operands[0].reg)
        if ins.mnemonic == "lea" and ins.operands[1].type == X86_OP_MEM and ins.operands[1].mem.base == X86_REG_RIP:
            regs[ins.operands[0].reg] = rip(ins, ins.operands[1])
        if ins.mnemonic == "mov" and ins.operands[0].type == X86_OP_MEM and ins.operands[1].type == X86_OP_REG:
            m = ins.operands[0].mem
            if m.disp == 0 and m.index == 0 and m.base in this and ins.operands[1].reg in regs:
                t = regs[ins.operands[1].reg]
                if vt_len(t) >= 20: return t
    return None

if __name__ == "__main__":
    sdk = json.load(open("sdk.json")); nat = json.load(open("natives.json")); res = {}
    miss = []
    for c in sdk["classes"]:
        sc = int(c["staticclass_va"], 16)
        ic = internal_ctor(sc); ct = ic and ctor_of(ic); vt = ct and own_vtable(ct)
        if not vt:
            miss.append(c["name"]); continue
        r = {"vtable": hex(vt), "len": vt_len(vt), "ctor": hex(ct), "slots": {}}
        for n, e in nat.get(c["name"], {}).items():
            if e["vcall"]:
                off = int(e["vcall"][0], 16)
                r["slots"][n] = {"vt_off": hex(off), "impl": hex(u64(vt + off)) if off < 8 * r["len"] else None}
            elif e["impl"]:
                r["slots"][n] = {"impl": e["impl"][0]}
        res[c["name"]] = r
    json.dump(res, open("vtables.json", "w"), indent=1)
    vts = [r["vtable"] for r in res.values()]
    print(len(res), "vtables,", len(set(vts)), "unique; missing", len(miss), miss[:20])
