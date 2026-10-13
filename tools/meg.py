"""Minimal reader for Petroglyph MEG v1 archives (Empire at War / Forces of Corruption).

Usage:
  python meg.py list <file.meg> [substring]
  python meg.py extract <file.meg> <outdir> [substring]
"""
import os
import struct
import sys


def read_meg(path):
    with open(path, "rb") as f:
        num_names, num_files = struct.unpack("<II", f.read(8))
        names = []
        for _ in range(num_names):
            (n,) = struct.unpack("<H", f.read(2))
            names.append(f.read(n).decode("ascii", "replace"))
        entries = []
        for _ in range(num_files):
            crc, idx, size, start, name_idx = struct.unpack("<IIIII", f.read(20))
            entries.append((names[name_idx], size, start))
    return entries


def main():
    cmd, meg = sys.argv[1], sys.argv[2]
    entries = read_meg(meg)
    if cmd == "list":
        sub = sys.argv[3].upper() if len(sys.argv) > 3 else ""
        for name, size, _ in entries:
            if sub in name.upper():
                print(f"{size:10d}  {name}")
    elif cmd == "extract":
        out = sys.argv[3]
        sub = sys.argv[4].upper() if len(sys.argv) > 4 else ""
        with open(meg, "rb") as f:
            for name, size, start in entries:
                if sub not in name.upper():
                    continue
                dest = os.path.join(out, *name.split("\\"))
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                f.seek(start)
                with open(dest, "wb") as o:
                    o.write(f.read(size))
                print(dest)


if __name__ == "__main__":
    main()
