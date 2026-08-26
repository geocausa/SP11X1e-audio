# SP11 native audio FullIO v19c audit/checklist — 2026-08-26

This checklist supersedes the microphone/profile portions of the older
speaker-only audit. It does not reopen system suspend/resume; that subject is
owned by a separate dedicated reverse-engineering effort.

| Area | State | Acceptance evidence / disposition |
|---|---|---|
| Promoted boot / rollback | **PASS** | v19c saved default; v18 + Golden-v33 entries retained |
| Golden protected render topology | **PASS** | SP/SPVI, VI/CPS, MSIIR, final VOL_CTRL and GRAPH_START accepted |
| Exact endpoint volume taper | **PASS** | Windows-L/R transaction, CKV deltas and GainStep changes observed under live playback |
| Endpoint DSP mute | **PASS** | exact DSP mute/unmute; no normal-path hardware/host fallback |
| WSA active/idle gain policy | **PASS** | RX84 active, RX81 Golden idle |
| UbiG v2 engine | **PASS** | filter-chain service stable; no profile-change restart |
| UbiG .deb profile backend | **PASS** | v2 desired/active ack for Dynamic, Movie, Music, Game, Voice, Course, Custom |
| UbiG compatibility CLI | **PASS / FIXED** | exact node resolution + v2 control page; no bypass mis-selection |
| Custom 20-band EQ | **PASS** | saved non-flat curve preserved and same curve reapplied through v2 |
| ALSA MicArray PCM | **PASS** | card0/device2 MultiMedia3 Capture |
| PipeWire MicArray source | **PASS** | Internal microphone array published and records nonzero stereo data |
| Full duplex | **PASS** | playback + capture simultaneously RUNNING, no graph-open/xrun faults |
| WSA/TX/VA runtime PM | **PASS** | all three suspended with usage 0 after idle |
| Mic-bias regulator release | **PASS** | vreg_l1b_1p8 and codec-vdd-micb released after capture |
| Topology reproducibility | **PASS** | Golden base hash gate + v19c byte-identical rebuild |
| Kernel-module reproducibility | **PASS** | pristine -> Golden v33 -> 0072+0078 reproduces common/VA/TX raw `.ko` bytes and exact srcversions |
| Initrd reproducibility | **PASS** | deterministic newc+zstd rebuild reproduces promoted initrd SHA `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d` |
| AudioReach object-ID hygiene | **PASS / FIXED** | builder rejects cross-class object-ID aliases; old `0x4003` collision removed |
| System suspend/resume | **EXTERNALLY DEFERRED** | known separate platform RE; deliberately not tested and not an audio acceptance blocker |

## Remaining audio-chain actions

There is no known functional or clean-reproduction blocker in the tested non-suspend built-in audio chain. The complete heavy gate `repro/native-audio-v19c/build-kernel-initrd-and-verify.sh` completed from pristine Linux 7.1.5 with exit code 0 and ended in `FULLIO v19c KERNEL + INITRD EXACT REPRODUCTION PASS`. Keep v18 as an explicit rollback entry even after v19c promotion. Future
changes should re-run at minimum: topology hash/build gate, speaker playback,
exact volume/mute transaction, PipeWire MicArray capture, duplex and runtime-PM
idle. Do not add system suspend/resume to this checklist unless the dedicated
suspend/resume RE hands back an accepted platform solution.
