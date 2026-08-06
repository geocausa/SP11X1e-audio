# Dolby native-chain progress checkpoint — 2026-08-04

> **SUPERSEDED ARCHITECTURE (2026-08-05):** this checkpoint was valuable intermediate RE, but its `DAPVR -> VLLDP -> limiter` target was superseded by Aug-4 hardware evidence proving the persistent tested order `VLLDP -> VR`. Preserve measurements/disassembly; use `docs/audit/2026-08-05-CANONICAL-DOLBY-PIPELINE.md` for current topology.

This checkpoint continues from `2026-08-04-DOLBY-PORT-STATE-OF-PLAY.md` at commit
`e0a5a7b` and records the new reverse-engineering work done after that handoff.

## Executive result

The current `sp11_dolby_chain.c` is **not** an exact end-to-end Dolby port yet.
Its limiter is exact, and the leveler/regulator parameter math is decoded, but
its per-sample leveler/regulator application is still provisional.

The real Dolby path is now much better constrained:

```text
DAPVR dynamic processing (volume leveler / regulator)
        -> VLLDP speaker optimisation / protection
        -> final output / limiter path
```

The missing Windows loudness is therefore upstream of the already executable
VLLDP core. The immediate next target is the modern DAPVR PCM processing path.

---

## 1. Re-measurement of the current assembled LADSPA chain

Measured in real 1024-frame host-style blocks with fresh state.

Windows targets:

```text
1 kHz @ -12 dBFS    +8.01 dB
75 Hz @ -30 dBFS   +16.82 dB
75 Hz @ -12 dBFS   +10.25 dB
limiter ceiling      -0.13 dBFS
```

Current `sp11_dolby_chain.c` result:

```text
1 kHz @ -12 dBFS    -0.56 dB     delta -8.57 dB
75 Hz @ -30 dBFS   +10.61 dB     delta -6.21 dB
75 Hz @ -12 dBFS    +0.14 dB     delta -10.11 dB
```

Conclusion: the limiter is not the remaining problem. The provisional sample
path is missing roughly 6–10 dB of the real dynamic behaviour.

---

## 2. Remaining modern VLLDP functions decompiled

The four functions listed in the earlier handoff were decompiled directly from
the exact `DolbyAudioProcessing.dll` whose SHA-256 is:

```text
900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
```

Addresses:

```text
FUN_180095460   per-channel main dynamic processing
FUN_1800986a8   per-channel synthesis/apply
FUN_180097178   pre-processing
FUN_180098290   per-channel analysis
```

Important correction to the old mental model: these are not simple broadband
sample gain helpers. They operate on the VLLDP 20-band analysis/synthesis
representation with state, exponent alignment, ring buffers, and transform
state.

`FUN_180095460` produces a 20-band dynamic correction vector. The modern
orchestrator adds that vector into the analysis/synthesis path before the exact
dB->linear converter.

---

## 3. Old VLLDP native bridge rechecked

The repository already contains a native ARM64 bridge using
`DolbyAPOvlldp150.dll` (`sp11_vlldp_exact.c`). It is useful as an executable
oracle but its older comments overstate what was proven.

### ABI note

The odd `LevelerFn` declaration in the bridge initially looked wrong because
Ghidra shows three leading float arguments. It is actually an intentional
AArch64 ABI trick: FP and integer arguments occupy separate register banks, so
the declaration still places the floats in `s0..s2` and integer/pointer
arguments in `x0..x7` as expected. Do not "fix" the typedef merely from source
appearance.

### Exact scalar mapping recovered

For the old orchestrator call to `FUN_180021e80`, the captured Windows scalar
values were:

```text
scalar9  = 0
scalar10 = -5.5208571e-09
scalar11 = -0.0307692308
```

**Aug-6 provenance correction:** the scalar10 value was numerically correct but
the source-field label `child1+0xC60` was not. Exact current-DLL process code
loads `core+0xC5C` (normally literal `1.0` returned by the final limiter),
converts it through `FUN_1800247C0`, multiplies by `0.046312306...`, passes that
derived value into `FUN_180021E80`, and only afterward writes the same derived
value to `core+0xC60` as readback. `C60` is therefore not the scalar's source or
a free runtime drive control. See
`2026-08-06-MAY-RUNTIME-STATE-AND-VLLDP-TELEMETRY-CLOSURE.md`.

