# SP11 MicArray EP16 TX/VA cross-macro build gate — 2026-08-25

Windows oracle commit `6a4f180` closed the ordered qcaucd register lifecycle for the normal Surface Pro 11 MicArray.  The important ownership boundary is:

```text
VA shared FS gates
TX shared gates
TX DEC0 <- DMIC1 / MSM_DMIC
TX DEC1 <- DMIC0 / MSM_DMIC
VA DMIC group0 acquire: reset-release, DIV4, RUN, reset-release
TX path clocks/rate startup
```

Teardown performs both TX stop preambles, clears VA DMIC group0 RUN, then clears TX path/shared gates and finally VA shared gates.

Patch `0078-ASoC-lpass-SP11-share-VA-DMIC-clock-with-TX-capture.patch` implements that ownership naturally.  `lpass-macro-common` provides a small shared-DMIC clock broker; the Denali VA macro registers the provider; the Denali TX DEC group requests DMIC1 then DMIC0 after all coalesced DEC PRE_PMU events and before the first TX POST_PMU path-clock write.  It releases both shared users after all DEC PRE_PMD events and before the first TX POST_PMD path-clock clear.

The TX consumer validates the exact Windows EP16 route before acquiring the provider:

```text
AIF1 active decimators = DEC0 | DEC1
DEC0 source = MSM_DMIC, DMIC mux = DMIC1
DEC1 source = MSM_DMIC, DMIC mux = DMIC0
AIF2/AIF3 inactive
```

The patch passed strict checkpatch with 0 errors, 0 warnings and 0 checks, and compiled on SP11 against the exact Golden-v33 `7.1.5-sp11-render-parity-v4+` kitchen.

Candidate module SHA-256 values:

```text
snd-soc-lpass-macro-common.ko a75a88d94f26d961c9234b52711e0b96fd2aead306c79b81e1b37f839a0c8a4d
snd-soc-lpass-va-macro.ko     85847b0f256be62da2f897a140129c29a225c9f8f09ca6b6af456902307081f0
snd-soc-lpass-tx-macro.ko     60af5240392909712d378966dccd7cebf3dd4fe458e96510fbf5f07e4c8c10bc
```

All three report the exact running Golden vermagic.  After the build, the Golden codecs source tree, codecs output tree, `Module.symvers`, and `modules.order` compared byte-for-byte with the pre-build snapshots.

Golden runtime, `/boot`, `/lib/modules`, topology and UCM were not changed by this build gate.
