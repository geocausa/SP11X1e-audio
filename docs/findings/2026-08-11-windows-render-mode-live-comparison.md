# Windows render-mode live comparison: DEFAULT vs NOTIFICATION

Date: 2026-08-11 (Europe/London)

## Result

A controlled live comparison on the current Surface Pro 11 corrects an important ambiguity in the earlier speaker-path diagram.

The duplicated Windows AudioReach render chains are **processing-mode alternatives**, not left/right speaker paths and not evidence for two `audiodg.exe` processes.

In the controlled tests below, every tested render was hosted by the same `audiodg.exe` process, PID 5312 (`0x14c0`). The Qualcomm miniport nevertheless selected different processing-mode keys depending on the client/API behavior:

| Stimulus | Live qcaudminiport flag | Source-backed mode mapping | `audiodg.exe` |
|---|---:|---|---:|
| Win32 `PlaySound(..., SND_SYSTEM)` using `Windows Notify System Generic.wav` | `0x01` | DEFAULT -> QCADCM enum 2 -> GKV 2 | 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Alerts` | `0x0a` | NOTIFICATION -> QCADCM enum 7 -> GKV 7 | 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Media` | `0x01` | DEFAULT -> QCADCM enum 2 -> GKV 2 | 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Movie` | `0x01` | DEFAULT -> QCADCM enum 2 -> GKV 2 | 5312 |

Therefore:

1. **"system sound" is not synonymous with the Qualcomm NOTIFICATION processing mode.** A WAV played through the Win32 system-sound path selected DEFAULT in this test.
2. **An explicit Alerts-category stream can select the separate NOTIFICATION graph family.**
3. **The tested Media and Movie WinRT categories did not select the statically supported MEDIA/MOVIE Qualcomm modes on this endpoint; they selected DEFAULT.** This does not erase the static MEDIA/MOVIE mappings; it means those mappings must not be drawn as active graph bodies without a separate live trigger.
4. **There is no evidence here for two `audiodg.exe` render engines.** All controlled cases used the same PID 5312. The evidence supports multiple client/session processing modes hosted by one Windows audio-engine process.

A second live probe also proves that the explicit Alerts/NOTIFICATION path does **not** simply bypass the persistent Dolby speaker APO stack: the known-hot `DolbyDax3Apo.dll` `APOProcess` wrapper at RVA `0xcd000` executed during the delayed Alerts playback in PID `0x14c0` = 5312.

Follow-up KDNET work on the same boot has now closed the alert-specific inner sequence. With YouTube closed, an isolated explicit Alerts/NOTIFICATION playback produced 946 `DolbyApoVr` outer-callback hits and 946 `DolbyAPOvlldp150` outer-callback hits in strict `VR -> VLLDP` alternation, all returning to the DAX3 equal-rate direct-call site. A real Edge/YouTube DEFAULT stream independently produced the same current-boot `VR -> VLLDP` callback ordering. The older `VLLDP -> VR` callback-order observation is therefore not universal and must not be treated as a fixed architectural invariant; see `docs/findings/2026-08-11-youtube-vs-alerts-dolby-kdnet.md`.

## Hash gate

The current target binaries remain the same reviewed builds:

- `qcadcm8380.sys` SHA-256 `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`.
- `qcaudminiport8380.sys` SHA-256 `79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531`.
- `DolbyDax3Apo.dll` SHA-256 `6ea1702c0f86766e45c2e248e169022e3d71eaa3c655b3fca159b4dd59f18d87`.
- `DolbyAPOvlldp150.dll` SHA-256 `a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1`.
- `DolbyApoVr.dll` SHA-256 `1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc`.

No evidence from a different driver/APO build is mixed into this finding.

## Live miniport boundary

The previously reviewed processing-mode translator in `qcaudminiport8380.sys` is at RVA `0x94080`. On this boot the module base was recovered as `0xfffff8003e680000`, giving live address `0xfffff8003e714080`.

Normal kernel virtual disassembly at that live address reproduced the exact static mapping logic:

```text
flag 0x01 -> return 2  -> DEFAULT       -> GKV 2
flag 0x02 -> return 1  -> RAW           -> GKV 1
flag 0x04 -> return 4  -> COMMUNICATIONS-> GKV 6
flag 0x08 -> return 3  -> SPEECH        -> GKV 5
flag 0x0a -> return 7  -> NOTIFICATION  -> GKV 7
flag 0x14 -> return 6  -> MEDIA         -> GKV 4
flag 0x28 -> return 5  -> MOVIE         -> GKV 3
```

The breakpoint was read-only and logged only `w1` plus the return address before immediate `gc`.

The first persistent log recorded the sequence:

```text
[MODE] flag=1
[MODE] flag=1
[MODE] flag=1
[MODE] flag=a
[MODE] flag=1
[MODE] flag=1
[MODE] flag=1
```

The controlled stimulus ordering around the distinct hits establishes the important cases: explicit WinRT Alerts produced `0x0a`; WinRT Media and Movie produced `0x01`. The earlier `SND_SYSTEM` notification WAV also produced `0x01`.

A later delayed Alerts run was queued on SP11 before KD attached so target-side PiSlave flapping could not cancel the stimulus. The fresh KD attach landed at the same mode translator during that Alerts initialization; `w1` evaluated to decimal 10 (`0x0a`), independently reconfirming NOTIFICATION selection.

## `audiodg.exe` and Dolby boundary

The first controlled playback created one `audiodg.exe`, PID 5312, and the same process remained the host for all subsequent controlled stimuli.

Its relevant loaded modules included:

- `audioeng.dll`;
- `SurfaceAPO.dll`;
- `DolbyDax3Apo.dll`;
- `DolbyAPOvlldp150.dll`;
- `DolbyApoVr.dll`.

Current `DolbyDax3Apo.dll` base was `0x00007ff98d050000`. Rizin disassembly against the exact hash-bound file revalidated the known wrapper entry at image RVA `0xcd000`, so live address `0x00007ff98d11d000` was used for a one-shot execution breakpoint.

During the delayed explicit Alerts playback KD logged:

```text
[DAX_ALERT] hit pid=14c0
```

`0x14c0` is PID 5312. Thus the NOTIFICATION-mode test executed the same persistent DAX3 APO wrapper in the same audio-engine process.

`DAX3API.exe` remains a separate Dolby control/policy service/process and must not be drawn as a serial PCM-processing block inside `audiodg.exe`.

## Correct graph model

The graph ledger already proves two complete render families that share the same root/protection graph.

### DEFAULT family — GKV 2

```text
SH_MEM_PULL 4660
 -> LOGGER 465c
 -> PCM_CNV 465f
 -> VOL 4663
 -> SWR_SINK 4662
 -> POPLESS_EQ 4664
 -> VOL 4669
 -> MFC 466a
 -> SOFT_PAUSE 466b
 -> SPR 412b
 -> LOGGER 47e9
 -> VOL 4a63
 -> SWR_SINK 4675
 -> MSIIR 489e
 -> MSIIR 48a1
 -> LOGGER 467a
 -> root SAL 4001:12