The live bridge had been passing `runtime[1] = -0.262019...` as scalar10.
That is not the same value/path.

A temporary instrumentation patch is preserved in:

```text
docs/findings/2026-08-04-vlldp-exact-scalar-instrumentation.patch
```

It is an experiment only and is intentionally **not applied** to the working
source.

### Result of corrected scalar experiment

With the correct scalar10 ~= 0 and scalar11 = -0.0307692308, the previously
observed runaway Phase-6 row mutation disappears completely. That agrees with
the June native mutability capture.

The exported old leveler `b60` vector remains active and adapts by tone/level,
but the putative per-channel correction scratch is zero in this old profile.
Therefore the old DLL does not directly provide the missing modern positive
loudness correction for this SP11 dynamic profile.

Using the mutated descriptor row for synthesis was also tested and rejected:
it produced degenerate/clipped behaviour (fundamental roughly 24–32 dB low
while peaks reached 0 dBFS). Do not use that path.

---

## 4. Phase-6 field mapping corrected

Fresh disassembly corrected an old June annotation.

The VLLDP Phase-6 synthesis base is:

```text
B[ch][band] = active_band[ch][band]
            + global offset
            + per-channel offset
```

Correct state fields:

```text
global offset       state+0x90
staged global       state+0x94
channel offset      state+0x5c0 + 4*channel
staged channel      state+0x5a0 .. +0x5bc
```

The old note that treated `+0xb80/+0xb88` as per-channel offsets was wrong;
that region belongs to leveler feedback/export state.

Both offset setters use the exact `/2080` scale. `vlldp-channel-gain` is
clamped to `-2080..0`, therefore it can only be unity/attenuation and cannot
explain the Windows positive loudness boost. `dap_vlldp_system_gain_set` is the
global counterpart.

The exact scale constant was byte-confirmed as:

```text
0x39fc0fc1 ~= 1/2080
```

---

## 5. Native VLLDP is now executable as a real DSP core

The ARM64 Windows VLLDP code can execute natively on this ARM64 Linux SP11 via
the existing PE loader. Constructor/reset/processing/destructor paths were
successfully exercised after shimming the small Windows runtime boundary
(synchronisation / stack-probe helpers). The DSP itself runs unchanged.

Configured from the real SP11 VLLDP tuning, it does **not** reproduce the
measured Windows loudness by itself. Representative measured transfer:

```text
1 kHz @ -12 dBFS    about -3.49 dB
75 Hz @ -30 dBFS   about -0.06 dB
75 Hz @ -12 dBFS   about -3.36 dB
```

Conclusion: VLLDP is downstream speaker optimisation/protection; it is not the
source of the +8 to +17 dB Windows loudness behaviour.

---

## 6. The real missing stage is DAPVR before VLLDP

Static tracing in modern `DolbyAudioProcessing.dll` found the actual DAPVR PCM
path. DAPVR processes the PCM before VLLDP.

Important processing addresses reached in the modern path:

```text
FUN_18004ea20
FUN_180061698
```

The DAPVR processor operates in 256-frame chunks on the PCM buffers.

The constructor/configuration path is now being mapped and multiple setters are
identified, including:

```text
FUN_1800451a0   volume leveler enable
FUN_180047910   speaker distortion regulator enable
FUN_1800479c0   regulator tuning/config commit
FUN_180047850   clamped regulator scalar, 0..192
FUN_1800478b0   clamped regulator scalar, 0..144
FUN_180047960   clamped regulator scalar, 0..16
FUN_180047800   boolean regulator-related control
```

The decoded setter family writes staged state around:

```text
0xf04 / 0xf10 / 0xf1c / 0xf28 / 0xf30 / 0xf38
```

and marks the parameter commit dirty via `+0x13b0`.

The SP11 `<tuning-cp>` dynamic-profile values available for this stage include:

```text
volume leveler: enabled
DRC: enabled
amount: 5
input/output target: -320 / -320
regulator: enabled
timbre: 12
relaxation: 96
slope: 14
stress: 216,216,0,0,0,0,0,0
```

These must be bound to the real setter fields rather than approximated.

---

## 7. Immediate next step

Do **not** tune `sp11_dolby_chain.c` to the Windows curve.

Continue by:

1. Finish the modern DAPVR constructor/setter map.
2. Instantiate the real DAPVR state on Linux.
3. Apply the exact SP11 `<tuning-cp>` dynamic-profile parameters.
4. Run PCM through real DAPVR in 256-frame blocks.
5. Feed that result into the already executable native VLLDP core.
6. Measure the same three Windows transfer targets.
7. Only after that decide which remaining native blocks need translation into
   a standalone clean-room C port.

This is now an execution/configuration problem, not a curve-fitting problem.

---

## 8. Safety / repository state

All work for this checkpoint was done in an isolated worktree based on commit
`e0a5a7b`. Unrelated Codex/user changes in the main checkout were not touched.

Temporary Ghidra analysis used a temporary project and can be regenerated from
the exact binary and known addresses. Durable conclusions are recorded here so
loss of `/tmp` does not lose the reasoning.

## Continuation checkpoint — native DABS DAPVR process now runs

Further reverse engineering established the exact low-level speaker-process ABI and removed the remaining native crash.

### DABS audio descriptor at parent+0x6f8

`CDolbyAudioProcessingModule::EnsureAideAndOarModules` constructs a normal 32-byte planar audio descriptor:

- `+0x00`: channel count from parent `+0x654`
- `+0x08`: stride = 1
- `+0x10`: format = 7 (float)
- `+0x18`: pointer-array address at parent `+0x710`
- each plane pointer is `parent+0x660 + frame_count * channel * 4`

This matches the C `AudioDesc` layout used by the native harness.

### Speaker process ABI correction

`FUN_18004e7b0` is a 20-byte struct-return function. Its visible arguments are:

```c
Result FUN_18004e7b0(void *core, AudioDesc *external_desc, void *scratch);
```

The third argument is the aligned scratch arena, **not another audio descriptor**. Raw AArch64 at `0x18004e828` passes that scratch pointer onward as x3 to `FUN_180060ce8`. Passing an output descriptor there was the cause of the previous `FUN_180063458` crash.

The descriptor direction is also now established:

- `FUN_180046fa0(..., param3, ...)` stores `param3` at `core+0x60` and that configured descriptor is the source side;
- `FUN_18004e7b0(core, external_desc, scratch)` uses the passed descriptor as the destination side.

Therefore the working neutral sequence is:

```text
configure(core, 1, input_descriptor, ...)
speaker_process(core, output_descriptor, scratch_arena)
```

`core+0x30` points to Dolby's 48 kHz format table whose first words are `48000, 48000, 256, 20`; specifically `+8 = 256`. Thus `core+0xc4 = (param2 << 8) / 256 = param2`, and `param2=1` activates one 256-frame DABS processing step.

### Native neutral gate passed

With stereo planar float, mode/default init, no runtime Dolby effects enabled:

- constructor succeeds;
- `FUN_180046fa0` returns `2` and sets `core+0xc4 = 1`;
- `FUN_18004e7b0` executes repeatedly without fault;
- after startup latency, a 0.1-amplitude 1 kHz sine exits at approximately `0.100057` peak (unity within ~0.005 dB).

This is the first clean execution of the modern DLL's DABS speaker DAPVR path on Linux. The next step is to apply the exact SP11 DAPVR leveler setters, measure them alone, then add regulator parameters and finally feed the output into the already-working native VLLDP stage.

## Continuation checkpoint — exact Windows stimulus replay and corrected parity target

### The old three-number shorthand was misleading

The Windows targets were not independent steady-state tone tests. They came from one continuous known-input WAV, so Dolby state carries through the full sequence.

Original stimulus:

```text
00-RE-archive/.../dolby/windows-loopback-captures/sp11-known-input-stimulus-48k.wav
```

Original Windows report:

```text
00-RE-archive/.../dolby/windows-loopback-captures/known-input/known-input-transfer-report.md
```

Original generator proves the 1 kHz reference is `amp=0.18`, approximately -14.89 dBFS peak / -17.9 dBFS RMS, **not -12 dBFS**. The old handoff label `1 kHz @ -12 -> +8.01 dB` must not be used as an exact stimulus description.

The exact stimulus sequence is:

