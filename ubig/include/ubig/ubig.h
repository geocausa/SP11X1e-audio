#ifndef UBIG_UBIG_H
#define UBIG_UBIG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UBIG_ABI_VERSION 1u
#define UBIG_SAMPLE_RATE 48000u
#define UBIG_CHANNELS 2u
#define UBIG_EQ_BANDS 20u
#define UBIG_INTERNAL_BLOCK 256u

typedef enum ubig_profile {
    UBIG_PROFILE_DYNAMIC = 0,
    UBIG_PROFILE_MOVIE,
    UBIG_PROFILE_MUSIC,
    UBIG_PROFILE_GAME,
    UBIG_PROFILE_VOICE,
    UBIG_PROFILE_COURSE,
    UBIG_PROFILE_CUSTOM,
    UBIG_PROFILE_COUNT
} ubig_profile;

typedef enum ubig_status {
    UBIG_OK = 0,
    UBIG_EINVAL = -1,
    UBIG_ENOMEM = -2,
    UBIG_ESTATE = -3,
    UBIG_EUNSUPPORTED = -4
} ubig_status;

typedef struct ubig_engine ubig_engine;

typedef struct ubig_engine_config {
    uint32_t abi_version;
    uint32_t sample_rate;
    uint32_t channels;
    ubig_profile initial_profile;
} ubig_engine_config;

/* Returns NULL for invalid configurations. All seven public profiles share the
 * exact native SP11 Stage-A audio path. Movie/Music retain their distinct
 * recovered first-stage family state, which is bit-transparent at this audio
 * boundary and is switched in place without resetting DSP history. */
ubig_engine *ubig_engine_create(const ubig_engine_config *config);
void ubig_engine_destroy(ubig_engine *engine);

/* Planar float32 stereo. No allocation, I/O, logging or blocking inside. */
int ubig_engine_process(ubig_engine *engine,
                        const float *in_l, const float *in_r,
                        float *out_l, float *out_r,
                        size_t frames);

/* Profile changes are in-place. Stage-A adaptive/filterbank history is preserved;
 * profile-specific Stage-B behavior is applied by later native stages as they
 * become available. */
int ubig_engine_set_profile(ubig_engine *engine, ubig_profile profile);
int ubig_engine_set_custom_eq(ubig_engine *engine, const int32_t values[UBIG_EQ_BANDS]);

/* Raw SP11 VLLDP endpoint postgain in the recovered DAX Q4-dB domain. Valid
 * values are -1200..0 (-75..0 dB). The setter changes only the native Stage-A
 * compressor runtime scalar and preserves all adaptive/filterbank history. */
int ubig_engine_set_postgain(ubig_engine *engine, int32_t postgain);
ubig_profile ubig_engine_profile(const ubig_engine *engine);

const char *ubig_profile_name(ubig_profile profile);
int ubig_profile_parse(const char *name, ubig_profile *out);

#ifdef __cplusplus
}
#endif
#endif
