# SP11 Dolby reverse-engineering chat handoff — 2026-08-05

This document is a continuity handoff for a new ChatGPT conversation. It is
intended to let the next chat resume the SP11 Dolby project without asking the
user to reconstruct months of context or rerun old work blindly.

## 0. User intent / working style

The user wants the assistant to continue this project **autonomously on the
Ubuntu SP11** using the connected remote-command capability. The user is not a
programmer and should not be asked to run shell commands that the assistant can
run itself.

Important interaction rules:

- Do the technical work on the SP11 directly.
- Give short plain-English progress updates periodically so the UI does not look
  stalled during long RE/tool phases.
- Do not drown the user in assembly unless it materially explains a result.
- Do not claim work continues in the background; it only progresses while tools
  are actively being called.
- Preserve unrelated work. Never reset/clean the user's dirty main checkout.
- Prefer evidence, exact replay, and source-of-truth captures over tuning by ear
  or fitting unlabeled historical waveforms.
- When a meaningful finding is reached, document it, commit it, and push it
  before pursuing the next branch.

## 1. Safe repository / machine state

### Main checkout — DO NOT MODIFY / CLEAN

```text
/home/geoca/Documents/SP11-PROJECT/01-audio
```

It contains substantial unrelated dirty work. At handoff it is on:

```text
agent/audio-v2-clean-rebuild
```

and has many modified/untracked files. Do **not** reset, clean, stash, overwrite,
or use it as the Dolby experiment checkout.

### Isolated Dolby worktree — use this

```text
/home/geoca/Documents/SP11-PROJECT/04-dolby-re-work
```

Branch:

```text
agent/dolby-completion-2026-08-05
```

Latest engineering checkpoint before this handoff document:

```text
503b7ccb8f08aae9aabd2158359c761c59ec205a
Preserve four-byte VR live-state isolation
```

That hash was verified identical locally and on `origin`.

### Windows partition

Internal Windows NTFS partition:

```text
/dev/nvme0n1p3
```

It was used only read-only for forensics and is **unmounted at handoff**. If it
must be mounted again, mount read-only and unmount it after the evidence pass.

### Current live Linux audio state

At handoff:

```text
Profile: dynamic (host: dynamic)
Custom GEQ: off
Default sink: effect_input.sp11_windows_dolby
Dolby volume: 0.13
Bypass volume: 0.18
Dolby host: active
filter-chain.service: active/running
MainPID: 92891
NRestarts: 0
Environment: SP11_DOLBY_PROFILE=dynamic SP11_DOLBY_GEQ=off
```

Do not change the live host simply to test a reverse-engineering hypothesis.
Prove changes offline first.

## 2. Recommended reading order in the new chat

Start with these files in this order:

```text
docs/handoff/2026-08-05-DOLBY-CHAT-HANDOFF.md

docs/audit/2026-08-05-CANONICAL-DOLBY-PIPELINE.md

docs/findings/2026-08-05-VR-LIVE-STATE-FOUR-BYTE-ISOLATION.md

docs/findings/2026-08-05-AUDIOENG-LIMITER-AND-LIVE-VR-CORE.md

docs/findings/2026-08-05-LIVE-VLLDP-CORE-RECOVERY.md

docs/findings/2026-08-05-MAY19-CAPTURE-PACK-RECHECK.md

docs/findings/2026-08-05-WINDOWS-KNOWN-INPUT-CURRENT-CHAIN.md

docs/findings/2026-08-05-ASAR-STEADY-SPEAKER-BOUNDARY.md

docs/findings/2026-08-05-MINIMAL-WINDOWS-RUNTIME-CLOSEOUT.md

docs/findings/2026-08-05-CUSTOM1-GEQ-FORENSIC-RECOVERY.md

docs/audit/2026-08-05-DOLBY-EVIDENCE-LEDGER.md
```

Then inspect recent Git history rather than assuming this document is the only
source:

```bash
git log --oneline -20
```

Important recent commits include:

```text
503b7cc Preserve four-byte VR live-state isolation
d696b91 Recheck May19 Windows capture pack
2c7d71d Localize persistent live VR state gap
a42793e Replay full live VLLDP state against Music
4ed7edd Strengthen AudioLimiter ETW correlation
9bbfbe0 Recover live VLLDP core and close sliding bass
143adf2 Prune VR state parity leads
b682af2 Recover live VR core and AudioEng limiter
c73e282 Consolidate canonical Dolby engineering model
21d5638 Preserve AudioEng limiter evidence
```

## 3. What is already functionally complete

The useful Linux speaker solution is not a speculative reimplementation. It
executes the original shipped Windows ARM64 Dolby code.

