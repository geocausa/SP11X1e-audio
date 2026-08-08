/*
 * Windows-derived VLLDP live init constants.
 *
 * Source evidence:
 *   outputs/vlldp_registry_props/vlldp_linux_init_pack.json
 *   outputs/179_vlldp_child1_state_map_20260613.md
 *   outputs/180_vlldp_registry_to_linux_init_pack_20260613.md
 *
 * Baseline profile:
 *   PreviousOs_Render__268abb8b-c3ee-4c00-998a-add6e681b6b7
 *
 * This is the profile whose PID17 compressor payload carries the live
 * four-group layout [2,7,16,20]. DolbyAPOVR/virtual bass is intentionally not
 * part of this baseline; the persisted sliding-bass curve is zeroed here.
 */

#ifndef SP11_VLLDP_INIT_PACK_H
#define SP11_VLLDP_INIT_PACK_H

#define SP11_VLLDP_BANDS 20
#define SP11_VLLDP_OPT_ROWS 8
#define SP11_VLLDP_GROUPS 4
#define SP11_VLLDP_STRESS_VALUES 8
#define SP11_VLLDP_SLIDING_BASS_POINTS 5

static const int sp11_vlldp_audio_optimizer_gains[SP11_VLLDP_OPT_ROWS][SP11_VLLDP_BANDS] = {
    { -16, 18, 16, 30, 16, -32, -16, -32, -16, -32, -48, -62, -64, -64, -16, -16, -16, 16, 80, 48 },
    {   0, 32, 32, 45, 16,   0, -16, -16, -16,   0, -32, -38, -48, -48,   0,   0,   0, 32, 96, 64 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
    {   0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,  0 },
};

static const int sp11_vlldp_threshold_high[SP11_VLLDP_BANDS] = {
    -74, -112, -192, -237, -238, -226, -157, 0, 0, 0,
      0,    0,    0,    0,    0,    0,    0, 0, 0, 0
};

static const int sp11_vlldp_threshold_low[SP11_VLLDP_BANDS] = {
    -266, -304, -384, -429, -430, -418, -349, -192, -192, -192,
    -192, -192, -192, -192, -192, -192, -192, -192, -192, -192
};

static const int sp11_vlldp_isolated_bands[SP11_VLLDP_BANDS] = {
    1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const int sp11_vlldp_stress_amount[SP11_VLLDP_STRESS_VALUES] = {
    216, 216, 0, 0, 0, 0, 0, 0
};

static const int sp11_vlldp_group_bounds[SP11_VLLDP_GROUPS] = {
    2, 7, 16, 20
};

static const int sp11_vlldp_group_attack_ms[SP11_VLLDP_GROUPS] = {
    3, 10, 10, 10
};

static const int sp11_vlldp_group_release_ms[SP11_VLLDP_GROUPS] = {
    20, 20, 20, 20
};

static const int sp11_vlldp_group_mix_or_offset[SP11_VLLDP_GROUPS] = {
    64, 64, 0, 0
};

static const int sp11_vlldp_sliding_bass_gain_curve[SP11_VLLDP_SLIDING_BASS_POINTS] = {
    0, 0, 0, 0, 0
};

#endif
