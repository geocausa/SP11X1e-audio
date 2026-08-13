# Windows final VOL_CTRL runtime endpoint-gain actuator gap — 2026-08-13

## Status

The recovered Windows endpoint **gain law** is correct on Linux, but the endpoint-gain **actuator / transition boundary** is not yet reproduced.

Fresh review of the preserved 2026-07-23 Windows KD capture proves that the final DEFAULT render `VOL_CTRL` instance `0x4a63` receives `PARAM_ID_VOL_CTRL_MULTICHANNEL_GAIN (0x08001038)` bodies whose Q28 values numerically match the later recovered `IAudioEndpointVolume` scalar->dB taper. Linux currently leaves `0x4a63` at unity and applies that same endpoint attenuation in the hidden downstream PipeWire/ALSA sink instead.

This is a real Windows/Linux lifecycle-boundary mismatch. It is a strong candidate for the user's volume-slider transient because Windows has a DSP-resident endpoint-gain update mechanism that Linux bypasses. It is **not yet proven** to cause the seek transient, and the original July operator action is not timestamped well enough to call the two extra `0x4a63` writes a proven slider gesture.

## Windows runtime evidence

Preserved raw capture:

`/home/geoca/Documents/SP11-PROJECT/01-audio/artifacts/live/kdnet-20260723/capture.log`

Reviewed capture inventory:

`artifacts/reviewed/windows-kdnet-20260723/`

All fourteen captured small `SET_CFG` bodies target the final render `VOL_CTRL` instances:

- DEFAULT: iid `0x4a63`;
- NOTIFICATION: iid `0x4a5f`;
- multichannel gain: pid `0x08001038`, 104-byte body;
- multichannel mute: pid `0x08001039`, 104-byte body.

The normal graph-open pattern is one gain body plus one mute body. In one DEFAULT interval the raw ordering instead contains:

1. DEFAULT graph OOB body;
2. normal `0x4a63/0x08001038` gain;
3. normal `0x4a63/0x08001039` mute;
4. **another** `0x4a63/0x08001038` gain, with left channel updated and right channel still at the old value;
5. **another** `0x4a63/0x08001038` gain, with both channels at the new value;
6. then a NOTIFICATION graph OOB body.

There is no intervening DEFAULT graph open between the two extra gain writes. They therefore cannot both be explained as ordinary one-time graph setup.

The canonical July assessment already described this as strong evidence of live per-channel volume/control updates while correctly leaving the interpretation of the transitional body as a staged gain update `[HYP]` until tied to timestamped Windows volume actions. That provenance discipline remains correct.

## The Q28 values are endpoint-volume state

The two captured DEFAULT values are:

| Q28 | Linear | dB | Recovered Windows taper match |
|---|---:|---:|---:|
| `0x0013615a` | `0.0047315136` | `-46.499998 dB` | ~4.0% UI -> `-46.506039 dB` |
| `0x007dda19` | `0.0307255723` | `-30.250000 dB` | ~13.04% UI -> `-30.253454 dB` |

The errors against the independently captured 201-point `IAudioEndpointVolume` table are only about 0.006 dB and 0.0035 dB respectively.

This rules out treating the `0x4a63` values as an unrelated protection calibration. They encode the same endpoint attenuation state that V01/V02 later reconstructed from Windows directly.

## Exact Windows body shape

The `0x08001038` parameter body is exactly 104 bytes:

- `num_config = 8`;
- entry 0: channel mask LSW `0x2`, MSW `0`, Q28 gain;
- entry 1: channel mask LSW `0x4`, MSW `0`, Q28 gain;
- six zero channel entries;
- four bytes of trailing padding.

Offline reconstruction reproduces captured bodies `setcfg_00`, `setcfg_04`, and `setcfg_08` byte-for-byte at `0x0013615a`, and `setcfg_11` byte-for-byte at `0x007dda19`.

`setcfg_10` is the transitional body: mask `0x2` already carries `0x007dda19` while mask `0x4` still carries `0x0013615a`. The next body has the new gain on both.

## Current Linux mismatch

The exact currently booted Headroom-Test topology contains final DEFAULT `VOL_CTRL 0x4a63`, but its startup multichannel-gain body hard-codes both L/R channels to unity:

`0x10000000` Q28 = 1.0.

The protected topology exposes no userspace mixer control for the `sp11.vol_ctrl.4a63` widget. Its only ordinary topology mixer is `MultiMedia1`.

Linux V01/V02 currently reproduce the endpoint law by:

1. recovering the visible virtual-sink scalar;
2. mapping it through the exact Windows taper;
3. writing VLLDP/Dolby postgain from the same dB state;
4. selecting the Windows MSIIR CKV from the same state;
5. applying endpoint attenuation to the hidden downstream PipeWire/ALSA sink.

