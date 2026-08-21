#include "stage_a_fft320_sp11.h"
#include "stage_a_fft320_tables.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define N 320u
#define F (2u * N)

static inline float mla(float acc,float a,float b){return fmaf(a,b,acc);}
static inline float mls(float acc,float a,float b){return fmaf(-a,b,acc);}

static void cmul(float ar,float ai,float br,float bi,float *rr,float *ii)
{
    float r=ar*br;r=fmaf(-ai,bi,r);
    float i=ai*br;i=fmaf(ar,bi,i);
    *rr=r;*ii=i;
}

static void dft4(const float x[4][2],float y[4][2])
{
    const float r0=x[0][0]+x[2][0],r1=x[1][0]+x[3][0];
    const float r2=x[0][0]-x[2][0],bd=x[1][0]-x[3][0];
    const float i0=x[0][1]+x[2][1],i1=x[1][1]+x[3][1];
    const float id=x[0][1]-x[2][1],bi=x[1][1]-x[3][1];
    y[0][0]=r0+r1;y[0][1]=i0+i1;
    y[1][0]=r2+bi;y[1][1]=id-bd;
    y[2][0]=r0-r1;y[2][1]=i0-i1;
    y[3][0]=r2-bi;y[3][1]=id+bd;
}

static void dft8(const float x[8][2],float y[8][2])
{
    float ei[4][2],oi[4][2],e[4][2],o[4][2],tw[4][2];
    for(unsigned i=0;i<4u;i++){
        ei[i][0]=x[2u*i][0];ei[i][1]=x[2u*i][1];
        oi[i][0]=x[2u*i+1u][0];oi[i][1]=x[2u*i+1u][1];
    }
    dft4(ei,e);dft4(oi,o);
    const float s=0x1.6a09e6p-1f;
    tw[0][0]=o[0][0];tw[0][1]=o[0][1];
    float u=o[1][0]*s,v=o[1][1]*s;
    tw[1][0]=u+v;tw[1][1]=v-u;
    tw[2][0]=o[2][1];tw[2][1]=-o[2][0];
    u=o[3][0]*s;v=o[3][1]*s;
    tw[3][0]=v-u;tw[3][1]=-(u+v);
    for(unsigned i=0;i<4u;i++){
        y[i][0]=e[i][0]+tw[i][0];y[i][1]=e[i][1]+tw[i][1];
        y[i+4u][0]=e[i][0]-tw[i][0];y[i+4u][1]=e[i][1]-tw[i][1];
    }
}

static void store8(float *dst,const float y[8][2])
{
    for(unsigned h=0;h<2u;h++)for(unsigned l=0;l<4u;l++){
        dst[h*8u+l]=y[h*4u+l][0];dst[h*8u+4u+l]=y[h*4u+l][1];
    }
}
static void load8(const float *src,float y[8][2])
{
    for(unsigned h=0;h<2u;h++)for(unsigned l=0;l<4u;l++){
        y[h*4u+l][0]=src[h*8u+l];y[h*4u+l][1]=src[h*8u+4u+l];
    }
}

static void entry8(float out[F],const float in[F])
{
    for(unsigned k=0;k<40u;k++){
        float x[8][2],y[8][2];
        for(unsigned m=0;m<8u;m++){
            const unsigned q=k+40u*m;x[m][0]=in[2u*q];x[m][1]=in[2u*q+1u];
        }
        dft8(x,y);store8(out+16u*k,y);
    }
}

static void dft5(float ar,float ai,float br,float bi,float cr,float ci,
                 float dr,float di,float er,float ei,float out[10])
{
    const float c0=ubig_fft320_radix5_c0,c1=ubig_fft320_radix5_c1;
    const float c2=ubig_fft320_radix5_c2,c3=ubig_fft320_radix5_c3;
    const float rbm=br-er,ibm=bi-ei,rbp=br+er,ibp=bi+ei;
    const float rcm=cr-dr,icm=ci-di,rcp=cr+dr,icp=ci+di;
    const float x0=mls(rbm*c0,rcm,c1),x1=mls(ibm*c0,icm,c1);
    const float y0=mla(ibm*c1,icm,c0),y1=mla(rbm*c1,rcm,c0);
    const float z0=mla(rbp*c2,rcp,c3),z1=mla(rbp*c3,rcp,c2);
    const float w0=mla(ibp*c2,icp,c3),w1=mla(ibp*c3,icp,c2);
    const float sr=rbp+rcp,si=ibp+icp,p1r=z0+ar,p1i=w0+ai;
    const float p2r=z1+ar,p2i=w1+ai;
    out[0]=sr+ar;out[1]=si+ai;
    out[2]=p1r+y0;out[3]=p1i-y1;
    out[4]=p2r+x1;out[5]=p2i-x0;
    out[6]=p2r-x1;out[7]=p2i+x0;
    out[8]=p1r-y0;out[9]=p1i+y1;
}

