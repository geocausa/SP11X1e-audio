#ifndef UBIG_STAGE_B_RT_H
#define UBIG_STAGE_B_RT_H
#include <stdint.h>

#define UBIG_STAGE_B_RT_MAX_SELECTED_ROWS 25u
#define UBIG_STAGE_B_RT_MAX_BANDS 20u

typedef struct {
    uint32_t group_count;
    uint32_t vectors_per_group;
    float ***groups;
} UbigStageBRtComplexGroups;

typedef struct {
    uint32_t vectors_per_group;
    float ***groups;
} UbigStageBRtExtraGroups;

typedef struct {
    uint32_t row_count;
    uint32_t band_count;
    float **rows;
    uint32_t capacity;
} UbigStageBRtBandRows;

typedef struct {
    int32_t **rows;
} UbigStageBRtTelemetryRows;

/* Exact complex-energy reducer used by the Stage-B RT band analyzer. Rows are
 * interleaved {real,imag}; begin/end are complex-bin indices. */
float ubig_stage_b_rt_complex_energy(float *const *rows,
                                     uint32_t row_count,
                                     uint32_t begin,
                                     uint32_t end);

/* Exact deployed Stage-B band-energy/log row builder. group_to_output selects
 * which main groups contribute to each output row. For selected main groups
 * 0 and 1, optional extra vectors are appended. band_ends contains cumulative
 * complex-bin boundaries. */
void ubig_stage_b_rt_band_log_process(float offset0,
                                      float offset1,
                                      const UbigStageBRtComplexGroups *main_groups,
                                      const UbigStageBRtExtraGroups *extra_groups,
                                      const uint32_t *band_ends,
                                      const int32_t *group_to_output,
                                      UbigStageBRtBandRows *output,
                                      UbigStageBRtTelemetryRows *telemetry);
#define UBIG_STAGE_B_RT_TARGET_PLANES 4u
#define UBIG_STAGE_B_RT_SP11_ROWS 2u
#define UBIG_STAGE_B_RT_SP11_BANDS 20u
#define UBIG_STAGE_B_RT_SP11_BINS 77u

typedef struct {
    float *plane[UBIG_STAGE_B_RT_TARGET_PLANES];
} UbigStageBRtTargetObject;

typedef struct {
    uint32_t object_count;
    uint32_t bin_count;
    UbigStageBRtTargetObject *objects;
} UbigStageBRtTargetSet;

/* Exact meaningful-bin contract of the universal deployed SP11 output shaper.
 * The reference vector loop touches one additional two-float SIMD padding bin;
 * UbiG deliberately excludes that non-audio padding from the semantic API. */
void ubig_stage_b_rt_output_shape(float row_offset,
                                  float linked_ceiling,
                                  const UbigStageBRtBandRows *input,
                                  UbigStageBRtBandRows *output,
                                  const uint32_t *object_to_row,
                                  const uint32_t *band_ends,
                                  UbigStageBRtTargetSet *targets);
#endif
