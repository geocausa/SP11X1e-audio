# Windows expansion: Dolby exonerated; remaining law is in the protected/actuator path

Date: 2026-08-18
Status: **Dolby is dynamic but is not the source of the missing positive physical expansion at the decisive rows. Remaining localization is downstream of Windows digital output, inside the protected/actuator path.**

## Decisive native-Windows internal captures

Fresh 25%-endpoint runs used the byte-identical consumer-matrix-v3 source, a
fresh `audiodg` graph for each selected row, raw float32 WASAPI packets, and a
full-memory dump while the target isolated tone was active. The existing dump
parser recovered VLLDP input and output staging directly.

At 630 Hz left, source 0.05 -> 0.2:

- VR/VLLDP-input source-normalized law: `-6.8265 dB`;
- VLLDP-output source-normalized law: `-7.8303 dB`;
- raw WASAPI source-normalized law: `-7.0844 dB`;
- SP7 physical source-normalized law: `+8.3234 dB`;
- physical-minus-raw-WASAPI law: `+15.4078 dB`.

At 1 kHz left:

- VR/VLLDP-input source-normalized law: `-5.1883 dB`;
- VLLDP-output source-normalized law: `-5.1868 dB`;
- raw WASAPI source-normalized law: `-5.5694 dB`;
- SP7 physical source-normalized law: `+5.6080 dB`;
- physical-minus-raw-WASAPI law: `+11.1774 dB`.

VLLDP postgain and final limiter state were unchanged across each low/high pair.
The recovered Dolby path therefore behaves dynamically, but in the **opposite
(compressive / low-level-supporting) direction** from the missing positive
physical expansion.

Reviewed exact values are in:

`artifacts/reviewed/2026-08-18-windows-vlldp-internal-level-law.json`

## Same-run original Windows-v3 cross-check

The original Windows v3 loopback and SP7 physical captures are two repeated
passes of the same matrix. Run 1 gives:

- 630 Hz left: digital high/low `+4.2736 dB`, physical `+20.0286 dB`, leaving
  `+15.7550 dB` downstream growth;
- 1 kHz left: digital high/low `+7.2115 dB`, physical `+17.4938 dB`, leaving
  `+10.2823 dB` downstream growth.

The physical rows repeat within fractions of a dB, so this is not a one-off
room impulse.

## Linux protected-path replay

The original Windows v3 digital loopback trajectory was copied read-only from
NTFS and replayed directly into Linux through `effect_input.sp11_dolby_bypass`,
thereby bypassing Linux Dolby while retaining the current Golden-v31 protected
AudioReach / WSA path, endpoint 25%, RX84 producer state, and the exact Windows
digital history/crossfeed.

SP7 RAW capture remained at 0.000 dB hardware gain. Across reasonable acoustic
alignment around the measured graph-start latency:

- Linux 630-Hz protected growth remains many dB below the Windows `+15.755 dB`
  downstream law;
- Linux 1-kHz protected growth remains near level-linear and many dB below the
  Windows `+10.282 dB` law.

A synchronized synthetic two-repeat discriminator independently gave a 630-Hz
protected law of only about `+0.27/+0.68 dB` per repeat.

Thus feeding Windows-like post-Dolby amplitudes into Linux does **not** recover
the Windows physical expansion. The remaining effect is genuinely downstream
of Dolby.

## Producer / consumer parity checks after localization

The current Golden-v31 `lpass-wsa-macro.c` is byte-for-byte identical to the
previous Windows-producer/no-HD2-v3 source. Live RX84/protected register reads
also confirm the Windows producer state: TOP_CFG1 `0x03`, compander enabled,
HD2 disabled, CFG1/CFG2 `0xef/0x8f`, half-dB disabled, Surface compander
coefficients, VBAT/BCL and CB_DECODE present symmetrically.

The loaded WSA8845 module is the same v28 module that closed the recovered
Windows cold/START/STOP lifecycle and CSR-off static gate; later v30/v31
transport/CKV work builds on that baseline. PA31/raw-DRE-zero was already
causally rejected.

## Static SP/SP_VI configuration

Current v31 successful GET_CFG replies return the complete SP and SP_VI live
static structures. SP reports 48 kHz / 16 bit / two speakers / feature mask
`0x1f` / 1-kHz control / 40-Hz pilot; SP_VI reports two speakers / 8-kHz VI /
40-Hz pilot / 1-kHz thermal control / 200-ms warm-up. These agree with the
recovered Windows-selected calibration payloads.

The full 10,464-byte graph-calibration aggregate warning is intentionally kept:
native Windows sends the same aggregate, receives status 3, logs a warning and
continues. The old 10,416-byte filtered experiment is not a promotion path.

## Remaining discriminator

The unresolved layer is now **dynamic protected behavior**, especially actual
CPS/SP feedback content and/or SP output behavior. P10 GREEN proves Windows-like
SoundWire CPS transport geometry; it does not prove the sample values delivered
to SP or calibrated limiter intervention.

The existing DSP graph already contains two ideal data-logging taps:

- `DATA_LOGGING 0x4003` after SP / splitter, before the WSA hardware path;
- `DATA_LOGGING 0x402a` immediately after the CPS CODEC_DMA_SOURCE.

The next preferred measurement is to collect those existing ADSP DIAG log
packets without changing the audio graph. This will distinguish a flat SP output
from a correct SP law followed by a later actuator mismatch, and directly show
whether CPS carries meaningful dynamic data.
