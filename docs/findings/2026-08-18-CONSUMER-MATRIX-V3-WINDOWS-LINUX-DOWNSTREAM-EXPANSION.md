# Consumer matrix v3 — matched Windows/Linux downstream expansion

Date: 2026-08-18
Status: **physical/digital discriminator GREEN; Windows-only downstream level law proven; exact implementation mechanism unresolved**

## Question

The matched v2 RAW multisine experiment proved that native Windows and Linux
RX84 have nearly identical digital Dolby/APO drive while their physical speaker
outputs still differ.  Because v2 used harmonically related simultaneous tones,
that residual could still have been produced by harmonic/intermodulation energy
rather than a true level-dependent fundamental-gain law.

v3 removes that ambiguity.  It excites one frequency and one source channel at
a time, at three source peaks, and normalizes each acoustic condition by the
same-run digital output on the same OS.

## Byte-identical source

Generator:

`tools/generate_sp11_consumer_matrix_v3.py`

Source WAV SHA-256:

`ED983FB77F7F42FF4F593D75C981AD41E26F25EAE7FD46D23C49A9867A8558FE`

The WAV was generated independently on Linux and native Windows and matched
byte-for-byte.  It is 78.0 s / 48 kHz PCM16 with:

- frequencies `100,250,315,500,630,1000,1250,1600,2000,2500,4000,6300 Hz`;
- source peaks `0.0125`, `0.05`, `0.2`;
- left/right conditions interleaved;
- one tone / one source channel at a time;
- a stereo 1-kHz sync marker;
- a central 0.5-s coherent-fundamental analysis window per condition.

## Linux RX84 side

The Linux side used Golden v31 plus the isolated active-RX84 policy, 25% visible
Windows-Dolby endpoint, and the SP7 microphone fixed at exactly 0.000 dB hardware
capture gain.

Physical SP7 RAW WAV:

`8ACE8E2A8A0CC3DAF065B8F2C6C8ECE674DC179FC0D4B918E6A6CAF67AFC836E`

Physical analysis JSON:

`93686019A5061578D9EE2996936D2663247D05E33A266438D69DF6AA2A5C4DD3`

Valid hidden-speaker-sink monitor capture:

`44FCAC279C6BECCB0FBA03D94546806BA241A862DAC21BE8BED832D69F5153FD`

Digital analysis JSON:

`969B8D20B5EA25F614656B2750827E607DBDFED8597246B330582668E1AB419A`

The Linux digital matrix is extremely stable: median two-pass fundamental
change `0.00256 dB`, p90 `0.01796 dB`, maximum `0.10052 dB`.

For the 17 frequency/channel pairs where both the 0.05 and 0.2 physical rows
repeat within 1 dB, the change in normalized physical/digital transfer from
0.05 -> 0.2 is:

- median **`-0.1069 dB`**;
- mean **`-0.0405 dB`**;
- minimum `-0.8785 dB`;
- maximum `+2.3098 dB`.

Thus the reliable Linux RX84 isolated-tone fundamental transfer is broadly
level-invariant.

## Native-Windows side

Windows generated the same source WAV independently and reproduced the exact
SHA above.  The native speaker endpoint was set from its retained 10% baseline
to 25% (`-20.74741 dB`) for the capture, then restored to 10%, unmuted.

SP7 RAW WAV:

`59BA32016B529DE8EE1F44FF1E3B69DFF58CFCBBD227FF532C9584023DD0D170`

The RAW recorder metadata confirms the SP7 capture endpoint remained exactly
`0.000 dB` for the full capture.

Windows WASAPI loopback WAV:

`390F52FED24C9A368965821DB1F23B0F682FD9E62FCB64D5071927B1D050B3A3`

Windows digital analysis JSON:

`81D8EB43560F51DE021AB5EBC5A7804850ECDE620848BDA97B0D78AEF69598AF`

The native Windows digital path itself changes across repeated plays: its median
condition-level two-pass change is about `0.53 dB`, with some rows moving by
several dB.  Therefore every physical Windows row was normalized by its
**same-run** digital fundamental, rather than by a cross-run median.

The normalized 72-row Windows transfer matrix is preserved as:

`artifacts/reviewed/windows-consumer-matrix-v3-normalized-transfer.csv`

SHA-256:

`5A146638B2C6160C26AA6FE4DFDB0FED82BC0AC7F8F7A3E7638F0AD3F3AB82C5`

For the 13 Windows frequency/channel pairs where both normalized 0.05 and 0.2
rows repeat within 1 dB, the 0.05 -> 0.2 normalized consumer-transfer change is:

- median **`+7.7169 dB`**;
- mean **`+6.6403 dB`**;
- minimum `-0.8255 dB`;
- maximum `+16.1251 dB`.

Representative reliable rows:

