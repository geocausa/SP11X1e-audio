/* sp11_vlldp_exact_ladspa_smoke.c
 * Minimal LADSPA host smoke for sp11_vlldp_exact.so.
 *
 * This validates plugin loading, port wiring, bypass behavior, chunked run(),
 * finite active output, non-silence, and no accidental DLL-load fallback to
 * bypass. It is not a Windows exactness oracle.
 */
#include <dlfcn.h>
#include <errno.h>
#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_IN_L 0
#define PORT_IN_R 1
#define PORT_OUT_L 2
#define PORT_OUT_R 3
#define PORT_BYPASS 4
#define MAX_CHUNK 1024

typedef const LADSPA_Descriptor *(*LadspaDescriptorFn)(unsigned long index);

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint32_t data_bytes;
    long data_offset;
} WavInfo;

typedef struct {
    double in_sumsq[2];
    double out_sumsq[2];
    double diff_sumsq[2];
    float in_peak[2];
    float out_peak[2];
    float diff_peak[2];
    uint64_t frames;
    uint64_t finite_bad;
} SmokeStats;

static uint16_t rd16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int read_wav_info(FILE *fh, WavInfo *info)
{
    unsigned char hdr[12];
    if (fread(hdr, 1, sizeof(hdr), fh) != sizeof(hdr))
        return -1;
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
        return -2;

    int saw_fmt = 0;
    int saw_data = 0;
    memset(info, 0, sizeof(*info));
    while (!saw_data) {
        unsigned char chunk[8];
        if (fread(chunk, 1, sizeof(chunk), fh) != sizeof(chunk))
            return -3;
        uint32_t size = rd32(chunk + 4);
        long payload = ftell(fh);
        if (payload < 0)
            return -4;

        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[40];
            size_t want = size < sizeof(fmt) ? size : sizeof(fmt);
            if (fread(fmt, 1, want, fh) != want)
                return -5;
            if (want < 16)
                return -6;
            uint16_t audio_format = rd16(fmt + 0);
            info->channels = rd16(fmt + 2);
            info->sample_rate = rd32(fmt + 4);
            info->bits_per_sample = rd16(fmt + 14);
            if (audio_format != 1 || info->channels != 2 || info->bits_per_sample != 16)
                return -7;
            saw_fmt = 1;
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!saw_fmt)
                return -8;
            info->data_bytes = size;
            info->data_offset = payload;
            saw_data = 1;
        }

        if (!saw_data) {
            long next = payload + (long)size + (long)(size & 1U);
            if (fseek(fh, next, SEEK_SET) != 0)
                return -9;
        }
    }
    return 0;
}

static int read_frames(FILE *fh, float *left, float *right, int frames)
{
    for (int i = 0; i < frames; i++) {
        unsigned char raw[4];
        if (fread(raw, 1, sizeof(raw), fh) != sizeof(raw))
            return i;
        int16_t l = (int16_t)rd16(raw);
        int16_t r = (int16_t)rd16(raw + 2);
        left[i] = (float)l / 32768.0f;
        right[i] = (float)r / 32768.0f;
    }
    return frames;
}

static void update_stats(SmokeStats *stats, const float *in_l, const float *in_r,
                         const float *out_l, const float *out_r, int frames)
{
    const float *in[2] = {in_l, in_r};
    const float *out[2] = {out_l, out_r};
    for (int i = 0; i < frames; i++) {
        for (int c = 0; c < 2; c++) {
            float iv = in[c][i];
            float ov = out[c][i];
            float dv = ov - iv;
            if (!isfinite(ov))
                stats->finite_bad++;
            float ia = fabsf(iv);
            float oa = fabsf(ov);
            float da = fabsf(dv);
            if (ia > stats->in_peak[c])
                stats->in_peak[c] = ia;
            if (oa > stats->out_peak[c])
                stats->out_peak[c] = oa;
            if (da > stats->diff_peak[c])
                stats->diff_peak[c] = da;
            stats->in_sumsq[c] += (double)iv * (double)iv;
            stats->out_sumsq[c] += (double)ov * (double)ov;
            stats->diff_sumsq[c] += (double)dv * (double)dv;
        }
    }
    stats->frames += (uint64_t)frames;
}

