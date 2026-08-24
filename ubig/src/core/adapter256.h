#ifndef UBIG_ADAPTER256_H
#define UBIG_ADAPTER256_H

#include <stddef.h>
#include <stdint.h>
#include "ubig/ubig.h"

typedef void (*ubig_block256_fn)(void *opaque,
                                 const float in[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS],
                                 float out[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS]);

typedef struct ubig_adapter256 {
    uint32_t fill;
    float block_in[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS];
    float block_out[UBIG_INTERNAL_BLOCK * UBIG_CHANNELS];
} ubig_adapter256;

void ubig_adapter256_reset(ubig_adapter256 *a);
void ubig_adapter256_process(ubig_adapter256 *a,
                             ubig_block256_fn fn, void *opaque,
                             const float *in_l, const float *in_r,
                             float *out_l, float *out_r,
                             size_t frames);

#endif
