# SP11 Windows Dolby reverse-engineering — new-chat handoff (2026-08-06)

> **Purpose:** this is the continuity file for a fresh ChatGPT chat. Read this first, then continue autonomously from the exact resume point near the end. Do not reopen closed theories unless new evidence contradicts the findings below.

## 1. User working style

The user wants this project driven autonomously and exhaustively. Make technical decisions without repeatedly asking for permission. Keep the user updated during long tool work, explain important results in plain English, and prefer source-of-truth reverse engineering over subjective tuning. Do not make blind live-audio changes.

The goal is **Windows/SP11 speaker-audio parity on Linux**, including tonal, dynamics, loudness and psychoacoustic/spatial behavior, using the original Windows Dolby code wherever it is proven to be the active path.

## 2. Machine / repository / safety rules

Target machine is the **Surface Pro 11 / SP11**. Use the **CustomConnector** target for machine work.

Project root:

```text
/home/geoca/Documents/SP11-PROJECT
```

The main checkout is dirty and must **not** be touched:

```text
/home/geoca/Documents/SP11-PROJECT
```

Use only the isolated Dolby worktree:

```text
/home/geoca/Documents/SP11-PROJECT/04-dolby-re-work
```

Branch:

```text
agent/dolby-completion-2026-08-05
```

At handoff creation time the isolated worktree was clean before documentation changes, and the branch/remote both pointed to:

```text
2ef97e4 Correct May18 known-input alignment oracle
```

Important ancestry: `359492a` is an ancestor of `2ef97e4`; the branch did not regress. Relevant recent history:

```text
2ef97e4 Correct May18 known-input alignment oracle
090cc66 Record stereo bypass live rollout
d6b71c9 Honor SP11 stereo virtualizer bypass
ed0e9ee Close SurfaceAPO render mode caveat
1724474 Map Windows shared ASAR plugin path
fd244e3 Close ASAR matching stereo path
359492a Close Windows stereo HRTF bypass policy
9b37c35 Prove DAX3 direct wrapper path
8fb43cf Record live in-place profile rollout
178c3be Preserve queued profiles across lazy instantiation
78520c6 Add runtime profile request channel
ed5ad95 Add in-place Dolby profile switching
5f9aa53 Prove Windows profile retune lifecycle
7ee4bbd Correct VLLDP limiter state interpretation
```

Do not commit Microsoft/Dolby binaries.

Windows NTFS partition:

```text
/dev/nvme0n1p3
```

Keep it unmounted. If one uniquely high-value Windows artifact is needed, mount **read-only with ntfs-3g**, copy only the needed file to `/tmp`, and unmount immediately. That policy was followed for `VirtualSurroundApo.dll` and `AudioEng.dll` checks.

Use temporary Ghidra projects under `/tmp`. Do not mutate archived Ghidra projects.

## 3. Current live-audio state

At the end of this chat the Dolby user service was found **inactive/dead**, with no Dolby/bypass sinks currently present. This was treated as the user's current machine state.

Do **not** restart it merely to continue RE.

Earlier in the session, before it became inactive, the most recent observed sink volumes were:

```text
Dolby sink   0.22
bypass sink  0.53
```

Those are historical live-state observations, not instructions to restore anything.

No live DSP/tuning change from the experiments below was deployed.

## 4. Canonical ordinary-stereo Windows topology

The persistent ordinary speaker path is proven:

```text
DAX3 wrapper
   ↓
DolbyAPOvlldp150
   ↓
DolbyApoVr
   ↓
Windows AudioEng / endpoint path
```

The stable per-cycle Dolby sample-processing order is:

```text
VLLDP → VR
```

This was hardware/ETL proven and must not be reopened based only on static SFX/MFX registration order.

### DAX3 wrapper

For the normal equal-rate 48 kHz speaker graph, DAX3 takes its direct inner-APO branch. Hundreds of live ETL stacks hit the equal-rate direct-call return site. The wrapper does not add a hidden SRC, gain, clamp or nonlinear sample stage around VLLDP/VR in this condition.

### Profile lifecycle

Windows Dynamic↔Music profile changes retune the existing VLLDP/VR instances in place. The same objects remain alive; long-memory state survives. Linux production was corrected to preserve state across PipeWire/LADSPA reset and to retarget profiles in place.