static int run_pass(
    const LADSPA_Descriptor *desc,
    const char *wav_path,
    float bypass_value,
    SmokeStats *stats)
{
    FILE *fh = fopen(wav_path, "rb");
    if (!fh) {
        fprintf(stderr, "cannot open wav %s: %s\n", wav_path, strerror(errno));
        return 10;
    }
    WavInfo info;
    int info_rc = read_wav_info(fh, &info);
    if (info_rc != 0) {
        fprintf(stderr, "wav parse failed rc=%d path=%s\n", info_rc, wav_path);
        fclose(fh);
        return 11;
    }
    if (fseek(fh, info.data_offset, SEEK_SET) != 0) {
        fclose(fh);
        return 12;
    }

    LADSPA_Handle handle = desc->instantiate(desc, info.sample_rate);
    if (!handle) {
        fclose(fh);
        return 13;
    }

    float in_l[MAX_CHUNK], in_r[MAX_CHUNK], out_l[MAX_CHUNK], out_r[MAX_CHUNK];
    desc->connect_port(handle, PORT_IN_L, in_l);
    desc->connect_port(handle, PORT_IN_R, in_r);
    desc->connect_port(handle, PORT_OUT_L, out_l);
    desc->connect_port(handle, PORT_OUT_R, out_r);
    desc->connect_port(handle, PORT_BYPASS, &bypass_value);
    if (desc->activate)
        desc->activate(handle);

    const int chunks[] = {1, 17, 64, 127, 256, 511, 1024, 3, 509};
    const int chunk_count = (int)(sizeof(chunks) / sizeof(chunks[0]));
    int chunk_index = 0;
    uint32_t remaining_frames = info.data_bytes / (uint32_t)(info.channels * sizeof(int16_t));

    while (remaining_frames > 0) {
        int want = chunks[chunk_index++ % chunk_count];
        if ((uint32_t)want > remaining_frames)
            want = (int)remaining_frames;
        int got = read_frames(fh, in_l, in_r, want);
        if (got <= 0)
            break;
        for (int i = 0; i < got; i++) {
            out_l[i] = 12345.0f;
            out_r[i] = 12345.0f;
        }
        desc->run(handle, (unsigned long)got);
        update_stats(stats, in_l, in_r, out_l, out_r, got);
        remaining_frames -= (uint32_t)got;
        if (got != want)
            break;
    }

    desc->cleanup(handle);
    fclose(fh);
    return 0;
}

static void print_stats(const char *label, const SmokeStats *s)
{
    double denom = s->frames ? (double)s->frames : 1.0;
    printf("%s_frames=%llu\n", label, (unsigned long long)s->frames);
    printf("%s_finite_bad=%llu\n", label, (unsigned long long)s->finite_bad);
    printf("%s_input_rms=[%.9g, %.9g] input_peak=[%.9g, %.9g]\n",
           label, sqrt(s->in_sumsq[0] / denom), sqrt(s->in_sumsq[1] / denom),
           s->in_peak[0], s->in_peak[1]);
    printf("%s_output_rms=[%.9g, %.9g] output_peak=[%.9g, %.9g]\n",
           label, sqrt(s->out_sumsq[0] / denom), sqrt(s->out_sumsq[1] / denom),
           s->out_peak[0], s->out_peak[1]);
    printf("%s_diff_rms=[%.9g, %.9g] diff_peak=[%.9g, %.9g]\n",
           label, sqrt(s->diff_sumsq[0] / denom), sqrt(s->diff_sumsq[1] / denom),
           s->diff_peak[0], s->diff_peak[1]);
}

int main(int argc, char **argv)
{
    const char *plugin_path = argc > 1 ? argv[1] : "./sp11_vlldp_exact.so";
    const char *wav_path = argc > 2 ? argv[2] :
        "../../../../outputs/vlldp_state_runs/20260615_223202_dolby_dynamic_clean_tone_v8_analyzer/test_tone_997hz_5s.wav";

    void *lib = dlopen(plugin_path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    LadspaDescriptorFn descriptor_fn = (LadspaDescriptorFn)dlsym(lib, "ladspa_descriptor");
    if (!descriptor_fn) {
        fprintf(stderr, "missing ladspa_descriptor: %s\n", dlerror());
        dlclose(lib);
        return 2;
    }
    const LADSPA_Descriptor *desc = descriptor_fn(0);
    if (!desc || !desc->instantiate || !desc->connect_port || !desc->run || !desc->cleanup) {
        fprintf(stderr, "invalid LADSPA descriptor\n");
        dlclose(lib);
        return 3;
    }
    printf("plugin_label=%s\n", desc->Label ? desc->Label : "(null)");
    printf("plugin_name=%s\n", desc->Name ? desc->Name : "(null)");
    printf("plugin_ports=%lu\n", desc->PortCount);

    SmokeStats bypass = {0};
    int rc = run_pass(desc, wav_path, 1.0f, &bypass);
    if (rc != 0) {
        dlclose(lib);
        return rc;
    }
    print_stats("bypass", &bypass);

    SmokeStats active = {0};
    rc = run_pass(desc, wav_path, 0.0f, &active);
    if (rc != 0) {
        dlclose(lib);
        return rc;
    }
    print_stats("active", &active);

    int bypass_ok = bypass.finite_bad == 0 &&
        bypass.diff_peak[0] <= 1.0e-7f && bypass.diff_peak[1] <= 1.0e-7f;
    int active_finite = active.finite_bad == 0;
    int active_non_silent = active.out_peak[0] > 1.0e-6f || active.out_peak[1] > 1.0e-6f;
    int active_not_bypass = active.diff_peak[0] > 1.0e-5f || active.diff_peak[1] > 1.0e-5f;
    int active_bounded = active.out_peak[0] < 16.0f && active.out_peak[1] < 16.0f;
    int frames_ok = active.frames > 0 && bypass.frames == active.frames;
    int pass = bypass_ok && active_finite && active_non_silent && active_not_bypass &&
        active_bounded && frames_ok;

    printf("bypass_ok=%d\n", bypass_ok);
    printf("active_finite=%d\n", active_finite);
    printf("active_non_silent=%d\n", active_non_silent);
    printf("active_not_bypass=%d\n", active_not_bypass);
    printf("active_bounded=%d\n", active_bounded);
    printf("frames_ok=%d\n", frames_ok);
    printf("RESULT: %s\n", pass ? "PASS" : "FAIL");

    dlclose(lib);
    return pass ? 0 : 20;
}
