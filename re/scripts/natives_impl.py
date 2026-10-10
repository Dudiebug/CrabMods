"""Second pass over natives.json: follow each exec thunk to the implementation it calls, skipping shared helpers."""
import sys, json, struct, bisect
from collections import Counter
sys.argv = ["x", "game.exe", "/dev/null"]
exec(open("scripts/ue_reflect.py").read().split("classes, structs, enums, funcs = {}, {}, {}, {}")[0])
# .pdata function bounds
pd = SECS[".pdata"]; FB = []
for a in range(pd[0], pd[1] - 12, 12):
    b, e = u32(a), u32(a + 4)
    if b and e > b: FB.append((BASE + b, BASE + e))
FB.sort(); starts = [b for b, _ in FB]
def fend(va):
    i = bisect.bisect_right(starts, va) - 1
    return FB[i][1] if i >= 0 and FB[i][0] <= va < FB[i][1] else va + 0x200
def body_calls(f):
    code = bytes(img[f - BASE: fend(f) - BASE]); d, v, j = [], [], []
    stop_at_ret = True
    for ins in md.disasm(code, f):
        if ins.mnemonic in ("call", "jmp"):
            op = ins.operands[0]
            if op.type == X86_OP_IMM:
                (d if ins.mnemonic == "call" else j).append(op.imm)
            elif op.type == X86_OP_MEM and op.mem.base != X86_REG_RIP:
                v.append(op.mem.disp)
        if stop_at_ret and (ins.mnemonic == "ret" or (ins.mnemonic == "jmp" and ins.operands[0].type == X86_OP_IMM and not (f <= ins.operands[0].imm < f + 0x400))): break
    return d, v, [x for x in j if not (f <= x < fend(f))]
nat = json.load(open("natives.json"))
cnt = Counter()
info = {}
for cls, fs in nat.items():
    for n, e in fs.items():
        t = int(e["thunk"], 16); d, v, j = body_calls(t); info[(cls, n)] = (d, v, j); cnt.update(set(d + j))
helpers = {a for a, c in cnt.items() if c >= 25}
print("helpers", len(helpers))
out = {}
for (cls, n), (d, v, j) in info.items():
    impl = [hex(x) for x in d + j if x not in helpers]
    out.setdefault(cls, {})[n] = {"thunk": nat[cls][n]["thunk"], "impl": impl, "vcall": [hex(x) for x in v]}
json.dump(out, open("natives.json", "w"), indent=1)
for c in ["ACrabPS", "ACrabPC", "ACrabPlayerC"]:
    for k, e in sorted(out[c].items()):
        if k.startswith(("Server", "OnRep_Inv", "Client")): print(c, k, e["impl"], e["vcall"])
