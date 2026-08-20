# 2026-08-20 active Offset2 runtime rejection

## Result

Native Windows active-bank chronology proved that WSA feedback master ports 10/11/13 use `Offset2=0` immediately before graph start, while `Offset2=0xff` belongs to stop/shadow programming. Golden Linux preserved `0xff` while enabling ChannelEnable and therefore produced `0x03ff....` rather than Windows active `0x0300....`.

A disposable Golden-v31 derivative corrected only that active-enable field under the opt-in module parameter `soundwire_qcom.sp11_feedback_active_offset2_zero=1`.

Loaded candidate SoundWire srcversion: `CE1DADE19E1CE61B7FC8843`.

At boot and at real direct-ALSA render start the candidate repeatedly showed the exact transition:

- port10: `0x00ff060f -> 0x0300060f`
- port11: `0x00ff0d0f -> 0x03000d0f`
- port13: `0x00ff001f -> 0x0300001f`

on both bank 0 and bank 1 as the SoundWire framework sequenced them.

## Canonical topology positive control

With canonical Golden Render-Parity topology, a 20 s direct `hw:0,0` 48 kHz stereo 997 Hz render produced 391 `cmd16` / `0x1586` audio frames. Decoding the Qualcomm logger header at offsets already established by prior captures gave:

- tap ID: 1
- sample rate: 48000 Hz
- payload: 192 bytes
- frames: 391
- nonzero frames: 390

This proves playback, DIAG router, log mask, packet decoder and ordinary render logging remained healthy on the Offset2 candidate.

Evidence:

`02-kernel/candidates/v31-feedback-active-offset2-zero-20260820/canonical-997hz-1586/`

## Correct disposable forced-topology transport

Simply embedding a forced topology in initrd was found insufficient because `audioreach_tplg_init()` requests `qcom/<driver>/<card>-tplg.bin` after switch-root. That first attempt still logged tap1 and is not counted as a tap3 result.

A safe disposable mechanism was then built without touching the real-root Golden firmware file:

- initramfs carries the known tap3 and tap2 topology binaries outside the normal firmware path;
- an initramfs pre-switch-root block copies the requested one into `/run/sp11-fw/...`;
- `/run` is moved into the real root by the normal initramfs flow;
- candidate cmdline sets `firmware_class.path=/run/sp11-fw` and `sp11_feedback_tplg=tap3` or `tap2`;
- real-root `/usr/lib/firmware/...Render-Parity-tplg.bin` remains canonical SHA `1b0c7217...` throughout.

The runtime override was verified after boot by hashing `/run/sp11-fw/...Render-Parity-tplg.bin`.

## CPS tap3 acceptance

Runtime topology SHA:

`e81a6deb919240d20c0479f64bbd0e8e1204673c3a899e0d9ba2464d158eb42a`

The topology disables ordinary tap1 log code and forces CPS logger tap3 immediate/out-of-island as in prior validated isolation runs.

During the 20 s direct ALSA 997 Hz render:

- playback completed normally;
- active port values were the exact Windows `0x0300....` forms throughout start/stop bank sequencing;
- DIAG total frames: 1;
- only frame: command `0x73` log-mask acknowledgement;
- `cmd16` / `0x1586` tap3 frames: **0**.

Evidence:

`02-kernel/candidates/v31-feedback-active-offset2-zero-20260820/tap3forced-fwselect-997hz/`

## VI tap2 acceptance

Runtime topology SHA:

`b5e4331b79957837d3625867e0bfa81709f4f1e8c3eab3336613888ff905d624`

During the same 20 s direct ALSA 997 Hz test pattern:

- playback completed normally;
- active ports again matched Windows `0x0300....` values;
- DIAG total frames: 1;
- only frame: command `0x73` log-mask acknowledgement;
- `cmd16` / `0x1586` tap2 frames: **0**.

Evidence:

`02-kernel/candidates/v31-feedback-active-offset2-zero-20260820/tap2forced-fwselect-997hz/`

## Decision

**Reject active SoundWire Offset2 as a sufficient VI/CPS fix.**

The Windows-vs-Linux active-bank mismatch was genuine and is now understood, but correcting it does not cause either CODEC_DMA_SOURCE branch to emit logger frames. Do not promote this patch and do not stack further bank/geometry guesses on it.

The remaining missing operation is downstream or orthogonal to this SoundWire master transport field. Continue from the already proven CODEC_DMA STM/HWD4 lifecycle and the evidence-backed WSA feedback producer/resource boundary.

Golden v31 remains the persistent fallback; none of these disposable entries changes `saved_entry=sp11-audio-golden-v31`.
