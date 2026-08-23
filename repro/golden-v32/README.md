# Golden v32 clean reproduction

This directory closes audit finding F01. It reconstructs the accepted SP11
Golden-v32 kernel module source state without using mutable historical build
objects.

## Inputs

- pristine local `02-kernel/linux-7.1.5`, verified by `verify-base-tree.py`;
- `source-overlay/` plus `v31-overlay.sha256` (23 files);
- `config-7.1.5-sp11-render-parity-v4` plus its SHA-256;
- ordered kernel patches 0069, 0070 and 0071, hash-pinned by `patches.sha256`.

The pristine base is not duplicated in Git. `base-tree.txt` records its required
identity. The overlay contains only project source/config state needed to turn
that base into the accepted Golden-v31 source baseline.

## Run

```bash
cd 01-audio-cps-review
JOBS=8 ./repro/golden-v32/build-and-verify.sh
```

Optional paths:

```bash
SP11_KERNEL_ROOT=/path/to/02-kernel \
SP11_LINUX_BASE=/path/to/linux-7.1.5 \
SP11_REPRO_WORK=/path/to/disposable-work \
JOBS=8 ./repro/golden-v32/build-and-verify.sh
```

The script deletes `SP11_REPRO_WORK` at startup. Never point that variable at a
historical kitchen or any directory containing evidence you intend to keep.

## Why the build modes are mixed

The promoted binaries were historically produced in two Kbuild contexts, and
`srcversion` is sensitive to that context. The verifier reproduces the deployed
identities rather than pretending every module came from one invocation:

| module | replay context |
| --- | --- |
| `snd-soc-lpass-wsa-macro.ko` | in-tree `O=` output after v32 patches |
| `snd-soc-wsa884x.ko` | scoped `M=` build against the same clean `O=` tree |
| `soundwire-qcom.ko` | scoped `M=` build against the same clean `O=` tree |
| `snd-soc-x1e80100.ko` | in-tree `O=` output; unchanged v31→v32 |
| `snd-q6apm.ko` | scoped `M=` output; unchanged v31→v32 |

## Gates

A successful run requires, in order:

1. whole-tree pristine-base digest;
2. config, overlay and patch SHA-256 manifests;
3. kernel release `7.1.5-sp11-render-parity-v4+`;
4. all five deployed Golden-v31 `srcversion`s;
5. exact v32 source hashes after applying only 0069-0071;
6. all five Golden-v32 `srcversion`s;
7. all five runtime ELF payload digests.

`runtime-module-digest.py` deliberately excludes path-sensitive DWARF,
symbol/string tables, compiler comments and GNU build-id while including
allocatable sections, relocation sections, `.modinfo` and `__versions`. This is
stronger for runtime-semantic replay than a raw `.ko` hash, which changes with
build paths and optional signing metadata.

The reviewed zero-state closure run completed on 2026-08-23 at 08:04:08 +01:00
with `GOLDEN v32 CLEAN REPRODUCTION PASS`.
