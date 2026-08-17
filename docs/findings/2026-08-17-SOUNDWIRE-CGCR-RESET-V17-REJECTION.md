# SoundWire CGCR-reset v17 rejection — 2026-08-17

## Question

Windows qcaucd's visible WSA SoundWire wake path enables two reference-counted 19.2-MHz Resource Hub resources, waits about 5 ms, writes v2 `CLK_START`, clears master clash status, and polls link state. Linux additionally pulses reset `swr_audio_cgcr`: LPASS AudioCC WSA CGCR register `0xb0`, bit 1, asserted for about 1 us then cleared on every normal clock-stop resume.

v17 tested whether that Linux pulse was the state-destroying difference.

## Candidate

v17 is exact v16 plus one parameterized normal-resume change:

`soundwire_qcom.sp11_skip_audio_cgcr_reset=1`

Only `reset_control_reset(ctrl->audio_cgcr)` is skipped. The v16 Windows-style post-stop frame-generator completion wait remains enabled; v13 WSA8845 analog-tail state, producer, machine driver, UCM and the 3000-ms Linux autosuspend delay are unchanged.

Patch: `patches/0058-SP11-soundwire-skip-cgcr-reset-v17-experiment.patch`.

## Structural gate

The experiment was electrically viable. On the controlled zero-stream cycle Linux logged:

- `SP11 SoundWire: skipping AudioCC WSA CGCR reset pulse on resume`;
- later `SP11 SoundWire: frame generator stopped before hclk gate`.

PCM entered `RUNNING`, returned `closed`, master and both WSA8845 slaves reached runtime suspend, and no link/SoundWire/PA/XRUN failure appeared.

## Physical result

The SP7 mic measurement was taken on the **next PA wake**, after one complete structural v17 PA cycle on the same boot.

Capture SHA-256:

`847522CCA7AB2EDD5EFF8785241C50F7048F828F5969E91D63E7896E21E72486`

Median steady diff-RMS was **`2.84080e-3`**. This is:

- `155.7x` Windows;
- `4.20x` v5;
- `8.95x` v13 cycle 1;
- `0.723x` v13 cycle 2;
- `0.991x` v15;
- **`1.35x` v16**.

Thus removing the CGCR reset pulse makes the repeat-cycle state materially worse than v16.

## Decision

**Reject v17.** Restore the normal Linux CGCR pulse. No additional acoustic cycle or program-audio escalation was run.

The next clean isolation is the independently live-proven timing difference: Surface Windows uses `SwrClockStopTimerMS=500`, while qcom Linux sets a 3000-ms SoundWire autosuspend delay. Test that timing on top of v16's beneficial post-stop completion wait, with the normal CGCR pulse restored.

Machine-readable result: `artifacts/reviewed/2026-08-17-v17-soundwire-skip-cgcr-rejection.json`.