static void mid5(float out[F],const float in[F])
{
    float a[40][8][2];
    for(unsigned k=0;k<40u;k++)load8(in+16u*k,a[k]);
    for(unsigned j=0;j<8u;j++)for(unsigned r=0;r<8u;r++){
        float v[5][2];v[0][0]=a[j][r][0];v[0][1]=a[j][r][1];
        for(unsigned t=1;t<5u;t++){
            const unsigned off=(t-1u)*16u+(r>>2)*8u+(r&3u);
            cmul(a[j+8u*t][r][0],a[j+8u*t][r][1],
                 ubig_fft320_sp11_mid_twiddle[off],ubig_fft320_sp11_mid_twiddle[off+4u],
                 &v[t][0],&v[t][1]);
        }
        float y[10];
        dft5(v[0][0],v[0][1],v[1][0],v[1][1],v[2][0],v[2][1],
             v[3][0],v[3][1],v[4][0],v[4][1],y);
        for(unsigned s=0;s<5u;s++){
            const unsigned base=j*80u+s*16u+(r>=4u?8u:0u),lane=r&3u;
            out[base+lane]=y[2u*s];out[base+4u+lane]=y[2u*s+1u];
        }
    }
}

static void final8_unscaled(float out[F],const float in[F])
{
    for(unsigned s=0;s<5u;s++)for(unsigned r=0;r<8u;r++){
        const unsigned q=r+8u*s;float x[8][2],y[8][2];
        for(unsigned j=0;j<8u;j++){
            const unsigned base=j*80u+s*16u+(r>=4u?8u:0u),lane=r&3u;
            const float ar=in[base+lane],ai=in[base+4u+lane];
            if(j==0u){x[j][0]=ar;x[j][1]=ai;}
            else{
                const unsigned off=(j-1u)*80u+(q>>2)*8u+(q&3u);
                cmul(ar,ai,ubig_fft320_sp11_final_twiddle[off],
                     ubig_fft320_sp11_final_twiddle[off+4u],&x[j][0],&x[j][1]);
            }
        }
        dft8(x,y);
        for(unsigned bin=0;bin<8u;bin++){
            const unsigned k=q+40u*bin;out[2u*k]=y[bin][0];out[2u*k+1u]=y[bin][1];
        }
    }
}

static void final8(float out[F],const float in[F])
{
    const float scale=0x1.99999ap-9f;
    for(unsigned s=0;s<5u;s++)for(unsigned r=0;r<8u;r++){
        const unsigned q=r+8u*s;float x[8][2],y[8][2];
        for(unsigned j=0;j<8u;j++){
            const unsigned base=j*80u+s*16u+(r>=4u?8u:0u),lane=r&3u;
            const float ar=in[base+lane]*scale,ai=in[base+4u+lane]*scale;
            if(j==0u){x[j][0]=ar;x[j][1]=ai;}
            else{
                const unsigned off=(j-1u)*80u+(q>>2)*8u+(q&3u);
                cmul(ar,ai,ubig_fft320_sp11_final_twiddle[off],
                     ubig_fft320_sp11_final_twiddle[off+4u],&x[j][0],&x[j][1]);
            }
        }
        dft8(x,y);
        for(unsigned bin=0;bin<8u;bin++){
            const unsigned k=q+40u*bin;out[2u*k]=y[bin][0];out[2u*k+1u]=y[bin][1];
        }
    }
}

void ubig_stage_a_fft320_sp11(float *out,const float *in,unsigned n,void *opaque)
{
    (void)opaque;
    if(n!=N){if(out!=in)memset(out,0,(size_t)n*2u*sizeof(float));return;}
    float s1[F],s2[F];entry8(s1,in);mid5(s2,s1);final8_unscaled(out,s2);
}

void ubig_stage_a_fft320_sp11_norm320(float *out,const float *in,unsigned n,void *opaque)
{
    (void)opaque;
    if(n!=N){if(out!=in)memset(out,0,(size_t)n*2u*sizeof(float));return;}
    float s1[F],s2[F];entry8(s1,in);mid5(s2,s1);final8(out,s2);
}
