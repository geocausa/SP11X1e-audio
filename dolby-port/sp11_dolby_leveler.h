/*
 * sp11_dolby_leveler.h — Dolby volume leveler decoded from the binary.
 *
 * Block layout mirrors instance + 0xdc8 in DolbyAudioProcessing.dll, as
 * recovered from 0x1800518a0 and 0x180051950:
 *
 *   +0    band count, pending        +40   number of bands
 *   +4    band count, active         +44   pending per-band gains
 *   +8    param A pending            +124  active per-band gains
 *   +12   param A active             +204  dirty flag
 *   +16   param B pending
 *   +20   param B active
 *   +24   param C pending
 *   +28   param C active
 *   +32   pointer to band centre array
 */

#ifndef SP11_DOLBY_LEVELER_H
#define SP11_DOLBY_LEVELER_H

#define SP11_LEV_MAX_BANDS 20

typedef struct {
    int   count_new;      /* +0   */
    int   count_old;      /* +4   */
    int   param_a;        /* +8   */
    int   active_a;       /* +12  */
    int   param_b;        /* +16  in-target  */
    int   active_b;       /* +20  */
    int   param_c;        /* +24  out-target */
    int   active_c;       /* +28  */
    const int *centres;   /* +32  band centre frequencies */
    int   nbands;         /* +40  */
    float gain_pending[SP11_LEV_MAX_BANDS];   /* +44  */
    float gain_active[SP11_LEV_MAX_BANDS];    /* +124 */
    int   dirty;          /* +204 */

    int   in_target;
    int   out_target;
    int   amount;
    int   drc;
    float ratio;
    float slope;
} Sp11Lev;

void  sp11_lev_init(Sp11Lev *l, const int *band_centres, int nbands,
                    int amount, int in_target, int out_target, int drc);
void  sp11_lev_recompute(Sp11Lev *l);
void  sp11_lev_commit(Sp11Lev *l);
float sp11_lev_band_gain(const Sp11Lev *l, int band);

#endif
