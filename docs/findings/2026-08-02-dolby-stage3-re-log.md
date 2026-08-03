# Dolby bass enhancer stage 3 — reverse engineering log

Working document. Updated as functions are resolved.

Binary: `DolbyAudioProcessing.dll`, ARM64 PE, image base `0x180000000`,
`.text` VMA `0x180001000` at file offset `0x400`.
sha256 `900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3`

Everything below is read from machine code with `objdump`, not from prior
notes. Where an earlier document disagrees, the machine code wins.

---

## Constants (verified by reading the file at the exact VA)

```text
0x180077208 = 0x34000000 = 1.192092896e-07   FLT_EPSILON, zero threshold
0x180075828 = 0x3f3504f3 = 0.7071067691      1/sqrt(2), rotation factor
0x180076000 = 0x43800000 = 256.0             FFT size
```

---

## `FUN_180075e80` — mode dispatcher — RESOLVED

Not the 4-way exclusive switch `bass_standalone.c` models. Every active path
ends in `FUN_180075688`:

```text
mode 1  -> 0x180075fb4 -> falls through to FUN_180075688
mode 2  -> 0x180075f58 -> FUN_180075830  THEN  FUN_180075688
mode 3  -> 0x180075f20 -> FUN_180075c70  THEN  FUN_180075688
else    -> 0x180075fe4 -> return, no processing
```

Also per block, unconditionally:

* calls `FUN_180077098` with the same bin range, **before** the mode path
* zeroes 2048 bytes at `x22 + 0x800` (= 512 floats = 256 complex bins), which
  is the accumulator `FUN_180075688` adds into
* loads per-mode gain from a runtime global: `ldr x8,[x19,#1872]` then
  `ldr s16,[x8]` / `[x8,#4]` / `[x8,#8]`, multiplied by a value from the
  instance at `[x24,#12/16/20]`

That the accumulator is zeroed per block, and that modes 2 and 3 run two
functions in sequence, is why `FUN_180075688` accumulates rather than
overwrites.

---

## `FUN_180077098` — spectral normalisation — RESOLVED

Called every block before the mode path. Normalises each bin to unit
magnitude, preserving phase and discarding magnitude.

```c
/* param_1: 3 selects stride 4, else stride 2
 * param_2: start bin (masked to & ~3)
 * param_3: end bin
 * param_4: spectrum, interleaved [re,im]
 * x0:      output, interleaved [re,im]
 */
for (bin = start & ~3; bin <= end; bin += stride) {
    re = spec[bin*2];
    im = spec[bin*2 + 1];

    /* threshold on max(|re|,|im|), computed via fneg/fcsel pairs */
    peak = max(fabsf(re), fabsf(im));

    if (peak <= 1.192092896e-07f) {
        out_re = 0.0f;
        out_im = 0.0f;
    } else {
        /* double precision, then narrowed back to float twice - the binary
         * really does fcvt d,s -> fdiv -> fcvt s,d -> fcvt d,s -> fcvt s,d */
        double m = (double)re*re + (double)im*im;
        double inv = 1.0 / m;
        float  r = sqrtf((float)inv);
        out_re = r * re;
        out_im = r * im;
    }
    out[bin*2] = out_re; out[bin*2+1] = out_im;
}
```

Second loop mirrors the negative-frequency half: index `256 - k` and
`513 - 2k`, confirming a 256-point real FFT independently of the size
constant.

Note `1/sqrt(re^2+im^2)` is computed as `sqrt(1/(re^2+im^2))`, and the
repeated float/double narrowing is preserved above because it is what the
binary does; it is not redundant if bit-matching is wanted.

---

## `FUN_180075688` — spectral rotation — RESOLVED

Signature and behaviour verified 2026-08-02, see
`2026-08-02-dolby-bass-stage3-fft-analysis.md`:

```c
void FUN_180075688(float gain,          /* master gain            */
                   longlong spec_in,    /* interleaved complex    */
                   longlong coeffs,     /* SEPARATE coeff array   */
                   uint start_x4,       /* start bin * 4          */
                   uint end_x4,         /* end bin * 4            */
                   uint *ramp_desc,     /* [0]=count [2]=float*   */
                   longlong accum_out); /* ACCUMULATES            */
```

