# Live YouTube DEFAULT vs Alerts NOTIFICATION Dolby comparison

Date: 2026-08-11 (Europe/London)

## Result

A fresh real Microsoft Edge / YouTube speaker stream was compared live against an isolated WinRT `AudioCategory=Alerts` stream using the existing SP7 -> SP11 KDNET setup.

The comparison closes two previously open questions:

1. **Real browser/YouTube media selects the Qualcomm DEFAULT render mode on this SP11.** A fresh Edge/YouTube stream created after closing the test browsers and allowing the endpoint to idle re-entered the hash-bound `qcaudminiport8380.sys` mode translator with `w1 = 0x01`, which maps exactly to DEFAULT -> QCADCM enum 2 -> GKV 2.
2. **Both the current YouTube DEFAULT stream and an isolated explicit Alerts/NOTIFICATION stream execute the same persistent Dolby VR and VLLDP150 outer callbacks in a repeating `VR -> VLLDP` order on this boot.** The two callbacks return to the same equal-rate direct-call site inside `DolbyDax3Apo.dll`.

This means the earlier project observation of a repeating `VLLDP -> VR` callback invocation order is **not universal**. It remains valid as a historical observation from that earlier session, but it must not be treated as the canonical current callback order or as a fixed architectural invariant. The later full-memory proof that the actual PCM sample dependency is `VR -> VLLDP` remains the stronger sample-flow result, and the new current-boot callback ordering agrees with it.

This finding does **not** prove that all applications or all Dolby configurations must always use `VR -> VLLDP` callback invocation order. It proves the exact current behavior for the two independently classified streams below.

## Hash / binary gate

The current SP11 still uses the reviewed binaries:

- `qcaudminiport8380.sys` SHA-256 `79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531`;
- `DolbyDax3Apo.dll` SHA-256 `6ea1702c0f86766e45c2e248e169022e3d71eaa3c655b3fca159b4dd59f18d87`;
- `DolbyApoVr.dll` SHA-256 `1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc`;
- `DolbyAPOvlldp150.dll` SHA-256 `a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1`.

No evidence from a different driver/APO build is mixed into the comparison.

## YouTube stimulus and fresh mode selection

The first browser stimulus was created interactively in Microsoft Edge from a YouTube search for a stereo left/right test. The initial result was video `6TWJaFD6R2s`; YouTube autoplay later advanced through other stereo-test videos.

For the mode-selection witness, all test browser windows were closed, the endpoint was allowed to idle for 75 seconds, and Edge was launched fresh at:

```text
https://www.youtube.com/watch?v=R_5xC-5BK88
```

The already-reviewed compact processing-mode translator in `qcaudminiport8380.sys` was validated again by normal kernel virtual disassembly at live address:

```text
fffff800`3e714080
```

The first instructions still matched the hash-bound translator:

```text
mov w0,#0
cmp w1,#1
beq ...
cmp w1,#2
...
```

A single read-only hardware execution breakpoint logged:

```text
[YT_MODE] flag=1 lr=fffff8003e6e14a4
```

Therefore the fresh real YouTube stream selected:

```text
flag 0x01 -> DEFAULT -> QCADCM enum 2 -> GKV 2
```

This is stronger than the earlier controlled WinRT `AudioCategory=Media` test because it proves the classification of an actual Edge/YouTube media client.

Page reload and a second Firefox client did **not** re-enter the translator while the existing endpoint graph remained alive. The translator therefore behaved as an endpoint/pin-creation boundary in this test, not as a per-page playback callback. The fresh idle -> new Edge launch was required to obtain the new mode witness.

## Current audiodg / module layout for the Dolby pass

The live audio-engine process was:

```text
audiodg.exe PID 3720 = 0x0e88
```

Relevant module bases recovered read-only from SP11 before KD attached:

```text
DolbyDax3Apo.dll       0x00007ff9b0ac0000
DolbyApoVr.dll         0x00007ff98f500000
DolbyAPOvlldp150.dll   0x00007ff997eb0000
DolbyAudioProcessing.dll 0x00007ff989280000
DolbyHrtfEnc.dll       0x00007ff9d98a0000
SurfaceAPO.dll         0x00007ff98d260000
```

The exact reviewed callback RVAs therefore resolved to:

```text
DAX3 CDolbyAPOWrapper::APOProcess
  RVA 0x000cd000 -> 0x00007ff9b0b8d000

DolbyApoVr live outer process callback
  RVA 0x001d10c8 -> 0x00007ff98f6d10c8

DolbyAPOvlldp150 live outer process callback
  RVA 0x00105050 -> 0x00007ff997fb5050

DAX3 equal-rate direct-call return site
  RVA 0x000cd664 -> 0x00007ff9b0b8d664