| Frequency | Channel | Windows 0.05 -> 0.2 normalized transfer |
|---:|:---:|---:|
| 100 Hz | R | +7.09 dB |
| 500 Hz | R | +9.79 dB |
| 630 Hz | L | +16.13 dB |
| 630 Hz | R | +8.88 dB |
| 1 kHz | L | +10.24 dB |
| 1 kHz | R | +3.02 dB |
| 2 kHz | L | +9.50 dB |
| 2.5 kHz | L | +9.47 dB |
| 4 kHz | L | +7.72 dB |
| 6.3 kHz | L | +1.37 dB |
| 6.3 kHz | R | +2.06 dB |

This is not a small calibration offset.  It is a strong, content-level-dependent
consumer law downstream of the Windows loopback boundary.

## Causal conclusion

The v2 residual is **not** explained solely by multitone intermodulation.
Native Windows has a strong downstream expansion/DRE-like fundamental-gain law
that the Linux RX84 path does not reproduce.

The comparison is deliberately normalized after the host/Dolby/APO boundary:
Windows digital THD on the reliable stronger rows is tiny (typically around
`10^-5 .. 10^-4`), while physical harmonic content is much larger.  The missing
behavior therefore sits downstream of the digital loopback / Dolby output
boundary.

This is the strongest concrete explanation so far for the remaining Windows
speaker fullness/punch difference.

## What is already ruled out

The exact REV_0D speaker-protection calibration objects are already present in
and serialized identically by the Linux integration:

- SP tag, IID `0x4027`, 7 parameters, SHA
  `096FCCA5DD925692F29DB589A7431EBAF6CD8BC1926418914670C3C1520F9800`;
- SPVI tag, IID `0x4024`, 5 parameters, SHA
  `C383B831DB8F91A0D33B6BA79FF04852658882B50D4A187B2DEDFEEAB281BC8C`;
- dynamic protection frames, SHA
  `96AC15BB5F7D9AED6F681FD660ABA46EE4D9EC57725E52C322AFDDC8B073227A`.

The dynamic frames are the same Windows-proven SP RX mode, R0/T0, VI mode and
ExVI mode payloads (`0x080011e9`, `0x080011f5`, `0x080011f4`, `0x080011ff`).
So the result is **not** simply explained by a missing protection calibration
blob.

Windows qcadcm nevertheless has an explicit `SetSpkrProtEndpointEffect` /
`SetSpkrProtEffectEpConfig` state machine around those payloads.  Its ordinary
render path applies tag `0x04010005` for the VI endpoint hardware/effect
configuration.  Linux sends the already-selected 64-byte endpoint body directly
to CODEC_DMA_SOURCE `0x4026` (8 kHz, 32-bit, two channels, WSA interface 1,
mask `0x3`).  The final payload semantics look equivalent, but exact tagged
endpoint-effect state remains a secondary mechanism candidate rather than a
closed cause.

## Newly narrowed WSA8845 candidate

A concrete codec-side mismatch remains:

- native Windows explicitly initializes both WSA8845 `DRE_CTL_1 = 0x00` and no
  ordinary runtime rewrite was observed;
- Golden v31 keeps CSR fallback **disabled**, but its retained PA Volume 24
  leaves stored CSR gain code 7, raw `DRE_CTL_1 ~= 0x0e`.

Historical attempts to force raw `DRE_CTL_1=0x00` were unsafe/noisy, but they
predated multiple later prerequisites now proven on the accepted stack:

- v28 DP2/COMP `OffsetCtrl2=0x07`, which made Windows-style CSR-off quiet;
- v30 transport completion;
- v31 CKV correction;
- the current active RX84 / 0-dB Windows producer policy.

The sign of the v3 result—physical transfer rising strongly with source level—is
also qualitatively DRE-like.  This makes the stored DRE/CSR gain field a newly
justified isolated candidate, **not yet a proven cause**.

## Next safe experiment

Do not create a new boot image or mutate Golden v31.  Use a reversible,
runtime-only A/B through the existing ALSA PA controls:

1. preserve PA Volume 24 / raw `DRE_CTL_1 ~= 0x0e` as the control;
2. mute before activating the candidate;
3. set PA Volume 31, which maps the stored CSR gain field to zero while leaving
   CSR enable clear;
4. first run only a digital-zero/static safety gate with SP7 RAW at 0 dB;
5. immediately restore PA Volume 24 on any crackle/broadband-floor increase or
   PA fault;
6. only if the zero gate is GREEN, run a very small isolated-tone level A/B
   (for example 630/1000/2000 Hz, 0.05 vs 0.2) and test whether the Windows-like
   positive transfer law appears;
7. restore PA Volume 24 after the test regardless of outcome.

If raw-zero now safely creates the Windows-like expansion, the cause is strongly
localized to the WSA8845 DRE/CSR state.  If not, return to AudioReach
speaker-protection endpoint-effect / feedback-state semantics rather than
forcing additional amp registers.
