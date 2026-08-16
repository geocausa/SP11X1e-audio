# WSA8845 explicit DRE1-init v11 rejection — 2026-08-17

## Question

The complete Windows WSA8845 init oracle performs an explicit physical `DRE_CTL_1=0x00` write. Linux v5 begins from a software regmap default and never issues that exact init transaction. Retired patch 0055 changed the regmap default and runtime policy; it did **not** isolate the Windows physical init write.

## One-variable candidate

v11 starts from exact source-verified v5 and adds one SP11 2S supply-init write immediately after the existing `DRE_IDLE_DET_CTL=0x0f` correction:

`DRE_CTL_1 = 0x00`

All later v5 CSR/gain/DRE/PA behavior is unchanged. Fresh machine code was verified to contain the `regmap_write(0x34b1,0x00)` before signing/staging.

## Result

The first gate was the standard 10-second 48 kHz stereo S16_LE digital-zero stream at visible endpoint 1% and muted, with physical PCM independently observed `RUNNING`.

SP7 capture SHA-256:

`636C056B2B45C5981590550FA52081757844C9173E66D7401874720F3D99AD29`

Steady diff-RMS:

- channel 0 `6.2493e-4`;
- channel 1 `7.4805e-4`;
- median **`6.8649e-4`**.

That is **37.61x Windows** and **1.015x v5**. No quieting is present.

## Decision

**Reject v11 at the digital-silence gate.** No music or chirp escalation. The explicit physical DRE1-zero init write alone is not the missing latch.

Together v9, v10 and v11 now close the simplest DRE-history hypotheses individually:

- early transient CSR enable -> no effect when removed;
- late-vs-init DRE0 `0xf0` -> no effect when moved earlier;
- missing explicit DRE1 `0x00` init transaction -> no effect when added.

The next isolation should move into the Windows board/supply analog transitions, especially `CURRENT_LIMIT 0xd4 -> 0x44` and `CKWD_CTL_1 0x1b -> 0x13`.

Machine-readable result: `artifacts/reviewed/2026-08-17-v11-dre1-explicit-init-zero-noise-rejection.json`.
