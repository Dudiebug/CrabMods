"""Index every call to perk/item query helpers with an immediate type arg -> (type, enclosing root fn)."""
import sys, json, re, collections
sys.path.insert(0, "scripts")
from calls import *
from xref import root_of
enums = {}
cur = None
for line in open("sdk/CrabChampions_enums.hpp"):
    m = re.match(r"enum class (\w+)", line)
    if m: cur = m.group(1); enums[cur] = {}
    m = re.match(r"\s+(\w+) = (\d+),", line)
    if m and cur: enums[cur][int(m.group(2))] = m.group(1)
HELPERS = {0x140d23b90: "HasPerk", 0x140d25450: "GetPerkBaseBuff", 0x140d2df60: "InvArr_GetBuff", 0x140d2e050: "InvArr_GetLevel",
           0x140d276e0: "UpdatePerkBuff"}
extra = sys.argv[1:]
for e in extra:
    a, n = e.split("=")
    HELPERS[int(a, 16)] = n
out = collections.defaultdict(list)
for h, hn in HELPERS.items():
    for c in calls_to(h):
        # walk back up to 8 insns for mov edx/dl, imm
        val = None
        for back in range(4, 48):
            insl = list(md.disasm(DATA_B[c - back - BASE:c - BASE], c - back))
            if not insl or insl[-1].address + insl[-1].size != c: continue
            for ins in reversed(insl):
                if ins.mnemonic == "mov" and ins.op_str.split(",")[0] in ("edx", "dl") and ins.operands[1].type == X86_OP_IMM:
                    val = ins.operands[1].imm; break
                if ins.mnemonic == "call": break
            if val is not None: break
        r = root_of(c)
        out[hn].append((val, hex(r) if r else None, hex(c)))
perk = enums["ECrabPerkType"]
res = {}
for hn, lst in out.items():
    for v, r, c in lst:
        res.setdefault(perk.get(v, f"?{v}") if v is not None else "?", []).append(f"{hn}@{c} in {r}")
json.dump(res, open("perk_sites.json", "w"), indent=1)
for k in sorted(res): print(k, len(res[k]), res[k][:4])
