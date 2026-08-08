#ifndef BASS_COEFFICIENTS_H
#define BASS_COEFFICIENTS_H

#include <stdint.h>

/* =========================================================================
 * DolbyAudioProcessing.dll — Extracted Bass Coefficients
 * Binary: DolbyAudioProcessing.dll v8.1.0 (ARM64)
 * Source: Ghidra project "surface", .rdata section (0x1800e9000-0x1803333ff)
 *
 * Organized by the 4-stage cascade:
 *   1. Bass Extraction (4-band crossover)
 *   2. Virtual Bass gain split & harmonics
 *   3. Bass Enhancer (4 modes)
 *   4. Sliding Bass dynamics
 * ========================================================================= */

/* -----------------------------------------------------------------------
 * STAGE 1: 4-Band Crossover / Bass Extraction
 * Functions: FUN_180053b40 (dispatch), FUN_180052430 (band sum)
 * ----------------------------------------------------------------------- */

/* Input gain scaler applied to all bands before filtering.
 * Loaded at FUN_180053b40: ldr  s0, [pc, #0x3c] -> DAT_180053f70
 * Value: 21.59375f */
#define BASS_CROSSOVER_INPUT_GAIN  21.5927734375f

/* The 4-band filter bank dispatches via callback at param_3+0x30.
 * Each band operates on per-channel state with 0x68-byte stride.
 * Filter coefficients are embedded in the init functions (FUN_180052430
 * sums accumulated band outputs). Actual filter taps are loaded at init
 * from struct offsets and may be computed at runtime from crossover
 * frequency parameters rather than being literal .rdata tables. */

/* -----------------------------------------------------------------------
 * STAGE 2: Virtual Bass — Harmonic Shaper & Dynamics
 * Functions: FUN_180069b10 (core), FUN_1800790e8 (envelope),
 *            FUN_180079530 (dynamics), FUN_1800796b0 (AGC),
 *            FUN_1800792b8 (mix)
 * ----------------------------------------------------------------------- */

/* --- 2a. Harmonic activation/weight table (fallback profile) ---
 * Address: 0x18024c740, 32 float32 values
 * Used when *param_2 != 1 (non-standard mode)
 * Harmonics 0-19 have zero weight (= disabled, sub-band fundamental).
 * Harmonics 20-31 carry the actual weight values.
 * The *(int*)==0 check on ARM64 is an optimization — comparing
 * IEEE 754 bit pattern against zero avoids an FP compare. */
static const float BASS_HARMONIC_WEIGHTS_FALLBACK[32] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.92880684f, 0.86709112f, 0.81307745f, 0.76540816f,
    0.72302681f, 0.68509954f, 0.65095878f, 0.62006426f,
    0.59197372f, 0.56632191f, 0.54280418f, 0.52116483f
};

/* --- 2b. Dynamics constants (FUN_180079530) ---
 * Address: 0x180079698-0x1800796ac */
#define VB_DYN_THRESH_LOW       (-0.0076923077f)  /* DAT_180079698 = 0xbbfc0fc1 */
#define VB_DYN_THRESH_HIGH      (-0.023076924f)    /* DAT_18007969c = 0xbcbd0bd1 */
#define VB_DYN_MULTIPLIER       64.0f              /* DAT_1800796a0 = 0x42000080 */
#define VB_DYN_COEFF_NEG        (-0.022307692f)    /* DAT_1800796a4 = 0xbcb6b49e */
#define VB_DYN_COEFF_POS        0.069224089f       /* DAT_1800796a8 = 0x3d8dc55c */
#define VB_DYN_CLAMP            0.046153847f       /* DAT_1800796ac = 0x3d3d0bd1 */

/* --- 2c. AGC/limiter constants (FUN_1800796b0) ---
 * Address: 0x180079870-0x180079884 */
#define VB_AGC_THRESHOLD        0.00038461538f     /* DAT_180079870 = 0x39c9a634 */
#define VB_AGC_GAIN_STEP        0.03125f           /* DAT_180079874 = 0x3d000000 */
#define VB_AGC_MULTIPLIER       32.0f              /* DAT_180079878 = 0x42000000 */
#define VB_AGC_COEFF_A          0.4361659f         /* DAT_18007987c = 0x3edf5123 */
#define VB_AGC_COEFF_B          0.56383407f        /* DAT_180079880 = 0x3f10576e */
#define VB_AGC_CLAMP            0.046153847f       /* DAT_180079884 = 0x3d3d0bd1 */

