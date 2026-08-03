/*
 * sp11_dolby_stage2.h — interface for the reconstructed Dolby virtual-bass
 * stage. See sp11_dolby_stage2.c for provenance and the honesty notes.
 */

#ifndef SP11_DOLBY_STAGE2_H
#define SP11_DOLBY_STAGE2_H

#define SP11_VB_MAX_CH 8

/* State layout read by FUN_180072890 / FUN_180072908 at x0:
 *   [0]=floor [4]=rate [8]=curve_k [12]=limit [16]=step  */
typedef struct {
    float floor;
    float rate;
    float curve_k;
    float limit;
    float step;
} Sp11EnvCoeffs;

typedef struct {
    Sp11EnvCoeffs fast_c;
    Sp11EnvCoeffs smooth_c;
    float fast;        /* state +0x0c */
    float smooth;      /* state +0x5c */
    int   rising;      /* state +0x1c, boolean written by the follower */
} Sp11EnvState;

typedef struct {
    int          nchan;
    const float *weights;
    int          nweights;
    float        sample_rate;
    Sp11EnvState env[SP11_VB_MAX_CH];
} Sp11VirtualBass;

float sp11_env_fast(Sp11EnvState *st, float target);
float sp11_env_smooth(Sp11EnvState *st, float target);

void  sp11_envelope_follow(Sp11EnvState *st, const float *const *chan_bufs,
                           int nchan, int frame, float offset);

float sp11_harmonics(const float *weights, int nweights, float x);

void  sp11_vb_init(Sp11VirtualBass *vb, int nchan, const float *weights,
                   int nweights, float sample_rate);
void  sp11_vb_process(Sp11VirtualBass *vb, float *const *chans, int nframes,
                      float agc_gain);

#endif /* SP11_DOLBY_STAGE2_H */
