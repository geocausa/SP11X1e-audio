#include "stage_a_export.h"
#include <math.h>

/* Exact float32 constructor values recovered from the original Stage-A
 * smoother coefficient object.  Hex literals preserve the bit patterns. */
#define UBIG_EXP_LIMIT_UP   (-0x1.696ce2p-4f) /* bdb4b671 */
#define UBIG_EXP_STEP_DOWN  (-0x1.43bf1ap-9f) /* bb21df8d */
#define UBIG_EXP_ALPHA_UP   ( 0x1.21f6b2p-2f) /* 3e90fb59 */
#define UBIG_EXP_ALPHA_DOWN ( 0x1.2905cep-5f) /* 3d1482e7 */

static float smooth(float previous, float target)
{
    if (target < previous) {
        float base = (1.0f - UBIG_EXP_ALPHA_DOWN) * previous;
        float weighted = fmaf(UBIG_EXP_ALPHA_DOWN, target, base);
        float limited = previous + UBIG_EXP_STEP_DOWN;
        return weighted > limited ? weighted : limited;
    }
    float base = (1.0f - UBIG_EXP_ALPHA_UP) * previous;
    float weighted = fmaf(UBIG_EXP_ALPHA_UP, target, base);
    float limited = target + UBIG_EXP_LIMIT_UP;
    return weighted > limited ? weighted : limited;
}

void ubig_stage_a_export(const float *previous,
                         const float *sources,
                         unsigned channels,
                         float out_state[UBIG_STAGE_A_BANDS],
                         int32_t out_raw[UBIG_STAGE_A_BANDS])
{
    for (unsigned band = 0; band < UBIG_STAGE_A_BANDS; band++) {
        float target = sources[band];
        for (unsigned ch = 1; ch < channels; ch++) {
            float v = sources[ch * UBIG_STAGE_A_BANDS + band];
            if (v > target) target = v;
        }
        float next = smooth(previous[band], target);
        out_state[band] = next;
        out_raw[band] = (int32_t)floorf(next * 2080.0f);
    }
}
