#define _GNU_SOURCE
#include "ubig/ubig_control.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int resolve_path(char out[512], const char *override)
{
    if (override && *override)
        return snprintf(out, 512, "%s", override) < 512 ? 0 : -1;
    const char *rt = getenv("XDG_RUNTIME_DIR");
    if (rt && *rt)
        return snprintf(out, 512, "%s/ubig-control-v2", rt) < 512 ? 0 : -1;
    return snprintf(out, 512, "/run/user/%lu/ubig-control-v2", (unsigned long)getuid()) < 512 ? 0 : -1;
}

int ubig_control_open(ubig_control_handle *h, const char *override, int create)
{
    if (!h) return UBIG_EINVAL;
    memset(h, 0, sizeof(*h));
    h->fd = -1;
    if (resolve_path(h->path, override)) return UBIG_EINVAL;

    int flags = O_RDWR | O_CLOEXEC | (create ? O_CREAT : 0);
    h->fd = open(h->path, flags, 0600);
    if (h->fd < 0) return -errno;
    if (fchmod(h->fd, 0600)) { int e=errno; close(h->fd); h->fd=-1; return -e; }

    struct stat st;
    if (fstat(h->fd, &st)) { int e=errno; close(h->fd); h->fd=-1; return -e; }
    if (create && st.st_size != (off_t)sizeof(ubig_control_page)) {
        if (ftruncate(h->fd, sizeof(ubig_control_page))) { int e=errno; close(h->fd); h->fd=-1; return -e; }
    }
    if (!create && st.st_size < (off_t)sizeof(ubig_control_page)) {
        close(h->fd); h->fd=-1; return UBIG_ESTATE;
    }

    void *p = mmap(NULL, sizeof(ubig_control_page), PROT_READ|PROT_WRITE, MAP_SHARED, h->fd, 0);
    if (p == MAP_FAILED) { int e=errno; close(h->fd); h->fd=-1; return -e; }
    h->page = p;

    if (create && (__atomic_load_n(&h->page->magic, __ATOMIC_ACQUIRE) != UBIG_CONTROL_MAGIC ||
                   h->page->abi_version != UBIG_CONTROL_ABI_VERSION ||
                   h->page->struct_bytes != sizeof(ubig_control_page))) {
        memset(h->page, 0, sizeof(*h->page));
        h->page->abi_version = UBIG_CONTROL_ABI_VERSION;
        h->page->struct_bytes = sizeof(ubig_control_page);
        h->page->desired_profile = UBIG_PROFILE_DYNAMIC;
        h->page->active_profile = UBIG_PROFILE_DYNAMIC;
        h->page->desired_postgain = 0;
        h->page->active_postgain = 0;
        __atomic_store_n(&h->page->magic, UBIG_CONTROL_MAGIC, __ATOMIC_RELEASE);
        msync(h->page, sizeof(*h->page), MS_SYNC);
    }
    return UBIG_OK;
}

void ubig_control_close(ubig_control_handle *h)
{
    if (!h) return;
    if (h->page) munmap(h->page, sizeof(*h->page));
    if (h->fd >= 0) close(h->fd);
    memset(h, 0, sizeof(*h));
    h->fd = -1;
}

int ubig_control_snapshot(const ubig_control_handle *h, ubig_control_page *out)
{
    if (!h || !h->page || !out) return UBIG_EINVAL;
    if (__atomic_load_n(&h->page->magic, __ATOMIC_ACQUIRE) != UBIG_CONTROL_MAGIC) return UBIG_ESTATE;
    /* Only one tiny writer is expected in M0. Generation is still sampled twice
       so readers do not report a payload spanning two requests. */
    for (int retry=0; retry<8; ++retry) {
        uint32_t g1=__atomic_load_n(&h->page->request_generation,__ATOMIC_ACQUIRE);
        memcpy(out,h->page,sizeof(*out));
        uint32_t g2=__atomic_load_n(&h->page->request_generation,__ATOMIC_ACQUIRE);
        if (g1==g2) return UBIG_OK;
    }
    return UBIG_ESTATE;
}

int ubig_control_request_profile(ubig_control_handle *h, ubig_profile p)
{
    if (!h || !h->page || p < 0 || p >= UBIG_PROFILE_COUNT) return UBIG_EINVAL;
    __atomic_store_n(&h->page->desired_profile, (uint32_t)p, __ATOMIC_RELAXED);
    __atomic_add_fetch(&h->page->request_generation, 1u, __ATOMIC_RELEASE);
    return UBIG_OK;
}

int ubig_control_request_custom_eq(ubig_control_handle *h, const int32_t v[UBIG_EQ_BANDS])
{
    if (!h || !h->page || !v) return UBIG_EINVAL;
    for (unsigned i=0;i<UBIG_EQ_BANDS;++i) if (v[i] < -192 || v[i] > 192) return UBIG_EINVAL;
    memcpy(h->page->custom_eq, v, sizeof(h->page->custom_eq));
    uint32_t flags=__atomic_load_n(&h->page->desired_flags,__ATOMIC_RELAXED);
    __atomic_store_n(&h->page->desired_flags, flags|UBIG_CONTROL_FLAG_CUSTOM_EQ_VALID, __ATOMIC_RELAXED);
    __atomic_store_n(&h->page->desired_profile, UBIG_PROFILE_CUSTOM, __ATOMIC_RELAXED);
    __atomic_add_fetch(&h->page->request_generation, 1u, __ATOMIC_RELEASE);
    return UBIG_OK;
}

int ubig_control_request_postgain(ubig_control_handle *h, int32_t postgain)
{
    if (!h || !h->page || postgain < -1200 || postgain > 0) return UBIG_EINVAL;
    __atomic_store_n(&h->page->desired_postgain, postgain, __ATOMIC_RELAXED);
    uint32_t flags=__atomic_load_n(&h->page->desired_flags,__ATOMIC_RELAXED);
    __atomic_store_n(&h->page->desired_flags, flags|UBIG_CONTROL_FLAG_POSTGAIN_VALID, __ATOMIC_RELAXED);
    __atomic_add_fetch(&h->page->request_generation, 1u, __ATOMIC_RELEASE);
    return UBIG_OK;
}
