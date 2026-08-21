#include "stage_a_fft320.h"
#include "stage_a_fft320_tables.h"
#include <string.h>
typedef struct { float r,i; } C;
static C mul(C a,float br,float bi){C z={a.r*br-a.i*bi,a.r*bi+a.i*br};return z;}
static void fft64(C a[64]){
    for(unsigned i=1,j=0;i<64;i++){
        unsigned bit=32;for(;j&bit;bit>>=1)j^=bit;j^=bit;
        if(i<j){C t=a[i];a[i]=a[j];a[j]=t;}
    }
    for(unsigned len=2;len<=64;len<<=1){
        unsigned half=len>>1,step=64/len;
        for(unsigned base=0;base<64;base+=len){
            for(unsigned j=0;j<half;j++){
                unsigned ti=j*step;C v=mul(a[base+j+half],ubig_w64_re[ti],ubig_w64_im[ti]);C u=a[base+j];
                a[base+j].r=u.r+v.r;a[base+j].i=u.i+v.i;
                a[base+j+half].r=u.r-v.r;a[base+j+half].i=u.i-v.i;
            }
        }
    }
}
void ubig_stage_a_fft320(float *out,const float *in,unsigned n,void *opaque){
    (void)opaque;if(n!=320){if(out!=in)memset(out,0,(size_t)n*2*sizeof(float));return;}
    C f[5][64];
    for(unsigned r=0;r<5;r++){for(unsigned m=0;m<64;m++){unsigned q=r+5*m;f[r][m].r=in[2*q];f[r][m].i=in[2*q+1];}fft64(f[r]);}
    for(unsigned k=0;k<320;k++){
        unsigned q=k&63u;C z=f[0][q];
        for(unsigned r=1;r<5;r++){
            size_t t=(size_t)(r-1)*320+k;C v=mul(f[r][q],ubig_w320_re[t],ubig_w320_im[t]);z.r+=v.r;z.i+=v.i;
        }
        out[2*k]=z.r;out[2*k+1]=z.i;
    }
}

void ubig_stage_a_fft320_norm320(float *out,const float *in,unsigned n,void *opaque)
{
    ubig_stage_a_fft320(out,in,n,opaque);
    if(n==320)
        for(unsigned i=0;i<640;i++) out[i] *= 0x1.99999ap-9f; /* exact float32 1/320 */
}
