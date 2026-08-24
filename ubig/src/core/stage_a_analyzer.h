#ifndef UBIG_STAGE_A_ANALYZER_H
#define UBIG_STAGE_A_ANALYZER_H
#include <stdint.h>
#include "stage_a_synth.h"
typedef struct {
    unsigned block_frames,hop_frames,transform_span,phases,bands;
    const float *pre_twiddle;      /* 2*transform_span, 4 real + 4 imag SIMD layout */
    const float *edge_window;      /* hop floats */
    const uint32_t *reduce_start;  /* bands */
    const uint32_t *reduce_count;  /* bands */
    const float *reduce_coeff;     /* sequential count sum floats */
    float log_scale;
} UbigStageAAnalyzerDesc;
typedef struct {
    unsigned phase_index;
    float *history;   /* at least hop floats */
    float *band_data; /* phases * 2 * transform_span */
} UbigStageAAnalyzerState;
int ubig_stage_a_analyzer_process(float input_scale,UbigStageAAnalyzerState *st,
                                  const UbigStageAAnalyzerDesc *d,const float *input,
                                  float *out_bands,float *scratch,unsigned scratch_floats,
                                  UbigSynthTransformFn transform,void *opaque);
#endif
