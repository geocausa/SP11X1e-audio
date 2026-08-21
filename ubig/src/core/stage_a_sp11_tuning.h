#ifndef UBIG_STAGE_A_SP11_TUNING_H
#define UBIG_STAGE_A_SP11_TUNING_H
#include "stage_a_core.h"

/* DEVICE_TUNING: populate the SP11 48 kHz Dynamic-family Stage-A contract
 * using only UbiG-owned static data generated from the public tuning spec. */
void ubig_stage_a_sp11_dynamic_config(UbigStageACoreConfig *cfg);
#endif
