"""Find addresses of ASCII strings in a PE and the instructions (from an objdump -M intel
listing) that reference them via RIP-relative addressing.

Usage: python xref.py <exe> <asm_listing> <string> [<string> ...]
"""
import re
import struct
import sys


def sections(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    optsz = struct.unpack_from("<H", d, pe + 20)[0]
    opt = pe + 24
    base = struct.unpack_from("<Q", d, opt + 24)[0]
    out = []
    for i in range(nsec):
        o = opt + optsz + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode()
        vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
        out.append((name, va, vs, ra, rs))
    return base, out


def off2va(base, secs, off):
    for _, va, vs, ra, rs in secs:
        if ra <= off < ra + rs:
            return base + va + off - ra
    return None


def main():
    exe, asm = sys.argv[1], sys.argv[2]
    d = open(exe, "rb").read()
    base, secs = sections(d)
    targets = {}
    for s in sys.argv[3:]:
        needle = b"\0" + s.encode() + b"\0"
        i = d.find(needle)
        while i != -1:
            va = off2va(base, secs, i + 1)
            targets[va] = s
            print(f"string {s!r} at {va:#x}")
            i = d.find(needle, i + 1)
    pat = re.compile(r"# 0x([0-9a-f]+)")
    func = None
    with open(asm, "r", errors="replace") as f:
        for line in f:
            if line.endswith(">:\n"):
                func = line.strip()
            m = pat.search(line)
            if m and int(m.group(1), 16) in targets:
                print(f"{targets[int(m.group(1), 16)]:<24} {line.strip()}   [{func}]")


if __name__ == "__main__":
    main()
