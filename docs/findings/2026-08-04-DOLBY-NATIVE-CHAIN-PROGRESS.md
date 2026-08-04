# Dolby native-chain progress checkpoint — 2026-08-04

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

For the old orchestrator call to `FUN_180021e80`, the captured Windows mapping
is:

```text
scalar9  = child1+0xddc = 0
scalar10 = child1+0xc60 = -5.5208571e-09   (live top scalar)
scalar11 = child1+0x0d0 = -0.0307692308
```

The live bridge had been passing `runtime[1] = -0.262019...` as scalar10.
That is not the same field.

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
