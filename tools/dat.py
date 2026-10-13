"""Dump a Petroglyph text DAT file (mastertextfile_*.dat) as KEY<TAB>value lines.

Usage: python dat.py <file.dat> [regex]
"""
import re
import struct
import sys


def read_dat(path):
    data = open(path, "rb").read()
    (count,) = struct.unpack_from("<I", data, 0)
    off = 4
    recs = []
    for _ in range(count):
        crc, vlen, klen = struct.unpack_from("<III", data, off)
        recs.append((vlen, klen))
        off += 12
    out = []
    values = []
    for vlen, _ in recs:
        values.append(data[off:off + vlen * 2].decode("utf-16-le", "replace"))
        off += vlen * 2
    for (vlen, klen), val in zip(recs, values):
        key = data[off:off + klen].decode("ascii", "replace")
        off += klen
        out.append((key, val))
    return out


if __name__ == "__main__":
    pat = re.compile(sys.argv[2], re.I) if len(sys.argv) > 2 else None
    for k, v in read_dat(sys.argv[1]):
        line = f"{k}\t{v}".replace("\n", "\\n")
        if pat is None or pat.search(line):
            sys.stdout.buffer.write((line + "\n").encode("utf-8"))
