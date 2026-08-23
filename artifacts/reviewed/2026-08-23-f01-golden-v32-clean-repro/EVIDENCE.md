# F01 Golden v32 clean-reproduction closure — 2026-08-23

Audit finding F01 is closed by a true zero-state replay using the tracked recipe
under `repro/golden-v32/`.

## Result

- start: `2026-08-23T07:10:47+01:00`
- finish: `2026-08-23T08:04:08+01:00`
- exit code: `0`
- terminal gate: `GOLDEN v32 CLEAN REPRODUCTION PASS`
- kernel release: `7.1.5-sp11-render-parity-v4+`
- pristine Linux 7.1.5 whole-tree SHA-256:
  `7e5f8ccd76f625cb678028fe6bab2d3ef0c03878c2af21433c96f4a78b813fef`
- Golden config SHA-256:
  `4fed1ee935cff7589ed2941d0bf2ddec4ddd2a03d919b9dc30ce20f5d85665ca`

The script deleted its disposable work directory, reconstructed source from the
pristine base plus the tracked 23-file Golden-v31 overlay, built clean v31,
proved all five deployed v31 srcversions, applied only patches 0069-0071,
proved the four promoted v32 source hashes, rebuilt the affected modules in the
historical Kbuild contexts, and proved all five v32 srcversions plus all five
runtime ELF payload digests.

`zero-state-final/PASS.txt` records every identity and digest. The raw `.ko`
SHA-256 values are retained only as run-local provenance in
`zero-state-final/v32-raw-ko.sha256`; they are not the semantic replay gate
because DWARF/build paths and optional module signatures can change raw bytes.

The only stderr output in the successful run was existing
`-Wframe-larger-than` compiler warnings in unrelated kernel code. There were no
patch, compile-fatal, modpost, srcversion, source-hash, runtime-digest or recipe
gate failures.

No module was installed, no initrd/boot image or GRUB configuration was changed,
and the running Golden v32 deployment was untouched.