* 45-degree rotation using `1/sqrt(2)`: `(re*rot - im*rot)`, `(re*rot + im*rot)`
* complex multiply against `coeffs[bin*8]`
* `*out = product * ramp + *out` — accumulation
* mirrored second loop at bin `0x100 - k`
* `ramp_desc` is a count plus a float pointer advancing one element per bin,
  not a per-bin table
* bin index clamped to 192 (`cmp w8,#0xc0; csel`)

---

## `FUN_180075830` — magnitude shaping / harmonic generation (mode 2) — RESOLVED

This is a **frequency doubler in the spectral domain**, not a gain table.

### Bin mapping is 2:3, not 1:1

```asm
add  w8, w2, w2, lsl #1     ; n*3
lsr  w8, w8, #3             ; (n*3)>>3
cmp  w8, #0xc0 ; csel       ; clamp to 192
```

and inside the loop:

```asm
lsl  w10, w23, #3           ; bin*8
udiv w9, w10, w28           ; / 3
add  w8, w9, w9, lsl #1     ; w9*3
sub  w10, w10, w8           ; remainder = (bin*8) % 3
cbnz w10, ...               ; three code paths on the remainder
```

So output bin `k` reads input around `k*8/3`, with the remainder selecting
which of two adjacent input bins is the primary and which the secondary.
That is a fractional-rate spectral map with linear interpolation between
neighbouring bins.

### Remainder 0: pure complex square

```asm
ldp  s20, s19, [x8]         ; re, im
fneg s18, s19               ; -im
fmul s17, s20, s20          ; re*re
fmul s16, s18, s19          ; -im*im
fadd s21, s17, s16          ; re^2 - im^2      <- Re(z^2)
fmul s17, s18, s20          ; -im*re
fmul s16, s19, s20          ;  im*re
fsub s20, s17, s16          ; -2*re*im         <- Im(z^2), negated
```

`z^2` doubles the frequency of a spectral component. That is exactly the
harmonic generation a virtual-bass system needs, done in the frequency domain
instead of with a time-domain nonlinearity.

### Remainder 1 and 2: two-bin interpolated form

Reads four floats — bins `n` and `n+1` as `(s11,s10)` and `(s17,s16)` — then
`fcsel ... eq` swaps which pair is primary when the remainder is 1, and an
unconditional 4-way `fmov` shuffle handles remainder 2.

Magnitudes are then formed for both bins:

```asm
fmul s15, s11, s11 ; fmul s14, s10, s10 ; fadd s18, s14, s15   ; |A|^2
fmul s16, s13, s13 ; fmul s17, s12, s12 ; fadd s17, s17, s16   ; |B|^2
fmul s16, s18, s18                                            ; |A|^4
fmul s18, s17, s16                                            ; |B|^2 * |A|^4
```

The result is passed through `bl 0x18007fd78` (a sqrt/rsqrt helper) after an
`fcmpe s0, #0.0 ; b.ls` guard that zeroes the output for non-positive input.
Then the same complex-square algebra is applied:

```asm
fmul s16, s10, s11 ; fadd s19, s16, s16   ; 2*re*im   = Im(z^2)
fsub s18, s15, s14                       ; re^2-im^2 = Re(z^2)
```

cross-multiplied with the secondary bin's components and scaled by `s0`, the
normalisation factor.

### Shared tail: ramp descriptor and exponent gain

```asm
ldr  w8, [x27]              ; ramp count
cmp  w23, w8 ; b.cc         ; below count -> ramp gain stays 1.0
sub  w9, w23, w8
ldr  x8, [x27, #8]          ; ramp float*
ldr  s18, [x8, w9, uxtw #2]
fmul s9, s18, s8            ; ramp value * master gain
...
fmov s17, w24               ; w24 = 0x3f800000 + (mode << 23)
fmul s18, s17, s18          ; power-of-two gain by exponent injection
```

`w24` is built as `1.0f` with the mode shifted into the exponent field
(`add w24, w9, w20, lsl #23` where `w9 = 0x3f800000`), i.e. a gain of
`2^mode`. The same `[x27]`/`[x27,#8]` count-plus-pointer ramp descriptor is
used here as in `FUN_180075688`.

---

## `FUN_180075c70` — combined / third harmonic (mode 3) — RESOLVED

