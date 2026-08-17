# v28: DP2 OffsetCtrl2 closes broadband static on the full Windows WSA lifecycle

Date: 2026-08-17  
Status: **GREEN — W03 broadband-static root cause closed on intended render-parity stack**

## Why v28 exists

The CPS-v3 B1 matrix established a causal coupled prerequisite:

- CSR fallback ON + Linux DP2 OffsetCtrl2 `0x00` -> quiet;
- CSR fallback OFF + Linux DP2 OffsetCtrl2 `0x00` -> broadband-noisy (`2.776e-3` median steady diff-RMS);
- CSR fallback OFF + Windows DP2 OffsetCtrl2 `0x07` -> room-floor quiet (`1.923e-5`, repeat `2.581e-5`).

That proved WSA8845 DP2/COMP `OffsetCtrl2=0x07` is the Windows prerequisite that makes the CSR-off consumer state safe. v28 asks the production question: does carrying only that prerequisite onto v27 remove the static while preserving the exact Windows WSA8845 lifecycle and clock-stop retention?

## Exact source delta

v28 is a strict v27 descendant. Its production code changes only three points:

1. `include/linux/soundwire/sdw.h`: add `SDW_DPN_SIMPLE_TRANSPORT_OFFSETCTRL2`;
2. `drivers/soundwire/stream.c`: when that capability is set, write the banked SIMPLE `OffsetCtrl2` register from the already-populated `t_params->offset2`;
3. `sound/soc/codecs/wsa884x.c`: advertise that capability **only for DP2/COMP**.

DP1 BlockCtrl3 and DP3 OffsetCtrl2 remain untouched so the static closure does not silently bundle other structural parity work.

Canonical patch:

`patches/0065-soundwire-wsa884x-add-SIMPLE-OffsetCtrl2-for-SP11-DP2.patch`

Source SHA-256 values:

- v27 `sdw.h`: `62ee2dcfaafc271381257609b51a7e55e7f9b5c8f50aed2b7fda72d08563ac9c`
- v28 `sdw.h`: `59fc44f9fdef5a35f6b3b5e6b4dd9056ce94bbf88b9084efceaee94e1b7b2203`
- v27 `stream.c`: `699bdbb999d7a381882a7f79caa74cd264ab1aca0466cf280db4e6ea108b12e9`
- v28 `stream.c`: `c7997fabec433d2b2b473922867b0df7aee514a87a166677297ddde7a9c7c5da`
- v27 `wsa884x.c`: `350d8a69d4467259583ff23c4b44baf11399af82ba73a994693c9428e2f04a1e`
- v28 `wsa884x.c`: `e26775843baf3315101f24dbfa1a44f220c35ed6d2d46f66db835e32f1399551`

## Build provenance

### SoundWire

The exact render-parity-v4 SoundWire object set was preserved in `build-softpause-full-20260813`. The v27 packaged module and that build output share:

- srcversion `31EA655550AE70F3DF2951E`;
- v27 build-id `d59ebe0e12aa5e9ba7a5e1148c9215528f53b6bc`.

Only `stream.o` was rebuilt for v28, then the untouched v27 object set and module metadata were relinked and signed with the render-v4 build key using SHA-512.

v28 SoundWire:

- `.ko` SHA-256 `9ad4fb208df302ab2737cefdf6790554265373126cce4a95e0fa377db404d753`
- `.ko.zst` SHA-256 `7ba04a6bb25c340a67c2bc5780eca3f33e4692063f7cdb087949a4b51bc82d93`
- srcversion remains `31EA655550AE70F3DF2951E`
- build-id `481d2bae6f96852d33f03d3dbfd6c81caebd5fc6`
- live sysfs build-id was verified byte-for-byte on the v28 boot.

### WSA8845

Before building v28, the exact v27 WSA source was rebuilt against the render-v4 output directory and reproduced the retained v27 unsigned module **byte-for-byte**:

`b909a32be257cceb4214a6298cb29b3fdec003eb49eb696770e68ab2a35b8c2e`

This establishes the WSA build context independently of directory naming.

v28 WSA8845:

- srcversion `EB74C0F5E4405EEE429136C`
- unsigned SHA-256 `2d2b144e489b7bb742d8f9febf4b4e58c54096d7ce2d92125b9d1c2e4a11a57b`
- signed SHA-256 `963e0353b06afdc48d8729639a99e9c2be1b253256ab84f6025e4094c855287c`
- `.ko.zst` SHA-256 `003427fc7388a5674f1c8c0ec184339d5ff57192f83868d96567f7217bba84a2`
- build-id `e4023c989b2aaf12ead3bc3f37c359c9d1e5cffa`
- exact render-parity-v4 vermagic and signing key.