/* --- 2d. Mix/AGC lookup tables ---
 * FUN_1800796b0: DAT_18028b500 (index by non-zero count-1)
 * FUN_1800792b8: DAT_18028b4a0 (index by non-zero count-1)
 * These are 32-entry 1/n-weighted tables for energy normalization */
static const float VB_AGC_WEIGHT_TABLE[32] = {
    1.0f, 0.5f, 0.333333f, 0.25f, 0.2f, 0.166667f, 0.142857f, 0.125f,
    0.111111f, 0.1f, 0.090909f, 0.083333f, 0.076923f, 0.071429f, 0.066667f, 0.0625f,
    0.058824f, 0.055556f, 0.052632f, 0.05f, 0.047619f, 0.045455f, 0.043478f, 0.041667f,
    0.04f, 0.038462f, 0.037037f, 0.035714f, 0.034483f, 0.033333f, 0.032258f, 0.03125f
};
#define VB_MIX_WEIGHT_TABLE VB_AGC_WEIGHT_TABLE  /* DAT_18028b4a0 = same series */

/* --- 2e. Mix constants (FUN_1800792b8) --- */
#define VB_MIX_GAIN_STEP        0.03125f       /* DAT_180079518 = 0x3d000000 */
#define VB_MIX_MULTIPLIER       32.0f          /* DAT_18007951c = 0x42000000 */

/* --- 2f. Output scale factors (FUN_180069b10) --- */
#define VB_OUTPUT_SCALE_A       2080.0f        /* DAT_180069e40 = 0x45020000 */
#define VB_OUTPUT_SCALE_B       4160.0f        /* DAT_180069e44 = 0x45820000 */
/* These scale float32 outputs to int32 for IPC at final mixing stage */

/* -----------------------------------------------------------------------
 * STAGE 3: Bass Enhancer (4 modes)
 * Function: FUN_180075e80 (dispatch)
 *   mode 0 = FUN_180075688 (FREQ_DOMAIN)
 *   mode 1 = FUN_180075830 (MAG_SHAPING)
 *   mode 2 = FUN_180075688? (FREQ_DOMAIN fallback)
 *   mode 3 = FUN_180075c70 (COMBINED)
 *   init  = FUN_180077098 (spectral normalization)
 * ----------------------------------------------------------------------- */

/* --- 3a. Common constants --- */
#define BE_FFT_WINDOW_SIZE      256.0f         /* DAT_180076000 = 0x43800000 */

/* --- 3b. Mode 0 — Frequency Domain (FUN_180075688) --- */
#define BE_ROTATION_FACTOR      0.70710678f    /* DAT_180075828 = 0x3f3504f3 = 1/sqrt(2) */

/* --- 3c. Mode 1 — Magnitude Shaping (FUN_180075830) --- */
#define BE_MAGNITUDE_FLOOR      1.1754944e-38f /* DAT_180075c68 = 0x00800000 */

/* --- 3d. Mode 3 — Combined (FUN_180075c70) --- */
#define BE_COMBINED_REAL        0.70710678f    /* DAT_180075e78 = 0x3f3504f3 */
#define BE_COMBINED_IMAG        (-0.70710678f) /* DAT_180075e7c = 0xbf3504f3 */

/* --- 3e. Init — Spectral floor threshold (FUN_180077098) --- */
#define BE_NORMALIZE_THRESHOLD  1.1920929e-7f  /* DAT_180077208 = 0x34000000 */

/* --- 3f. Mode gain tables (via PTR_DAT_180334750) ---
 * Tables contain frequency-domain coefficients indexed by FFT bin.
 * Mode 0/2 use (cos, -sin) twiddle factors for complex rotation.
 * Mode 1 is a sub-range of mode 0. Mode 3 uses complex gains +
 * a post-smoothing envelope for gradual gain application.
 *
 * PTR_DAT_180334750 (0x180334750):
 *   +0x00 -> 0x18028e220  (Mode 0, 32 float32 = 12 gain + 20 twiddle)
 *   +0x04 -> 0x18028e230  (Mode 1, 16 float32 = 8 gain + 8 twiddle)
 *   +0x08 -> 0x18028e250  (Mode 2, 32 float32 = twiddle only)
 *   +0x0c -> 0x18028e390  (Mode 3, 48 float32 = 24 complex + 24 envelope)
 */

/* Mode 0: FREQ_DOMAIN gain + twiddle table (0x18028e220, 32 float32)
 *   [ 0..11] frequency-domain gain curve (bins 0-11)
 *   [12..31] (cos(pi*k*5/32), -sin(pi*k*5/32)) twiddle factors for k=0..9
 */
