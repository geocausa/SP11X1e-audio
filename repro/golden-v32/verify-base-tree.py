#!/usr/bin/env python3
import hashlib, os, pathlib, sys
EXPECTED = "7e5f8ccd76f625cb678028fe6bab2d3ef0c03878c2af21433c96f4a78b813fef"

def tree_digest(root: pathlib.Path):
    h = hashlib.sha256(); count = 0; total = 0
    for p in sorted(root.rglob("*"), key=lambda x: x.relative_to(root).as_posix()):
        rel = p.relative_to(root).as_posix()
        if p.is_symlink():
            h.update(b"L\0" + rel.encode() + b"\0" + os.readlink(p).encode() + b"\0"); count += 1
        elif p.is_file():
            fh = hashlib.sha256(); size = 0
            with p.open("rb") as f:
                for b in iter(lambda: f.read(1 << 20), b""):
                    fh.update(b); size += len(b)
            h.update(b"F\0" + rel.encode() + b"\0" + str(size).encode() + b"\0" + fh.digest())
            count += 1; total += size
    return h.hexdigest(), count, total

if len(sys.argv) != 2:
    raise SystemExit(f"usage: {sys.argv[0]} /path/to/linux-7.1.5")
got, count, total = tree_digest(pathlib.Path(sys.argv[1]))
print(f"tree_sha256={got} entries={count} regular_file_bytes={total}")
if got != EXPECTED:
    raise SystemExit(f"base tree mismatch: expected {EXPECTED} got {got}")
