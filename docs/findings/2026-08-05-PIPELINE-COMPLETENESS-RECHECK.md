# SP11 Windows speaker pipeline completeness re-check — 2026-08-05

This checkpoint supersedes older architecture claims where newer live evidence exists. It intentionally separates module residency, proven sample-processing execution, lower AudioReach topology, protection/calibration, and final waveform parity.

## Executive status

The current Linux implementation is no longer accurately described as a speculative Dolby clone. The persistent Windows-hot user-mode speaker path has been recovered as original ARM64 Windows code and is running on Linux:

```text
application PCM
 -> DolbyDax3Apo wrapper
 -> DolbyAPOvlldp150 (original 432 scheduler -> 256 accumulator/core)
 -> DolbyApoVr (original wrapper/core)
 -> endpoint / Qualcomm AudioReach path
 -> SPv5 + SP_VI protection / WSA amps
```

However, **the complete Windows-to-speaker system is not yet certified bit-for-bit equivalent**. The strongest remaining parity gap is end-to-end measurement under the same deterministic stimulus/profile, plus a small number of lower-DSP/configuration and mode-dependent edge cases described below.

## 1. Bass Enhancer switch: runtime OFF is directly proved

The historical archive contains 33 saved direct DAX RPC state captures with a `bass_enhancer_enable` field.

Exhaustive enumeration:

```text
captures             33
bass_enhancer_enable 0 : 33
bass_enhancer_enable 1 : 0
profile 5 / bass 0      : 26
profile 1 / bass 0      : 7
```

This is independent of XML defaults.

A second independent proof comes from the full June-8 `DAX3API.exe` service process dump. Parsing its MiniDump Memory64 ranges and the MSVC `std::map<std::wstring,std::wstring>` layout used by the decompiled DAX getter recovers a live map node:

```text
DAX3API service PID 5940
node VA: 0x000001a63a7b1560
key:     "bass-enhancer-enable"
value:   "0"
```

The getter/setter decompilation confirms DAX reads and writes this named tuning-manager entry. The setter accepts only 0/1 and propagates the explicit change. No saved runtime capture has been found showing a `SetBassEnhancerEnable`, `BassEnhancerEnableChanged`, or state value of 1.

Important provenance correction: the directory named `20260614_165537_dolby_youtube_live_165532` is misleading. Its own metadata says `no browser/music; deterministic tone`. Do not cite that folder as a YouTube capture merely from its name.

Therefore the ordinary DAX Bass Enhancer switch being OFF is a strong historical runtime fact. It does **not** mean the speaker chain is bass-neutral: VLLDP compressor/regulator/leveler state, VR processing, MSIIR/static endpoint calibration, gain staging, and SP/SPVI protection all remain capable of materially altering perceived bass and loudness.

## 2. July-30 Firefox/YouTube kernel dump: what it proves and what it cannot

Dump:

```text
$SP11_PROJECT_ROOT/Gemini/dumps/WINDOWS_KERNEL_DUMP/sp11_kernel_mcp_windbg.dmp
```

The dump itself retains an audio-session string binding the current internal-speaker MMDEVAPI endpoint to:

```text
\Device\HarddiskVolume3\Program Files\Mozilla Firefox\firefox.exe
```

This is direct in-dump playback provenance and is stronger than relying only on operator recollection.

The dump is ARM64 `RDMPDUMP`, DumpType `0x8` (`.dump /k`). It does **not** retain the `audiodg.exe` process page-table root or all private user pages. Nevertheless, contrary to an older note, selected user/runtime pages are present.

A genuine live VLLDP state page was recovered. Internal self/child pointer geometry matches the later August live object and solves approximately:

```text
July VLLDP state              0x0000018BD668C360
July DolbyAPOvlldp150 base    0x00007FFBC9F00000
```

Direct named controls in that page are:

```text
state+0xEF4 compressor deviation    96
state+0xF0C slow compressor enable   1
state+0xF14 slow mix                103
```

These are exactly the Movie/Music-family values. Dynamic-family values are `0, 0, 256`.

Thus the actual Firefox/YouTube capture proves **Movie/Music-family VLLDP state**, not Dynamic, for that moment. The retained VLLDP page cannot distinguish Movie from Music.

