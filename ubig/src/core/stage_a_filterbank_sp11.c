#include "stage_a_filterbank_sp11.h"
#include "stage_a_filterbank_sp11_tables.h"

static const UbigStageAAnalyzerDesc analyzer_desc = {
    .block_frames=256u,.hop_frames=64u,.transform_span=320u,.phases=2u,.bands=20u,
    .pre_twiddle=ubig_sp11_filterbank_matrix,.edge_window=ubig_sp11_filterbank_window,
    .reduce_start=ubig_sp11_reduce_start,.reduce_count=ubig_sp11_reduce_count,
    .reduce_coeff=ubig_sp11_reduce_coeff,.log_scale=0x1.7b63f2p-6f
};
static const UbigStageASynthDesc synth_desc = {
    .block_frames=256u,.hop_frames=64u,.transform_span=320u,.phases=2u,.bands=20u,
    .post_twiddle=ubig_sp11_filterbank_matrix,
    .band_start={ubig_sp11_synth_start_0,ubig_sp11_synth_start_1},
    .band_count={ubig_sp11_synth_count_0,ubig_sp11_synth_count_1},
    .mix_coeff={ubig_sp11_synth_coeff_0,ubig_sp11_synth_coeff_1}
};
const UbigStageAAnalyzerDesc *ubig_stage_a_sp11_analyzer_desc(void){return &analyzer_desc;}
const UbigStageASynthDesc *ubig_stage_a_sp11_synth_desc(void){return &synth_desc;}