static const float BE_MODE0_GAINS[32] = {
     2.0f,           1.63299322f,    1.41421354f,    0.0f,
     0.0480161756f,  0.51071525f,    0.86612171f,    0.85069323f,
     0.50053972f,    0.11860373f,   -0.03345971f,    0.00121333427f,
     1.0f,          -0.0f,           0.88192123f,   -0.47139674f,
     0.55557019f,   -0.83146966f,    0.09801710f,   -0.99518472f,
    -0.38268349f,   -0.92387950f,   -0.77301049f,   -0.63439322f,
    -0.98078531f,   -0.19509023f,   -0.95694029f,    0.29028478f,
    -0.70710671f,    0.70710683f,   -0.29028457f,    0.95694035f
};

/* Mode 1: MAG_SHAPING gain table (0x18028e230, 16 float32)
 * Starts at offset 4 of the mode 0 table (first 4 bins skipped).
 * Contains bins 4-11 gains + twiddle factors for bins 12-19. */
static const float BE_MODE1_GAINS[16] = {
     0.0480161756f,  0.51071525f,    0.86612171f,    0.85069323f,
     0.50053972f,    0.11860373f,   -0.03345971f,    0.00121333427f,
     1.0f,          -0.0f,           0.88192123f,   -0.47139674f,
     0.55557019f,   -0.83146966f,    0.09801710f,   -0.99518472f
};

/* Mode 2: FREQ_DOMAIN alt table (0x18028e250, 32 float32)
 * Pure (cos, -sin) twiddle factors for 64-point FFT with stride 5.
 * No DC-bin gain pre-pended — starts directly with twiddle pairs. */
static const float BE_MODE2_GAINS[32] = {
     1.0f,          -0.0f,           0.88192123f,   -0.47139674f,
     0.55557019f,   -0.83146966f,    0.09801710f,   -0.99518472f,
    -0.38268349f,   -0.92387950f,   -0.77301049f,   -0.63439322f,
    -0.98078531f,   -0.19509023f,   -0.95694029f,    0.29028478f,
    -0.70710671f,    0.70710683f,   -0.29028457f,    0.95694035f,
     0.19509046f,    0.98078525f,    0.63439339f,    0.77301037f,
     0.92387962f,    0.38268328f,    0.99518472f,   -0.09801732f,
     0.83146954f,   -0.55557036f,    0.47139657f,   -0.88192135f
};

/* Mode 3: COMBINED gain table (0x18028e390, 48 float32)
 *
 *   [ 0..23] frequency-domain complex gains (12 complex pairs)
 *   [24..47] 24-point symmetric envelope window (raised-cosine-like)
 *            used for smoothing gain transitions across the spectrum.
 *            The envelope is symmetric: [24..35] == [47..36] within float32.
 */
static const float BE_MODE3_GAINS[48] = {
    -0.70710915f,   -0.70710438f,   -0.95694131f,   -0.29028142f,
    -0.98078460f,    0.19509368f,   -0.77300829f,    0.63439596f,
    -0.38268024f,    0.92388088f,    0.0980205759f,  0.99518436f,
     0.55557311f,    0.83146769f,    0.88192290f,    0.47139367f,
     0.99971032f,    1.0f,           0.99894351f,    0.99248689f,
     0.97525710f,    0.94148302f,    0.88643306f,    0.80778307f,
     0.70670223f,    0.58832252f,    0.46131510f,    0.33649007f,
     0.22464535f,    0.13419010f,    0.06921866f,    0.0285360217f,
     0.00634680968f, 0.0290078036f, 0.06984161f,    0.13484114f,
     0.22518454f,    0.33685324f,    0.46151119f,    0.58840042f,
     0.70670211f,    0.80767977f,    0.88606614f,    0.94046706f,
     0.97287965f,    0.98759812f,    0.98983163f,    0.98430663f
};
/* Mode 3 also has a post-smoothing envelope (offsets 0x100+) stored
 * in the same table region, used for gradual gain application. */

/* -----------------------------------------------------------------------
 * STAGE 4: Sliding Bass / LLDP Dynamics
 * Functions: FUN_18005db60 (orchestrator), FUN_18006a420 (envelope),
 *            FUN_180069fd8 (gain calc dispatcher),
 *            FUN_18006a2d8 (smoothing), FUN_180069e78 (crossfade)
 * ----------------------------------------------------------------------- */

