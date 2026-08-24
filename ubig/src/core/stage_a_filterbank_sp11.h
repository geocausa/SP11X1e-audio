#ifndef UBIG_STAGE_A_FILTERBANK_SP11_H
#define UBIG_STAGE_A_FILTERBANK_SP11_H
#include "stage_a_analyzer.h"
#include "stage_a_synth.h"
#define UBIG_SP11_REDUCE_COEFFS 844u
#define UBIG_SP11_SYNTH0_COEFFS 3008u
#define UBIG_SP11_SYNTH1_COEFFS 416u
const UbigStageAAnalyzerDesc *ubig_stage_a_sp11_analyzer_desc(void);
const UbigStageASynthDesc *ubig_stage_a_sp11_synth_desc(void);
#endif