```

Control: `POPLESS_EQ 4664 <-> VOL 4663`; SPR output 3 also feeds the speaker-loopback SAL path.

### NOTIFICATION family — GKV 7

```text
SH_MEM_PULL 469e
 -> LOGGER 469a
 -> PCM_CNV 469d
 -> VOL 46a1
 -> SWR_SINK 46a0
 -> POPLESS_EQ 46a2
 -> VOL 46a7
 -> MFC 46a8
 -> SOFT_PAUSE 46a9
 -> SPR 4137
 -> LOGGER 47ed
 -> VOL 4a5f
 -> SWR_SINK 46b3
 -> MSIIR 48a8
 -> MSIIR 48a9
 -> LOGGER 46b8
 -> root SAL 4001:18
```

Control: `POPLESS_EQ 46a2 <-> VOL 46a1`; SPR output 3 also feeds the speaker-loopback SAL path.

These are **mode alternatives**. They must not be relabeled as physical left/right speaker chains.

### Shared render root

Both families feed the shared root:

```text
SAL 4001
 -> CHMIXER 402c
 -> SPEAKER_PROTECTION 4027
 -> SPLITTER 4002
 -> LOGGER 4003
 -> CODEC_DMA_SINK 4157
```

The selected root hardware endpoint is established independently as WSA interface 1, 48 kHz, fixed-point PCM, 16-bit, two channels, active-channel mask `0x3`.

## Protection / feedback are side paths, not serial render stages

The old approximation also needs a structural correction here. VI and CPS are feedback/control side paths around speaker protection, not an inline `SP_VI -> protection -> CPS` audio chain.

VI data path:

```text
CODEC_DMA_SOURCE 4026
 -> LOGGER 4025
 -> SP_VI 4024
 -- INTENT_ID_SP control --> SPEAKER_PROTECTION 4027