The dump did not retain the recognizable `DolbyApoVr` wrapper/core page containing `core+0xC90`, so it cannot directly sample the July VR Bass Enhancer bit. The independent DAX runtime evidence above is therefore important.

## 3. Persistent user-mode Dolby path: Windows-hot and reproduced

August-4 hardware execution traps establish the continuously hot tested stereo/music path:

```text
DolbyDax3Apo APOProcess       HOT
DolbyAPOvlldp150 outer path   HOT
VLLDP core FUN_18001f7a8      HOT (~31 hits/s sampled)
DolbyApoVr wrapper/core       HOT in the second DAX instance
```

The stable per-cycle order is:

```text
VLLDP -> VR
```

The Linux bridge executes the original Windows ARM64 PE code for both processors. VLLDP includes the original 432 scheduler, 256 accumulator and core. VR uses the original wrapper/core. Linux shims are limited to Windows runtime/allocation/logging/locking plumbing.

Tests include long real and synthetic streams, finite-output checks, transparent bypass and million-frame arbitrary-buffer chunk invariance.

This supersedes the old theory that `DolbyAPOvlldp150` merely computes MSIIR coefficients without filtering user audio. The recovered/hot Windows VLLDP code demonstrably processes samples itself.

## 4. DAX profile changes do not normally retune Qualcomm MSIIR

A purpose-built Windows capture changed Dolby Access **Dynamic -> Music** while a 997-Hz tone remained active.

Observed:

```text
same audiodg.exe PID before/after
same Dolby/VLLDP/Surface module bases
QGPR records: 1314
GRAPH_OPEN:    0
SET_CFG:       0
```

Therefore this profile switch is an in-place user-mode Dolby reconfiguration, not a Qualcomm graph rebuild or captured lower-DSP retune.

A separate **Music IEQ Off -> Detailed** capture did show graph stop/flush and two `SET_CFG` operations, but those decoded as hardware-endpoint / timestamp configuration rather than raw Dolby/MSIIR parameter writes. Controls must not all be assumed to cross the same boundary, but the evidence does not support the older model of DAX continually pushing profile-dependent Dolby biquads into AudioReach MSIIR.

## 5. Lower Windows AudioReach topology: Grade-A live evidence

The July-23 KD/QGPR work recovered the live internal-speaker topology.

Per-channel render branches contain, in simplified form:

```text
RD_SHMEM
 -> SAL_V2
 -> DATA_LOG
 -> POPLESS_EQ
 -> SAL_V2
 -> MFC
 -> UNKNOWN_0x32
 -> SAL_V2 / VOL_CTRL
 -> MSIIR
 -> MSIIR
 -> shared root
```

Shared root:

```text
SAL -> CHMIXER -> SPv5 -> SPLITTER -> LOGGER -> CODEC_DMA_SINK
```

VI feedback contains `CODEC_DMA_SRC -> LOGGER -> SP_VI` plus the companion feedback branch.

Later API-name recovery corrects another stale label:

```text
0x08001038 = PARAM_ID_VOL_CTRL_MULTICHANNEL_GAIN
0x08001039 = PARAM_ID_VOL_CTRL_MULTICHANNEL_MUTE
```

The repeated Windows writes previously described as a mysterious dynamic limiter are therefore normal gain/mute controls, not evidence of an undiscovered limiter algorithm.

## 6. Linux lower graph/protection: active and substantially Windows-derived

The current loaded `snd_q6apm` implementation contains a fail-safe that bypasses protection if VI feedback is unavailable. On the current boot that fail-safe has **not** fired.

Current boot evidence repeatedly reports:

```text
SPVI configuration query accepted
SPVI R0/T0 accepted
VI endpoint calibration accepted
SP/SPVI enabled with VI feedback accepted
```

and reports zero `speaker protection is bypassed` / `SP/SPVI fail-safe bypass` messages.

The current protected graph was reconstructed from live Windows graph order plus official REV_0D ACDB. Later validation established both-amp 8-kHz VI feedback and R0/T0 values byte-identical to the Windows capture (approximately 4.956 ohm / 38.7 C and 5.370 ohm / 37.0 C).