```text
1.0 s silence
2.0 s 1 kHz, amp 0.18
0.5 s silence
8.0 s log sweep, 35 Hz -> 18 kHz, amp 0.18
0.5 s silence
55 Hz bursts, amp 0.50
90 Hz bursts, amp 0.45
140 Hz bursts, amp 0.35
0.5 s silence
75 Hz stepped levels: -30,-24,-18,-12,-9,-6,-3 dBFS, each 1 s + 0.25 s silence
1.0 s silence
```

The Windows analysis windows are also recovered exactly from `compare_known_input_output.py`:

```text
1 kHz                 1.0 .. 3.0 s
log sweep             3.5 .. 11.5 s
55 Hz bursts         12.0 .. 14.4 s
90 Hz bursts         14.4 .. 16.8 s
140 Hz bursts        16.8 .. 19.2 s
75 Hz stepped        19.7 .. 28.45 s
individual 75 levels: 1.0 s windows beginning at 19.7 s, spaced by 1.25 s
```

### Native persistent-state replay works

New harness:

```text
dolby-port/sp11_dolby_native_known_input.c
```

It processes the original 16-bit stereo stimulus through one persistent native modern-DLL DAPVR -> VLLDP state in 256-frame blocks and writes float output for comparison.

Current structurally grounded baseline includes:

- exact DAPVR leveler: enable=1, amount=5, DRC=1, in/out target=-320
- exact DAPVR profile regulator values
- DAPVR output mode 11 with the XML stereo mix matrix
- VLLDP audio optimizer with the exact two 20-band device rows
- VLLDP detailed regulator threshold/stress/slope tuning
- VLLDP speaker-distortion control disabled in this baseline because enabling it directly without the higher routing context over-attenuates severely
- no fitted gain or EQ

Native pipeline latency from correlation is about 689 samples = 14.35 ms.

### Stateful replay results

Using the same Windows report windows:

```text
segment                    native gain   Windows gain   delta
1 kHz reference              +13.65         +8.01       +5.64 dB
log sweep                    +13.71        +10.64       +3.07 dB
55 Hz bursts                  +5.91         +6.01       -0.10 dB
90 Hz bursts                  +7.19         +6.91       +0.28 dB
140 Hz bursts                 +9.19         +6.08       +3.11 dB
75 Hz stepped whole           +6.75         +5.51       +1.24 dB

75 Hz -30 dBFS              +17.42        +16.82       +0.60 dB
75 Hz -24 dBFS              +17.87        +14.76       +3.11 dB
75 Hz -18 dBFS              +14.53        +13.47       +1.06 dB
75 Hz -12 dBFS              +11.76        +10.25       +1.51 dB
75 Hz  -9 dBFS               +8.78         +7.48       +1.30 dB
75 Hz  -6 dBFS               +5.79         +4.51       +1.28 dB
```

> **2026-08-06 correction:** this row was an alignment artifact, not a dropout.
> The periodic 1-kHz aligner locked one second late.  Robust alignment gives
> `0.726333 s` for the first May run and proves that the true `-3 dBFS` tone is
> present at the ceiling with strong H3/H5.  See
> `2026-08-06-MAY18-KNOWN-INPUT-ALIGNMENT-CORRECTION.md`.

The stateful replay is much more informative than fresh isolated tones. In particular, the quiet 75 Hz point moves from a +2.9 dB steady-state error to only +0.60 dB when replayed in the original sequence.

### Native peak protection is already active

The native modern-DLL serial chain itself reaches about 0.9999 (-0.001 dBFS) on the original stimulus. Therefore the earlier simple statement that the -0.13 dBFS ceiling is only an external decoded limiter is incomplete: there is already strong peak protection in the modern native path before any separately ported limiter is appended. Exact ceiling provenance still needs separating.

### Exact DAPVR/IEQ/output-mode findings added during replay work

- volume-leveler DRC setter is `FUN_180045150` (string/callback verified)
- volume-leveler in-target is `FUN_1800492e0`
- volume-leveler out-target is `FUN_180049370`
- IEQ data preparer is `FUN_180044fa8(core,count,center_freqs,target_gains)`
- `ieq_balanced` exact targets are `157,167,218,218,203,188,192,192,205,213,218,209,193,159,134,97,71,22,-90,-283`
- output-mode setter is `FUN_180046dd0(core,processing_mode,nb_output_channels,mix_matrix)`
- XML `processing_mode=11` maps internally to mode 6 for stereo
- `volmax-boost=96` alone creates an exact -3.00 dB shift in the current low-level path; it therefore cannot be applied in isolation without the higher routing context that normally accompanies it
- surround-decoder enable and surround-boost=96 alone are neutral on the simple reference tones
- dialog enhancer amount 5 is neutral on the simple reference tones