### HRTF / spatial

The exact DAX/HRTF policy is closed for ordinary matching stereo:

- HRTF infrastructure loads and executes.
- DAX endpoint configuration sets stereo bypass.
- matching 2-channel stereo takes the HRTF/DAP stereo-bypass path.
- generic personalized HRTF is a separate headphone subsystem and should not be grafted onto ordinary SP11 speaker stereo.

The SP11 Atmos operator policy uses stereo-virtualizer bypass and has Spatial OFF/ON profile-policy differences, but stereo channel-bed HRTF virtualization is not the missing ordinary-stereo tonal effect.

### ASAR / VirtualSurround

`VirtualSurroundApo.dll` was reverse engineered from an exact Windows copy. Its hot children are standard `AudioMeter` and `AudioVolume` APOs and its wrapper side is ASAR transport/sample-buffer plumbing. The matching-stereo ASAR path was subsequently closed as identity/bypass for this condition. Do not treat VirtualSurroundApo itself as a hidden convolution box.

## 5. SurfaceAPO protection marker — closed false positive

A previously suspicious memory marker at:

```text
SurfaceAPO + 0x201998
```

was investigated directly.

It has no direct executable Ghidra references. The bytes around it are a dense PE metadata table. `llvm-readobj` identifies the containing structure as the PE **Guard CF Function Table**:

```text
1,892 entries
5 bytes per entry
```

The old scanner's `0xB0000001` “speaker protection” signature was therefore a byte-pattern coincidence inside CFG metadata, not a Surface speaker-protection object or state.

Close this fork unless independent executable/runtime evidence appears.

## 6. May-18 known-input alignment correction — hard mute does not exist

Canonical source:

```text
.../windows-loopback-captures/sp11-known-input-stimulus-48k.wav
```

Canonical output:

```text
.../known-input/windows-loopback-20260518-153312.wav
```

The old transfer report treated a correlation result near `1.726 s` as whole-file lag. The analysis script had actually correlated a reference slice beginning at source `t=1.0 s`.

Correct lag:

```text
1.726 - 1.000 ≈ 0.726 s
```

The apparent final `-3 dBFS` “hard mute” was therefore an alignment error. All seven staircase tones are present. The low-level `[-2,-1,0]` PCM region after the last tone is the intended 250 ms gap/end silence, not protection mute. True zero-filled loopback appears later when playback goes idle.

This is already committed in:

```text
docs/findings/2026-08-06-MAY18-KNOWN-INPUT-ALIGNMENT-CORRECTION.md
commit 2ef97e4
```

Do not reopen the nonexistent hard-mute problem.

## 7. Corrected May-18 75-Hz transfer oracle

The aligned seven-step 75-Hz reference is approximately:

```text
input     fundamental    H3 dBc    H5 dBc
-30 dBFS   -19.12        -64.74    -71.29
-24        -12.67        -80.28    -83.30
-18         -8.76        -82.53    -89.71
-12         -3.84        -90.95    -96.14
 -9         -1.05        -64.26    -76.59
 -6         -0.87        -34.38    -44.25
 -3         -0.85        -34.24    -42.35
```

The loudest Windows right-channel peak is approximately:

```text
-0.12764 dBFS
```

which is essentially the known Windows `CAudioLimiter` ceiling:

```text
0.985 linear ≈ -0.131 dBFS
```

The source staircase is mono. Windows creates roughly a `0.93 dB` L/R fundamental split, while H3/H5 remain nearly identical in dBc in L, R and mid. Therefore the large odd-harmonic signature is not a stereo/HRTF artifact.

## 8. AudioEng limiter — relevant ceiling, not harmonic generator

The exact `CAudioLimiter` behavior was replayed.

It correctly clamps the reconstructed stream to the Windows `0.985` / `~ -0.13 dBFS` ceiling.

However, adding 0–8 dB extra drive once the limiter is active does **not** create the missing H3/H5; the linked limiter simply reduces gain and preserves the incoming waveform shape.

Therefore:

```text
AudioEng limiter = real final ceiling
AudioEng limiter ≠ source of May's -34 dBc H3
```

