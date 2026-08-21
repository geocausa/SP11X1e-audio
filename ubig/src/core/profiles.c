#include "ubig/ubig.h"
#include "profiles_internal.h"
#include <ctype.h>
#include <string.h>

typedef struct ubig_profile_contract {
    const char *name;
    int leveler_enable, leveler_amount;
    int dialog_enable, dialog_amount;
    int ieq_enable, ieq_amount;
    int mi_steering, surround_boost, surround_decoder;
    int virt_front, virt_height, virt_surround;
    int volmax_boost;
    int raw_output_mode;
    int alternate_first_stage_family;
} ubig_profile_contract;

/* DEVICE_TUNING: transcribed from recovered SP11 profile behavior. These are
   specification data, not an implementation of the reference algorithm. */
static const ubig_profile_contract contracts[UBIG_PROFILE_COUNT] = {
    [UBIG_PROFILE_DYNAMIC] = {"Dynamic", 1,5, 1,5, 1,10, 1,96,1, 10,10,10, 96,11,0},
    [UBIG_PROFILE_MOVIE]   = {"Movie",   1,0, 1,2, 0, 6, 0,72,1, 16,10,16,104,11,1},
    [UBIG_PROFILE_MUSIC]   = {"Music",   1,0, 0,5, 0, 6, 0,24,0, 10,10,10, 96, 1,1},
    [UBIG_PROFILE_GAME]    = {"Game",    1,0, 0,7, 0,10, 0, 0,1, 10,10,10, 96,11,0},
    [UBIG_PROFILE_VOICE]   = {"Voice",   0,0, 1,8, 0,10, 0, 0,0, 10,10,10, 96, 1,0},
    [UBIG_PROFILE_COURSE]  = {"Course",  1,0, 1,5, 0,10, 0, 0,0, 10,10,10, 64, 1,0},
    [UBIG_PROFILE_CUSTOM]  = {"Custom",  1,3, 1,10,0,10, 0,48,1, 10,10,10, 96,11,0},
};

const char *ubig_profile_name(ubig_profile p)
{
    return p >= 0 && p < UBIG_PROFILE_COUNT ? contracts[p].name : "Unknown";
}

static int eqname(const char *a, const char *b)
{
    while (*a && *b) {
        unsigned ca=(unsigned char)*a++, cb=(unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == 0 && *b == 0;
}

int ubig_profile_parse(const char *name, ubig_profile *out)
{
    if (!name || !out) return UBIG_EINVAL;
    for (int i = 0; i < UBIG_PROFILE_COUNT; ++i) {
        if (eqname(name, contracts[i].name)) {
            *out = (ubig_profile)i;
            return UBIG_OK;
        }
    }
    if (eqname(name, "onlinecourse") || eqname(name, "online-course")) {
        *out = UBIG_PROFILE_COURSE;
        return UBIG_OK;
    }
    if (eqname(name, "personal") || eqname(name, "personalize")) {
        *out = UBIG_PROFILE_CUSTOM;
        return UBIG_OK;
    }
    return UBIG_EINVAL;
}

int ubig_profile_uses_alternate_first_stage(ubig_profile p)
{
    return p >= 0 && p < UBIG_PROFILE_COUNT ? contracts[p].alternate_first_stage_family : 1;
}