```

Selected VI hardware endpoint: WSA interface 1, 8 kHz, 32-bit, two-channel, mask `0x3`.

CPS data path:

```text
CODEC_DMA_SOURCE 402b
 -> LOGGER 402a
 -> MUX_DEMUX 4029
 -> CPS_DATA_ROUTER_V5 4028
 -- INTENT_ID_CPS control --> SPEAKER_PROTECTION 4027
```

Selected CPS endpoint: WSA interface 3, 24 kHz, S32, two-channel, mask `0x3`.

The already-closed qcaucd/SoundWire evidence for CPS is one shared physical WSA master port 13 with two WSA8845 DP6 slaves: both masks `0x03`; left identity `0x0000000402170220` OffsetCtrl1 `0`; right identity `0x0000000402170221` OffsetCtrl1 `25`; interval 800 clocks (`SampleCtrl1/2 = 0x1f/0x03`), HCtrl `0xff`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00`.

## What remains deliberately unclaimed

- Static QCAUD mappings for RAW, COMMUNICATIONS, SPEECH, MEDIA and MOVIE are real, but complete AudioReach graph bodies are not all recovered. Do not invent them in an evidence-bound diagram.
- The WinRT Media/Movie tests selecting DEFAULT do not prove that Qualcomm MEDIA/MOVIE modes are unreachable; they prove only that these particular client paths did not request them.
- Explicit Alerts is now also proven to execute both persistent inner Dolby callbacks on the current boot in strict `VR -> VLLDP` alternation. Exact alert-specific coefficients/state are still not claimed from callback execution alone; see `docs/findings/2026-08-11-youtube-vs-alerts-dolby-kdnet.md`.
- The Windows ordinary render root is proven to WSA interface 1 and two protected speakers, but the CPS DP6 per-slave transport details must not be incorrectly copied onto the ordinary render stream.

## Debugger safety / capture notes

No direct physical MMIO read was performed. No MMIO, DSP, SoundWire, or driver-state write was performed.

One attempted set of three simultaneous ARM hardware execution breakpoints (mode translator + DAX wrapper + VLLDP callback) failed on processor 0 with `Too many data breakpoints`. The target was immediately recovered by clearing all breakpoints and detaching. Subsequent probing used one user-mode execution breakpoint at a time. Future sessions should not arm three concurrent hardware execution breakpoints on this target.

Both sessions ended with breakpoints cleared, `.logclose`, and `qd`. Post-session SP7 checks found zero `kd.exe`, zero `windbg.exe`, and zero running PiMaster debugger jobs.

## Raw evidence outside Git

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_RENDER_MODES_20260811_0808BST.log`

- size: 2,592 bytes
- SHA-256: `39f987688377b65a9d729b3b5d990b2654f93c08706ef9ec5dba35e3e43aae55`

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_ALERT_DOLBY_20260811_0823BST.log`

- size: 2,987 bytes
- SHA-256: `f66b129d662b79b6c3ed6825e8a63a1d4250f744e1d5800241ec350b699572e7`

Related reviewed sources:

- `docs/audit/2026-07-25-canonical-windows-linux-graph-ledger.md`
- `docs/findings/2026-07-26-windows-render-mode-selection.md`
- `artifacts/reviewed/windows-render-mode-gkv-mapping.json`
- `docs/findings/2026-08-04-DOLBY-LIVE-KDNET-HOT-PATH.md`
- `docs/findings/2026-07-26-windows-root-codec-dma-wsa-interface.md`
- `docs/findings/2026-08-11-qcaucd-dp6-private-boundary-runtime.md`
