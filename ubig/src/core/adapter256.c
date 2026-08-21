#include "adapter256.h"
#include <string.h>

void ubig_adapter256_reset(ubig_adapter256 *a)
{
    if (!a) return;
    memset(a, 0, sizeof(*a));
}

void ubig_adapter256_process(ubig_adapter256 *a,
                             ubig_block256_fn fn, void *opaque,
                             const float *in_l, const float *in_r,
                             float *out_l, float *out_r,
                             size_t frames)
{
    size_t pos = 0;
    if (!a || !fn || !in_l || !in_r || !out_l || !out_r) return;

    /* Decoded persistent accumulator contract: a full internal block is
       processed at the top of the next iteration. Output is read from the
       same offset in the previously processed block. This yields exactly one
       256-frame startup/history block and is invariant to host chunking. */
    while (pos < frames) {
        if (a->fill >= UBIG_INTERNAL_BLOCK) {
            fn(opaque, a->block_in, a->block_out);
            a->fill = 0;
        }

        uint32_t space = UBIG_INTERNAL_BLOCK - a->fill;
        size_t take = frames - pos;
        if (take > space) take = space;

        for (size_t j = 0; j < take; ++j) {
            uint32_t f = a->fill + (uint32_t)j;
            a->block_in[2u * f] = in_l[pos + j];
            a->block_in[2u * f + 1u] = in_r[pos + j];
            out_l[pos + j] = a->block_out[2u * f];
            out_r[pos + j] = a->block_out[2u * f + 1u];
        }
        a->fill += (uint32_t)take;
        pos += take;
    }
}
