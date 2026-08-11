#!/usr/bin/env python3
"""Verify that a staged kernel module tree matches one Module.symvers ledger."""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


MODULE_SUFFIXES = (".ko", ".ko.gz", ".ko.xz", ".ko.zst")


def parse_symvers(path: pathlib.Path) -> dict[str, str]:
    symbols: dict[str, str] = {}
    for line_number, line in enumerate(path.read_text().splitlines(), 1):
        fields = line.split()
        if len(fields) < 2:
            continue
        crc, symbol = fields[0].lower(), fields[1]
        previous = symbols.get(symbol)
        if previous is not None and previous != crc:
            raise ValueError(
                f"{path}:{line_number}: conflicting CRCs for {symbol}: "
                f"{previous} and {crc}"
            )
        symbols[symbol] = crc
    if not symbols:
        raise ValueError(f"no symbols found in {path}")
    return symbols


def module_paths(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.name.endswith(MODULE_SUFFIXES)
    )


def module_imports(path: pathlib.Path) -> list[tuple[str, str]]:
    result = subprocess.run(
        ["modprobe", "--dump-modversions", str(path)],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    imports: list[tuple[str, str]] = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) == 2:
            imports.append((fields[0].lower(), fields[1]))
    return imports


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--module-tree", required=True, type=pathlib.Path)
    parser.add_argument("--symvers", required=True, type=pathlib.Path)
    args = parser.parse_args()

    symbols = parse_symvers(args.symvers)
    modules = module_paths(args.module_tree)
    if not modules:
        raise ValueError(f"no modules found under {args.module_tree}")

    missing: list[str] = []
    mismatched: list[str] = []
    import_count = 0
    for module in modules:
        for required_crc, symbol in module_imports(module):
            import_count += 1
            provided_crc = symbols.get(symbol)
            if provided_crc is None:
                missing.append(f"{module}: {symbol} requires {required_crc}")
            elif provided_crc != required_crc:
                mismatched.append(
                    f"{module}: {symbol} requires {required_crc}, "
                    f"build provides {provided_crc}"
                )

    if missing or mismatched:
        for heading, records in (
            ("missing symbols", missing),
            ("CRC mismatches", mismatched),
        ):
            if records:
                print(f"{heading} ({len(records)}):", file=sys.stderr)
                for record in records:
                    print(f"  {record}", file=sys.stderr)
        return 1

    print(
        f"PASS: {len(modules)} modules, {import_count} versioned imports, "
        f"{len(symbols)} build symbols; zero missing symbols or CRC mismatches"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