### Proven persistent Windows render order

Fresh/current Windows evidence repeatedly establishes:

```text
DolbyAPOvlldp150 -> DolbyApoVr
```

VLLDP executes before VR every persistent callback cycle.

### VLLDP

Key identities:

```text
DLL: DolbyAPOvlldp150.dll
production SHA-256:
a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1

outer callback RVA:       0x105050
outer scheduler RVA:      0x0ED348
live scheduler limit:     432
descriptor shim RVA:      0x35160
core orchestrator:        FUN_18001F7A8
inner accumulator RVA:    0x33640
inner block size:         256
Windows host callback:    480 frames
```

Exact 480 -> 256 adaptation is implemented and tested.

### VR

```text
DLL: DolbyAPOVR.dll
SHA-256:
1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc

live outer callback RVA:  0x1D10C8
real outer constructor:   0x1800D3B18
LibWrapperVr offset:      outer+0x12C2F0
embedded arena:           outer+0x12C430, size 0x294000
transition init:           0x1801B96B8
```

Direct original Windows ARM64 VR processing works on Ubuntu and is chunk
invariant across 1/64/480/1024/odd/mixed call sizes up to one million frames.
There are no hot-path heap allocations.

### Combined production plugin

Tracked source:

```text
dolby-port/sp11_dolby_windows_chain_ladspa.c
```

Production build:

```text
~/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so
```

Current GEQ-capable installed host SHA-256:

```text
230932e53734c0fc0749eb54c8b8db462c739d7a7bf32cd937be4cb635d9be2b
```

It executes the actual Windows VLLDP then VR code. Only OS/runtime/lock/resource
plumbing is shimmed. RT audio buffers are fixed/preallocated.

Production private DLLs:

```text
~/.local/lib/sp11-dolby/DolbyAPOvlldp150.dll
~/.local/lib/sp11-dolby/DolbyAPOVR.dll
```

Reproducible production build:

```text
deploy/dolby/build-production.sh
Makefile target: windows-chain-production
```

The build verifies exact private DLL hashes and rejects the wrong revisions.

## 4. Seven native profiles are implemented

Profiles:

```text
dynamic
movie
music
game
voice
onlinecourse
personalize
```

Selected through `SP11_DOLBY_PROFILE` outside the RT callback.

The tuning is recovered from the OEM DAX XML and original DLL setters. Dynamic
regression against the earlier single-profile build is bit-identical. All seven
profiles pass chunk-invariance tests and a 1,413,600-frame finite-output
stimulus with no NaN/Inf.

Live helper:

```text
~/.local/bin/sp11-dolby
tracked source: deploy/dolby/sp11-dolby
```

Useful commands:

```text
sp11-dolby status
sp11-dolby on|off|restart
sp11-dolby profile NAME
sp11-dolby dynamic|movie|music|game|voice|onlinecourse|personalize
sp11-dolby geq
sp11-dolby geq reset
sp11-dolby geq set <20 integers>
```

The helper preserves Dolby sink volume/mute across profile restarts and clears
the deliberate systemd restart-rate limit before switching.

## 5. Personalize / Custom GEQ is implemented through original Dolby code

Recovered original handler:

```text
graphic-equalizer-enable -> 0x180032780
```

The 20-band target path uses the original trusted Dolby band-grid/target
functions. Public DAX Custom GEQ range is exactly `-192..+192` for each of 20
integer values.

Environment:

```text
SP11_DOLBY_GEQ
```

Rules:

- missing/off/flat => disabled;
- GEQ is active only with `personalize`;
- invalid input safely disables it rather than aborting the host;
- other profiles keep GEQ off even if a saved curve exists.

Persistent helper curve:

```text
~/.config/sp11-dolby/geq
```

### Historical Custom1 curve hard wall

The exact old 20-value Custom1 array from the June-12 Dolby Access screenshot
was never captured numerically. The RPC probe at the time decoded the SAFEARRAY
incorrectly. Current/transaction registry hives and read-only NTFS forensics do
not recover it. The implementation is complete; only that historical evidence
array is missing.

Do not invent a replacement and call it the recovered Custom1 curve.

## 6. Static profile-gap questions already resolved

### Partial virtualizer flags

The OEM partial-surround / partial-height booleans simply mirror processing mode
for every profile. The DLL exposes output mode/count/matrix but no independent
partial setters. Treat them as upstream policy metadata, not missing DSP knobs.

### speaker-peq-enable

Although XML varies this flag, the speaker PEQ filter payload is empty. Controlled
Dynamic -> Music memory captures show no separate PEQ graph/setter. Do not hunt a
missing speaker PEQ block for this endpoint unless new evidence appears.

