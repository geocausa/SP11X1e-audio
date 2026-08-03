/* sp11_full_chain_core_test.c
 * Standalone live-core gate before LADSPA: mapped VLLDP DLL analyzer ->
 * native leveler -> exact export smoother -> gain store -> native synthesis.
 */
#include "sp11_vlldp_pe_loader.h"
#include "sp11_vlldp_init_pack.h"
#include "sp11_vlldp_v8_runtime_contract.h"
#include "sp11_vlldp_fun18001de90.h"

#include <math.h>

#define ANALYZER_VA      0x180023DB0ULL
#define LEVELER_VA       0x180021E80ULL
#define SYNTHESIS_VA     0x1800240E0ULL
#define STATE_INIT_VA    0x180021DA8ULL
#define TABLE_48K_256    0x180116C40ULL
#define SYNTH_DESC_VA    0x180116A20ULL
#define ANALYZER_CB_VA   0x18014D390ULL

#define CHANNELS 2
#define FRAME_SIZE 256
#define BANDS 20
#define PHASE_COUNT 2
#define MATRIX_SPAN 640
#define WINDOW_FLOATS 128
#define BAND_FLOATS (PHASE_COUNT * MATRIX_SPAN)
#define GAIN_FLOATS 128
#define OVERLAP_FLOATS MATRIX_SPAN
#define OPT_STATE_SIZE 0x900
#define ANALYZER_SCRATCH_FLOATS 8192
#define SYNTH_SCRATCH_FLOATS 4096

typedef void (*AnalyzerFn)(float, void*, void*, void*, void*, void*);
typedef void (*LevelerFn)(void*, void*, void*, void*, void*, void*, void*,
                          uint32_t, float, float, float, void*, void*, void*, void*);
typedef uint64_t (*SynthesisFn)(void*, void*, void*, void*, void*);
typedef void* (*InitFn)(void*, void*, void*);

typedef struct { uint32_t count; uint32_t pad; uint64_t ptrs; } Desc;

typedef struct {
    uint8_t state[0x40];
    float window[WINDOW_FLOATS];
    float band[BAND_FLOATS];
    float gains[GAIN_FLOATS];
    float overlap[OVERLAP_FLOATS];
    float input[FRAME_SIZE];
    float analyzed[BANDS];
    float output[FRAME_SIZE];
} LiveChannel;

typedef struct {
    uint64_t windows_base;
    uint8_t opt_state[OPT_STATE_SIZE];
    int has_opt_state;
    float side_a[BANDS];
    float side_b[BANDS];
    int32_t mask[BANDS];
    float runtime[5];
    float p12[BANDS];
    int has_live_fields;
    float analyzer_boundary[CHANNELS][BANDS];
    float phase6_boundary[CHANNELS][BANDS];
    int32_t target_b60[BANDS];
    int has_boundary;
} Seed;

static void wr_u64(uint8_t *p, uint64_t value)
{
    memcpy(p, &value, sizeof(value));
}

