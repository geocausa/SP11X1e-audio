#include "stage_b_leveler_primitives.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t rng=0x6a168123u;static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float lo,float hi){return lo+(hi-lo)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned n=0;n<20000;n++){uint32_t mode=ru()&1u;float c[3]={fr(.05f,.95f),fr(.05f,.95f),fr(.05f,.95f)};float a=fr(0,.999f),b=fr(0,.999f),v=fr(.70f,1.25f),o[3];ubig_stage_b_leveler_coeff_triplet(mode,c,a,b,v,&o[0],&o[1],&o[2]);h=h64(h,o,sizeof o);}if(h!=0xbb435c3d5066b2bcULL){fprintf(stderr,"Stage-B leveler primitive hash %016llx\n",(unsigned long long)h);return 2;}
UbigStageBLevelerHistory st;memset(&st,0,sizeof st);for(unsigned i=0;i<51;i++)st.bins[i]=fr(-.1f,.1f);st.total=fr(-.1f,.1f);st.count=ru()%81;for(unsigned i=0;i<80;i++){st.ring_bin[i]=ru()%50;st.ring_lo[i]=fr(-.01f,.01f);st.ring_hi[i]=fr(-.01f,.01f);st.ring_total[i]=fr(0,.02f);}st.ring_pos=ru()%80;st.phase=fr(0,.499f);st.reset_max=ru()&1u;st.max_a=fr(.45f,.85f);st.max_b=fr(.001f,1.0f);for(unsigned n=0;n<20000;n++)ubig_stage_b_leveler_history_update(&st,fr(.001f,.20f),fr(.40f,.90f),fr(.001f,1.2f));uint64_t hh=h64(1469598103934665603ULL,&st,sizeof st);if(hh!=0x2244caafb36558e1ULL){fprintf(stderr,"Stage-B history hash %016llx\n",(unsigned long long)hh);return 3;}puts("PASS Stage-B leveler primitive regressions");return 0;}
