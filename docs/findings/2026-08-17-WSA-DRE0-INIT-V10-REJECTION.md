# WSA8845 DRE0-init v10 rejection — 2026-08-17

## Question

Windows establishes `DRE_CTL_0=0xf0` inside the coherent codec-init block before `DRE_CTL_1=0`. Exact v5 instead begins from generic `0x70` and only changes the DRE program-delay field to reach `0xf0` at speaker POST_PMU. Could that lifecycle placement explain why Linux CSR-off is noisy?

## One-variable candidate

v10 starts from source-verified v5 and adds one SP11 2S supply-init write immediately after the existing `DRE_IDLE_DET_CTL=0x0f` correction:

```text
DRE_CTL_0 = 0xf0
```

Everything else remains v5, including UCM/stored CSR gain, CSR enable history, PDM watchdog, current-limit, CKWD, producer/RX84/no-HD2 and PA transaction. Fresh machine code was verified before staging.

## Result

The first gate used the standard muted 10-second digital-zero oracle at endpoint 1%. Physical PCM was observed `RUNNING` and returned to `closed`. SP7 capture SHA-256:

`682EAA062132960BDCA4FB15FD1CB4E7806D0F7BE8E6F8BD69E9CF2558A0C4A3`

Steady diff-RMS:

- channel 0 `6.2774e-4`;
- channel 1 `7.4290e-4`;
- median **`6.8532e-4`**.

That is **37.55x Windows** and **1.013x v5**. Early DRE0 programming therefore produces no meaningful quieting.

## Decision

**Reject v10 at the digital-silence gate.** No program audio or chirp escalation. DRE0 init timing alone is closed as the primary quiet-CSR-off cause.

The retired patch 0055 does not close the next DRE-history question: it changed the Linux regmap default for `DRE_CTL_1` and removed runtime DRE writes, but did not add the explicit physical `DRE_CTL_1=0` transaction that Windows issues in the codec-init sequence.

Machine-readable result: `artifacts/reviewed/2026-08-17-v10-dre0-init-zero-noise-rejection.json`.
