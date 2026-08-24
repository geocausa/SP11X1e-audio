#ifndef UBIG_STAGE_A_FFT320_H
#define UBIG_STAGE_A_FFT320_H
void ubig_stage_a_fft320(float *out_interleaved,const float *in_interleaved,unsigned n,void *opaque);
void ubig_stage_a_fft320_norm320(float *out_interleaved,const float *in_interleaved,unsigned n,void *opaque);
#endif
