#include "stage_a_lowband.h"
#include "stage_a_math.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static float lf(const unsigned char*p,unsigned o){float v;memcpy(&v,p+o,4);return v;}
static void sf(unsigned char*p,unsigned o,float v){memcpy(p+o,&v,4);}

void ubig_stage_a_lowband_process(void *state,
                                  uint32_t active_channels,
                                  const struct ubig_stage_a_lowband_config *config,
                                  const struct ubig_float_rows *input,
                                  const struct ubig_float_rows *output,
                                  int32_t telemetry[8][20])
{
    unsigned char *s=state;
    memcpy(s+0x20,&active_channels,4);
    memcpy(s+0x44,config,40);
    memset(telemetry,0,8u*20u*sizeof(int32_t));

    const uint32_t n=config->band_count;
    for(uint32_t ch=0;ch<active_channels;++ch){
        float converted[5]={0};
        ubig_stage_a_exp2_scaled(converted,input->rows[ch],n,0x1.597cp+4f);
        float total=0.0f;
        for(uint32_t i=0;i<n;++i) total+=converted[i];
        if(total<0.0f)total=0.0f;
        if(total>1.0f)total=1.0f;
        sf(s,4u*ch,total);

        float prev=lf(s,0x24u+4u*ch);
        float next;
        if(prev<total){
            const float candidate=prev+config->rise_step;
            next=(candidate<total)?candidate:total;
        }else{
            const float candidate=prev-config->fall_step;
            next=(total>candidate)?total:candidate;
        }
        if(next<0.0f)next=0.0f;
        if(next>1.0f)next=1.0f;
        sf(s,0x24u+4u*ch,next);
    }

    for(uint32_t ch=0;ch<active_channels;++ch){
        const float level=lf(s,0x24u+4u*ch);
        for(uint32_t band=0;band<5u;++band){
            float gain;
            if(level>config->threshold){
                gain=0.0f;
            }else if(level<config->boundary){
                gain=config->gain[band];
            }else{
                const float numerator=(config->threshold-level)*config->gain[band];
                const float denominator=config->threshold-config->boundary;
                gain=(float)((double)numerator/(double)denominator);
            }
            telemetry[ch][band]=(int32_t)floorf(gain*2080.0f);
            input->rows[ch][band]+=gain;
            output->rows[ch][band]+=gain;
        }
    }
}
