"""Given VAs of function pointers inside vtables, print the owning class (via RTTI) and slot.

Usage: python whose_vtable.py <exe> <va> [<va> ...]
"""
import struct
import sys

d = open(sys.argv[1], "rb").read()
pe = struct.unpack_from("<I", d, 0x3C)[0]
nsec = struct.unpack_from("<H", d, pe + 6)[0]
optsz = struct.unpack_from("<H", d, pe + 20)[0]
opt = pe + 24
base = struct.unpack_from("<Q", d, opt + 24)[0]
secs = []
for i in range(nsec):
    o = opt + optsz + i * 40
    vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
    secs.append((va, max(vs, rs), ra))


def r2o(r):
    for va, sz, ra in secs:
        if va <= r < va + sz:
            return r - va + ra


def col_name(ptr):
    r = ptr - base
    o = r2o(r)
    if o is None or o + 24 > len(d):
        return None
    sig, ofs, cdo, td, chd, selfr = struct.unpack_from("<IIIIII", d, o)
    if sig != 1 or selfr != r:
        return None
    to = r2o(td)
    name = d[to + 16:d.index(b"\0", to + 16)].decode(errors="replace")
    return name, ofs


for a in sys.argv[2:]:
    va = int(a, 16)
    p = va - 8
    while True:
        q = struct.unpack_from("<Q", d, r2o(p - base))[0]
        n = col_name(q)
        if n:
            vt = p + 8
            print(f"{va:#x}: {n[0]} (this+{n[1]:#x}) vtable {vt:#x} slot +{va - vt:#x}")
            break
        p -= 8
