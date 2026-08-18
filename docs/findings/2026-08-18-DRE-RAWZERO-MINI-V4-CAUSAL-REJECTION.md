# DRE raw-zero mini-v4 causal A/B — safe but not the Windows expansion

Date: 2026-08-18
Status: **DRE/CSR stored-gain-field hypothesis rejected as primary expansion cause**

## Context

Consumer matrix v3 proved a large native-Windows downstream expansion law after
same-run digital normalization.  On reliable isolated-tone rows, Windows gains
about `+7.72 dB` median normalized transfer when source peak increases from
0.05 to 0.2, while Linux RX84 remains approximately level-invariant.

The remaining WSA8845 mismatch was concrete: Windows cold/active state uses
`DRE_CTL_1=0x00`, while Golden v31 normally retains PA Volume 24 / stored CSR
gain code 7 (`DRE_CTL_1 ~= 0x0e`) with CSR enable clear.  The immediately
preceding muted digital-zero gate proved PA31/raw-zero is now quiet on the
completed v31+DP2+RX84 stack, making a tiny unmuted causal test safe enough.

## Test design

No boot image or persistent Golden state changed.

One fresh 25%-derived Movie/VLLDP generation was created.  A continuous digital
zero stream kept one RX84/protection/PA generation active while a deterministic
12.5-second left-only mini stimulus was played twice:

1. PA24/24 control;
2. endpoint muted, PA31/31 written and read back;
3. endpoint unmuted, identical source played again;
4. endpoint muted, PA24/24 restored;
5. all temporary streams stopped.

Stimulus:

- frequencies: 630, 1000, 2000 Hz;
- source peaks: 0.05 and 0.2 at each frequency;
- source WAV SHA-256:
  `EB142AB18746B583B04866BF66D7AA1162B447665E709574EC1C29B1CC0C6712`;
- schedule SHA-256:
  `BEEA70E86C16E6D5B9590C012FFB864FEFAD6B24FA829DD57513A67C1D238D27`.

The hidden hardware-sink monitor recorded the same-run digital boundary
continuously.  Its SHA-256 is:

`D42AAC007CC2F3CCCE0338DA19617165B7F2CA37FAD8146EB6C8299FD6E19873`.

The digital source origins were `2.9756875 s` and `16.5810208 s`.  Every active
digital fundamental was numerically identical between PA24 and PA31 to
measurement precision.  Thus the PA A/B is downstream-clean.

## Physical capture

SP7 RAW microphone WAV:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\v31-rx84-dre-mini-v4\external-mic-raw-20260818-182242.wav`

SHA-256:

`701056BE4408BC9CF611E4916D224B5F72FEDFFE4032C0A093F6B83C46CD3E33`

Metadata SHA-256:

`683268FD37A79A53BFE046AFCF462EB341877FA677BFA6106DCE16E0A3C7E9FB`

The recorder metadata confirms SP7 hardware capture gain stayed at `0.000 dB`.
The physical 4-kHz marker fit gives source origins `37.5069792 s` (PA24) and
`51.1019792 s` (PA31).

Physical transfer analysis SHA-256:

`41420A0685996153639290834D6AA5E5DF59F252D7AC3F0486810A434BE09044`.

## Same-run normalized result

`transfer = 20*log10(physical coherent fundamental / digital coherent fundamental)`.

| Frequency | PA24 0.05->0.2 law | PA31 0.05->0.2 law | Native Windows v3 |
|---:|---:|---:|---:|
| 630 Hz | +0.50 dB | **-1.35 dB** | **+16.13 dB** |
| 1 kHz | -0.77 dB | **+4.64 dB** | **+10.24 dB** |
| 2 kHz | +3.20 dB | **+2.07 dB** | **+9.50 dB** |

PA31 is not a scaled version of the Windows behavior.  It is not even
directionally consistent across the three discriminating frequencies.

The direct PA31-minus-PA24 transfer change also fails to reveal a coherent
Windows-like law: at 630 Hz PA31 changes low/high transfer by about +3.26/+1.41
dB, at 1 kHz by -4.17/+1.24 dB, and at 2 kHz by +1.18/+0.06 dB.

No new WSA, SoundWire, XRUN or PA fault occurred.

## Restoration

After measurement the machine was returned to:

- PA24/24;
- 6% visible endpoint, unmuted;
- a newly instantiated **6%-derived** Movie/VLLDP generation;
- current VLLDP postgain `-658` = `-41.125 dB`;
- RX81 idle.

This is the same normal operating state as before the experiment.

## Conclusion

PA31 / stored-CSR-gain-zero is **not the missing Windows downstream expansion
mechanism**.  The historical raw-zero state is now safe on the completed stack,
but it does not reproduce the Windows isolated-tone level law.

Do not promote PA31 and do not spend additional experiments force-tuning DRE
register bits from analogy.  The primary investigation returns to the remaining
Windows-specific **AudioReach speaker-protection endpoint-effect / feedback
state semantics**, where qcadcm owns explicit `SetSpkrProtEndpointEffect` and
`SetSpkrProtEffectEpConfig` state beyond the already byte-identical SP/SPVI
calibration payloads.