### VLLDP setter ABI audit

Fresh decompilation confirms the current native harness calling conventions are correct:

```text
FUN_180090120(core, audio_optimizer_array, count)
FUN_180091988(core, threshold_high, threshold_low, count)
FUN_180091670(core, isolated_band_array, count)
FUN_180091890(core, stress_count, stress_array)
```

The scalar setters for timbre, slope, speaker-distortion, peak, target power, system gain, postgain and MB enable were also rechecked. The remaining transfer mismatch is not explained by a reversed array/count ABI.

### Current interpretation

The best evidence-backed baseline already matches the 55 Hz and 90 Hz burst gains within about 0.3 dB and the stateful 75 Hz -30 point within 0.6 dB, while it remains too loud around 140 Hz and 1 kHz. This pattern now points toward a missing frequency/routing stage rather than a missing broadband makeup gain.

Next exact task: compare the aligned native output directly against the captured Windows output over the log sweep, deriving the Windows/native transfer difference versus frequency as evidence. Use that to identify which decoded/routed stage is absent; do not fit a replacement EQ.

## Continuation checkpoint — profile provenance and residual sweep fingerprint

### Known-input Windows profile was not proven `dynamic`

The May 18 known-input capture was played by PowerShell `System.Media.SoundPlayer` (`Run-SP11KnownInputLoopback.ps1`), not Edge/browser playback.

Operator settings captured from the same Windows environment say:

```text
APP msedge.exe -> dynamic
internal_speaker + spatial_audio=on  -> movie
internal_speaker + spatial_audio=off -> music
```

Therefore the earlier assumption that the known-input reference used `dynamic` is not justified. `dynamic` is explicitly mapped to browser playback; ordinary SoundPlayer playback should fall back to `music` when spatial audio is off or `movie` when spatial audio is on.

Exact low-level subset replays were therefore run with both profiles (without blindly applying high-level volmax/virtualizer controls whose routing context is not yet reconstructed).

`movie` (leveler amount 0, output mode 11) still leaves +4.70 dB excess on the 1 kHz segment and +3.07 dB at 140 Hz, while matching 55/90 Hz within 0.3 dB.

`music` (leveler amount 0, output mode 1) still leaves +4.59 dB excess on the 1 kHz segment and +3.04 dB at 140 Hz, while matching 55/90 Hz within 0.3 dB. It improves several 75 Hz stepped levels.

So wrong profile selection was a real methodological error, but it does not by itself explain the midband residual.

### Direct Windows/native log-sweep residual

The native and Windows known-input outputs were aligned and compared in short windows over the original 35 Hz -> 18 kHz sweep. Approximate `Windows gain - native gain` residual for the current evidence-backed baseline:

```text
40 Hz      -1.5 dB
55 Hz      -1.8 dB
75 Hz      -2.5 dB
90 Hz      -2.8 dB
140 Hz     -2.5 dB
200 Hz     -1.9 dB
300 Hz     -3.5 dB
500 Hz     -3.8 dB
750 Hz     -4.5 dB
1 kHz      -5.3 dB
1.5 kHz    about -5 dB
~1.8 kHz   about -6.4 dB
2.5 kHz    -4.0 dB
4 kHz      -2.1 dB
6 kHz      about -0.4 dB
```

This is a frequency-shaped residual, not a missing broadband makeup gain.

Naively enabling VLLDP speaker-distortion regulation does not reproduce this transfer: it happens to approach Windows around 1–2 kHz but over-attenuates low bass by roughly 8–15 dB. Fresh verification proves the VLLDP regulator threshold argument order is correct (`high`, then `low`, then count), so this is not an H/L array swap.

### VLLDP constructor fourth argument corrected

The fourth argument of `FUN_1800907d8` is `max operations`, not a generic mode flag. The real module logs:

```text
Initializing VLLDP with max channel count %d and max operations %d
```

The SP11 DABS XML explicitly specifies:

```text
max_num_channels = 2
max_num_operations = 0
```

