"""Find direct call/jmp sites to a target VA."""
import re, struct
from xref import *
import xref
_T0, _T1 = xref._T0, xref._T1
_CALLS = None
def calls_to(va):
    global _CALLS
    if _CALLS is None:
        _CALLS = {}
        for m in re.finditer(rb'[\xe8\xe9]', DATA_B[_T0:_T1]):
            a = _T0 + m.start()
            t = a + 5 + struct.unpack_from("<i", DATA_B, a + 1)[0]
            if _T0 <= t < _T1:
                _CALLS.setdefault(BASE + t, []).append(BASE + a)
    return _CALLS.get(va, [])
