#!/usr/bin/env python3
"""Hash the runtime-relevant ELF payload of a Linux module.

Deliberately ignores DWARF, symbol/string tables, compiler comments and GNU
build-id because those vary with source/build paths while not changing the
module loaded by the kernel. Includes allocatable sections, relocations,
.modinfo and __versions.
"""
import hashlib, re, subprocess, sys

def digest(path: str) -> str:
    text = subprocess.check_output(["readelf", "-SW", path], text=True, errors="replace")
    names = []
    for line in text.splitlines():
        m = re.search(r"\[\s*\d+\]\s+(\S+)\s+(\S+)\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+)", line)
        if not m:
            continue
        name, typ, flags = m.groups()
        if name.startswith(".debug") or name in {
            ".comment", ".note.gnu.build-id", ".symtab", ".strtab", ".shstrtab"
        }:
            continue
        if "A" in flags or typ in {"REL", "RELA"} or name in {".modinfo", "__versions"}:
            names.append(name)
    h = hashlib.sha256()
    for name in names:
        data = subprocess.check_output(
            ["objcopy", "--dump-section", f"{name}=/dev/stdout", path],
            stderr=subprocess.DEVNULL,
        )
        h.update(name.encode() + b"\0" + data)
    return h.hexdigest()

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} MODULE.ko")
    print(digest(sys.argv[1]))
