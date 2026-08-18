# Golden v31 prior/new-CKV 40 Hz physical gate

Date: 2026-08-18
Status: **first fixed-geometry physical gate GREEN; independent repeat pending**

## Candidate

Golden v31 is v30 plus one exact runtime semantic correction: GainStep-dependent
OOB calibration is emitted only when prior/new GainStep CKV comparison changes
key `0x01000011`.  Same-CKV channel calls send final stereo VOL_CTRL only.

Loaded q6apm srcversion:
`687B16CF9C43B43E90C0746`.

Userspace mode at boot:

```text
mode=windows-lr
ckv_delta=prior-new
volume_only_control_values=16
endpoint_mute=exact-dsp
```

Persistent GRUB default remained `sp11-audio-golden-v28` and the one-shot v31
entry was consumed.

## Exact physical reproduction

The SP7 microphone remained in the fixed keyboard-length fixture.  The source
is the same 48 kHz stereo PCM16, 40 Hz, -36 dBFS WAV used for the matched native
Windows oracle:

`D900CE43A0C815FA8AC054629E65E3042BDB6E0CE9F1AD44135AA3F8F889B3E3`.

Sequence:

- visible endpoint at 46%, unmuted;
- warm continuous 40 Hz for 15 s;
- 20 native-path 2% Volume-Down keys, 46 -> 6%;
- 20 Volume-Up keys, 6 -> 46%;
- repeat DOWN and UP once more;
- restore 6%, unmuted.

SP7 external-mic capture:

- path:
  `C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\linux-v31-ckv-delta-lf40-gate\external-mic-20260818-105256.wav`
- SHA-256:
  `2351D7AB50849E962F052B772E1D5A10C010D32D494B30C2246B7406CD6CC1F2`.

SP11 stage log SHA-256:
`55A8A66EB22C00A3396138F4379C1B0F5B4D3DF24BF9FF6370FBBF240F13EB6A`.

## Result

Using the exact same fourth-order high-pass / per-key analysis method as the
v30 and Windows fixed-geometry captures:

| Metric | Native Windows | v30 | v31 |
|---|---:|---:|---:|
| HP500 DOWN p95 | 6.1546e-5 | 6.0663e-5 | 6.8198e-5 |
| HP500 UP p95 | 6.1937e-5 | 2.7855e-3 | **6.6466e-5** |
| HP500 UP/DOWN | 1.006x | 45.918x | **0.975x** |
| HP2000 UP p95 | 4.8846e-5 | 1.6833e-3 | **5.1062e-5** |
| HP6000 UP p95 | 3.6976e-5 | 1.0200e-4 | **4.0197e-5** |

v31 reduces the v30 HP500 UP p95 by **41.91x**.  Its UP p95 is only 1.073x
the matched Windows reference and its DOWN p95 is 1.108x Windows, well within
the small run/room-floor variation seen in the SP7 fixture.  More importantly,
the one-sided directionality disappears entirely.

No measured v31 key edge reached the `10^-3` class that defined the Linux v30
defect.  The largest HP500 edge was `8.0510e-5`.

## Runtime transaction proof

The service exercised all three recovered prior/new cases during the physical
run:

- same-row `windows-ckv:vol->vol`;
- upward row change `windows-ckv:cal->vol`;
- downward row change `windows-ckv:vol->cal`.

There were no transaction failures.  The only qcom-apm error during the run is
timestamped at graph start before the measured key sequence and matches the
already-known unsupported startup-calibration record.

## Conclusion

The v30 40 Hz Volume-Up crackle was caused by re-applying GainStep-dependent
runtime calibration when the Windows prior/new CKV delta did not contain a
GainStep change.  Reproducing Qualcomm's prior/new CKV semantics removes the
physical defect without changing Dolby, ramp policy, WSA8845, SoundWire, PA,
DP1/DP2/DP3 transport, endpoint taper, or the recovered calibration values.

This first physical gate is strong enough to mark the candidate technically
promising but not yet promote it.  Required next gates are an independent 40 Hz
repeat and ordinary program/mute/seek listening.

## Independent repeat — GREEN

A second fresh SP7 external-microphone capture repeated the exact fixed-geometry
40 Hz sequence without changing software, device positions, source, or key
cadence.

Repeat capture:

- SHA-256 `19497C9EF15242CEC79DAD16C63F595ABF323EB9378B547BEC3D5D9D226F5E91`;
- stage log SHA-256 `9D5B9566A669220C60AACD3121A8DE41C18E24E1CC763DAF5FAD494336EF84EF`.

Repeat physical result:

- HP500 DOWN p95 `6.7856417e-5`;
- HP500 UP p95 `6.4095212e-5`;
- HP500 UP/DOWN `0.94457x`;
- HP2000 DOWN p95 `5.1471087e-5`;
- HP2000 UP p95 `5.1471284e-5`;
- HP2000 UP/DOWN `1.000004x`;
- HP6000 DOWN p95 `4.0229492e-5`;
- HP6000 UP p95 `3.9858982e-5`;
- HP6000 UP/DOWN `0.99079x`;
- largest repeat HP500 UP edge `6.5330607e-5`.

The independent repeat therefore confirms the first gate.  The pathological
one-sided v30 transient remains absent and v31 stays at the fixed-fixture noise
class in both directions.  The 40 Hz Volume-Up physical defect is **objectively
closed** on v31.  Promotion still requires ordinary program/mute/seek smoke and
operator listening approval.
