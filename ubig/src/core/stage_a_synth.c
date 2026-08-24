#include "stage_a_synth.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static void transform_post(float *packed,float *tmp,const float *tw,unsigned n,
                           UbigSynthTransformFn fn,void *opaque)
{
    fn(tmp,packed,n,opaque);
    for(unsigned i=0;i<n;i+=4){
        const float *tb=tw+2*i;
        for(unsigned lane=0;lane<4;lane++){
            float ar=tmp[2*(i+lane)], ai=tmp[2*(i+lane)+1];
            float br=tb[lane], bi=tb[4+lane];
            float r=ar*br;
            r=fmaf(-ai,bi,r);
            float im=ar*bi;
            im=fmaf(ai,br,im);
            packed[i+lane]=r;
            packed[n+i+lane]=im;
        }
    }
}

int ubig_stage_a_synth_process(UbigStageASynthState *st,const UbigStageASynthDesc *d,
                               float *output,float *scratch,unsigned scratch_floats,
                               UbigSynthTransformFn transform,void *opaque)
{
    if(!st||!d||!output||!scratch||!transform||d->phases!=2||d->bands!=20)return -1;
    const unsigned n=d->transform_span;
    if(scratch_floats<4*n)return -2;
    float *mix=scratch;
    float *packed=scratch+2*n;
    memset(mix,0,2*n*sizeof(float));

    for(unsigned p=0;p<d->phases;p++){
        unsigned hist=(st->phase_index+d->phases-p)%d->phases;
        const float *in=st->band_data+(size_t)hist*2*n;
        const float *g=st->gains+(size_t)hist*d->bands;
        const uint32_t *start=d->band_start[p],*count=d->band_count[p];
        const float *coef=d->mix_coeff[p];
        for(unsigned b=0;b<d->bands;b++){
            unsigned k=start[b], end=k+count[b];
            float gain=g[b];
            for(;k<end;k+=4){
                size_t q=(size_t)2*k;
                for(unsigned lane=0;lane<4;lane++){
                    float a=in[q+lane]*gain;
                    float c=in[q+4+lane]*gain;
                    float x=mix[q+lane];
                    x=fmaf(a,coef[lane],x);
                    float y=mix[q+4+lane];
                    y=fmaf(-c,coef[lane],y);
                    x=fmaf(-c,coef[4+lane],x);
                    y=fmaf(-a,coef[4+lane],y);
                    mix[q+lane]=x;
                    mix[q+4+lane]=y;
                }
                coef+=8;
            }
        }
    }

    /* Exact 16-float -> two 8-float packing performed by the NEON zip/rev block. */
    for(unsigned s=0;s<n/2;s+=4){
        const float *a=mix+4*s;
        float *front=packed+2*s;
        front[0]=a[0]; front[1]=a[4]; front[2]=a[2]; front[3]=a[6];
        front[4]=a[8]; front[5]=a[12];front[6]=a[10];front[7]=a[14];
        float *back=packed+(2*n-8-2*s);
        back[0]=a[11];back[1]=a[15];back[2]=a[9]; back[3]=a[13];
        back[4]=a[3]; back[5]=a[7]; back[6]=a[1]; back[7]=a[5];
    }

    transform_post(packed,mix,d->post_twiddle,n,transform,opaque);

    const unsigned block=d->block_frames;
    const unsigned tail=n-block;
    unsigned i=0;
    for(;i<2*tail;i++){
        output[i]=packed[i]+st->overlap[i];
        st->overlap[i]=packed[block+i]+st->overlap[block+i];
    }
    for(;i<block;i++){
        output[i]=packed[i]+st->overlap[i];
        st->overlap[i]=packed[block+i];
    }
    for(;i<2*n-block;i++)st->overlap[i]=packed[block+i];
    return 0;
}
