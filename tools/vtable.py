"""Locate MSVC x64 vtables for a class via RTTI and dump their slots.

Usage: python vtable.py <exe> <ClassName> [slots]
"""
import struct
import sys


def load(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    opt = pe + 24
    base = struct.unpack_from("<Q", d, opt + 24)[0]
    secs = []
    for i in range(nsec):
        o = opt + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode()
        vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
        secs.append((name, va, vs, ra, rs))
    return d, base, secs


def main():
    exe, cls = sys.argv[1], sys.argv[2]
    nslots = int(sys.argv[3]) if len(sys.argv) > 3 else 80
    d, base, secs = load(exe)

    def off2rva(o):
        for _, va, vs, ra, rs in secs:
            if ra <= o < ra + rs:
                return va + o - ra

    def rva2off(r):
        for _, va, vs, ra, rs in secs:
            if va <= r < va + max(vs, rs):
                return r - va + ra

    name = f".?AV{cls}@@".encode()
    o = d.find(name + b"\0")
    td_rva = off2rva(o - 0x10)  # TypeDescriptor: pVFTable, spare, name
    print(f"TypeDescriptor RVA {td_rva:#x}")
    # CompleteObjectLocator (x64): sig=1, offset, cdOffset, pTypeDescriptor(rva), pClassHierarchy, pSelf
    needle = struct.pack("<I", td_rva)
    i = d.find(needle)
    while i != -1:
        col = i - 12
        sig, ofs, cdo, td, chd, selfr = struct.unpack_from("<IIIIII", d, col)
        col_rva = off2rva(col)
        if sig == 1 and selfr == col_rva:
            ptr = struct.pack("<Q", base + col_rva)
            j = d.find(ptr)
            while j != -1:
                vt = off2rva(j) + 8
                print(f"COL {col_rva:#x} (this offset {ofs:#x}) -> vtable RVA {vt:#x} / VA {base + vt:#x}")
                for s in range(nslots):
                    f = struct.unpack_from("<Q", d, rva2off(vt) + 8 * s)[0]
                    print(f"   +{8 * s:#05x}  {f:#x}")
                j = d.find(ptr, j + 1)
        i = d.find(needle, i + 1)


if __name__ == "__main__":
    main()