The protected startup sequence includes graph/subgraph calibration, SP/SPVI configuration/query, R0/T0, SP/SPVI tag calibration, render/VI endpoint calibration, VOL_CTRL gain, MSIIR calibration, VOL_CTRL mute, channel mixer and graph start.

One graph calibration record remains deliberately filtered because the DSP rejects it as unsupported:

```text
IID 0x412b
param 0x0800113d
size 28
-EOPNOTSUPP
```

106/107 captured calibration frames are accepted. This is very close, but it is not literal 107/107 byte-for-byte replay.

Protection telemetry also has a residual gap: the core SP/SPVI configuration is live, but the Windows-style ongoing result/event callback path is not fully reproduced. A protection telemetry GET / event-registration operation is rejected on the Linux SPF path, so passive logging is retained instead. This is primarily an observability/completion gap; it has not been shown to disable the SP/SPVI DSP itself.

## 7. No accidental old MSIIR injector is running

Current process/service/timer/autostart inspection found no `sp11_msiir`/MSIIR runtime injector running alongside the new native Dolby plugin. Therefore the current production path is not obviously double-processing via an old userspace coefficient injector.

An experimental `sp11_asar_parent_probe` process is still alive under GDB, but it owns no `/dev/snd` or PipeWire/Pulse audio file descriptor and is not part of the render graph.

## 8. SurfaceAPO / modern ASAR / HRTF actors

`SurfaceAPO_0D.json` shows the 48-kHz stereo `R/MFX/DEFAULT/defaultEQ` disabled with identity coefficients. The AudioReach Popless EQ capture is likewise mathematically flat in the relevant configuration. These rule out the easy hidden endpoint-EQ explanation for the large Dolby coloration.

Modern `DolbyAudioProcessing.dll` / ASAR and AIDE code exists and can be mapped, but August hardware traps on the tested ordinary stereo/music condition found the modern speaker DAPVR wrapper, embedded VLLDP and AIDE core **cold**, while the persistent DAX/VLLDP150 path was hot. Treat ASAR/AIDE/HRTF as mode-dependent/present rather than part of every steady stereo speaker callback unless a mode-specific trap proves otherwise.

Module residency alone is not proof of sample participation.

## 9. Recovered May-18 known-input Windows loopback oracle

A previously overlooked controlled pair still exists in the older `/AUDIO/dolby` archive:

```text
input:  sp11-known-input-stimulus-48k.wav
output: known-input/windows-loopback-20260518-153312.wav
```

The stimulus is 29.45 s / 48 kHz stereo and deliberately contains a 1-kHz reference, log sweep, 55/90/140-Hz bursts and a 75-Hz stepped-level staircase. The Windows output is a WASAPI default-render loopback, so it is primarily an oracle for the audio-engine/APO side rather than the physical Qualcomm/amp transfer path.

The May runtime copies of `DolbyAPOvlldp150.dll`, `DolbyAPOVR.dll` and `DolbyDax3Apo.dll` are SHA-256 identical to the July/Aug binaries used by the current Linux bridge.

After aligning latency and fitting only one global scalar gain, current original-code profiles compare to the May Windows waveform approximately as follows:

```text
profile          waveform corr    residual SNR
movie               0.96308          11.40 dB
dynamic             0.96214          11.29 dB
music               0.96059          11.12 dB
```

The old capture did not preserve a trustworthy selected-profile record, so the narrow Movie lead is **not** enough to label the May profile. The nonlinear segment transfer still differs by roughly 1--3 dB in parts of the 75-Hz staircase; this is strong agreement but not bit-exact parity.

An actor-ablation comparison is more decisive:

```text
VLLDP only       corr 0.94093   residual SNR  9.41 dB   fitted gain +9.97 dB
VLLDP -> VR      corr 0.96308   residual SNR 11.40 dB   fitted gain +0.62 dB  (Movie candidate)
```

Thus the old loopback independently supports the recovered VLLDP -> VR topology and shows that VR is acoustically material in this path. It also provides a real historical waveform oracle for future parameter/state fitting.

