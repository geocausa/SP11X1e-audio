#!/usr/bin/env python3
"""Verify private Dolby runtime-derived fixtures by exact SHA-256.

The fixtures are intentionally excluded from Git because several contain
captured Windows VLLDP state/pointers. This script makes local evidence tests
fail closed if a different fixture set is supplied.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EXPECTED = {
    "analyzer_inputs.bin": (9908, "cde48666858e1f184f8d0023bc7cae1e51f9ff39bda9591584c230f56ee19e00"),
    "leveler_inputs.bin": (2916, "635d8c3f056ce7b32fce21affa289fd9c93dd2ef435f67e7e0a34f403751aabd"),
    "synth_inputs.bin": (8892, "a82de8d7225480b6612d4b639443324add701fad473414977deb4422534e0a49"),
    "full_chain_seed.bin": (18208, "7f250ab12e9f21d3ce678856710bbb3ce1cd08194db7e5dd9cb588f17c3fec96"),
    "full_chain_seed_pre081602_p0.bin": (18628, "b090d4184f5dcad80842a19bafff292a1cdc6c61f5a47f0030e9216c8aff7886"),
    "full_chain_seed_pre081602_p1.bin": (18628, "81efa22294d5d9057355026e518713927dda104d9e245a592b04379df052d517"),
    "full_chain_seed_post081812_p0.bin": (18628, "d0a50123df9ff79958a369a89aba45a5aca83e62e8bac1f8f6c34c6ce04ea532"),
    "full_chain_seed_post081812_p1.bin": (18628, "224bd9ebf396cf9f41a21633e553cfaba4140ebf71c44dad4ae22e9a3e3f6928"),
}


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument(
        "--required-only",
        action="store_true",
        help="verify only analyzer/leveler/synth/full_chain_seed required by current make targets",
    )
    args = parser.parse_args()
    names = list(EXPECTED)
    if args.required_only:
        names = names[:4]

    failed = False
    for name in names:
        expected_size, expected_sha = EXPECTED[name]
        path = args.directory / name
        if not path.is_file():
            print(f"MISSING {path}")
            failed = True
            continue
        size = path.stat().st_size
        actual_sha = digest(path)
        ok = size == expected_size and actual_sha == expected_sha
        print(
            f"{'OK' if ok else 'FAIL'} {name} size={size} sha256={actual_sha}"
        )
        failed |= not ok
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
