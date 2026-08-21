#include "stage_a_analyzer.h"
#include "stage_a_math.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
static void pack_complex4(float *dst,const float v[4],const float *tw){
    for(unsigned l=0;l<4;l++){dst[2*l]=v[l]*tw[l];dst[2*l+1]=v[l]*tw[4+l];}
}
static void prepack(UbigStageAAnalyzerState *st,const UbigStageAAnalyzerDesc*d,const float*in,float*out){
    unsigned j=0;
    for(;j<d->hop_frames;j+=4){float v[4];for(unsigned l=0;l<4;l++)v[l]=st->history[j+l]*d->edge_window[j+l];pack_complex4(out+2*j,v,d->pre_twiddle+2*j);}
    for(;j<d->block_frames;j+=4){float v[4];for(unsigned l=0;l<4;l++)v[l]=in[j-d->hop_frames+l];pack_complex4(out+2*j,v,d->pre_twiddle+2*j);}
    int edge=(int)d->hop_frames-4;
    for(;j<d->transform_span;j+=4,edge-=4){float v[4],w[4];for(unsigned l=0;l<4;l++){v[l]=in[j-d->hop_frames+l];st->history[j-d->block_frames+l]=v[l];w[l]=d->edge_window[edge+3-(int)l];v[l]*=w[l];}pack_complex4(out+2*j,v,d->pre_twiddle+2*j);}
    for(;j<d->transform_span;j++)out[2*j]=out[2*j+1]=0.0f;
}
static void unpack(const float *packed,float *dst,unsigned n){
    for(unsigned s=0;s<n/2;s+=4){const float*f=packed+2*s;const float*b=packed+(2*n-8-2*s);float*a=dst+4*s;
        a[0]=f[0];a[1]=b[6];a[2]=f[2];a[3]=b[4];
        a[4]=f[1];a[5]=b[7];a[6]=f[3];a[7]=b[5];
        a[8]=f[4];a[9]=b[2];a[10]=f[6];a[11]=b[0];
        a[12]=f[5];a[13]=b[3];a[14]=f[7];a[15]=b[1];
    }
}
static void reduce(float input_scale,const UbigStageAAnalyzerDesc*d,const float*phase,float*out){
    const float *coef=d->reduce_coeff;
    for(unsigned b=0;b<d->bands;b++){
        float even=0.0f,odd=0.0f;unsigned start=d->reduce_start[b],end=start+d->reduce_count[b];
        for(unsigned k=start;k<end;k+=4){
            const float*x=phase+2*k;
            float t0=x[0]*coef[0],t1=x[1]*coef[1],t2=x[2]*coef[2],t3=x[3]*coef[3];
            float t4=x[4]*coef[0],t5=x[5]*coef[1],t6=x[6]*coef[2],t7=x[7]*coef[3];
            even=fmaf(t0,t0,even); odd=fmaf(t1,t1,odd);
            even=fmaf(t2,t2,even); odd=fmaf(t3,t3,odd);
            even=fmaf(t4,t4,even); odd=fmaf(t5,t5,odd);
            even=fmaf(t6,t6,even); odd=fmaf(t7,t7,odd);
            coef+=4;
        }
        float p=even+odd;if(p>0.0f){float v=ubig_stage_a_log2_approx(p*input_scale)*d->log_scale;out[b]=v>-1.0f?v:-1.0f;}else out[b]=-1.0f;
    }
}
int ubig_stage_a_analyzer_process(float input_scale,UbigStageAAnalyzerState *st,
                                  const UbigStageAAnalyzerDesc *d,const float *input,
                                  float *out_bands,float *scratch,unsigned scratch_floats,
                                  UbigSynthTransformFn transform,void *opaque)
{
    if(!st||!d||!input||!out_bands||!scratch||!transform||d->phases!=2||d->bands!=20)
        return -1;
    unsigned n=d->transform_span;
    if(scratch_floats<4*n)
        return -2;
    float *p0=scratch,*p1=scratch+2*n;
    prepack(st,d,input,p0);
    st->phase_index=(st->phase_index+1)%d->phases;
    transform(p1,p0,n,opaque);
    float *phase=st->band_data+(size_t)st->phase_index*2*n;
    unpack(p1,phase,n);
    reduce(input_scale,d,phase,out_bands);
    return 0;
}
