# CSR-off active-path noise oracle — 2026-08-16

## Result

A four-way fixed-geometry SP7 microphone comparison now gives a direct physical discriminator for H03:

| state | steady median RMS | steady median diff-RMS | diff-RMS vs Windows |
|---|---:|---:|---:|
| Windows, active non-zero tail | 0.00009449 | 0.00001825 | 1.00x |
| Linux CPS-v3, CSR on | 0.00008845 | 0.00001862 | 1.02x |
| Linux render-parity v5, CSR off | 0.00120023 | 0.00067653 | 37.06x |
| Linux DRE-cold v8, CSR off | 0.00139785 | 0.00079094 | 43.33x |

This is stronger than the earlier “static” listening reports because the discriminator does not depend on program material or subjective timbre. The broadband/difference floor changes by roughly 37--43x when the Linux amp is held in the CSR-off family, while Windows and the generic CSR-assisted Linux fallback remain essentially identical at the recorder floor.

Machine-readable evidence:

`artifacts/reviewed/2026-08-16-csr-off-active-noise-fourway.json`

The original SP7 analysis JSON is preserved off-repo at:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\csr-off-active-noise-fourway-20260816.json`

SHA-256:

`27B1D8B18BA537C9147A046AB46CDAE261B837586E468A6FF9EF5ABE55E841EE`

## Linux zero-stream controls

All three Linux cases used the same 10-second 48 kHz stereo S16_LE all-zero PCM stimulus. The visible Windows-Dolby endpoint was 1% and muted; the physical ALSA speaker PCM was independently observed as `RUNNING` during the stimulus and returned to `closed` afterward.

The CPS-v3 capture remains at the external-mic floor throughout the open PCM interval:

- capture SHA-256 `0BE1C590BF96A1B07300D88C7AED265986B771F07DA0C18E76E7F36BBC997B8C`;
- median steady diff-RMS `1.8615e-5`.

The v5 CSR-off capture produces a persistent broadband floor once the PA path opens:

- capture SHA-256 `58ED278A55A4CC23719B2B98FD2B737F59D2B5D5FD4CBC53798E51608190BA65`;
- median steady diff-RMS `6.7653e-4`;
- about `37.1x` the Windows value.

DRE-cold v8 shows the same noise class, modestly worse:

- capture SHA-256 `3A12957D596502F7EF92E912D15CB56B048787FE93A3E9B354BDFD18CADFF25F`;
- median steady diff-RMS `7.9094e-4`;
- about `43.3x` Windows and `1.17x` v5.

Therefore the initial v8-only interpretation was too strong. v8 did not *introduce* the PA-open noise; v5 already exhibits it. v8 still has no promotion case because it does not improve the defect and is slightly worse on this metric.

## Windows active-path proof

An exact-zero Windows tail alone would leave a possible loophole that the Windows driver might power-gate on sample content. The final control deliberately removes that ambiguity.

The Windows endpoint was set to 12% and unmuted. One continuous shared-mode WASAPI stream carried:

1. one second of stereo 997 Hz at amplitude `0.25`;
2. eleven more seconds of stereo 997 Hz at amplitude `0.0001` (-80 dBFS).

The tail therefore remains **non-zero for the full stream**.

SP7 clearly records the opening marker:

- channel 0 997-Hz amplitude about `0.00873`;
- channel 1 about `0.01052`.

That proves the real Windows speaker path was activated. During the following continuously non-zero -80 dBFS tail, the recorder returns immediately to its pre-stream broadband floor:

- channel-0 steady diff-RMS `1.7872e-5`;
- channel-1 steady diff-RMS `1.8635e-5`;
- median `1.8253e-5`.

Capture SHA-256:

`785369E8E092D56B7417B3714E3238A604BCC56CA1211956E707C84D4E5DEEF6`

Thus Windows is demonstrably capable of **CSR-off + active non-zero stream + quiet physical output**.

## Interpretation

The physical split is now:

```text
Windows CSR off      quiet
Linux CPS-v3 CSR on  quiet
Linux v5 CSR off     broadband hiss/static
Linux v8 CSR off     broadband hiss/static
```

This makes CSR itself unlikely to be a mysterious Windows-only acoustic effect. On Linux, generic CSR assistance is masking or compensating for another missing amp state. Windows can leave CSR disabled because an earlier WSA8845 initialization/state contract is different.

The next work therefore belongs before ordinary PA unmute: compare complete Windows vs Linux codec initialization and route-construction write history. Do not reopen DRE value sweeping.