Not "complex rotation + magnitude" as the old note guessed. It is the **cube**,
`z^3`, i.e. the third harmonic.

```asm
fmul s16, s18, s19 ; fadd s20, s16, s16    ; 2*re*im       = Im(z^2)
fmul s17, s18, s18 ; fmul s16, s19, s19
fsub s22, s17, s16                        ; re^2 - im^2   = Re(z^2)

fmul s16, s20, s19                        ; Im(z^2)*im
fmul s17, s22, s18                        ; Re(z^2)*re
fsub s21, s17, s16                        ; Re(z^2)*re - Im(z^2)*im = Re(z^3)
fmul s17, s22, s19                        ; Re(z^2)*im
fmul s16, s20, s18                        ; Im(z^2)*re
                                          ; -> Im(z^3)
```

That is the standard complex-cube expansion: `z^3 = z^2 * z`.

Bin mapping is **2:1** here (`lsr w8, w2, #1`), against **8:3** in
`FUN_180075830` (`(n*3)>>3`). Both clamp to 192.

The ramp descriptor is read the same way but is loaded once up front rather
than per iteration:

```asm
csel x11, x4, x8, ne        ; descriptor, or the stack fallback if null
ldr  w15, [x11]             ; count
ldr  x14, [x11, #8]         ; float*
...
cmp  w8, w15 ; b.cc         ; below count -> keep previous gain
ldr  s16, [x14] ; add x14, x14, #4
fmul s23, s0, s16           ; master gain * ramp[n]
```

---

## The three modes form a harmonic ladder

This is the key structural finding, and it explains why every mode path ends
in `FUN_180075688`:

| Mode | Pre-pass | Operation | Bin map |
|---|---|---|---|
| 1 | none | rotation only, `z` | 1:1 |
| 2 | `FUN_180075830` | `z^2`, second harmonic | 8:3 |
| 3 | `FUN_180075c70` | `z^3`, third harmonic | 2:1 |

Each pre-pass writes its harmonic into the shared 256-bin accumulator, then
`FUN_180075688` rotates and adds the fundamental on top. The accumulator is
zeroed once per block by the dispatcher. `FUN_180077098` normalises each bin
to unit magnitude first, so the harmonics are generated from **phase only** —
magnitude is reintroduced by the ramp and gain terms.

That is a complete, coherent spectral harmonic synthesiser, and it is a
fundamentally different design from the time-domain Chebyshev approach used
in `sp11_dolby_stage2.c`.

---

## Instance structure and bin-range derivation — RESOLVED

### The bin range is computed from FREQUENCIES, not passed in

Dispatcher prologue:

```asm
ldr  s18, 0x180076000        ; 256.0 (FFT size)
ldp  s16, s17, [x24, #4]     ; start_freq, end_freq  (normalised 0..1)
fmul s16, s16, s18 ; frintm ; fcvtzs w8, s16
cmp  #0xc0 ; csel w21, ...   ; start bin, clamped to 192
fmul s16, s17, s18 ; frintm ; fcvtzs w8, s16
cmp  #0xc0 ; csel w20, ...   ; end bin, clamped to 192
```

So `[x24,#4]` and `[x24,#8]` are **normalised frequencies** (fraction of
Nyquist); multiplying by 256 and flooring gives the bin index. That is why
the bin range never appears as a constant anywhere.

### Instance layout at `x24`

| Offset | Type | Meaning |
|---|---|---|
| `+0x00` | `u32` | **mode**: 1, 2, 3; anything else = no processing |
| `+0x04` | `f32` | start frequency, normalised |
| `+0x08` | `f32` | end frequency, normalised |
| `+0x0c` | `f32` | gain, mode 1 |
| `+0x10` | `f32` | gain, mode 2 |
| `+0x14` | `f32` | gain, mode 3 |
| `+0x18` | `ptr` | ramp block pointer (mode 2); `+0x10` inside it is the float array |
| `+0x20` | `ptr` | ramp block pointer (mode 3) |

### The ramp count comes from `frexp`

Mode 2 branch:

```asm
ldr  s16, [x24, #16]         ; gain for this mode
add  x0, sp, #0x14           ; &exponent
fcvt d0, s16
bl   0x1800a4468             ; frexp(gain, &exp)
fcvt s17, d0                 ; mantissa
ldr  w4, [sp, #20]           ; exponent  -> ramp count argument
```