static uint32_t float_to_u32(float value)
{
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float u32_to_float(uint32_t value)
{
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float gain_convert_one(float value)
{
    const float c3 = u32_to_float(0x3D714000U);
    const float c2 = u32_to_float(0x3E827800U);
    const float c1 = u32_to_float(0x3F2FB000U);
    const float scale = 21.5927734375f;
    float scaled = value * scale;
    int truncated = (int)scaled;
    float truncated_f32 = (float)truncated;
    float correction = truncated_f32 > scaled ? 1.0f : 0.0f;
    float floored = truncated_f32 - correction;
    float fraction = scaled - floored;
    int32_t fixed_bits = (int32_t)((int)floored << 23);
    float fraction2 = fraction * fraction;
    float fraction3 = fraction2 * fraction;
    float polynomial = 1.0f + fraction * c1 + fraction2 * c2 + fraction3 * c3;
    return u32_to_float((float_to_u32(polynomial) + (uint32_t)fixed_bits) & 0xFFFFFFFFU);
}

static void init_live_channel(LiveChannel *ch, const Sp11PeImage *img)
{
    memset(ch, 0, sizeof(*ch));
    for (int i = 0; i < GAIN_FLOATS; i++)
        ch->gains[i] = 1.0f;
    wr_u64(ch->state + 0x00, (uint64_t)(uintptr_t)ch->window);
    wr_u64(ch->state + 0x08, (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(img, ANALYZER_CB_VA));
    wr_u64(ch->state + 0x18, (uint64_t)(uintptr_t)ch->band);
    wr_u64(ch->state + 0x20, (uint64_t)(uintptr_t)ch->overlap);
    wr_u64(ch->state + 0x28, (uint64_t)(uintptr_t)ch->gains);
}

static int read_exact(FILE *fh, void *dst, size_t size)
{
    return fread(dst, 1, size, fh) == size ? 0 : -1;
}

static uint32_t read_u32_file(FILE *fh)
{
    uint32_t value = 0;
    if (fread(&value, sizeof(value), 1, fh) != 1)
        return 0;
    return value;
}

static int read_f32_block(FILE *fh, float *dst, uint32_t capacity)
{
    uint32_t count = read_u32_file(fh);
    if (count > capacity)
        return -1;
    if (fread(dst, sizeof(float), count, fh) != count)
        return -1;
    for (uint32_t index = count; index < capacity; index++)
        dst[index] = 0.0f;
    return 0;
}

static int read_i32_block(FILE *fh, int32_t *dst, uint32_t capacity)
{
    uint32_t count = read_u32_file(fh);
    if (count > capacity)
        return -1;
    if (fread(dst, sizeof(int32_t), count, fh) != count)
        return -1;
    for (uint32_t index = count; index < capacity; index++)
        dst[index] = 0;
    return 0;
}

static void relocate_state_ptr(uint8_t *state, int offset, void *ptr)
{
    wr_u64(state + offset, (uint64_t)(uintptr_t)ptr);
}

static int load_seed(const char *path, const Sp11PeImage *img, Seed *seed, LiveChannel ch[CHANNELS])
{
    FILE *fh = fopen(path, "rb");
    if (!fh)
        return -1;
    char magic[4];
    if (read_exact(fh, magic, sizeof(magic)) != 0 || memcmp(magic, "VFC1", 4) != 0) {
        fclose(fh);
        return -2;
    }
    if (fread(&seed->windows_base, sizeof(seed->windows_base), 1, fh) != 1) {
        fclose(fh);
        return -3;
    }
    uint32_t opt_size = read_u32_file(fh);
    if (opt_size != OPT_STATE_SIZE || read_exact(fh, seed->opt_state, OPT_STATE_SIZE) != 0) {
        fclose(fh);
        return -4;
    }
    seed->has_opt_state = 1;
    for (int c = 0; c < CHANNELS; c++) {
        uint32_t state_size = read_u32_file(fh);
        if (state_size != sizeof(ch[c].state) || read_exact(fh, ch[c].state, sizeof(ch[c].state)) != 0) {
            fclose(fh);
            return -5;
        }
        if (read_f32_block(fh, ch[c].window, WINDOW_FLOATS) != 0 ||
            read_f32_block(fh, ch[c].band, BAND_FLOATS) != 0 ||
            read_f32_block(fh, ch[c].overlap, OVERLAP_FLOATS) != 0 ||
            read_f32_block(fh, ch[c].gains, GAIN_FLOATS) != 0) {
            fclose(fh);
            return -6;
        }
        uint64_t callback = sp11_rd64(ch[c].state + 0x08);
        if (seed->windows_base && callback >= seed->windows_base && callback < seed->windows_base + img->size) {
            callback = (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(img, img->image_base + (callback - seed->windows_base));
        } else {
            callback = (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(img, ANALYZER_CB_VA);
        }
        relocate_state_ptr(ch[c].state, 0x00, ch[c].window);
        wr_u64(ch[c].state + 0x08, callback);
        relocate_state_ptr(ch[c].state, 0x18, ch[c].band);
        relocate_state_ptr(ch[c].state, 0x20, ch[c].overlap);
        relocate_state_ptr(ch[c].state, 0x28, ch[c].gains);
    }
    if (read_f32_block(fh, seed->side_a, BANDS) == 0 &&
        read_f32_block(fh, seed->side_b, BANDS) == 0 &&
        read_i32_block(fh, seed->mask, BANDS) == 0 &&
        read_f32_block(fh, seed->runtime, 5) == 0 &&
        read_f32_block(fh, seed->p12, BANDS) == 0) {
        seed->has_live_fields = 1;
    }
    if (read_f32_block(fh, seed->analyzer_boundary[0], BANDS) == 0 &&
        read_f32_block(fh, seed->analyzer_boundary[1], BANDS) == 0 &&
        read_f32_block(fh, seed->phase6_boundary[0], BANDS) == 0 &&
        read_f32_block(fh, seed->phase6_boundary[1], BANDS) == 0 &&
        read_i32_block(fh, seed->target_b60, BANDS) == 0) {
        seed->has_boundary = 1;
    }
    fclose(fh);
    return 0;
}

static int relocate_embedded_vlldp_pointers(uint8_t *state, uint32_t state_size, const Sp11PeImage *img, uint64_t windows_base)
{
    int patched = 0;
    for (uint32_t off = 0; off + 8 <= state_size; off += 8) {
        uint64_t value = sp11_rd64(state + off);
        if (windows_base && value >= windows_base && value < windows_base + img->size) {
            uint64_t reloc = (uint64_t)(uintptr_t)sp11_pe_ptr_for_va(img, img->image_base + (value - windows_base));
            wr_u64(state + off, reloc);
            patched++;
        }
    }
    return patched;
}

static int read_wav_block(FILE *fh, float left[FRAME_SIZE], float right[FRAME_SIZE])
{
    for (int i = 0; i < FRAME_SIZE; i++) {
        int16_t samples[2];
        if (fread(samples, sizeof(samples), 1, fh) != 1)
            return -1;
        left[i] = (float)samples[0] / 32768.0f;
        right[i] = (float)samples[1] / 32768.0f;
    }
    return 0;
}

static int skip_wav_blocks(FILE *fh, int blocks)
{
    if (blocks <= 0)
        return 0;
    long bytes = (long)blocks * FRAME_SIZE * CHANNELS * (long)sizeof(int16_t);
    return fseek(fh, bytes, SEEK_CUR) == 0 ? 0 : -1;
}

static int skip_wav_header(FILE *fh)
{
    uint8_t header[44];
    if (fread(header, 1, sizeof(header), fh) != sizeof(header))
        return -1;
    if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0)
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    const char *dll = (argc > 1) ? argv[1] :
        "/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/SOURCE/Dolby/SpeakerDLLs/DolbyAPOvlldp150.dll";
    const char *wav = (argc > 2) ? argv[2] :
        "/run/media/ubi/Local Disk/Users/GEOCA/Documents/Research_Hub/Audio/outputs/vlldp_state_runs/20260615_223202_dolby_dynamic_clean_tone_v8_analyzer/test_tone_997hz_5s.wav";
    const int blocks = (argc > 3) ? atoi(argv[3]) : 200;
    const char *seed_path = (argc > 4) ? argv[4] : NULL;
    const char *target_seed_path = (argc > 5) ? argv[5] : NULL;
    const int start_block = (argc > 6) ? atoi(argv[6]) : 0;

    Sp11PeImage img;
    if (sp11_pe_load(&img, dll) != 0) {
        fprintf(stderr, "pe load failed\n");
        return 1;
    }

    uint64_t tbl[8];
    for (int i = 0; i < 8; i++)
        tbl[i] = sp11_pe_read_u64_va(&img, TABLE_48K_256 + (uint64_t)i * 8);
    void *analyzer_desc = (void *)(uintptr_t)tbl[0];
    void *synth_desc = sp11_pe_ptr_for_va(&img, SYNTH_DESC_VA);

    Seed seed;
    memset(&seed, 0, sizeof(seed));
    LiveChannel ch[CHANNELS];
    init_live_channel(&ch[0], &img);
    init_live_channel(&ch[1], &img);

    uint8_t opt_state[OPT_STATE_SIZE];
    memset(opt_state, 0, sizeof(opt_state));
    if (seed_path && strcmp(seed_path, "-") != 0) {
        int seed_rc = load_seed(seed_path, &img, &seed, ch);
        if (seed_rc != 0) {
            fprintf(stderr, "seed load failed rc=%d path=%s\n", seed_rc, seed_path);
            return 2;
        }
        memcpy(opt_state, seed.opt_state, sizeof(opt_state));
        int patched = relocate_embedded_vlldp_pointers(opt_state, OPT_STATE_SIZE, &img, seed.windows_base);
        fprintf(stderr, "loaded captured seed, relocated optimizer pointers=%d\n", patched);
    } else {
        InitFn init = (InitFn)sp11_pe_ptr_for_va(&img, STATE_INIT_VA);
        if (init((void *)(uintptr_t)tbl[2], (void *)(uintptr_t)tbl[6], opt_state) != opt_state) {
            fprintf(stderr, "optimizer init returned unexpected pointer\n");
            return 2;
        }
    }

    Seed target_seed;
    memset(&target_seed, 0, sizeof(target_seed));
    if (target_seed_path && strcmp(target_seed_path, "-") != 0) {
        LiveChannel target_ch[CHANNELS];
        init_live_channel(&target_ch[0], &img);
        init_live_channel(&target_ch[1], &img);
        int target_rc = load_seed(target_seed_path, &img, &target_seed, target_ch);
        if (target_rc != 0) {
            fprintf(stderr, "target seed load failed rc=%d path=%s\n", target_rc, target_seed_path);
            return 2;
        }
        fprintf(stderr, "loaded target seed path=%s\n", target_seed_path);
    } else {
        target_seed = seed;
    }

    float side_a[BANDS], side_b[BANDS], runtime[5], p12[BANDS];
    int32_t mask[BANDS];
    for (int b = 0; b < BANDS; b++) {
        side_a[b] = (float)sp11_vlldp_threshold_low[b] / 2080.0f;
        side_b[b] = (float)sp11_vlldp_threshold_high[b] / 2080.0f;
        mask[b] = sp11_vlldp_isolated_bands[b];
        p12[b] = (b < SP11_VLLDP_STRESS_VALUES) ? (float)sp11_vlldp_stress_amount[b] / 2080.0f : 0.0f;
    }
    runtime[0] = 0.75f;
    runtime[1] = -0.26201921701431274f;
    runtime[2] = 0.0f;
    runtime[3] = 0.0f;
    runtime[4] = 0.0f;
    if (seed.has_live_fields) {
        memcpy(side_a, seed.side_a, sizeof(side_a));
        memcpy(side_b, seed.side_b, sizeof(side_b));
        memcpy(mask, seed.mask, sizeof(mask));
        memcpy(runtime, seed.runtime, sizeof(runtime));
        memcpy(p12, seed.p12, sizeof(p12));
    }

    FILE *fh = fopen(wav, "rb");
    if (!fh || skip_wav_header(fh) != 0) {
        fprintf(stderr, "cannot read wav %s\n", wav);
        return 3;
    }
    if (skip_wav_blocks(fh, start_block) != 0) {
        fprintf(stderr, "cannot seek wav start_block=%d\n", start_block);
        return 3;
    }

    AnalyzerFn analyzer = (AnalyzerFn)sp11_pe_ptr_for_va(&img, ANALYZER_VA);
    LevelerFn leveler = (LevelerFn)sp11_pe_ptr_for_va(&img, LEVELER_VA);
    SynthesisFn synth = (SynthesisFn)sp11_pe_ptr_for_va(&img, SYNTHESIS_VA);

    float analyzer_scratch[ANALYZER_SCRATCH_FLOATS];
    float synth_scratch[SYNTH_SCRATCH_FLOATS];
    float phase6_in[CHANNELS][BANDS], phase6_source[CHANNELS][BANDS], phase6_out[CHANNELS][BANDS];
    uint64_t in_ptrs[CHANNELS], out_ptrs[CHANNELS];
    Desc in_desc = { CHANNELS, 0, (uint64_t)(uintptr_t)in_ptrs };
    Desc out_desc = { CHANNELS, 0, (uint64_t)(uintptr_t)out_ptrs };
    for (int c = 0; c < CHANNELS; c++) {
        in_ptrs[c] = (uint64_t)(uintptr_t)phase6_in[c];
        out_ptrs[c] = (uint64_t)(uintptr_t)phase6_out[c];
    }

    float previous_bbc[BANDS];
    for (int b = 0; b < BANDS; b++)
        previous_bbc[b] = sp11_vlldp_v8_child1_bbc_previous[b];

    int32_t b60[BANDS], target_b60[BANDS], side_scratch[CHANNELS * BANDS * 4], export_count[1];
    int32_t c0c[BANDS];
    float bbc[BANDS];
    int bad_output = 0;
    double input_sumsq[CHANNELS] = {0.0, 0.0};
    double output_sumsq[CHANNELS] = {0.0, 0.0};
    float input_peak[CHANNELS] = {0.0f, 0.0f};
    float output_peak[CHANNELS] = {0.0f, 0.0f};
    unsigned long sample_count = 0;

    int iterations = blocks == 0 ? 1 : blocks;
    for (int block = 0; block < iterations; block++) {
        if (blocks == 0) {
            for (int b = 0; b < BANDS; b++) {
                ch[0].analyzed[b] = seed.has_boundary ? seed.analyzer_boundary[0][b] : sp11_vlldp_v8_analyzer_ch0[b];
                ch[1].analyzed[b] = seed.has_boundary ? seed.analyzer_boundary[1][b] : sp11_vlldp_v8_analyzer_ch1[b];
            }
        }
        if (blocks != 0) {
            if (read_wav_block(fh, ch[0].input, ch[1].input) != 0) {
                fprintf(stderr, "wav ended before block %d\n", block);
                return 4;
            }
            for (int c = 0; c < CHANNELS; c++) {
                for (int i = 0; i < FRAME_SIZE; i++) {
                    float sample = ch[c].input[i];
                    float mag = fabsf(sample);
                    input_sumsq[c] += (double)sample * (double)sample;
                    if (mag > input_peak[c])
                        input_peak[c] = mag;
                }
            }
            for (int c = 0; c < CHANNELS; c++) {
                memset(analyzer_scratch, 0, sizeof(analyzer_scratch));
                analyzer(1.1111111640930176f, ch[c].state, analyzer_desc, ch[c].input, ch[c].analyzed, analyzer_scratch);
            }
        }
        for (int b = 0; b < BANDS; b++) {
            float base0 = seed.has_boundary
                ? seed.phase6_boundary[0][b] - seed.analyzer_boundary[0][b]
                : sp11_vlldp_v8_phase6_input_ch0[b] - sp11_vlldp_v8_analyzer_ch0[b];
            float base1 = seed.has_boundary
                ? seed.phase6_boundary[1][b] - seed.analyzer_boundary[1][b]
                : sp11_vlldp_v8_phase6_input_ch1[b] - sp11_vlldp_v8_analyzer_ch1[b];
            phase6_in[0][b] = blocks == 0 && seed.has_boundary ? seed.phase6_boundary[0][b] :
                (blocks == 0 ? sp11_vlldp_v8_phase6_input_ch0[b] : ch[0].analyzed[b] + base0);
            phase6_in[1][b] = blocks == 0 && seed.has_boundary ? seed.phase6_boundary[1][b] :
                (blocks == 0 ? sp11_vlldp_v8_phase6_input_ch1[b] : ch[1].analyzed[b] + base1);
            phase6_source[0][b] = phase6_in[0][b];
            phase6_source[1][b] = phase6_in[1][b];
            phase6_out[0][b] = base0;
            phase6_out[1][b] = base1;
            b60[b] = 0;
        }
        memset(side_scratch, 0, sizeof(side_scratch));
        export_count[0] = 0;
        leveler(opt_state, side_a, side_b, mask, runtime, &in_desc, &out_desc,
                CHANNELS, 0.0f, runtime[1], 0.0f, p12, b60, side_scratch, export_count);

        sp11_vlldp_fun18001de90_export(previous_bbc, phase6_source[0], phase6_source[1], bbc, c0c);
        for (int b = 0; b < BANDS; b++)
            previous_bbc[b] = bbc[b];
        for (int c = 0; c < CHANNELS; c++) {
            int phase = *(int32_t *)(ch[c].state + 0x10) % PHASE_COUNT;
            for (int b = 0; b < BANDS; b++)
                ch[c].gains[phase * BANDS + b] = gain_convert_one(bbc[b]);
            for (int i = 0; i < SYNTH_SCRATCH_FLOATS; i++)
                synth_scratch[i] = 0.0f;
            uint64_t rc = synth(ch[c].state, synth_desc, NULL, ch[c].output, synth_scratch);
            if (rc != 0)
                bad_output++;
            for (int i = 0; i < FRAME_SIZE; i++) {
                if (!isfinite(ch[c].output[i]))
                    bad_output++;
                float sample = ch[c].output[i];
                float mag = fabsf(sample);
                output_sumsq[c] += (double)sample * (double)sample;
                if (mag > output_peak[c])
                    output_peak[c] = mag;
            }
        }
        sample_count += FRAME_SIZE;
    }
    fclose(fh);

    long sum = 0;
    int max_abs = 0;
    for (int b = 0; b < BANDS; b++)
        target_b60[b] = target_seed.has_boundary ? target_seed.target_b60[b] :
            (seed.has_boundary ? seed.target_b60[b] : sp11_vlldp_v8_target_b60[b]);
    printf("final_b60=[");
    for (int b = 0; b < BANDS; b++)
        printf("%d%s", b60[b], b + 1 == BANDS ? "" : ", ");
    printf("]\n");
    printf("target_b60=[");
    for (int b = 0; b < BANDS; b++)
        printf("%d%s", target_b60[b], b + 1 == BANDS ? "" : ", ");
    printf("]\n");
    printf("diff=[");
    for (int b = 0; b < BANDS; b++) {
        int d = b60[b] - target_b60[b];
        int ad = d < 0 ? -d : d;
        sum += ad;
        if (ad > max_abs)
            max_abs = ad;
        printf("%d%s", d, b + 1 == BANDS ? "" : ", ");
    }
    printf("]\n");
    printf("analyzed_ch0=[");
    for (int b = 0; b < BANDS; b++)
        printf("%.6f%s", ch[0].analyzed[b], b + 1 == BANDS ? "" : ", ");
    printf("]\n");
    printf("phase6_ch0=[");
    for (int b = 0; b < BANDS; b++)
        printf("%.6f%s", phase6_source[0][b], b + 1 == BANDS ? "" : ", ");
    printf("]\n");
    printf("target_analyzer_ch0=[");
    for (int b = 0; b < BANDS; b++)
        printf("%.6f%s",
               target_seed.has_boundary ? target_seed.analyzer_boundary[0][b] : sp11_vlldp_v8_analyzer_ch0[b],
               b + 1 == BANDS ? "" : ", ");
    printf("]\n");
    double mae = (double)sum / (double)BANDS;
    double denom = sample_count ? (double)sample_count : 1.0;
    double input_rms_l = sqrt(input_sumsq[0] / denom);
    double input_rms_r = sqrt(input_sumsq[1] / denom);
    double output_rms_l = sqrt(output_sumsq[0] / denom);
    double output_rms_r = sqrt(output_sumsq[1] / denom);
    int silent_output = (output_peak[0] <= 1.0e-12f && output_peak[1] <= 1.0e-12f);
    printf("blocks=%d start_block=%d b60_mae=%.2f max_abs=%d bad_output=%d export_count=%d final_c0c0=%d\n",
           blocks, start_block, mae, max_abs, bad_output, export_count[0], c0c[0]);
    printf("input_rms=[%.9g, %.9g] input_peak=[%.9g, %.9g]\n",
           input_rms_l, input_rms_r, input_peak[0], input_peak[1]);
    printf("output_rms=[%.9g, %.9g] output_peak=[%.9g, %.9g] silent_output=%d\n",
           output_rms_l, output_rms_r, output_peak[0], output_peak[1], silent_output);
    printf("RESULT: %s\n", (mae <= 10.0 && bad_output == 0) ? "PASS" : "FAIL");

    sp11_pe_unload(&img);
    return (mae <= 10.0 && bad_output == 0) ? 0 : 5;
}
