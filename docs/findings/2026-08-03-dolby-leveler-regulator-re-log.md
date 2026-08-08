# Dolby leveler / regulator — reverse engineering log

Working document. This is the chain the SP11 **actually runs**; the bass
chain is disabled on this device in all ten profiles.

Binary: `DolbyAudioProcessing.dll`, ARM64 PE, image base `0x180000000`,
`.text` VMA `0x180001000` at file offset `0x400`.
sha256 `900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3`

All addresses and constants below are read from the binary with `objdump`
and by seeking to the exact file offset. Nothing here is inferred from
behaviour or fitted to a measurement.

---

## Why this file exists

An earlier attempt in this session wrote a leveler and regulator from scratch
and tuned their parameters until the output approached the measured Windows
transfer curve. That is an approximation, not a port, and it was the wrong
approach. This log records the actual implementation instead.

---

## Setter entry points (from the existing RE journal, addresses verified)

```text
dap_vr_volume_leveler_enable_set          FUN_1800451a0
dap_vr_volume_leveler_amount_set
dap_vr_volume_leveler_drc_enable_set
dap_vr_volume_leveler_in_target_set       vtable slot 0x1803148f0
dap_vr_regulator_enable_set
dap_vr_regulator_overdrive_set
dap_vr_regulator_timbre_preservation_set
dap_vr_regulator_relaxation_amount_set
dap_vr_regulator_tuning_set               nb_bands, p_band_centers,
                                          p_low_thresholds, p_high_thresholds
dap_vr_regulator_speaker_distortion_enable_set  FUN_180047910
dap_vr_ieq_enable_set                     FUN_180045050
dap_vr_ieq_amount_set
dap_vr_ieq_bands_set
```

---

## `FUN_1800451a0` — `dap_vr_volume_leveler_enable_set` — RESOLVED

```asm
ldr  w8, [x19, #1988]        ; current enable state
cmp  w8, w20
b.eq skip                    ; no change, no dirty flag
mov  w8, #0x1
str  w20, [x19, #1988]       ; store new enable
str  w8,  [x19, #5040]       ; set dirty flag
skip:
add  x0, x19, #0x38          ; &instance.leveler_block
bl   0x180051638             ; commit
```

### Instance offsets established

| Offset | Meaning |
|---|---|
| `+0x38` | leveler parameter block base |
| `+1988` | `volume_leveler_enable` |
| `+5040` | dirty / recompute flag |

`0x180051638` ends in `blr x8`, i.e. a **vtable dispatch** — the commit path
is virtual, so the concrete processor is selected at init.

---

## `0x180051658` — leveler state init — RESOLVED

```asm
ldr  s18, 0x180051878        ; 0.09230769426  = 6/65
ldr  s16, 0x18005187c        ; -0.00390625    = -1/256
mov  x8, #0xc0000000c0       ; 192 | (192 << 32)
str  wzr, [x19, #204]        ; clear
stp  xzr, x8, [x19]          ; [x19+0]=0, [x19+8]=192, [x19+12]=192
mov  x9, #0xc8               ; 200
movk x9, #0xc8, lsl #32      ; 200 | (200 << 32)
ldr  s0, 0x180051880         ; 200.0
str  x1, [x19, #32]          ; caller-supplied pointer
```

### Constants read at their exact addresses

```text
0x180051878 = 0.09230769426    = 6/65      smoothing coefficient
0x18005187c = -0.00390625      = -1/256    step size
0x180051880 = 200.0                        time constant, ms
0x180051884 = 0.015625         = 1/64      fixed-point scale
0x180051888 = 0.002403846243   = 1/416
0x18005188c = 64.0                         band / block count
```

`-1/256` and `1/64` are the fixed-point scalings the XML units imply: the
tuning file stores levels in 1/16 dB, and these convert into the internal
representation. `192` appears twice as an initial state value, and `200`
twice as a paired time constant, consistent with a two-channel or
two-timeconstant (attack/release) structure.

---

## `0x1800518a0` — leveler parameter commit — RESOLVED

Called from the process path at `0x180048694` with the leveler block
(`instance + 0xdc8`).

```asm
ldr  w8, [x19, #204]         ; dirty flag
cbz  w8, skip_recompute
bl   0x180051950             ; <-- recompute coefficients
ldr  w8, [x19, #8]  ; str w8, [x19, #12]     ; pending -> active
ldr  w8, [x19, #16] ; str w8, [x19, #20]
ldr  w8, [x19, #24] ; str w8, [x19, #28]
skip_recompute:
ldp  w10, w8, [x19]          ; [0]=new count, [4]=old count
cmp  w8, w10 ; b.ne changed
...
str  wzr, [x19, #204]        ; clear dirty
; then per band: copy [x19 + 44 + 4*i] -> [x19 + 124 + 4*i]
```

So `+44` is the pending per-band gain array and `+124` the active one, with
`+40` holding the band count. When the count is zero the active array is
zero-filled instead.

