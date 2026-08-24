#ifndef UBIG_STAGE_A_FFT320_SP11_H
#define UBIG_STAGE_A_FFT320_SP11_H
/* DERIVED: SP11 live-callback arithmetic schedule for the 320-point forward
 * DFT. The unscaled synthesis and 1/320-normalized analysis callbacks share
 * radix-8 -> radix-5 -> radix-8 staging; roots are generated mathematically. */
void ubig_stage_a_fft320_sp11(float *out_interleaved,
                              const float *in_interleaved,
                              unsigned n,
                              void *opaque);
void ubig_stage_a_fft320_sp11_norm320(float *out_interleaved,
                                      const float *in_interleaved,
                                      unsigned n,
                                      void *opaque);
#endif