The missing odd harmonics must already exist upstream.

## 9. June warm-state hypotheses — further eliminated

The exact June VR object was re-extracted from the full minidump at its original heap geometry. Known long-memory scalar:

```text
outer + 0x1F1768 = 0.814902425
```

A controlled experiment fed the **same VLLDP output from the real May stimulus** into:

1. fresh Music VR;
2. captured June VR core;
3. full captured June VR state.

Result: authentic June warm VR state makes the loud 75-Hz steps about `0.23–0.33 dB` quieter and leaves H3 around the weak reconstructed regime (~`-63 dBc`), i.e. it moves in the wrong direction.

So the missing May loud-step nonlinearity is **not** recovered by transplanting authentic June VR history.

The equivalent VLLDP captured-state replay was already known to converge to fresh Music after a few blocks; its residual stable mismatch is a disabled, bit-transparent sliding-bass block.

## 10. Runtime-only VR / spatial controls — closed for this residual

Original VR public controls were systematically compared against what production initializes.

Ordinary omitted families such as audio optimizer, bass enhancer/extraction, calibration/pregain and volume modeler are disabled/zero on REV_0D and were previously proven bit-inert when explicitly forced.

`spatial-enable` was traced in the original wrapper. It only affects shared-memory/channel-expansion logic when output channel count exceeds input channel count. For normal SP11 `2 → 2` stereo it does not change DAP core parameters or sample-domain transfer.

`speaker-virtualizer-enable` is separately forced off/bypassed by the proven stereo policy.

Neither explains the 75-Hz residual.

## 11. NcpVolumeManager — not hidden DSP

`DAX3API.exe` `NcpVolumeManager` was decompiled.

Its callbacks track:

```text
m_currentAmount
m_masterVolumeScalar
m_maxVolume
m_minVolume
```

but the actual action is to compute and set a **Windows endpoint volume scalar** through the endpoint-volume interface. It does not process PCM and does not write hidden VLLDP/VR DSP parameters.

SP11 operator settings also have:

```text
compensate_volume = false
override_device_volume = false
```

Do not treat NCP as the missing nonlinear audio stage.

## 12. VLLDP endpoint-volume feedback — major runtime dimension

DAX volume feedback is real. Endpoint dB is converted to fixed-point:

```text
postgain = round(master_volume_dB * 16)
```

A preserved June 20% state corresponds to:

```text
postgain = -385
≈ -24.06 dB endpoint level
```

VLLDP live state stores/scales this feedback (including `DSP +0x65C = postgain / 2080`).

Production currently initializes the VLLDP bridge with postgain 0; May's actual master volume was never captured.

New controlled replay result:

- VLLDP `postgain=-385` materially changes the 75-Hz transfer.
- Movie + `postgain=-385` matches the **seven May fundamental levels to about 0.15 dB RMS overall**.
- VR `postgain=-385` alone is bit-identical to VR postgain 0 for this stereo Movie path.
- setting both VLLDP and VR postgain is identical to VLLDP-only.

So the amplitude-transfer match comes from VLLDP endpoint-volume feedback.

This is important, but it does **not** solve the harmonic residual: H3/H5 remain much weaker than Windows.

Construction sequencing was tested too:

```text
construct with -385
vs
construct at 0 → scheduler init → runtime set -385 → apply
```

The complete outputs are **bit-identical**. There is no hidden “postgain applied at wrong lifecycle time” gap.

## 13. VLLDP system gain — strong diagnostic, unsupported as SP11 runtime state

DAX has a separate `VlldpSystemGain` getter. Exact decompile shows it reads decimal `value` from a named config/device-info node. If the node is absent it returns:

```text
0
```

No packaged SP11 XML/JSON or recovered live state currently provides a nonzero value. Preserved live VLLDP state matches:

```text
system-gain = 0
```

Nevertheless it is diagnostically powerful.

Positive VLLDP system gain causes odd saturation **inside VLLDP before VR**:

```text
system gain +90...+240
VLLDP H3 ≈ -36...-37 dBc
VLLDP H5 ≈ -45...-46 dBc
```

This is close to May Windows.

However it raises quieter staircase levels too early. No postgain/system-gain pair simultaneously matches the full seven-step transfer and the harmonic onset.