`0x1800a4468` is confirmed `frexp`: it masks the exponent field against
`0x7ff0000000000000`, writes `-1` to `*x0` for infinities, and sets
`errno = 22` (EINVAL) when `x0` is null. So the gain is split into mantissa
and exponent, the **mantissa** scales the master gain and the **exponent**
becomes the ramp length.

That explains the `w24 = 0x3f800000 + (mode << 23)` trick seen in
`FUN_180075830`: the exponent is reinserted by direct manipulation of the
float's exponent field rather than by multiplication.

### Master gain is a product of two sources

```asm
ldr  x8, [x19, #1872]        ; x19 = adrp 0x180334000, global table
ldr  s16, [x8, #4]           ; per-mode global gain (offset 0/4/8 by mode)
fmul s0, s16, s17            ; global gain * frexp mantissa
```

So the final master gain passed to the harmonic function is
`global_table[mode] * mantissa(instance_gain[mode])`.

### Ramp pointer indirection

```asm
add  x25, x24, #0x18         ; &instance.ramp_ptr
ldr  x9, [x25]
add  x8, x9, #0x10           ; ramp block + 0x10
cmp  x9, #0 ; csel x5, x8, xzr, ne
```

The descriptor passed to the harmonic function is `ramp_block + 0x10`, or
null if the block pointer is null. Inside `FUN_180075688` and
`FUN_180075830` that descriptor is read as `[0]` = count, `[2]` = `float*`,
which matches a block whose header occupies the first 16 bytes.

---

## Still unresolved

| Item | Status |
|---|---|
| FFT / IFFT and overlap-add | not in any extracted source; must be written |
| Contents of the ramp float arrays | pointers mapped, values not dumped |
| Global gain table at `0x180334000 + 1872` | offset known, values not dumped |
| Which mode/frequencies the speaker profile sets | layout known, values not dumped |

Note what is NOT on this list any more. The algorithm is fully understood:
normalise to unit magnitude, raise to the power `n` for mode `n`, scale by
`global_gain[mode] * mantissa(instance_gain[mode]) * ramp[bin]`, accumulate
into a shared 256-bin buffer, then rotate and add the fundamental.

The four remaining items are all **runtime values**, not structure. They can
be obtained either by dumping the initialised structures from a running
Windows session, or by choosing sensible values and validating against the
measured Windows transfer curve in
`SP11_DOLBY_LINUX_RECONSTRUCTION_PLAN_20260518.md`:

```text
1 kHz reference        +8.01 dB
75 Hz at -30 dBFS     +16.82 dB
75 Hz at -12 dBFS     +10.25 dB
loud bass              limited near -0.13 dBFS
```

---

## Implementation notes for a reconstruction

Per block, for a 256-point real FFT:

1. window and forward FFT the input band
2. `FUN_180077098`: normalise every bin in `[start_bin, end_bin]` to unit
   magnitude, zeroing bins whose `max(|re|,|im|)` is below `FLT_EPSILON`
3. zero the 256-bin (2048-byte) accumulator
4. if mode 2: for each output bin `k`, read input near `k*8/3` with two-bin
   interpolation, compute `z^2`, scale, accumulate.
   if mode 3: read input at `k/2`, compute `z^3`, scale, accumulate
5. `FUN_180075688`: rotate each bin by 45 degrees using `1/sqrt(2)`, complex
   multiply by the coefficient array, scale by the ramp, accumulate
6. mirror bins `256-k` for the negative-frequency half
7. inverse FFT and overlap-add

Gain for each stage is `global_gain[mode] * mantissa(instance_gain[mode])`,
and the ramp advances one float per bin once `bin >= ramp_count`, where
`ramp_count = exponent(instance_gain[mode])` from `frexp`.


---

## Why `bass_standalone.c` stage 3 cannot be repaired

`freq_domain_process()` reads `BE_MODE*_GAINS` as `[re,im]` gain pairs with a
loop bound of `i < 64`. `bass_coefficients.h` documents those tables as 12
gain floats followed by **FFT twiddle factors**. The tables are 16 to 48
floats; the loop needs 128. It therefore treats twiddles as audio gains and
reads up to 96 floats out of bounds.

It also overwrites instead of accumulating, has no mirrored loop, and does not
call the normalisation pass.
