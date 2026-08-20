# 2026-08-20 DP14 shadow runtime proof and acceptance setup

Status: **Windows runtime parity gap proven; Linux disposable candidate built; tap2/tap3 acceptance pending**

## Windows runtime proof

A fresh Windows KDNET capture armed qcaucd8380's generic MMIO helper at live base `fffff800`43ab0000 + `0x1bf80`, filtered to writes targeting physical `0x06b11e64`.

During the qcaudminiport/qcaucd protected speaker hardware-create path it captured:

- physical address: `0x06b11e64`
- value: `0x00ff191f`
- qcaucd LR: `+0x3f7a8`
- thread stack:
  - `qcaucd+0x1bf80`
  - `qcaucd+0x3f7a8`
  - `qcaucd+0x3e094`
  - `qcaucd+0x3f644`
  - `qcaucd+0x32094`
  - `qcaucd+0x36f4c`
  - `qcaucd+0x1ca98`
  - `qcaucd+0x1de84`
  - `qcaucd+0x286d4`
  - `qcaucd+0x2c4e0`
  - `qcaucd+0x2573c`
  - `qcaucd+0x268e8`
  - `qcaucd+0x50064`
  - `qcaucd+0x4d8d0`
  - `qcaucd+0x4f218`
  - `qcaudminiport+0x6d9b0`
  - `qcaudminiport+0x6ce30`
  - `qcaudminiport+0x7a7f4`
  - `qcaudminiport+0x75e18`
  - `qcaudminiport+0x921fc`

KD log:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\dp14-11e64-caller-20260820_1c18_2026-08-20_09-01-01-298.log`

Fresh qcaucd decompiles:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\qcaucd-dp14-runtime-stack-decompile-20260820.txt`
- `C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\qcaucd-slot14-skip-vs-shadow-20260820.txt`

## Static reconciliation of the previous slot-14 conclusion

The earlier statement that software slot 14 is not a physical master port remains correct for the **normal master-port programming block**, but was incomplete.

`FUN_14003bf40` iterates software slots 1..14. When a slot is pending, the normal master-register block is explicitly skipped for `uVar10 == 0x0e`, while the right WSA8845 slave DP6 is still programmed and the slot is marked for a deferred/pending phase.

`FUN_14003df18` runs afterward and iterates pending slots 1..14. For every pending slot it calls `FUN_14003f6f8()` on the target bank's `DPn PORT_CTRL` shadow using:

`(slot * 4 + bank) * 0x40 + 0x1024`

For slot 14 and bank 1 this is exactly `0x1e64`. The template assembles:

- `si low = 0x1f`
- `offset1 = 0x19`
- `offset2 = 0xff`

therefore `PORT_CTRL = 0x00ff191f`.

No slot-14 channel-enable operation was observed or introduced. The physical CPS stream remains master port 13 shared by both WSA8845 speakers. DP14 is only a shadow-bank transport-state write.

## Linux disposable candidate

Candidate directory:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/v31-wsa-dp14-shadow-20260820`

The patch is Denali + WSA + active master port 13 + CPS timing (`pcfg->si == 0x031f`) only. Inside `qcom_swrm_transport_params()` it writes the same target bank's DP14 PORT_CTRL shadow to `0x00ff191f` and does nothing else.

It does **not**:

- create master port 14 as a stream;
- create a new DAI;
- enable any DP14 channels;
- alter CPS master port 13 geometry;
- alter WSA slave DP6 programming.

Candidate signed module:

- SHA256 `c2db886c3a8f55aad0544a06774773830d29fc1db9e7ebd115ef69afbcfc0b1c`
- srcversion `A1AD340206B206114780A1E`
- vermagic `7.1.5-sp11-render-parity-v4+`
- signed by the existing build-time kernel key.

The first one-shot boot proved the candidate loaded and executed:

- `/proc/cmdline` contained `sp11_dp14_shadow_diag=1`
- `/sys/module/soundwire_qcom/srcversion` = `A1AD340206B206114780A1E`
- repeated successful kernel markers included:
  - `bank=1 reg=0x1e64 value=0xff191f ret=0`
  - `bank=0 reg=0x1e24 value=0xff191f ret=0`
- persistent GRUB remained `saved_entry=sp11-audio-golden-v31`, `next_entry=` after the one-shot was consumed.

## Initrd topology correction before acceptance

The first DP14 candidate initrd had been cloned from the older `v31-cps-pcm-port-ctrl-105c` diagnostic tree. That tree still contained an old experimental generic SP11 topology (`ac82587d...`) even though the root filesystem had already been restored to canonical Golden.

This does not prove that the wrong topology was actually selected at runtime, because previous forced-logger boots demonstrate the Render-Parity filename is the relevant topology path, but it creates unnecessary ambiguity.

Therefore the acceptance initrds were rebuilt with:

- generic `X1E80100-Microsoft-Surface-Pro-11-tplg.bin` = canonical Golden SHA `1b0c7217...`
- CPS Headroom topology = canonical Golden SHA `1b0c7217...`
- only `X1E80100-Microsoft-Surface-Pro-11-Render-Parity-tplg.bin` replaced by the known forced logger topology.

### Tap3-forced acceptance initrd

- Render-Parity topology SHA `e81a6deb919240d20c0479f64bbd0e8e1204673c3a899e0d9ba2464d158eb42a`
- initrd SHA `701254901059c79d6f7e587f76915f54f00b96fb8e1419e0ba70f10e030ffb68`
- embedded DP14 module byte-exact SHA `c2db886...`

### Tap2-forced acceptance initrd

- Render-Parity topology SHA `b5e4331b79957837d3625867e0bfa81709f4f1e8c3eab3336613888ff905d624`
- initrd SHA `d29204cbc64f2f0121b47f69d9fa5fa6d39b517d21904a1afa7652de3a8f6a1f`
- embedded DP14 module byte-exact SHA `c2db886...`

The reusable DP14 initrd tree was restored afterward so both generic and Render-Parity topology files are canonical Golden `1b0c7217...`.

## Acceptance gate

Use the same Linux DIAG router collector and direct ALSA oracle as the previous v31 discriminators:

- `/home/geoca/Tools/diag-router-sp11.tmp/diag-router`
- `/home/geoca/Tools/diag-router-sp11.tmp/capture_log_1586.py`
- `/home/geoca/Tools/diag-router-sp11.tmp/tap3-997hz-20s.wav`
- `aplay -D hw:0,0 ...`

Run tap3-forced and tap2-forced in separate disposable one-shot boots. When practical, capture the external SP7 microphone concurrently to prove physical 997 Hz output.

Promotion requires real nonzero 24 kHz CPS tap3 and real nonzero 8 kHz VI tap2. If the forced streams remain zero while physical render is proven, reject DP14 shadow as a sufficient fix and keep the Windows parity finding only.

Golden must remain the persistent fallback throughout.