Compensating the source by exactly the system-gain amount restores the entire waveform, including H3/H5, to baseline. Therefore system gain acts as ordinary gain into VLLDP's nonlinearity, not as separate speaker-stress metadata.

Do not deploy a fitted nonzero system-gain value.

A purely diagnostic combination:

```text
VLLDP system gain +105
VR VolMax 32
```

gets roughly `0.74 dB` RMS fundamental error and loud harmonics within a few dB, but neither value pair is OEM/runtime evidence. It only describes the kind of gain staging that would reproduce May.

## 14. Where baseline reconstructed nonlinearity comes from

Ablating VR controls in the Movie/postgain experiment shows:

- Leveler OFF: strong baseline loud-step nonlinearity disappears.
- VolMax 0: strong baseline loud-step nonlinearity disappears.
- regulator OFF, IEQ OFF, dialog OFF: little/no effect on the relevant odd saturation.
- Leveler amount 0: essentially unchanged in this condition.
- DRC OFF: modest effect on the last step.

So the reconstructed chain's ordinary weak odd harmonics are mainly a **VR Leveler × Volume-Maximizer interaction**.

But the stronger May-like nonlinear family can also be created **inside VLLDP** when VLLDP is driven/configured into its nonlinear branch. This distinction is important.

Static VolMax sweeps up to API max 192 do not reproduce May while preserving the transfer curve, and simple pre-VR scalar gain up to 10 dB plus VolMax compensation also fails.

## 15. VLLDP regulator/static tuning audit — no missing XML field

Production matches the exact REV_0D VLLDP/device tuning for the relevant controls:

```text
target power              -80
peak level                  0
stress                216,216,0,0,0,0,0,0
distortion slope           14
overdrive                   0
timbre                      12
speaker-distortion enable   1    (VLLDP/device layer)
system gain                 0
```

Do not confuse this with the separate CP/VR regulator, where `speaker-distortion-enable=0`.

Live-state byte matching previously proved the VLLDP regulator high/low thresholds, isolated flags, stress table, audio-optimizer region and scalar controls match real Windows state.

## 16. New highest-value lead: VLLDP `peak-level`

This is the strongest new result from the end of the chat.

Production/original tuning normally sets:

```text
vlldp-peak-level = 0
```

The original setter is:

```text
DolbyAPOvlldp150.dll
FUN_18001D100
```

Exact behavior:

```c
requested value is clamped to [-48, 0]
store at DSP/core +0xDD4
set dirty flag at +0x66C
```

The installed exact VLLDP binary checked during this work had:

```text
SHA-256 a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1
```

Offline sensitivity is highly selective:

- most stress/slope/timbre/target-power perturbations are inert for this 75-Hz staircase;
- `peak-level` negative values leave the first six steps essentially unchanged;
- sufficiently negative values trigger a nonlinear regime on the final `-3 dBFS` step.

Representative result:

```text
peak-level <= about -40

final -3 dBFS step:
H3 ≈ -32.74 dBc
H5 ≈ -42.41 dBc
```

May Windows:

```text
H3 ≈ -34.24 dBc
H5 ≈ -42.35 dBc
```

The H5 match is extremely close.

All requests at or below `-48` collapse to the same state because the original setter clamps at `-48`. Making it “more negative” therefore cannot move the onset to the `-6 dBFS` step.

Critical constraint: exact REV_0D XML and preserved June live VLLDP state both say:

```text
peak-level = 0
```

Therefore **do not change production to -48**. The experiment proves that this parameter activates exactly the kind of nonlinear branch we are missing, but it does not prove Windows used a negative value in May.

## 17. Exact resume point for the new chat

Resume here first.

### Priority A — trace `peak-level` through the VLLDP apply path

Use the exact VLLDP binary/project and follow:

```text
setter: FUN_18001D100
staged field: core/DSP +0xDD4
dirty flag: +0x66C
apply routine: FUN_18001D280
```

Determine exactly:

1. every read of `+0xDD4`;
2. how `FUN_18001D280` transforms/applies it;
3. the nested object/field it ultimately controls;
4. whether it changes a limiter threshold, compressor peak reference, level detector, or another derived coefficient;
5. why negative values selectively wake the final-step odd-saturation branch.