### Sliding bass / fake bass

The June live VLLDP core directly shows sliding-bass enable = 0. Windows does
initialize dormant parameters differently:

```text
band boundary 6
attack 712 ms
release 500 ms
max level 52
min 0
curve all zero
```

Applying those exact values through the original setters while enable remains 0
produces bit-identical Dynamic/Movie/Music output, including the full known
input. The disabled gate is real.

## 7. AudioEng `CAudioLimiter` — real stage, not the bass source

The Windows AudioEng graph contains a coherent processing cluster including:

```text
Surface render MFX
Dolby DAX MFX
Adaptive Spatial Audio Renderer
CAudioLimiter
AudioFormatConvert
```

The exact built-in limiter has been decoded:

```text
48-kHz stereo look-ahead: 64 frames
ceiling:                   0.9850000143 (~-0.131 dBFS)
linked stereo detector:    max(abs(L),abs(R))
release constant:          2.205 / sample_rate
```

Analysis oracle:

```text
dolby-port/sp11_audioeng_limiter_oracle.py
```

Applying it after the current Dolby chain does not create the missing loud
75-Hz behavior; the current candidate often reaches those segments below the
limiter threshold.

The limiter's **actual live object** was recovered from both June `audiodg`
dumps. At each snapshot:

```text
current gain = 1.0
attack countdown = 0
```

Thus it is instantiated and live but was not attenuating at the exact captured
moments. Treat it as the final safety ceiling, not the ordinary source of the
speaker character.

## 8. May historical capture correction

Do not assign the May-18/19 loopback capture a Dolby profile from endpoint
registry tuning values. Controlled June profile switching proves those registry
families are endpoint/default data and do not change with the active profile.

The 12-file May-19 capture pack remains profile-unlabelled.

Current exact VLLDP->VR pack-wide RMS error:

```text
Dynamic: mean abs 1.049 dB, RMSE 1.232 dB
Movie:   mean abs 0.899 dB, RMSE 1.021 dB
Music:   mean abs 0.958 dB, RMSE 1.315 dB
```

Music is closest on 7/12 individual signals, Movie has best mean error. This is
strong transfer-level validation but **does not prove the active profile**.

The May-19 clean 75-Hz -18 dBFS capture is nearly harmonic-clean, whereas an
older loud 75-Hz staircase develops strong H3/H5 only near the digital ceiling.
Therefore the strong odd harmonics are level/ceiling dependent; they are not
evidence for an always-on Virtual Bass generator.

## 9. June full `audiodg.exe` dumps are the strongest current source of truth

Evidence:

```text
.../WINDOWS_LIVE_CAPTURE_20260608/
  02_process_memory_dumps_20260608_1742_audio_dolby_runtime/
    audiodg.exe_260608_174744.dmp
    audiodg.exe_260608_174832.dmp
```

These contain the actual live persistent VLLDP and VR objects from the same
`audiodg.exe` process and live interval.

### Live VR object

```text
DolbyApoVr.dll base:      0x00007FFD07A60000
LibWrapperVr:             0x000002453913C2F0
VR core:                  0x00000245391DD808
geometry:                 512 / 256
fill across dumps:        96 -> 160
```

Direct live core reads prove:

```text
Bass Enhancer enable = 0
Bass Extraction enable = 0
```

Across 34 stable scalar profile discriminators:

```text
Music = 34/34
Movie = 28/34
others lower
```

The June VR core is therefore unambiguously **Music**, and the reconstructed
Music static configuration matches it field-for-field on those discriminators.

### Live VLLDP object

```text
wrapper:      0x000002453968C1F8
main state:   0x000002453968C360
geometry:     2 channels / 256 frames
fill:         96 -> 160
```

Stable profile discriminators match the Movie/Music VLLDP family 18/18, which
is consistent with the same process's VR core identifying Music.

## 10. Full live VLLDP-state replay result — strong parity evidence

The complete captured Windows-warm VLLDP state can be replayed on Ubuntu at its
**original Windows heap address**, with the original VLLDP DLL mapped at the
captured Windows module address. Constructor pointer geometry matches.

A misleading first result (~4x apparent gain) was proven to be stored program
ring-out: with zero new input, the captured state emits ~0.137 RMS on the first
block, ~0.005 on the second, ~0.00046 on the third, then effectively zero.

With repeated 0.05-amplitude 997-Hz tone, after old history clears:

```text
captured Windows-warm VLLDP, block 3 onward:
  RMS  ~0.031597400
  peak ~0.055477425

fresh reconstructed Music VLLDP, block 3 onward:
  RMS  ~0.031597399/400
  peak ~0.055477429/425
```

