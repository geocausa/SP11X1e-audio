# SP11 protection-diagnostic candidate — 2026-08-02

> **Erratum — rejected for promotion on 2026-08-02:** Windows does not filter
> the 48-byte `PARAM_ID_SPR_SESSION_TIME` frame. The hash-matched Windows GSL
> implementation sends the full selected aggregate and treats status `3` from
> this calibration boundary as a warning. The later Clean2 promotion of this
> filtered topology reproduced a physical right-only failure. Retain this
> document as diagnostic history; use
> `docs/findings/2026-08-02-windows-graph-calibration-warning-policy.md` for the
> corrected decision.

## Status and scope

`7.1.5-sp11-audio-protectdiag+` is an isolated diagnostic candidate built from
the accepted `7.1.5-sp11-audio-clean+` baseline. Its corrected boot and
controlled playback are complete. It validated the filtered graph-calibration
aggregate and rejected both active readback probes non-fatally. It remains a
diagnostic entry and does not replace the known-good saved default.

This candidate changes only the protection evidence path:

- remove the single GET-only SPR session-time record from the SET_CFG graph
  calibration aggregate;
- retain all 106 settable graph-calibration records in their captured order;
- query SP `0x4027:0x080011f2` one second after graph start and log the returned
  68-byte TMax/XMax telemetry;
- subscribe non-fatally to the Windows-proven normal-playback diagnostics pair,
  SP_VI `0x4024` event `0x0800138c`;
- validate, preserve and decode any returned per-speaker diagnostic event.

The Dolby boundary remains inserted in bypass mode. This candidate does not
introduce Dolby processing, change the accepted PA gain, or revive the removed
18 dB PA_AUX experiment.

## Boot isolation and rollback

- Kernel release: `7.1.5-sp11-audio-protectdiag+`
- GRUB id: `sp11-audio-protectdiag`
- GRUB title: `Ubuntu SP11 7.1.5 AUDIO PROTECTION DIAGNOSTIC (filtered calibration + telemetry)`
- Boot directory: `/boot/sp11-7.1.5-audio-protectdiag`
- Module directory: `/lib/modules/7.1.5-sp11-audio-protectdiag+`
- Preserved saved default: `sp11-7.1.5-clean`

The diagnostic entry was selected with `grub-reboot`, not `grub-set-default`.
The saved default remains the accepted clean entry.

The normal topology filename remains the clean baseline, SHA-256
`ac82587d145743537f1aa50bc764bd4aebc47ca6c03f344f8e65e95fa5078d8d`.
The diagnostic DTB changes only the sound-card model to
`X1E80100-Microsoft-Surface-Pro-PD`, making the driver request the isolated
`X1E80100-Microsoft-Surface-Pro-PD-tplg.bin`. That uniquely named file is
installed on the root filesystem and embedded in the diagnostic initramfs.
The clean entry continues to request its original filename.

## Reproducible identity

- Kernel source commit: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- Source state: that commit plus patch 0026 below
- Patch 0026 SHA-256:
  `aae6b7906ac60342f53b9e06a7909ca83ee8353acbb7ba3ded8500950e2ded04`
- Full build log SHA-256:
  `4f039eb259d6b7fee81177ba5fb87b2e78db9ee869f204b1ad6ad35128d82e62`
- Module-install log SHA-256:
  `caa6eacb4849f60984fdb962bf7a6d6a14fb462dbddbdbdff2fa8a75f4b53bed`
- Installed module count: 7,886

### Boot assets

| Asset | SHA-256 |
|---|---|
| kernel Image | `b710d0d6560964e9dfb7741c868883ea8fd19efb6b9009c673030a24e7c707da` |
| initramfs | `d3c4eadc125d8797e4e2b9dda191d9270bf8c2dbec89184794a7cc4cbff07db4` |
| Phase91 DTB with isolated topology model | `4d207c6593c30a81f7984ccb7d0ecc45a47a6bca577073d43b680b6345157fdd` |
| System.map | `d787af9d217523f7b5541378fbaa22f15d6aaba6f803df7c751dc974a284722e` |
| kernel config | `0d47e23b030cacfff0aecfb24bf72948d077865f62e7c1a040c856f2f625263e` |

### Topology and calibration

| Asset | Size | SHA-256 |
|---|---:|---|
| filtered graph-calibration aggregate | 10,416 bytes / 106 frames | `6b111c9c26fe190a94e1709f650666f25a3afb5c54e7ae1cad6662af5dcf9971` |
| candidate topology | compiled ALSA topology | `8ce471ec9776432aeed18b60a3b29e0760c4dd2ec9a32ead642e6b70695a81bd` |

The removed input was exactly frame 63, IID `0x412b`, parameter
`PARAM_ID_SPR_SESSION_TIME` (`0x0800113d`), an aligned 48-byte GET-only record.
The original aggregate was 10,464 bytes / 107 frames, SHA-256
`2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`.

The candidate topology passed binary decoding, an `alsatplg` parse, and the
project topology linter: 25 module definitions and no duplicate instance IDs.

### Critical installed modules

| Module | SHA-256 |
|---|---|
| `snd-q6apm.ko.zst` | `09a10d0d80d1aa28a06c95850a16e3453c3eebc81f4f33550858df3149ea4d46` |
| `snd-soc-x1e80100.ko.zst` | `fbafaa4e49f355df1e35f3061da84ee4b14ab4d56a403a7c00437d2b8683645f` |
| `snd-soc-wsa884x.ko.zst` | `ee0c25ad67d701c0d9ed174a663af81b1b3e482dcd7cdf68d21e16052bc5628a` |
| Phase91 `gpi.ko.zst` | `b58972012d2c24a8638b350567b217b0d3f7796ec1eaf4426e2b21ac75bd4f6a` |
| Phase91 `spi-geni-qcom.ko.zst` | `f029d9fe65cb78f7a1616c7f5ee6c26775ba0b9134958081e129ad9ebb625136` |
| Phase91 `mshw0485_touch.ko.zst` | `d24009e1976a9d28f365130b741c6f61aaf1715bb412944960881a570b67331b` |