So the current native constructor tuple `(256, 48000, 2, 0, arena)` is correct for this device. The module's "must be set" checks concern property-presence flags, not requiring a nonzero max-operations value.

### Current highest-value hypothesis

The Windows loopback reference is the output of the full Windows endpoint render path, while the current native oracle explicitly runs only `DolbyAudioProcessing.dll` DAPVR + VLLDP. The project separately recovered Qualcomm/AudioReach endpoint processing (EQ, SAL_V2 B, SWR_SINK, MFC, etc.).

The remaining Windows/native residual may therefore belong partly or entirely to the non-Dolby endpoint DSP rather than to an undecoded Dolby stage.

Next exact task: compare the recovered Windows Qualcomm EQ/SAL transfer against the measured residual above before adding or fitting any new Dolby processing.

## Continuation checkpoint — hardware EQ ruled out; AIDE becomes primary missing stage

### Windows AudioReach Popless EQ is flat

The exact REV_0D ACDB body for EQ module `0x07001045`, param `0x0800110c`, was recovered with `tools/acdb_setcfg_inventory.py` from the captured SP11 ACDB.

Decoded with Qualcomm `popless_equalizer_api.h`:

```text
pregain Q27 = 1.0
preset = 18 (custom external)
num_bands = 5

band 0: BAND_BOOST 60 Hz,    gain 0 mdB, Q=100/256
band 1: BAND_BOOST 230 Hz,   gain 0 mdB, Q=100/256
band 2: BAND_BOOST 910 Hz,   gain 0 mdB, Q=100/256
band 3: BAND_BOOST 3600 Hz,  gain 0 mdB, Q=100/256
band 4: BAND_BOOST 14000 Hz, gain 0 mdB, Q=100/256
```

Therefore the Windows AudioReach Popless EQ is mathematically flat and cannot explain the Windows/native -5 to -6 dB midband residual.

`SurfaceAPO_0D.json` also shows the 48 kHz stereo `R/MFX/DEFAULT/defaultEQ` as `Enabled=false` with identity coefficients. The easy non-Dolby endpoint-EQ explanation is therefore ruled out.

### AIDE is part of the real Windows Dolby path and our direct oracle bypasses it

Older full-pipeline RE plus fresh Ghidra confirms `DefaultDAPModule::EncodeAudioData` contains an AIDE stage in addition to DAPVR/VLLDP. AIDE is the **Adaptive Intelligent Dynamic Equalizer** and its core is exactly the kind of frequency-shaped adaptive processing missing from the current direct DAPVR -> VLLDP oracle.

Freshly rechecked functions:

```text
FUN_180006910  AIDEModule::Initialize
FUN_180006e40  AIDEModule::SetParams
FUN_180007d90  per-channel vector/ring copy helper used by AIDE process
FUN_18003a438  main AIDE QMF/adaptive-EQ core
FUN_1800399d8  AIDE processing-context creator
FUN_18003bde8  AIDE adaptive/steering parameter parser
FUN_18003aab0  adaptive steering coefficients
```

Fresh `FUN_180006910` decompile shows only two visible arguments:

```c
uint32_t FUN_180006910(void *aide, uint32_t count_or_channels);
```

It allocates the AIDE internal buffers/context, creates per-channel pointer/vector arrays, sets `+0x28` initialized/enabled, and stores the second argument at `+0x38/+0x88`. Its semantic meaning must be recovered from the raw caller before use.

Fresh `FUN_180006e40` is the real AIDE SetParams parser. It accepts a compact TLV-like payload and feeds adaptive settings into `FUN_18003bde8`; do not hand-set AIDE context fields when the original SP11 parameter blob can be recovered.

Fresh `FUN_18003a438` confirms the DSP topology:

```text
per-channel QMF analysis
-> adaptive steering via FUN_18003aab0 when ctx+0x8ec enabled
-> IEQ gain / second-pass synthesis
-> optional DAP-VR integration
-> output copy
```

This is now the primary exact-port target. The current DAPVR -> VLLDP native harness is a useful partial oracle, not the complete Windows Dolby APO chain.

## Continuation checkpoint — ETW/kernel/profile correlation preserved

Historical June ETW/RPC captures and the July KDNET/kernel capture were
cross-correlated to resolve the "different app / notification pipe" question.
Full evidence and paths are preserved in:

```text
docs/findings/2026-08-04-DOLBY-ETW-KERNEL-CORRELATION.md
```

Key conclusions:

- `{9CF2A70B-F377-403B-BD6B-360863E0355C}` is definitively
  `AUDIO_SIGNALPROCESSINGMODE_NOTIFICATION`; older outputs that call it
  "unknown" are stale.
- System-sound ETW shows DEFAULT and NOTIFICATION as distinct Windows
  processing modes, but Dolby DAX3 wrapper instances appear in both. The safe
  interpretation is separate graph/configuration mode, **not** "notifications
  bypass Dolby entirely".
- Current DAX RPC mapping is `active_profile=5 -> Dynamic` and
  `active_profile=1 -> Music` for the captured build.
- The purpose-labelled Dynamic -> Music active-tone capture stayed in the same
  `audiodg.exe` PID with the same Dolby/VLLDP/Surface module base addresses and
  generated no `GRAPH_OPEN` or `SET_CFG`; that profile change is live in-place
  reconfiguration of the running graph.
- Music IEQ Off -> Detailed behaved differently: its QGPR trace contains graph
  stop/flush plus two endpoint `SET_CFG` operations. UI controls must not all be
  assumed to cross the same runtime boundary.
- The July kernel-dump session is a known Firefox + YouTube active-media anchor.
  The dump itself is kernel-only, but accompanying KDNET notes preserve an
  audiodg module inventory containing `DolbyAudioProcessing.dll`,
  `DolbyDax3Apo.dll`, `DolbyAPOvlldp150.dll`, `DolbyApoVr.dll`, and
  `DolbyHrtfEnc.dll` while media was actively rendering.

Working architecture is now explicitly split into Windows stream processing
mode / graph selection, DAX profile policy, Dolby DSP state/history, and the
endpoint/Qualcomm path. This prevents conflating Notification-vs-media routing
with Dynamic-vs-Music profile changes.

## Continuation checkpoint — live KDNET changes the hot-path priority

Live KDNET hardware-execution tracing on the Windows SP11 now proves that the
persistent DAX/VLLDP150 path, not the current low-level modern-DLL oracle, is
the continuously hot stereo/music path under the tested condition.

Full evidence, live addresses, module identities, breakpoint-method correction,
and next traps are preserved in:

```text
docs/findings/2026-08-04-DOLBY-LIVE-KDNET-HOT-PATH.md
```

Most important corrections:

- `DolbyDax3Apo` wrapper `APOProcess` is hardware-trap HOT.
- `DolbyAPOvlldp150` outer `FUN_180105000` is hardware-trap HOT.
- VLLDP150 top-level `FUN_18001f7a8` is hardware-trap HOT (~31 hits/s sample).
- Modern `FUN_18004e7b0` DAPVR speaker wrapper, embedded VLLDP
  `FUN_1800922f8`, and AIDE core `FUN_18003a438` were hardware-trap COLD during
  the same active stereo/music condition.
- The earlier `FUN_180061698` live control was the wrong DAPVR subpath for the
  DABS speaker route; the native speaker harness actually uses
  `FUN_18004e7b0 -> FUN_180060ce8`.
- Software user-mode breakpoints in this KD setup produced false-negative
  non-hits. Hardware execution breakpoints are required for hot-path claims.
- AIDE remains proven present in `DefaultDAPModule`/ASAR code, but the statement
  that it is part of the currently hot internal-speaker PCM path is no longer
  proven. Reclassify it as **present, live participation unresolved/cold in the
  tested stereo condition**.
- The modern DLL identifies itself as `msft-asar-dap`, "Dolby Audio Processing
  for Microsoft Spatial Audio", version 7.3.7.0. Treat it as an ASAR layer that
  cooperates with, rather than replaces, the persistent DAX/VLLDP150 stack.
- DAX UI/profile activity reached `Dax3DapControl!SetDapVariantParam`; the child
  DAX3API process is the one that also loads `CaptureStreamMonitor.dll`.

Immediate parity priority is therefore to reconstruct/call the live VLLDP150
`FUN_18001f7a8` orchestration with correct state/snapshot history, while the
modern ASAR/OAR/Crossfade path is mapped separately by high-level hardware
traps instead of assumed from static reachability.