```

## YouTube Dolby execution

By the Dolby capture, YouTube autoplay had advanced to another stereo-test video (`W5TqjWz09IE`). The important property for this pass is that an ordinary real YouTube speaker stream was active in Edge; the exact test-video content is not used as an architectural input.

A one-shot hardware execution trap on the exact DAX3 wrapper logged:

```text
[YT_DAX] pid=e88 x0=000001c61a9f0a60
```

Thus the YouTube stream executes the same persistent DAX3 wrapper in the live `audiodg.exe` process.

Two simultaneous hardware execution traps were then placed on the exact VR and VLLDP150 outer callbacks. This is within the target's previously established safe two-breakpoint limit.

The bounded capture contains 32 actual callback events: 16 VR and 16 VLLDP. Every event is in PID `0xe88`, and every callback returns to DAX3 `+0xcd664`:

```text
[YT_VR]  n=1  pid=e88 lr=00007ff9b0b8d664
[YT_VLL] n=2  pid=e88 lr=00007ff9b0b8d664
[YT_VR]  n=3  pid=e88 lr=00007ff9b0b8d664
[YT_VLL] n=4  pid=e88 lr=00007ff9b0b8d664
...
[YT_VR]  n=31 pid=e88 lr=00007ff9b0b8d664
[YT_VLL] n=32 pid=e88 lr=00007ff9b0b8d664
```

A strict sequence check over all 32 events found no reversal:

```text
VR -> VLLDP -> VR -> VLLDP -> ...
```

## Isolated Alerts / NOTIFICATION Dolby execution

The Edge YouTube test window was closed before the Alerts comparison. The same `audiodg.exe` PID 3720 remained alive and the same reviewed Dolby module bases remained loaded.

The isolated stimulus was the already-used WinRT media player path:

```text
AudioCategory = Alerts
source        = C:\Windows\Media\Alarm01.wav
```

The separate mode-selection finding already proves that this exact client category selects:

```text
flag 0x0a -> NOTIFICATION -> QCADCM enum 7 -> GKV 7
```

With only the exact VR and VLLDP150 outer callback hardware traps armed, the isolated Alerts playback produced:

```text
946 VR callback hits
946 VLLDP callback hits
1892 total callback events
```

A strict sequence check over all 1,892 events found no reversal. The entire sequence is:

```text
VR -> VLLDP -> VR -> VLLDP -> ...
```

Every event is again in PID `0xe88`, and both callback types return to:

```text
0x00007ff9b0b8d664 = DolbyDax3Apo + 0xcd664
```

This closes the previously explicit gap: the current Alerts/NOTIFICATION path does not merely hit the DAX3 wrapper; it enters both persistent inner processors and, on this boot, uses the same observed VR -> VLLDP callback ordering as the real YouTube DEFAULT stream.

## Interpretation and correction of older callback-order evidence

The project has two different concepts that must not be conflated:

1. **PCM sample dependency / buffer provenance.** Full-memory Aug-8 evidence already proved:

   ```text
   source PCM -> DolbyApoVr -> DolbyAPOvlldp150
   ```

2. **Observed callback invocation order in a particular DAX wrapper-chain lifetime.** An older Aug-4 session recorded `VLLDP -> VR` callback markers. The current Aug-11 YouTube and Alerts captures both record `VR -> VLLDP` without reversal.

Therefore the old callback-order result is reclassified as session/configuration-specific rather than architectural. It must not override the byte-proven sample dependency, and it must not be used as a fixed Linux parity requirement.

The current boot's callback order agrees with the established sample dependency, but this finding deliberately does not claim that wrapper order can never change again. If a future stream produces a different order, the correct model is dynamic/session-dependent wrapper-chain ordering around a sample dependency that must be proven from buffer ownership.

## Consequences for the stream-model question

The user's original suspicion was that basic notifications/system sounds might be rendered through a different pipeline than music/movie media.

The evidence now gives a more precise answer:

- the **AudioReach DSP render family can differ** by Windows processing mode: DEFAULT and NOTIFICATION are separate recovered graph families;
- a real Edge/YouTube media stream is now directly proven DEFAULT;
- explicit Alerts is directly proven NOTIFICATION;
- despite the different AudioReach family selection, both current streams enter the **same persistent user-mode Dolby DAX3 -> VR/VLLDP processing stack** in the same `audiodg.exe` process;
- therefore "different processing mode / DSP graph family" is real, but "completely separate Windows audio-engine/Dolby pipeline" is not supported by the current evidence.

The basic Win32 `SND_SYSTEM` system-sound path was already proven to select DEFAULT. Its exact current inner VR/VLLDP callback sequence has not yet been independently re-captured in this new comparison because SP11's PiSlave reverse tunnel remained offline after the final KD detach. That small witness can be added later without reopening any closed DSP or SoundWire experiment.

## Raw evidence outside Git

### YouTube mode

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_YOUTUBE_MODE_20260811_0915BST.log`

- size: 1,726 bytes
- SHA-256: `e36a0e35644a410b62b04e48365f5e335a55affd4af1027cff9c74034475fc1c`
- actual mode hits: 1

### YouTube Dolby

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_YOUTUBE_DOLBY_20260811_0929BST.log`

- size: 4,683 bytes
- SHA-256: `0008db70b5ec982671be2b93cda3a602adc2aaf5b36bd629602eca63d4aa45bb`
- actual DAX wrapper hits: 1
- actual VR hits: 16
- actual VLLDP hits: 16
- callback sequence check: 32/32 alternating `VR -> VLLDP`

### Isolated Alerts Dolby

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_ALERTS_VR_VLL_20260811_0936BST.log`

- size: 79,064 bytes
- SHA-256: `bc257e6106ead25740ef1c75fa727a80b4b4117c6c910d7bc261293d98458f49`
- actual VR hits: 946
- actual VLLDP hits: 946
- callback sequence check: 1,892/1,892 alternating `VR -> VLLDP`

Machine-readable summary:

`artifacts/reviewed/2026-08-11-youtube-vs-alerts-dolby-kdnet.json`

## Debugger safety / closeout

No direct physical MMIO read was performed. No debugger MMIO write, DSP write, SoundWire register write, or driver-state write was performed.

The mode pass used one read-only hardware execution breakpoint. The Dolby passes used at most two simultaneous hardware execution breakpoints, respecting the proven ARM target limit. One attempted automatic event-count self-clear in the YouTube pass did not stop at the intended count, so the capture was manually interrupted at 32 callback events; this caused no target fault and all breakpoints were then cleared normally.

Every completed KD session used the required closeout sequence:

```text
bc *
.logclose
qd
```

Post-detach debugger ownership should continue to be checked before the next attach. The SP11 PiSlave reverse tunnel was still offline at the end of this finding; no forced target restart or connector restart was attempted.