All six report vermagic `7.1.5-sp11-audio-protectdiag+`. The three Phase91
overrides are signed with the build-time kernel key and were explicitly
embedded in the initramfs alongside the candidate topology.

## First boot: isolation defect found before playback

The first diagnostic boot reached the exact kernel and command-line marker;
Wi-Fi, touch, the audio card and every intended module were present. No audio
was played. PipeWire's capability probe failed with `EINVAL`, and the kernel
reported `No graph found with id 0`, leaving the HiFi profile unavailable.

This did not indicate a malformed filtered topology. Binary decompilation of
the clean and candidate files showed exactly the intended change: the outer
private-data size changed from 10,464 to 10,416 bytes and the single aligned
48-byte SPR GET-only frame disappeared. All other topology text was identical.

The cause was deployment timing. The card probes about ten seconds after boot,
after the real root filesystem has replaced the initramfs. It therefore loaded
the clean 10,464-byte topology from the normal filename; the diagnostic driver
correctly requires 10,416 bytes and declined to construct that mismatched
graph. Merely embedding the candidate under the normal filename in initramfs
was insufficient.

The correction uses the sound-card model as the driver's existing topology
selector. The Phase91 DTB was edited in place with `fdtput`; a semantic DTS
comparison proves its only change is:

```text
model = "X1E80100-Microsoft-Surface-Pro-11";
model = "X1E80100-Microsoft-Surface-Pro-PD";
```

The uniquely named root and embedded topology copies both match SHA-256
`8ce471ec9776432aeed18b60a3b29e0760c4dd2ec9a32ead642e6b70695a81bd`.
The three extracted initramfs overrides also match their installed hashes
byte-for-byte.

## Pre-boot verification

- The full project regression suite passes: 76 tests.
- Patch 0026 passes `git apply --reverse --check` against the built source.
- The four modified kernel files pass `git diff --check`.
- GRUB syntax passes `grub-script-check`.
- The diagnostic menu entry and exact release marker are present in generated
  `grub.cfg`.
- The running system remains on `7.1.5-sp11-audio-clean+` during deployment.
- Root has approximately 52 GB free after cleaning obsolete build outputs.

## Corrected boot and playback result

The corrected boot passed the platform and topology-isolation gates:

1. `uname -r` was `7.1.5-sp11-audio-protectdiag+`, with the exact command-line
   marker.
2. The diagnostic sound-card model selected the unique candidate topology from
   the real root filesystem; the HiFi speaker profile and Dolby-bypass sink
   were available.
3. The intended modules loaded from this release; Wi-Fi and touch remained
   functional.

Automatic graph probes and one controlled 48 kHz, 16-bit stereo playback gave
the following deterministic result:

1. The 10,416-byte/106-frame aggregate was accepted. The former aggregate
   `AR_EUNSUPPORTED` warning disappeared.
2. Every remaining graph, protection, endpoint, gain, filter, mute and channel
   mixer stage was accepted, followed by successful `GRAPH_START`.
3. SP `0x4027:0x080011e8` returned a valid 92-byte response and SP_VI
   `0x4024:0x080011f6` returned a valid 68-byte response. Both still decode as
   two-speaker protection with 48 kHz render and 8 kHz VI feedback.
4. The exact diagnostics subscription `0x4024:0x0800138c` was rejected with
   APM status `3` (`AR_EUNSUPPORTED`, Linux `-EOPNOTSUPP`). No diagnostic event
   was emitted.
5. The post-start GET `0x4027:0x080011f2` returned a correctly framed 92-byte
   reply, but both command status and module error were `1`; its 68-byte body
   was zero-filled. Linux reported `-EINVAL`. A rejected body must not be
   interpreted as valid zero-channel telemetry.
6. The stereo stream stayed alive. There was no speaker dropout, PA or
   SoundWire fault, XRUN, protected-graph command timeout, or
   protection-triggered shutdown. One earlier boot-enumeration `qcom-apm`
   timeout for opcode `0x01001021` occurred before the audio DAI registered;
   it was not part of the protected playback transaction.

The full journal and strict decoder output are preserved under
`artifacts/diagnostic-boots/20260802-protectdiag-cc441a2243644f1f94639d0841ad55c4/`.
The reviewed machine-readable result is
`artifacts/reviewed/linux-protectdiag-20260802-result.json`.

## Historical candidate disposition (superseded)

> The text below records the decision made during the diagnostic run. It was
> reversed later on 2026-08-02 after the Windows GSL warning policy was proven
> and the Clean2 filtered topology reproduced a physical right-only failure.
> The current decision is to preserve the full Windows aggregate; see the
> erratum at the top of this document and
> `docs/findings/2026-08-02-windows-graph-calibration-warning-policy.md`.

The filtered aggregate is promoted: it removes a public-API-proven GET-only
record from a SET transaction and is accepted at runtime without altering any
other topology frame. The two active probes are not promoted:

- the diagnostics registration is a conditional Windows code path not observed
  in surviving live Windows traces and is rejected by this DSP graph;
- the TMax/XMax request is live-proven as a Windows outbound GET, but its
  Windows response was never captured and the Linux response reports failure.

The next clean kernel therefore retains the 10,416-byte filtered calibration,
the passive bounded event logger, and all accepted protection setup. It removes
the default diagnostics subscription and delayed telemetry work. Neither
rejected probe is required to construct or start the protected playback graph.
