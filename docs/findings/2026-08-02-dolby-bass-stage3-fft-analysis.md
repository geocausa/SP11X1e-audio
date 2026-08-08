# Dolby bass enhancer stage 3 — what `FUN_180075688` actually does

Date: 2026-08-02
Status: **`bass_standalone.c` stage 3 is incorrect. Do not build on it.**

Verified two ways: against `BASS-CONTROLFLOW.md`'s decompile, and independently
against the machine code with `objdump -d` on the shipped binary. The
disassembly confirms every structural claim below.

Binary: `DolbyAudioProcessing.dll`
sha256 `900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3`
ARM64 PE, image base `0x180000000`, `.text` `0x180001000`..`0x1800e892c`.

---

## 1. Why this matters

`bass_standalone.c` implements a four-stage bass pipeline. Stages 1, 2 and 4
(crossover, virtual bass, sliding bass) have implementations that match their
extracted coefficients. **Stage 3 does not**, and the file says so:

```c
/* NOTE: Real FFT implementation needed here for production use. */
memset(fft_re, 0, sizeof(fft_re));
for (int i = 0; i < num_samples && i < BE_FFT_HALF; i++)
    fft_re[i] = high[i];          /* not a transform */
BassEnhancer_Process(&p->enhancer, fft_re, fft_im, BE_FFT_HALF, 1.0f);
/* IFFT would go here in production */
```

Adding an FFT in front of `freq_domain_process()` would still be wrong,
because that function misreads its inputs. That is the finding here.

---

## 2. What the real function is

```c
void FUN_180075688(float param_1,      /* master gain                      */
                   longlong param_2,   /* INPUT spectrum, interleaved cplx */
                   longlong param_3,   /* COEFFICIENT array (twiddles)     */
                   uint param_4,       /* start bin * 4                    */
                   uint param_5,       /* end bin * 4                      */
                   uint *param_6,      /* gain-ramp descriptor             */
                   longlong param_7)   /* OUTPUT ACCUMULATOR               */
```

Confirmed in the disassembly at `0x1800756b0`ff:

```asm
lsr  w8, w2, #2          ; param_4 >> 2          bin index from byte offset
cmp  w8, #0xc0           ; clamp to 192
csel w9, w8, w12, cc
ldr  w15, [x11]          ; descriptor[0] = ramp length
ldr  x14, [x11, #8]      ; descriptor[2] = float* ramp
...
ldr  s18, [x14]          ; take next ramp value
add  x14, x14, #0x4      ; advance ONE float
fmul s18, s0, s18        ; gain = param_1 * ramp[n]
...
add  x10, x0, w13, uxtw #3   ; stride-8 complex indexing into param_2
```

### Structure

1. **Complex rotation by 45 degrees.** `DAT_180075828` = `0.70710678` = 1/sqrt(2)
   is applied as `(re*rot - im*rot)` and `(re*rot + im*rot)`. This is a
   rotation, not a magnitude scale.

2. **Complex multiply against `param_3`.** Two floats are read per bin at
   `param_3 + uVar3*8*4`, then combined as a full complex product:

   ```c
   *pfVar2      = (fVar11*fVar9 - fVar12*fVar13) * fVar10 + *pfVar2;
   pfVar2[1]    = (fVar11*fVar13 + fVar12*fVar9) * fVar10 + pfVar2[1];
   ```

3. **It ACCUMULATES into `param_7`.** Note `+ *pfVar2`. The output is added to
   an existing buffer, not overwritten. `bass_standalone.c` overwrites.

4. **Second loop handles the mirrored half.** Bins are indexed
   `0x100 - param_4`, i.e. the negative-frequency image of a **256-point real
   FFT**. `bass_standalone.c` has no second loop at all.

5. **`param_6` is a gain ramp descriptor**, not a per-bin table: a count and a
   float pointer that advances one element per bin once `uVar3 >= count`.

---

## 3. The gain tables are not gain tables

`bass_coefficients.h` documents `BE_MODE0_GAINS[32]` honestly:

```text
[ 0..11]  frequency-domain gain curve (bins 0-11)
[12..31]  (cos(pi*k*5/32), -sin(pi*k*5/32)) twiddle factors for k=0..9
```

Only the first 12 floats are an audio gain curve. The remaining 20 are **FFT
twiddle factors** — transform internals.

`freq_domain_process()` reads the entire array as `[re, im]` gain pairs with a
loop bound of `i < 64`, so it:

* treats twiddle factors as audio gains from bin 6 onward, and
* reads `gain_table[126]` from a 32-element array — **96 floats out of bounds**.

Table sizes: MODE0 32, MODE1 16, MODE2 32, MODE3 48 floats. None reaches the
128 floats the loop would require.

---

## 4. Consequence

Stage 3 cannot be fixed by adding an FFT. Correct reconstruction needs:

* a real 256-point real-input FFT with the documented window
  (`BE_FFT_WINDOW_SIZE` = 256.0, `DAT_180076000` = `0x43800000`),
* the coefficient array `param_3` identified and extracted — it is indexed
  separately from the mode tables and is **not** `BE_MODE*_GAINS`,
* accumulation semantics preserved,
* the mirrored negative-frequency loop,
* the `param_6` gain-ramp descriptor reconstructed,
* inverse transform and overlap-add.

`FUN_180075e80` (the 4-mode dispatcher) and `FUN_180077098` (spectral
normalisation init) are the next functions to decompile, since the dispatcher
determines which of rotation / magnitude-shaping / combined actually runs for
the speaker profile.

---

## 5. Recommendation

Build the modular pipeline with stages 1, 2 and 4 enabled and **stage 3
explicitly disabled**, not stubbed-and-called. A stubbed stage that silently
processes garbage is worse than an absent one, and the current stub reads out
of bounds.

Stage 3 is a genuine open RE task, not integration work. It should not be
described as "Dolby is ported" until it is resolved.