Use Ghidra temp projects under `/tmp`.

### Priority B — prove whether Windows ever writes nonzero `vlldp-peak-level`

Trace the DAX public/internal parameter mapping for:

```text
vlldp-peak-level
```

Search:

- DAX3API parameter-map construction;
- runtime update paths;
- ETL/provider payloads;
- June/July DAX process dumps;
- any live VLLDP state snapshots;
- archived logs that may preserve May-18 runtime values.

If every real Windows source remains zero, treat the peak-level experiment only as mechanism localization and move to the next runtime input that changes the same nested branch.

### Priority C — keep endpoint postgain in all May oracle work

May master volume is unknown. Do not compare May against a hard-coded VLLDP postgain 0 and call the amplitude curve a DSP mismatch.

The current strongest amplitude candidate is Movie with realistic negative VLLDP postgain, especially the June-observed `-385` sensitivity point, but profile and volume provenance for May remain unproven.

### Priority D — do not reopen closed branches

Do not spend new cycles on:

- May hard mute;
- SurfaceAPO `+0x201998` protection marker;
- generic HRTF for matching stereo;
- VirtualSurroundApo as hidden DSP;
- ASAR matching-stereo convolution;
- DAX3 equal-rate wrapper SRC/gain;
- warm June VR transplant as the missing May drive;
- VLLDP sliding bass;
- named Bass Enhancer / Bass Extraction / Volume Modeler;
- nonzero VlldpSystemGain without source evidence;
- simple AudioEng limiter clipping as the H3 generator;
- reversing VLLDP→VR order.

## 18. Other still-open items after the peak-level investigation

1. Exact May-18 active profile remains unlabeled. Waveform scoring makes Movie/Dynamic families useful candidates, but provenance is not proof.
2. May master volume is unknown.
3. `mb_compressor_limiter_gain` / public `0x850` backend semantics/readback linkage is not fully closed.
4. A purpose-built future Windows same-stimulus capture with profile, DAX state, Spatial state and endpoint volume recorded simultaneously remains the cleanest final certification oracle.
5. Lower Qualcomm/AudioReach protection/calibration should only be revisited if it can affect WASAPI loopback or feed a control decision upstream; a purely downstream speaker mute cannot explain loopback PCM.
6. Exact historical Custom1 GEQ remains unrecovered unless old artifacts resurface.

## 19. Useful tools / paths

Ghidra headless:

```text
/home/geoca/Desktop/Ghidra/support/analyzeHeadless
```

Tracked Ghidra scripts:

```text
tools/ghidra/
```

Live-VR diagnostics:

```text
tools/diagnostics/live-vr-state/
```

ETL environment:

```text
/home/geoca/Documents/SP11-PROJECT/.venv-etl
```

Recovered research corpus:

```text
/home/geoca/Documents/SP11-PROJECT/00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/Research_Hub_Audio
```

KD archive:

```text
/home/geoca/Documents/SP11-PROJECT/00-RE-archive/kdnet/2026-08-04-chatgpt/
```

## 20. First safety/status command in the new chat

Before editing:

```bash
cd /home/geoca/Documents/SP11-PROJECT/04-dolby-re-work
git status --short --branch
git rev-parse HEAD
git ls-remote origin refs/heads/agent/dolby-completion-2026-08-05
findmnt /dev/nvme0n1p3 || true
systemctl --user show filter-chain.service -p ActiveState -p MainPID -p NRestarts -p Environment
```

Expected principle, not necessarily exact service state:

- isolated worktree/branch is the only checkout to modify;
- local and remote branch should be synchronized after this handoff commit;
- Windows partition remains unmounted;
- do not restart/change the live audio service merely because the previous chat found it inactive.

## 21. Engineering standard for continuation

Keep three labels separate:

- **proven:** direct original-code, runtime, byte/state, ETL/KD or reproducible replay evidence;
- **candidate:** waveform-equivalent or sensitivity result without provenance;
- **open:** not enough evidence.

Do not convert fitted parameter combinations into OEM settings. Do not add hand-written “better sounding” DSP and call it parity. The current Linux boundary should remain original proven VLLDP→VR code until a missing stage/control is source-proven.
