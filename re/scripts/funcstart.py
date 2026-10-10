"""Resolve a code address to its function's primary start, following chained .pdata unwind entries."""
import sys
from xref import *
import xref

def _rf_at(rva):
    # RUNTIME_FUNCTION whose range contains rva (any entry, primary or chained)
    f = func_of(BASE + rva)
    return f

def primary_start(va):
    f = func_of(va)
    if not f: return None
    b = f[0]
    for _ in range(16):
        # find this entry's unwind info
        pd = SECS[".pdata"]
        ui = None
        for a in range(pd[0], pd[1] - 12, 12):
            if BASE + u32(a) == b: ui = u32(a + 8); break  # rvas; u32 takes a VA
        if ui is None: return b
        flags = img[ui] >> 3
        if not (flags & 4): return b
        n = img[ui + 2]
        off = ui + 4 + ((n + 1) & ~1) * 2
        b = BASE + u32(BASE + off)
    return b

if __name__ == "__main__":
    for a in sys.argv[1:]:
        s = primary_start(int(a, 16))
        print(a, hex(s) if s else None)
