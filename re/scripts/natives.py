"""Map every native UFunction to its exec thunk and the implementation the thunk calls."""
import sys, json, struct
sys.argv = ["x", "game.exe", "/dev/null"]
exec(open("scripts/ue_reflect.py").read().split("classes, structs, enums, funcs = {}, {}, {}, {}")[0])
sdk = json.load(open("sdk.json"))
# FNameNativePtrPair { const char* NameUTF8; FNativeFuncPtr Pointer; } arrays, sorted by name.
tables = []
va = RDATA[0]
while va < DATA[1] - 16:
    n = 0
    while ident(u64(va + 16 * n)) and is_text(u64(va + 16 * n + 8)):
        n += 1
    if n >= 1:
        tables.append((va, [(ident(u64(va + 16 * i)), u64(va + 16 * i + 8)) for i in range(n)]))
        va += 16 * n
    else:
        va += 8
by_names = {}
for c in sdk["classes"]:
    native = frozenset(f["name"] for f in c["functions"] if "Native" in f["flags"])
    if native:
        by_names.setdefault(native, []).append(c["name"])
out = {}
for tva, ents in tables:
    key = frozenset(n for n, _ in ents)
    owners = by_names.get(key)
    if not owners or len(owners) != 1:
        continue
    cls = owners[0]
    for n, thunk in ents:
        # implementation = last direct call in the thunk before ret (UHT: P_FINISH; P_THIS->Impl(args))
        code = bytes(img[thunk - BASE: thunk - BASE + 600])
        calls, vcalls = [], []
        for ins in md.disasm(code, thunk):
            if ins.mnemonic == "call":
                op = ins.operands[0]
                if op.type == X86_OP_IMM: calls.append(op.imm)
                elif op.type == X86_OP_MEM and op.mem.base != X86_REG_RIP: vcalls.append(op.mem.disp)
            if ins.mnemonic in ("ret", "int3"): break
            if ins.mnemonic == "jmp" and ins.operands[0].type == X86_OP_IMM:
                calls.append(ins.operands[0].imm); break
        out.setdefault(cls, {})[n] = {"thunk": hex(thunk), "calls": [hex(c) for c in calls], "vtable_offsets": [hex(v) for v in vcalls]}
json.dump(out, open("natives.json", "w"), indent=1)
print("tables", len(tables), "matched classes", len(out), "funcs", sum(len(v) for v in out.values()))
