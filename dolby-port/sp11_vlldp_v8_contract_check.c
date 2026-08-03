#include "sp11_vlldp_fun18001de90.h"
#include "sp11_vlldp_fun18001de90_cdb_vectors.h"

static int sp11_check_fun18001de90_first_transition(void)
{
    float out_bbc[SP11_VLLDP_CONTRACT_BANDS];
    int32_t out_c0c[SP11_VLLDP_CONTRACT_BANDS];
    sp11_vlldp_fun18001de90_export(
        sp11_vlldp_fun18001de90_hit1_bbc_before,
        sp11_vlldp_fun18001de90_hit1_source_ch0,
        sp11_vlldp_fun18001de90_hit1_source_ch1,
        out_bbc,
        out_c0c
    );
    for (int band = 0; band < SP11_VLLDP_CONTRACT_BANDS; band++) {
        int32_t expected = sp11_vlldp_fun18001de90_hit1_predicted_c0c_after[band];
        int32_t next = sp11_vlldp_fun18001de90_hit2_c0c_before[band];
        if (out_c0c[band] != expected || out_c0c[band] != next)
            return 0;
    }
    return 1;
}

static int sp11_check_fun18001de90_cdb_transitions(void)
{
    int exact_bands = 0;
    int total_bands = 0;
    int max_abs = 0;

    for (int hit = 0; hit < SP11_VLLDP_FUN18001DE90_CDB_VECTOR_TRANSITIONS; hit++) {
        float out_bbc[SP11_VLLDP_CONTRACT_BANDS];
        int32_t out_c0c[SP11_VLLDP_CONTRACT_BANDS];
        sp11_vlldp_fun18001de90_export(
            sp11_vlldp_fun18001de90_cdb_bbc_before[hit],
            sp11_vlldp_fun18001de90_cdb_source_ch0[hit],
            sp11_vlldp_fun18001de90_cdb_source_ch1[hit],
            out_bbc,
            out_c0c
        );

        for (int band = 0; band < SP11_VLLDP_CONTRACT_BANDS; band++) {
            int diff = out_c0c[band] - sp11_vlldp_fun18001de90_cdb_c0c_before[hit + 1][band];
            if (diff < 0)
                diff = -diff;
            if (diff == 0)
                exact_bands++;
            if (max_abs < diff)
                max_abs = diff;
            total_bands++;
        }
    }

    return total_bands == SP11_VLLDP_FUN18001DE90_CDB_TOTAL_BANDS &&
           exact_bands >= SP11_VLLDP_FUN18001DE90_CDB_EXACT_BANDS - 1 &&
           max_abs == SP11_VLLDP_FUN18001DE90_CDB_MAX_ABS;
}

int main(void)
{
    return sp11_vlldp_v8_target_b60[0] == -625 &&
           sp11_vlldp_v8_target_c0c[19] == -1528 &&
           sp11_vlldp_v8_c0c_source_impossible_mask[6] == 1 &&
           sp11_vlldp_v8_c0c_source_impossible_mask[14] == 1 &&
           sp11_vlldp_v8_c0c_source_impossible_mask[15] == 0 &&
           SP11_VLLDP_CONTRACT_RELOCATED_POINTERS == 27 &&
           SP11_VLLDP_FUN18001DE90_CDB_ENTRY_HITS == 1000 &&
           SP11_VLLDP_FUN18001DE90_CDB_TRANSITIONS == 999 &&
           SP11_VLLDP_FUN18001DE90_CDB_MAX_ABS == 1 &&
           SP11_VLLDP_FUN180021B60_PARAM0_EXACT == 1 &&
           sp11_vlldp_fun18001de90_hit1_predicted_c0c_after[0] == -624 &&
           sp11_vlldp_fun18001de90_hit2_c0c_before[19] == -1210 &&
           sp11_vlldp_fun180021b60_param0_validation_value < -0.088f &&
           sp11_vlldp_fun180021b60_param0_validation_value > -0.089f &&
           sp11_vlldp_fun180021b60_param1_downward_step < -0.0024f &&
           sp11_vlldp_fun180021b60_param2_up_alpha > 0.28f &&
           sp11_vlldp_fun180021b60_param3_down_alpha > 0.036f &&
           sp11_check_fun18001de90_first_transition() &&
           sp11_check_fun18001de90_cdb_transitions()
        ? 0
        : 1;
}
