#ifndef UBIG_CONTROL_H
#define UBIG_CONTROL_H

#include <stdint.h>
#include "ubig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UBIG_CONTROL_MAGIC 0x55424947u /* 'UBIG' */
#define UBIG_CONTROL_ABI_VERSION 2u
#define UBIG_CONTROL_FLAG_CUSTOM_EQ_VALID (1u << 0)
#define UBIG_CONTROL_FLAG_POSTGAIN_VALID  (1u << 1)

typedef struct ubig_control_page {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t struct_bytes;
    uint32_t reserved0;

    uint32_t request_generation;
    uint32_t ack_generation;
    uint32_t desired_profile;
    uint32_t active_profile;
    uint32_t desired_flags;
    int32_t  custom_eq[UBIG_EQ_BANDS];
    int32_t  desired_postgain;
    int32_t  active_postgain;

    int32_t  last_error;
    uint32_t engine_flags;
    uint32_t reserved[10];
} ubig_control_page;

typedef struct ubig_control_handle {
    int fd;
    ubig_control_page *page;
    char path[512];
} ubig_control_handle;

int ubig_control_open(ubig_control_handle *h, const char *path_override, int create);
void ubig_control_close(ubig_control_handle *h);
int ubig_control_snapshot(const ubig_control_handle *h, ubig_control_page *out);
int ubig_control_request_profile(ubig_control_handle *h, ubig_profile profile);
int ubig_control_request_custom_eq(ubig_control_handle *h, const int32_t values[UBIG_EQ_BANDS]);
int ubig_control_request_postgain(ubig_control_handle *h, int32_t postgain);

#ifdef __cplusplus
}
#endif
#endif