This is direct original-code execution, not fitting. VLLDP is therefore very
strongly validated once history is normalized.

## 11. Current hottest lead: persistent live VR state/lifecycle gap

The complete June live VR outer allocation is present contiguously:

```text
outer base: 0x0000024539010000
size:       0x3C0430
```

It can be mapped at that exact address on Ubuntu while mapping the exact DLL at
its Windows ASLR base.

### Correct comparison methodology

Use a **continuous-phase** tone across calls. Do not regenerate a sine starting
at phase zero for every 256-frame block; captured and fresh FIFO fills differ
and block-edge phase discontinuities create a false steady separation.

For continuous 997-Hz stereo tone, amplitude 0.05, 256-frame calls, 4096 blocks:

```text
fresh reconstructed Music VR:
  RMS  ~0.153542091
  peak ~0.215435341

captured Windows Music VR:
  RMS  ~0.120047398
  peak ~0.168434650
```

The difference remains after ~22 seconds, so it is not initial ring-out.

### Exact-address hybrid isolation

Building the fresh Music VR object at the same Windows addresses removes pointer
relocation noise and permits byte-compatible hybrids.

Representative 1024-block RMS:

```text
fresh object                    0.153500586
captured core only              0.125827543
captured embedded arena         0.119955883
captured full object            ~0.11998-0.12005
```

Most of the persistent difference lives in the VR core. A smaller dependent
contribution lives in another arena subobject and only matters when the captured
core is present.

### Secondary contribution reduced to one float

Binary localization:

```text
outer+0x1EC430..0x20C430
 -> outer+0x1F0430..0x1F2430
 -> outer+0x1F1430..0x1F1830
 -> outer+0x1F1730..0x1F1770
 -> outer+0x1F1768..0x1F176C
```

Exact scalar:

```text
absolute VA:            0x0000024539201768
outer offset:           0x1F1768
captured Windows float: 0.81490242
fresh Music float:      0.80197930
```

With captured core already present:

```text
captured-core baseline RMS     0.125830478
+ only captured 4-byte float   0.119904641
```

So those four bytes reproduce essentially the entire remaining secondary
contribution.

### Hardware watchpoint proof

A GDB hardware read watchpoint on:

```text
*(float *)0x24539201768
```

triggers in the original shipped VR DLL at:

```text
PC:                  0x00007FFD07ACA33C
DolbyApoVr.dll RVA:  0x6A33C
function:             FUN_18006A2D0
```

The routine compares indexed floating state and is real multiband dynamics code.
Ghidra shows rising/falling state, smoothing, table interpolation and updates to
multiple related per-index arrays.

Known callers:

```text
FUN_180058990 -> FUN_18006A2D0
FUN_18006A0E0 -> FUN_18006A2D0
```

**Do not give this routine a feature name yet.** It has not been proven to be
Leveler, Regulator, Optimizer, Virtual Bass, etc.

Detailed finding:

```text
docs/findings/2026-08-05-VR-LIVE-STATE-FOUR-BYTE-ISOLATION.md
```

Repro harnesses:

```text
tools/diagnostics/live-vr-state/
```

The captured 3.9-MB outer binary is intentionally not committed; re-extract it
from the June full process dump when needed.

## 12. Best next work — resume here

This is the recommended order for the next chat.

### Priority A — semantic ownership of `FUN_18006A2D0`

1. Decompile and identify `FUN_180058990` and `FUN_18006A0E0`.
2. Trace their callers upward until the parent VR submodule is identifiable.
3. Determine the structures passed as the multiband routine's state/input
   arguments.
4. Use a hardware **write watchpoint** in a controlled fixed-address replay if
   useful to identify who initializes/updates `0x24539201768`.
5. Do not hard-code `0.81490242`; it is captured runtime state, not yet a proven
   static configuration value.

### Priority B — localize the dominant core-only difference

The core-only hybrid already moves RMS roughly:

```text
0.1535 -> 0.1258
```

which is larger than the isolated secondary 4-byte effect. Use the same
exact-address binary-search hybrid strategy **inside the VR core** to identify
which core range(s) own the dominant persistent difference.

Avoid naive whole-core scalar diffing: many fields are history arrays, pointers,
locks, or inert cached tables. Let acoustic A/B localization decide which bytes
matter first, then map those bytes to code.

### Priority C — reproduce lifecycle, not a frozen warm snapshot

Once the owning state mechanism is understood, reproduce it through the
original Dolby initialization/update path. Production should not embed a warm
Windows memory snapshot or magic history value.

Validate any fix across:

- multiple input levels;
- cold and warm trajectories;
- continuous tones and broadband signals;
- known-input / May pack only as secondary evidence;
- chunk invariance and hot-path allocation rules.

Only after offline validation should the production plugin be changed or the
live PipeWire host restarted.

## 13. ASAR/AIDE/OAR boundary — separate remaining Windows-runtime question

Modern DLL:

```text
DolbyAudioProcessing.dll
SHA-256:
900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
version 7.3.7.0 / 7.3.7.rel
```

Fresh static analysis proved the high-level initialized AIDE branch calls:

```text
ASAR + 0x3A438
```

unconditionally if AIDE is active. August hardware live evidence shows zero
hits there while persistent VLLDP/VR is hot. Therefore **AIDE is conclusively
not a steady per-buffer contributor in the tested speaker condition**.

OAR and Crossfade are not closed statically. Exact future Windows hardware
breakpoints:

```text
ASAR + 0x241E8   OAR processing core
ASAR + 0x1C378   crossfade/high-level state path
ASAR + 0x3A438   AIDE known-cold control
```

Keep a known-hot persistent VLLDP/VR callback breakpoint as positive control.
Do not treat software-breakpoint non-hits as evidence in this KD setup.

This requires a Windows runtime/KDNET session and is a genuine Ubuntu-side hard
wall. It is not necessary for the current usable Linux Dolby speaker chain.

## 14. Important evidence / archive paths

KD archive:

```text
/home/geoca/Documents/SP11-PROJECT/00-RE-archive/kdnet/2026-08-04-chatgpt/
```

KD live log:

```text
logs/KDNET-50005-terminal-live-copy-20260804.log
```

Recovered old research hub:

```text
/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/Research_Hub_Audio
```

Windows loopback material includes:

```text
Research_Hub_Audio/SOURCE/UbuntuConceptEliteX/windows-loopback-captures/
```

June process dumps are under the `WINDOWS_LIVE_CAPTURE_20260608` tree described
above.

## 15. Reverse-engineering tools

Ghidra headless:

```text
/home/geoca/Desktop/Ghidra/support/analyzeHeadless
```

Tracked scripts are under:

```text
tools/ghidra/
```

Useful scripts include the address decompiler, string-xref, symbol-matching,
caller/range helpers present in that directory. Prefer temporary Ghidra
projects under `/tmp`; do not mutate old archived Ghidra projects in place.

For live-state replay diagnostics, use:

```text
tools/diagnostics/live-vr-state/
```

## 16. Things that should NOT be redone without new evidence

Do not spend another cycle assuming any of the following is the missing bass
mechanism unless new source-of-truth evidence contradicts the existing work:

- VR Bass Enhancer — directly OFF in live core.
- Bass Extraction — directly OFF in live core.
- VLLDP sliding bass — enable 0, exact dormant settings proven bit-inert.
- speaker PEQ — empty filter configuration / no independent active graph found.
- partial virtualizer flags — policy metadata mirroring output mode.
- AudioEng limiter — real but downstream safety ceiling; not the normal drive.
- AIDE — live steady-state path ruled out in tested speaker condition.
- `VR core+0x5E0` — lazy/cache/output-mode state; bit-inert for the relevant
  Music path.
- historical May active-profile inference from endpoint registry values — invalid.
- input-amplitude fitting to the old VLLDP C0C vector — invalid because the
  Windows capture was already warm/history-bound.

## 17. Genuine unresolved hard walls

1. A purpose-built **same-stimulus Windows capture with contemporaneous profile,
   DAX active_profile, Windows volume, enhancement state and spatial state** is
   still needed for rigorous end-to-end waveform parity.
2. OAR/Crossfade live relevance still needs the small Windows KDNET hardware
   breakpoint session above.
3. The exact historical Custom1 20-band curve remains unrecovered unless an old
   screenshot/hive/library artifact resurfaces.

None of those blocks the current usable Linux original-code VLLDP->VR host.

## 18. Suggested first action in the new chat

After reading this document and the four-byte isolation finding, run a quick
safety/status check in the isolated worktree:

```text
git status --short --branch
git rev-parse HEAD
git ls-remote origin refs/heads/agent/dolby-completion-2026-08-05
~/.local/bin/sp11-dolby status
systemctl --user show filter-chain.service -p ActiveState -p MainPID -p NRestarts -p Environment
findmnt /dev/nvme0n1p3
```

Expected safe state:

- isolated worktree clean;
- remote equals local branch head;
- main checkout remains dirty and untouched;
- live host active on Dynamic / GEQ off unless the user changed it;
- Windows NTFS unmounted.

Then resume at **Priority A / Priority B** in section 12 rather than reopening
already-closed theories.