Therefore V01/V02 remain valid for **gain-law/state parity**, but not for **actuator/transition-boundary parity**.

The clean kernel lineage does contain generic runtime DSP volume-update infrastructure: `audioreach_put_vol_ctrl_audio_mixer()` was fixed to push a live gain when a topology volume control is changed while its widget is powered. That infrastructure is not wired to this evidence-locked final `0x4a63` multichannel-gain body in the protected topology. In other words, the generic transport survived, but this exact Windows endpoint actuator was not integrated.

## Why it is relevant to the physical transient

The final `0x4a63` block lives in the protected AudioReach render chain downstream of the host-side loopback/monitor boundary. This is compatible with the existing localization result: Windows WASAPI loopback can reach nearly full scale after a seek while the physical Windows speakers remain smoothly capped.

The current Linux endpoint attenuation occurs in a different host-side gain path. Moving the Linux master slider therefore changes the PipeWire/ALSA gain boundary while also updating Dolby postgain and, at some values, MSIIR CKV. Windows has direct evidence for endpoint gain being represented inside final DSP `VOL_CTRL` instead.

That makes the actuator mismatch a strong explanation candidate for the user's slider spike. It does **not** yet prove that `0x4a63` itself performs the missing seek smoothing. Endpoint gain is normally unchanged during a seek, so a seek link requires further lifecycle/control evidence.

The adjacent `0x4a63/0x08001037` policy is now independently recovered from the exact Windows `qcadcm8380.sys` `SetVolume` builder. The decompile logs its three fields verbatim as `Ramp period_ms:0x%x step_us:0x%x ramping_curve:%u`, and uses parameter literal `0x08001037`. The exact booted Linux topology already carries the same 12-byte payload on final `0x4a63`: `10, 1000, 3`, i.e. a 10 ms ramp period, 1000 us step and ramping-curve ID 3. The intermediate Windows-like `VOL_CTRL 0x4669` carries the same tuple.

This materially sharpens the mismatch: Linux has already deployed the Windows final-VOL_CTRL **ramp policy**, but endpoint-volume changes never reach that block because `0x4a63/0x08001038` remains at unity. The preserved Windows small runtime `SET_CFG` capture contains `0x1038` gain and `0x1039` mute bodies, not a separate `0x1037` body, which is consistent with the ramp policy being configured once and then reused by later gain writes. See `artifacts/reviewed/2026-08-13-final-volctrl-ramping-policy.json`.

## Isolated diagnostic candidate

No live control was added. The isolated exact-target candidate is preserved in
the canonical repository as patch `0047` and accompanying offline/A-B tools.

Patch: `patches/0047-q6apm-add-SP11-final-endpoint-volume-Q28-control.patch`.

Patch SHA-256:

`e604bdeb118a2961687380f9980da5d930a3885407629dca3db8f4717429c13c`

Userspace offline generator: `tools/sp11_final_volume_q28.py`.

Safe actuator A/B wrapper: `tools/sp11_volume_actuator_ab.py`, covered by
`tests/test_sp11_volume_actuator_ab.py`.

Generator SHA-256:

`974d8c82385ddd27478f5e9720f2887009006e8270b009ed8fe0a5a36bd4a91b`

The kernel candidate deliberately does **not** widen the existing MSIIR arbitrary-payload allowlist. It adds one dedicated operation only:

- fixed iid `0x4a63`;
- fixed pid `0x08001038`;
- one user-supplied Q28 scalar;
- values above Q28 unity rejected;
- exact 104-byte L/R Windows body generated in-kernel.

Strict `checkpatch` reports 0 errors, 0 warnings and 0 checks.

Exact-release build result for the combined isolated q6apm scratch tree:

- `snd-q6apm.ko` SHA-256 `75aa626abb7253dcb37050fa844726e2ea72dd2529389dccb2761f886d3da440`;
- srcversion `7F8E1452BC021273EECD2C7`;
- vermagic `7.1.5-sp11-cps-v3+ SMP preempt mod_unload modversions aarch64`.

The module is unsigned and **has not been installed or loaded**.

## Next gate

When a live listening window is appropriate, compare two gain-actuator paths on an already-running graph using a muted/low-volume safety setup first:

- current host-side PipeWire/ALSA endpoint attenuation;
- exact final-DSP `0x4a63/0x08001038` endpoint attenuation, avoiding double attenuation.

The test must preserve the exact Windows taper, Dolby postgain and MSIIR state. Only the location/transition actuator should differ. Physical slider-transient behavior then decides whether V04 closes or whether the remaining cause lies elsewhere.

Do not deploy this candidate while the user is away.
