# DRE raw-zero runtime candidate on v31 + RX84 — static safety gate

Date: 2026-08-18
Status: **GREEN for muted digital-zero static safety; program/tone behavior not yet tested**

## Why this was reopened

Historical PA-Volume-31 / raw-`DRE_CTL_1=0x00` experiments were rejected because
they produced roughly `2.1e-3` SP7 first-difference RMS, about 116x the retained
Windows room-floor reference. Those tests predated the later DP2/COMP
`OffsetCtrl2=0x07` closure and the active RX84/0-dB Windows producer policy.

Consumer matrix v3 has now independently proved a strong native-Windows
**downstream level-dependent expansion law** that Linux RX84 lacks. Native
Windows also initializes the WSA8845 `DRE_CTL_1` register to `0x00`, while the
accepted Linux runtime keeps CSR fallback disabled but retains stored CSR gain
code 7 through PA Volume 24 (`DRE_CTL_1 ~= 0x0e`). This made a bounded runtime
retest justified on the completed stack.

## Candidate and safety constraints

No kernel, GRUB, UCM or persistent Golden-v31 state was changed.

The candidate was applied only through the existing ALSA PA controls while the
normal speaker graph was active:

- baseline: `SpkrLeft/Right PA Volume = 24`;
- candidate: both controls = `31`;
- restore: both controls = `24`.

The established control mapping makes value 31 the stored-CSR-gain-zero state.
The candidate was not treated as a direct register/MMIO experiment.

Throughout the measured sequence:

- visible endpoint stayed at 6%;
- endpoint DSP mute was ON;
- source was stereo 48-kHz PCM16 digital zero;
- physical PCM was RUNNING;
- active producer policy reached RX84;
- SP/SPVI/CPS and PBR/COMP/BOOST remained otherwise unchanged;
- an unconditional cleanup trap restored PA24/24 and the pre-test endpoint mute
  state.

After the test the machine returned to 6%, unmuted, PA24/24, PCM idle and RX81.
No new WSA, SoundWire, XRUN or PA fault was observed. The only qcom-apm status-3
record was the already-accepted graph-start calibration continue condition.

## Physical capture

The SP7 microphone externally recorded the SP11 speakers. The SP11 microphone
path was not used.

SP7 RAW WAV:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\v31-rx84-dre-rawzero-static-valid\external-mic-raw-20260818-181616.wav`

SHA-256:

`36BE4BB5410E05DE907462E8EC74A3EB24AE56C5274B7EDDBCC90F632DE97F9D`

Recorder metadata SHA-256:

`BC58CBEA898F2192415C703B9A504EE06BD74FF107BC16566FDCD234F92B21CA`

The metadata confirms the SP7 hardware capture endpoint remained exactly
`0.000 dB` for the full 75-second capture.

## Result

The same 0.5-second first-difference combined-RMS metric family used for the
v28/v30 physical static closure was applied to 16 central bins per 10-second
hardware state:

| State | Median diff-RMS | Min | Max |
|---|---:|---:|---:|
| PA24 baseline | `2.6277072e-5` | `2.5186773e-5` | `2.8440945e-5` |
| PA31 candidate | `2.6174020e-5` | `2.5201791e-5` | `2.8671289e-5` |
| PA24 restore | `2.6003608e-5` | `2.5003526e-5` | `3.7101915e-5` |

Ratios:

- candidate / baseline = **`0.99608x`**;
- candidate / restore = **`1.00655x`**;
- baseline / restore = `1.01052x`.

The high-frequency secondary discriminator is likewise stable: HP6k RMS is
`1.6604e-5` baseline, `1.6359e-5` candidate and `1.6482e-5` restore.

No transition-window needle comparable to a PA crackle was found around either
the 24->31 or 31->24 write.

Reviewed analysis:

`artifacts/reviewed/2026-08-18-dre-rawzero-static-analysis.json`

Original SP7 machine-generated analysis SHA-256:

`40EC6C3866C66FA769C34FE30E0BBE5F3DABD3E77EFF424F2D597B1B95B97067`

## Conclusion

The historical raw-zero broadband failure **does not reproduce on the completed
Golden-v31 + DP2/COMP + active-RX84 stack**. PA31 remains in the same room-floor
class as PA24 under muted digital-zero playback.

This is a meaningful safety-boundary change from the rejected v26/v27 era, but
it does **not** yet prove that the DRE/CSR field causes the Windows v3 expansion.
The next allowed test is a very small isolated-tone transfer A/B at PA24 versus
PA31. If the Windows-like positive level law appears only at PA31, the cause is
strongly localized. If it does not, PA31 must be restored and the investigation
returns to the AudioReach speaker-protection endpoint-effect/feedback state.