### Leveler block layout (offsets from `instance + 0xdc8`)

| Offset | Meaning |
|---|---|
| `+0` | band count, pending |
| `+4` | band count, active |
| `+8`, `+16`, `+24` | pending parameters |
| `+12`, `+20`, `+28` | active copies of the above |
| `+32` | pointer to band centre-frequency array (int) |
| `+40` | number of bands |
| `+44 + 4i` | pending per-band gain |
| `+124 + 4i` | active per-band gain |
| `+204` | dirty flag |

---

## `0x180051950` — leveler coefficient recomputation — RESOLVED

This is the function that turns the XML parameters into per-band gains. It is
the thing an approximation would have to guess at.

### Scalar setup

```asm
ldr  w8, [x19, #24] ; scvtf s17, w8      ; param C -> float
fmul s19, s17, s16                       ; * 6/65
ldr  s16, 0x180051b14 ; fmul s16, s19, s16   ; * -1/256
ldr  w8, [x19, #16] ; scvtf s0, w8       ; param B -> float
fcvt d18, s16
ldr  s16, 0x180051b18 ; fmul s16, s17, s16   ; C * 0.01562452316
fcvt d16, s16
fdiv d16, d18, d16                       ; ratio
fcvt s8, d16
ldr  s16, 0x180051b1c ; fmul s9, s19, s16    ; * 1/64
bl   0x180067620                         ; log2(B)
fmul s16, s0, s8
fadd s8, s16, s9                         ; slope = log2(B)*ratio + offset
```

### Per-band loop

```asm
ldr  x8, [x19, #32]                      ; band centre array
ldr  w8, [x8, w20, uxtw #2]              ; centre[i]
scvtf s0, w8
bl   0x180067620                         ; log2(centre[i])
ldr  s16, [sp, #16]
fmul s16, s0, s16                        ; log2(f) * scale
fsub s18, s8, s16
fcmp s16, s8
fadd s19, s16, s9
fcsel s17, s16, s8, gt                   ; clamp at the knee
fneg s16, s18
```

Then a degree-4 polynomial in the clamped value, using the constants below.

### Constants, read at their exact addresses

```text
0x180051878 = 0.09230769426    = 6/65
0x18005187c = -0.00390625      = -1/256
0x180051880 = 200.0                       time constant, ms
0x180051884 = 0.015625         = 1/64
0x180051888 = 0.002403846243   = 1/416
0x18005188c = 64.0                        band count

0x180051b14 = -0.00390625      = -1/256
0x180051b18 = 0.01562452316               ~1/64, deliberately not exact
0x180051b1c = 0.015625         = 1/64
0x180051b20 = 0.002403846243   = 1/416
0x180051b24 = 64.0
0x180051b28 = 0.6351512671     ]
0x180051b2c = 0.2364733815     ]  degree-4 polynomial
0x180051b30 = 0.03075440228    ]  coefficients
0x180051b34 = 0.001447245595   ]
0x180051b38 = -8.452257855e-14            residual / guard term
```

The four polynomial coefficients are the leveler's transfer curve. Note
`0x180051b18` is `0.01562452316`, very slightly below `1/64` — that asymmetry
is deliberate and would never be recovered by fitting.

`0x180067620` is a fast `log2` built on `frexp` (`bl 0x1800a4468`, confirmed
by its exponent masking against `0x7ff0000000000000` and `errno = 22` on a
null pointer).

---

## Component map — master init `0x180046cc0`

Every DSP block lives at a fixed offset in one instance:

```text
+0x7b4   -> 0x180044e30
+0xa88   -> 0x180049698      process: 0x1800496d8
+0xb20   -> ...              process: 0x180056030
+0xdc8   -> 0x180051658      LEVELER      process: 0x1800518a0
+0xe98   -> 0x180052850
+0xef8   -> 0x180051b38      process: 0x180051cf0 (takes +0x1308 as arg)
+0x1308  -> 0x180051ea0
+0x1898  -> 0x180055268
```

---

## Dead ends ruled out

* `blr x8` at `0x180051638` resolves via `adrp 0x1800e9000 + 1040` to
  `0x1800e9410`, which is an **import address table** slot pointing at a
  KERNEL32 name string. Not the leveler.
* `0x18000fb1c` is parameter **serialisation**: `0x180031ce8` called on
  consecutive 8-byte offsets `0xdc8`, `0xdd0`, `0xdd8`.

---

## `FUN_180097228` — final envelope limiter — RESOLVED

The last stage of the VLLDP chain, and the one that produces the measured
`-0.13 dBFS` output ceiling. It is a **look-ahead peak limiter with spectral
weighting**, not a simple clipper.

### Signature