/* --- 4a. Orchestrator constants (FUN_18005db60) ---
 * Address: 0x18005df18-0x18005df24 */
#define SB_ENVELOPE_SCALE       43.185547f     /* DAT_18005df18 = 0x422cbe00 */
#define SB_NEG_CONST            (-0.15384616f) /* DAT_18005df1c = 0xbe1d89d9 */
#define SB_GAIN_SCALE           0.0634765625f  /* DAT_18005df20 = 0x3d820000 */
#define SB_Q15_FACTOR           32768.0f       /* DAT_18005df24 = 0x47000000 = 2^15 */

/* --- 4b. Envelope detection constants (FUN_18006a420) ---
 * Address: 0x18006a598-0x18006a59c */
#define SB_ENV_ATTACK           0.04f          /* DAT_18006a598 = 0x3d23d70a */
#define SB_ENV_DECAY            0.99f          /* DAT_18006a59c = 0x3f7d70a4 */

/* --- 4c. Gain smoothing constants (FUN_18006a2d8) ---
 * Address: 0x18006a410-0x18006a41c */
#define SB_SMOOTH_GAIN          2.03125f       /* DAT_18006a410 = 0x40020000 */
#define SB_SMOOTH_CLAMP_NEG     (-0.69230771f) /* DAT_18006a414 = 0xbf313b14 */
#define SB_SMOOTH_CLAMP_UPPER   0.011538462f   /* DAT_18006a418 = 0x3c3d0bd1 */
#define SB_SMOOTH_CLAMP_LOWER   0.0038461538f  /* DAT_18006a41c = 0x3b7c0fc1 */

/* --- 4d. Crossfade/gain blending constants (FUN_180069e78) ---
 * Address: 0x180069fc8-0x180069fd0 */
#define SB_XFADE_ALPHA          0.1f           /* DAT_180069fc8 = 0x3dcccccd */
#define SB_XFADE_ONE_MINUS_A    0.9f           /* DAT_180069fcc = 0x3f666666 */
#define SB_XFADE_CLAMP          (-0.69230771f) /* DAT_180069fd0 = 0xbf313b14 */

/* -----------------------------------------------------------------------
 * Address map summary
 * -----------------------------------------------------------------------
 * 0x180053f70  BASS_CROSSOVER_INPUT_GAIN          float32
 * 0x180069e40  VB_OUTPUT_SCALE_A                  float32
 * 0x180069e44  VB_OUTPUT_SCALE_B                  float32
 * 0x180069fc8  SB_XFADE_ALPHA / SB_XFADE_ONE_MINUS_A / SB_XFADE_CLAMP
 * 0x18006a410  SB_SMOOTH_GAIN / CLAMP_NEG / CLAMP_UPPER / CLAMP_LOWER
 * 0x18006a598  SB_ENV_ATTACK / SB_ENV_DECAY
 * 0x18005df18  SB_ENVELOPE_SCALE / NEG_CONST / GAIN_SCALE / Q15_FACTOR
 * 0x180075828  BE_ROTATION_FACTOR                 float32
 * 0x180075c68  BE_MAGNITUDE_FLOOR                 float32
 * 0x180075e78  BE_COMBINED_REAL / BE_COMBINED_IMAG  float32 pair
 * 0x180076000  BE_FFT_WINDOW_SIZE                 float32
 * 0x180077208  BE_NORMALIZE_THRESHOLD              float32
 * 0x180079518  VB_MIX_GAIN_STEP / VB_MIX_MULTIPLIER
 * 0x180079698  VB_DYN_THRESHOLD_LOW/HIGH/MULT/COEFF_NEG/COEFF_POS/CLAMP
 * 0x180079870  VB_AGC_THRESHOLD/GAIN_STEP/MULT/COEFF_A/COEFF_B/CLAMP
 * 0x18024c740  BASS_HARMONIC_WEIGHTS_FALLBACK      32 x float32
 * 0x18028b4a0  VB_MIX_WEIGHT_TABLE                 32 x float32
 * 0x18028b500  VB_AGC_WEIGHT_TABLE                 32 x float32
 * 0x18028e220  BE_MODE0_GAINS                      32 x float32
 * 0x18028e230  BE_MODE1_GAINS                      16 x float32
 * 0x18028e250  BE_MODE2_GAINS                      32 x float32
 * 0x18028e390  BE_MODE3_GAINS                      48 x float32
 * 0x180334750  PTR_DAT_180334750 (pointer table to above 4 gain tables)
 * ----------------------------------------------------------------------- */

#endif /* BASS_COEFFICIENTS_H */
