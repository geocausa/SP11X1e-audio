# UbiG volume-law correction checkpoint — 2026-08-23

Status: **implementation defects fixed on `ubig/deblob-main`; not promoted; downstream 160 Hz residual still under investigation**.

## Why this checkpoint exists

A subjective Windows-vs-UbiG listening preparation exposed a reproducible endpoint-volume-dependent tonal mismatch. The resulting 10/15/20/30/40/50% reverse-engineering campaign found two concrete UbiG defects and localized the remaining low-frequency residual beyond the userspace Dolby/UbiG boundary.

## Defect 1 — VLLDP postgain lifecycle

The prior candidate assumed VLLDP postgain was fixed for one Dolby/filter-chain generation. Direct same-process native-Windows memory reads disproved that inference.

Native Windows VLLDP `core+0xBB0` (applied) and `core+0xBB4` (staged) followed `round(endpoint_dB * 16)` exactly and reversibly:

- 8%: -595
- 10%: -545
- 15%: -452
- 17%: -423
- 20%: -385
- 25%: -332
- 30%: -289
- 40%: -220
- 50%: -167

The UbiG synchronizer now keys the queued state by `(filter-chain generation, endpoint postgain)` instead of generation alone. Replacement engines still force a requeue; duplicate numerical states are suppressed.

## Defect 2 — native candidate sample-flow order

Later Windows full-memory RE had already proven the speaker-path dependency:

`source -> DolbyApoVr -> VLLDP -> limiter`

and the proprietary Golden Linux bridge already implemented VR before VLLDP. The source-owned UbiG candidate had accidentally reintroduced the obsolete order:

`VLLDP -> VR`

The candidate now runs:

`input -> native VR/Stage-B -> native Stage-A/VLLDP -> output`

A deterministic regression gate (`ubig/tests/test_sp11_candidate_order.c`) locks this ordering.

### High-volume nonlinear-order oracle

10% -> 50%, stationary 500 Hz + 997 Hz:

| implementation | 500 Hz | 997 Hz | 500/997 shape |
| --- | ---: | ---: | ---: |
| Native Windows | -2.72771 dB | -0.17188 dB | -2.55583 dB |
| Old UbiG VLLDP->VR | about -0.566 dB | about -0.008 dB | about -0.558 dB |
| Corrected UbiG VR->VLLDP | -2.75528 dB | -0.17660 dB | -2.57868 dB |

The corrected candidate is within roughly 0.02–0.03 dB of the native Windows high-volume law, while the old order is wrong by about 2 dB in spectral-shape change.

## Proprietary Golden oracle parity

The corrected source-owned UbiG candidate was compared offline against the original proprietary Golden VR->VLLDP plugin over the full 10/15/20/30/40/50% postgain sequence using the same 12-frequency matrix.

At 50%, UbiG-minus-Golden DSP volume-law error was only:

- 125 Hz: -0.0126 dB
- 160 Hz: -0.0068 dB
- 250 Hz: -0.0045 dB
- 315 Hz: -0.0027 dB
- 500 Hz: -0.0019 dB
- 630 Hz: -0.0015 dB

This exonerates the source-owned VR/VLLDP DSP reconstruction for the remaining physical 160 Hz anomaly.

## Full corrected physical sweep

Authoritative native-Windows SP7 RAW capture:

`2D00DFACAF8F18C83A8423AF8059A41A10463593ABFD35F32652A54E5B763E62`

Authoritative corrected-Linux SP7 RAW capture:

`9B7AEB785D0F3DDDEB754BA9CE6B314A983615B5FBF6C57D57C70D2C735D8D15`

Corrected-Linux post-UbiG digital capture:

`826b9c2db854ff5f3207f4b8ac4476331527ecdc4e673ebf2c0d7253f4f519a2`

SP7 physical volume-law analysis JSON:

`CCF91977984BAE57032BA7B591ABBD5E0D1565CEE37921056FD112FCB9477C12`

Adjacent Windows-vs-Linux volume-law error at 315 Hz+:

- 10->15%: MAE 0.188 dB
- 15->20%: MAE 0.060 dB
- 20->30%: MAE 0.047 dB
- 30->40%: MAE 0.067 dB
- 40->50%: MAE 0.038 dB

At 630 Hz+ the last three transitions are approximately 0.029, 0.044, and 0.022 dB MAE.

The remaining clear outlier is 160 Hz:

- 30->40% Linux-minus-Windows increment: about -2.70 dB
- 40->50%: about -1.26 dB

250 Hz is much smaller (-0.08 and -0.35 dB respectively).

## MSIIR functional proof

At fixed 40% / Windows-equivalent CKV12, a stationary 160 Hz + 1 kHz probe was played while the exact production TLV path forced:

`CKV12 -> CKV16 -> CKV12`

Both live writes succeeded through `SP11 MSIIR Inject` (`tlv_write rc=0`) while PCM remained RUNNING.

Physical SP7 RAW capture:

`6FA7848B59327CFA66C9A9EBE2FAE30F2DF670CA6A1758489D7EEF38BB14B065`

Measured 160 Hz / 1 kHz ratio:

- CKV12 before: about +0.496 dB (rolling stable baseline ~+0.39 dB)
- CKV16: about -0.625 dB (rolling ~-0.85 dB)
- CKV12 after: about +0.755 dB (rolling ~+0.43 dB)
- measured CKV12->16 change: **-1.121 dB**
- decoded CKV12->16 prediction: about **-1.25 dB**

The reversible physical response closely follows the decoded MSIIR filter change. Therefore the persistent 160 Hz Windows-vs-Linux residual is downstream of VR/VLLDP and is not explained by a dead/wrong MSIIR application path.

## Current localization

Proven-correct / effectively closed for this issue:

1. Windows endpoint taper / Q28 volume transaction
2. live VLLDP endpoint postgain lifecycle (now fixed)
3. native VR->VLLDP sample ordering (now fixed)
4. source-owned UbiG vs proprietary Golden userspace DSP
5. CKV row selection and functional MSIIR application

Remaining territory:

`MSIIR output -> WSA8845 / CPS speaker protection -> amplifier / SoundWire actuator -> physical driver`

The 160 Hz residual may be protection-state, VI/CPS interpretation, WSA gain/protection policy, actuator nonlinearity, or residual physical/acoustic-system variance. Do not compensate it with a guessed EQ or alter MSIIR tables without new boundary evidence.

## Runtime state at checkpoint

- Golden-v32 kernel/boot baseline remains intact.
- UbiG everyday state restored to Custom, saved exact 20-band EQ, 14% visible volume, postgain -468, request=ack, last_error=0.
- filter-chain and volume-sync services active with zero restarts at last health check.
- UbiG promotion remains blocked.
- subjective A/B remains postponed until the 160 Hz downstream question is dispositioned.