The actually installed production plugin at `~/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so` was also checked offline: its Dynamic/Movie/Music outputs are byte-identical to a fresh build from the current source. The stale untracked `.so` inside the worktree is older and must not be mistaken for the deployed binary.

## 10. Current confidence matrix

| Actor / boundary | Windows runtime evidence | Linux state | Confidence / caveat |
|---|---|---|---|
| DAX Bass Enhancer switch | 33/33 RPC reads = 0; DAX service heap value `"0"` | native profile config keeps it off | Very high |
| DolbyDax3Apo wrapper | hardware-trap hot | host role reproduced around original inner processors | High |
| VLLDP150 sample processing | hardware-trap hot, scheduler/core decoded | original Windows code running | Very high |
| DolbyApoVr sample processing | second persistent DAX instance hot | original Windows code running | High |
| Dynamic -> Music profile boundary | no QGPR SET_CFG/GRAPH_OPEN | user-mode profile switching | High |
| Surface default EQ | disabled / identity | not separately emulated | High no-op for captured stereo mode |
| Qualcomm render topology | live KD/QGPR graph | reconstructed protected graph | High topology confidence |
| MSIIR endpoint calibration | captured Windows stages / ACDB | loaded once in protected graph | High static-stage confidence; final transfer parity still needed |
| SPv5/SP_VI topology | live Windows graph | active, VI feedback present | High |
| R0/T0 / endpoint protection calibration | captured Windows values/order | later Linux values byte-identical and accepted | High |
| Complete graph calibration | 107 Windows records | 106 accepted, one unsupported filtered | Not byte-for-byte complete |
| Protection event/result telemetry | Windows callbacks/results exist | subscription/readback partially unsupported | Incomplete observability |
| Modern ASAR/AIDE speaker path | present but tested cores cold | not in steady native chain | Mode-dependent unresolved |
| HRTF/spatial path | module may load | not in ordinary stereo chain | Spatial-mode only/unclosed |
| Historical Windows-vs-Linux waveform | May-18 exact stimulus/output pair recovered; profile not pinned | full chain reaches ~0.96 waveform correlation | Strong partial validation, not final certificate |

## 11. What can and cannot be claimed now

Supported:

- the named Bass Enhancer switch was historically OFF in every saved direct DAX runtime read;
- strong bass/loudness therefore comes from other active Dolby/gain/protection actors rather than requiring that switch to be ON;
- the persistent tested Windows Dolby sample path is VLLDP -> VR and both original processors run on Linux;
- the lower Windows topology and speaker-protection architecture have live KD evidence;
- current Linux SP/SPVI protection is genuinely enabled with VI feedback and is not in its bypass branch;
- normal Dynamic -> Music profile switching is not accompanied by Qualcomm SET_CFG in the purpose-built capture;
- no old Linux MSIIR userspace injector is concurrently modifying the production path.

Not yet supported:

- that the July `.dump /k` by itself certifies the entire user-mode + DSP pipeline;
- that every Windows mode (spatial, notification, communication, ASAR) has the same actor set as ordinary stereo music;
- that the Linux graph is literally byte-for-byte identical at every lower calibration record;
- that protection telemetry/event handling is fully Windows-equivalent;
- that Windows and Linux produce the same final waveform/transfer function under a **state-matched** deterministic stimulus/profile; the recovered May pair is highly correlated but its profile/runtime state is not pinned.

## 12. Highest-value remaining test

A historical same-stimulus Windows loopback has now been recovered and already gives about 0.96 waveform correlation with the current full original-code chain, but its exact DAX profile/volume/runtime state is not sufficiently documented. The clean final discriminator is therefore a **new or independently state-resolved same stimulus / same Windows profile / same sample format / same endpoint-volume** loopback capture followed by transfer-function and waveform comparison against the current Linux original-code chain.

For the July Firefox/YouTube state specifically, the recovered VLLDP state belongs to the Movie/Music family, while the current Linux production profile was returned to Dynamic after profile-switch testing. Exact reproduction of that July oracle therefore requires first resolving Movie versus Music (preferably from retained DAX/VR/profile state) or deliberately testing both candidates.
