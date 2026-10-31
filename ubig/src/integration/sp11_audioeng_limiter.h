#ifndef SP11_AUDIOENG_LIMITER_H
#define SP11_AUDIOENG_LIMITER_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/* Exact 48-kHz stereo state machine recovered from Microsoft AudioEng.dll
 * CAudioLimiter.  The production SP11 endpoint is fixed at stereo/48 kHz, so
 * the generic 16/32/64/128-frame rate buckets collapse to the Windows 64-frame
 * look-ahead used by this graph. */
#define SP11_AUDIOENG_LIMITER_LOOKAHEAD 64u
#define SP11_AUDIOENG_LIMITER_CEILING 0.9850000143051147f
#define SP11_AUDIOENG_LIMITER_CATASTROPHIC 128.0f
#define SP11_AUDIOENG_LIMITER_RELEASE_K 2.205

typedef struct {
    float delay[SP11_AUDIOENG_LIMITER_LOOKAHEAD][2];
    uint32_t delay_pos;
    float gain;
    float envelope;
    float target;
    float step;
    uint32_t attack_left;
    double release_up;
    double release_down;
    uint32_t catastrophic_guard;
} Sp11AudioEngLimiter;

static inline void sp11_audioeng_limiter_init(Sp11AudioEngLimiter *l){
    memset(l,0,sizeof(*l));
    l->gain=1.0f;
    l->envelope=SP11_AUDIOENG_LIMITER_CEILING;
    l->target=1.0f;
    l->release_up=exp(SP11_AUDIOENG_LIMITER_RELEASE_K/48000.0);
    l->release_down=exp(-SP11_AUDIOENG_LIMITER_RELEASE_K/48000.0);
}

static inline void sp11_audioeng_limiter_process_frame(
    Sp11AudioEngLimiter *l,float in_l,float in_r,float *out_l,float *out_r)
{
    float p=fmaxf(fabsf(in_l),fabsf(in_r));

    if(p>l->envelope){
        if(p>SP11_AUDIOENG_LIMITER_CATASTROPHIC){
            /* Matches the original APO's catastrophic-input early-out guard.
             * Keep the delayed sample silent instead of propagating an invalid
             * block into the endpoint. Normal audio never enters this path. */
            l->catastrophic_guard=1;
        } else {
            float new_target=SP11_AUDIOENG_LIMITER_CEILING/p;
            if(new_target<l->gain){
                if(l->attack_left==0){
                    l->target=new_target;
                    l->step=(l->gain-new_target)/(float)SP11_AUDIOENG_LIMITER_LOOKAHEAD;
                    l->attack_left=SP11_AUDIOENG_LIMITER_LOOKAHEAD;
                } else {
                    float new_step=(l->gain-new_target)/(float)SP11_AUDIOENG_LIMITER_LOOKAHEAD;
                    if(l->gain-(float)l->attack_left*new_step<=l->target){
                        l->step=new_step;
                        l->attack_left=SP11_AUDIOENG_LIMITER_LOOKAHEAD;
                    } else if(l->step>0.0f){
                        float extra=(l->target-new_target)/l->step;
                        l->attack_left+=(uint32_t)ceilf(extra);
                    }
                    l->target=new_target;
                }
                l->target=new_target;
            }
            l->envelope=p;
        }
    }

    if(l->attack_left==0){
        if((double)p+(double)p<=(double)l->envelope && l->gain<1.0f){
            l->envelope=(float)((double)l->envelope*l->release_down);
            l->gain=(float)((double)l->gain*l->release_up);
            if(l->gain>=1.0f || l->envelope<=SP11_AUDIOENG_LIMITER_CEILING){
                l->gain=1.0f;
                l->envelope=SP11_AUDIOENG_LIMITER_CEILING;
            }
        }
    } else {
        l->attack_left--;
        l->gain-=l->step;
    }

    float dl=l->delay[l->delay_pos][0];
    float dr=l->delay[l->delay_pos][1];
    l->delay[l->delay_pos][0]=in_l;
    l->delay[l->delay_pos][1]=in_r;
    l->delay_pos++;
    if(l->delay_pos==SP11_AUDIOENG_LIMITER_LOOKAHEAD)l->delay_pos=0;

    if(l->catastrophic_guard){
        *out_l=0.0f;*out_r=0.0f;
    } else {
        *out_l=dl*l->gain;
        *out_r=dr*l->gain;
    }
}

#endif
