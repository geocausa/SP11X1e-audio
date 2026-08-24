#ifndef UBIG_STAGE_A_SYNTH_H
#define UBIG_STAGE_A_SYNTH_H
#include <stdint.h>
#define UBIG_SYNTH_PHASES 2u
#define UBIG_SYNTH_BANDS 20u
typedef void (*UbigSynthTransformFn)(float *out_interleaved,const float *in_packed,unsigned n,void *opaque);
typedef struct {
    unsigned block_frames;
    unsigned hop_frames;
    unsigned transform_span;
    unsigned phases;
    unsigned bands;
    const float *post_twiddle; /* interleaved complex, 2*transform_span */
    const uint32_t *band_start[UBIG_SYNTH_PHASES];
    const uint32_t *band_count[UBIG_SYNTH_PHASES];
    const float *mix_coeff[UBIG_SYNTH_PHASES];
} UbigStageASynthDesc;
typedef struct {
    unsigned phase_index;
    float *band_data; /* phases * 2 * transform_span */
    float *overlap;   /* at least 2*transform_span */
    float *gains;     /* phases * bands */
} UbigStageASynthState;
int ubig_stage_a_synth_process(UbigStageASynthState *st,const UbigStageASynthDesc *d,
                               float *output,float *scratch,unsigned scratch_floats,
                               UbigSynthTransformFn transform,void *opaque);
#endif