```c
uint FUN_180097228(float threshold,      /* param_1: linear ceiling      */
                   uint  *state,         /* param_2                      */
                   longlong coeffs,      /* param_3                      */
                   longlong *audio,      /* param_4: channel buffers     */
                   longlong chan_data,   /* param_5                      */
                   uint  nframes,        /* param_6                      */
                   uint  nchan,          /* param_7                      */
                   longlong scratch);    /* param_8                      */
```

### Coefficient block at `param_3`

| Offset | Meaning |
|---|---|
| `+0x04` | attack coefficient (peak rising) |
| `+0x08` | release coefficient (peak falling) |
| `+0x0c` | slow-release coefficient, second envelope |
| `+0x10` | pointer to gain-interpolation ramp (4, 8 or 16 taps by mode) |
| `+0x18` | pointer to 16 spectral weights |

### State block at `param_2`

| Offset | Meaning |
|---|---|
| `+0x06` | band peak history pointer (15 floats copied in/out) |
| `+0x08` | reset flag |
| `+0x09` | fast envelope, persisted across blocks |
| `+0x0a` | slow envelope, persisted across blocks |
| `+0x0c` | second history pointer |
| `+0x0f` | last computed gain |
| `+0x10` | gain-ramp continuity buffer |

### Algorithm

1. **Per-band peak detection.** `|x|` reduced with NEON `fmax`/`fmaxp` pairs
   over 64-sample groups. Three code paths selected by `state[0]` (0, 1, or
   other) giving 64, 128 or 256 samples per band — that is the mode-dependent
   band resolution.

2. **Cross-band reduction** to one peak per sub-block, again by `fmax`.

3. **Dual-envelope smoothing**, the core of the limiter:

   ```c
   coef = (peak > fast) ? attack : release;   /* [+4] : [+8] */
   fast = peak + (fast - peak) * coef;
   slow = peak + (slow - peak) * slow_release; /* [+0xc] */
   out  = max(peak, fast, slow);
   ```

   Both envelopes are persisted in `state[9]` and `state[10]`, and flushed to
   zero when their magnitude falls below `DAT_180098288`.

4. **Spectral weighting.** A 16-tap dot product of the band peaks against the
   weights at `[+0x18]`, fully unrolled. This is what makes the limiter
   frequency-aware rather than broadband.

5. **Limiter gain**, the operative line:

   ```c
   gain = (weighted_peak <= threshold) ? 1.0f : threshold / weighted_peak;
   ```

   Unity below the ceiling, exact reciprocal above. `threshold` is `param_1`,
   which the caller supplies as the linear form of `-0.13 dBFS`.

6. **Gain interpolation.** Rather than stepping gain per sub-block, the gain
   is ramped between successive values using the coefficients at `[+0x10]`:

   ```c
   d = g[n] - g[n-1];
   out[k] = g[n-1] + d * ramp[k];
   ```

   4, 8 or 16 ramp taps by mode. This is what avoids zipper noise.

7. **Application.** The interpolated per-sample gain multiplies the audio in
   place, and the tail of the ramp is carried into `state[0x10]` so the next
   block starts continuous.

### Why this matters for the port

The measured Windows curve limits at `-0.13 dBFS`, and this is the function
that does it. The current `sp11_dolby_chain.c` uses a hard clamp at that
level, which matches the ceiling but not the behaviour: it has no envelope,
no spectral weighting and no ramp, so it distorts where Dolby would smoothly
reduce gain.

---

## Still to decode

| Item | Status |
|---|---|
| leveler process function (behind `blr x8`) | vtable slot not yet resolved |
| regulator process function | not started |
| `dap_vr_regulator_tuning_set` band tables | signature known, body not read |
| PEQ / IEQ apply functions | not started |

The vtable dispatch means the process function has to be found either by
locating the vtable that `x8` is loaded from, or by searching for functions
that reference the leveler block offset `+0x38` and the constants above.

---

## Parameters for this device (from the tuning XML, `dynamic` profile)

Recorded here so the decoded implementation can be driven with real values
rather than fitted ones.

```text
volume-leveler-enable          1
volume-leveler-amount          5
volume-leveler-in-target       -320     (1/16 dB => -20.0 dBFS)
volume-leveler-out-target      -320
volume-leveler-drc-enable      1
regulator-enable               1
regulator-overdrive            0
regulator-timbre-preservation  12
regulator-relaxation-amount    96
regulator-distortion-slope     14
regulator-stress-amount        216,216,0,0,0,0,0,0
regulator-speaker-dist-enable  0
speaker-peq-enable             1
ieq-enable                     1,  ieq-amount 10
dialog-enhancer-enable         1,  amount 5
pregain / postgain / system-gain / calibration-boost   all 0
```

Speaker PEQ curves, 20 bands, 1/16 dB:

```text
ch_00 = -16,18,16,30,16,-32,-16,-32,-16,-32,-48,-62,-64,-64,-16,-16,-16,16,80,48
ch_01 =   0,32,32,45,16,  0,-16,-16,-16,  0,-32,-38,-48,-48,  0,  0,  0,32,96,64
```