All source/build artifacts were restored after candidate copies were emitted.

## Packaging isolation

v28 was produced from the **deployed v27 initrd**, SHA-256:

`afc918eaaa56b1530bf49cc222a9cfe75ccd68cc697132b2024660e7da5527f2`

The v28 initrd is:

`94f6716ea210c0b3d82eb3403f08102de90be13ea526c1fb4f273324db9f754d`

Extraction/repack roundtrip contained 4394/4394 entries with zero semantic differences. Comparing v27 to v28 yielded exactly two changed files, both with unchanged `0644` mode:

- `soundwire-bus.ko.zst`
- `snd-soc-wsa884x.ko.zst`

The v28 kernel and DTB are byte-identical to v27:

- kernel SHA-256 `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- DTB SHA-256 `3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca`

Persistent GRUB saved entry remained `sp11-audio-cps-v3`; v28 was one-shot only.

## Live identity

The v28 boot reported:

- kernel `7.1.5-sp11-render-parity-v4+`;
- cmdline marker `sp11_wsa_dp2_offsetctrl2_v28=1`;
- WSA srcversion `EB74C0F5E4405EEE429136C`;
- SoundWire srcversion `31EA655550AE70F3DF2951E`;
- live SoundWire build-id `481d2bae6f96852d33f03d3dbfd6c81caebd5fc6`;
- unchanged WSA-macro srcversion `4AF6F542C17BA6DD46586DA`;
- ALSA PCM closed at idle;
- no early WSA/SoundWire/XRUN fault evidence.

Thus the measured system is the intended v27 stack plus only the proven DP2 prerequisite.

## SP7 external-mic zero-noise gate

All acoustic measurements use the **SP7 microphone externally recording the SP11 speakers**. The SP11 microphone/capture path is not used.

Stimulus:

- visible Windows-Dolby endpoint 1%;
- endpoint muted;
- stereo S16_LE 48 kHz digital zero;
- physical ALSA PCM independently observed RUNNING;
- post-playback PCM returned closed.

### First v28 cycle

WAV SHA-256:

`975DC05819D7DEEA3B59295EEE7B64C3C289C424E8A1688068D2C47211DDF9C4`

Median steady diff-RMS:

`2.4665239517531827e-05`

That is ~86x quieter than v27 cleanA and ~95x quieter than v27 cleanB. Stable active bins sit at the room-floor scale rather than the v27 `~2e-3` broadband plateau.

### Idle / clock-stop gate

After the first cycle:

- PCM returned `closed`;
- WSA macro, VA macro and SoundWire runtime-suspended;
- no WSA/SoundWire/XRUN fault evidence;
- after 20 s idle, the `detected VPHX supply configuration: 2S` count remained exactly **2 total** (one per physical amp), proving no cold-init replay across the ordinary clock-stop cycle.

### Second v28 wake

WAV SHA-256:

`78FC305042BA34E7E136F778FB7C6A7512660365C88A3F113094692F668239F5`

Median steady diff-RMS:

`1.8179778081257844e-05`

The retained Windows active reference is `1.8253227918889202e-05`, so this repeat is **0.996x Windows** on the same diff-RMS metric. It is ~117x quieter than v27 cleanA and ~129x quieter than v27 cleanB.

Every steady 0.5 s bin in the repeat remains roughly `1.74e-5 .. 2.04e-5`; there is no sustained broadband PA floor.

## Conclusion

W03's confirmed downstream physical static is closed on the intended full render-parity stack.

The root cause was not a missing upstream Dolby/q6apm process, microphone spill, generic WSA analog value, or SoundWire clock-stop timing. The decisive coupling was:

**Windows consumer policy disables WSA8845 CSR fallback, which requires the COMP dataport's Windows SoundWire DP2 OffsetCtrl2=`0x07` geometry. Linux left that SIMPLE-port register at zero. CSR fallback in quiet CPS-v3 masked the malformed COMP transport; v27 exposed it by correctly adopting the Windows CSR-off consumer state.**

Adding only the missing DP2 transport field restores room-floor behavior while retaining v27's exact 63/10/6 WSA lifecycle and resident clock-stop retention.

Remaining DP1 BlockCtrl3 and DP3 OffsetCtrl2 differences are now structural-fidelity questions, not the explanation for the closed broadband-static defect. Do not bundle them into this causal fix.
