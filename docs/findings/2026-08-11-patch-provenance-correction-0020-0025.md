# Patch provenance correction: 0020 / 0025 — 2026-08-11

## Why this note exists

The repository patch files evolved during months of live debugging. They are
useful engineering artifacts, but some are no longer byte-for-byte copies of
the historical `git format-patch` object they originally represented. Future
reconstruction must therefore use explicit source/hash manifests rather than
assuming every old `index old..new` line still describes the current patch text.

## Directly reproduced 0020 discrepancy

Starting from official Linux v7.1.5 and replaying `0003` through `0014` gives
WSA884x Git blob:

`c44e9f212943dad310a55b5faefd87e8795bdaca`

That exactly matches the old side of the WSA diff header in current `0020`:

`index c44e9f2..fa9b9a3 100644`

Applying the **current text** of `0020-sp11-audio-vi-cumulative.patch` produces:

- Git blob: `0bb977291e47f291bf79fcb018c6732ab9b68304`
- SHA-256: `8c392c272b0a2c37b8c5497d60a65c75d1f8b21f90fb43fd6b68dd63fb0a2b10`

Therefore the present patch text does **not** produce the recorded historical
`fa9b9a3` WSA blob. The file was edited after that diff header was generated.
This is a provenance correction, not evidence that the later WSA change itself
was wrong.

## 0025 is also a living engineering artifact

Current `0025-sp11-finalize-clean-protected-playback.patch` contains code comments
explicitly marked `added 2026-08-01` for SP11 runtime MSIIR parameter injection.
It must therefore not be treated as an immutable July-only source export merely
because the patch number originated in the earlier clean-playback sequence.

Current file hashes at the time of this audit:

- `0020`: `31d48f107c85232ff38d86d4796727b0989968718322ac14aaec15bc67568dde`
- `0025`: `a56b4c1f5bb1523ecb92abb1925e5741d2ed86ed2fc705724b9bc344c7ff4984`

## New rule

New closure work is frozen as a fresh patch plus explicit per-file SHA-256
manifest. The CPS candidate begins with `0026` and is generated against a
reconstructed, hashed pre-CPS effective tree. Historical patch numbers remain
for archaeology and reproducibility but are no longer silently treated as
immutable source-control commits.
