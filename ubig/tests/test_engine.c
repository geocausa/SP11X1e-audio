#include "ubig/ubig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rng=0x9e3779b9u;static float rf(void){rng=rng*1664525u+1013904223u;int32_t v=(int32_t)(rng>>8)-(1<<23);return(float)v/(float)(1<<24);}
int main(void)
{
    ubig_engine_config cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,UBIG_PROFILE_DYNAMIC};
    ubig_engine *a=ubig_engine_create(&cfg),*b=ubig_engine_create(&cfg);if(!a||!b)return 2;
    if(ubig_engine_profile(a)!=UBIG_PROFILE_DYNAMIC)return 3;
    if(ubig_engine_set_profile(a,UBIG_PROFILE_GAME))return 4;
    if(ubig_engine_profile(a)!=UBIG_PROFILE_GAME)return 5;
    if(ubig_engine_set_profile(a,UBIG_PROFILE_MOVIE)!=UBIG_EUNSUPPORTED)return 6;
    if(ubig_engine_profile(a)!=UBIG_PROFILE_GAME)return 7;
    ubig_engine_config alt={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,UBIG_PROFILE_MUSIC};if(ubig_engine_create(&alt)!=NULL)return 8;
    int32_t eq[UBIG_EQ_BANDS]={0};eq[0]=-192;eq[19]=192;if(ubig_engine_set_custom_eq(a,eq))return 9;eq[4]=193;if(ubig_engine_set_custom_eq(a,eq)!=UBIG_EINVAL)return 10;
    enum{N=1536};float il[N],ir[N],al[N],ar[N],bl[N],br[N];for(int i=0;i<N;i++){il[i]=rf();ir[i]=rf();}
    if(ubig_engine_process(a,il,ir,al,ar,N))return 11;
    size_t pos=0;const size_t chunks[]={127,353,1,64,480,511};for(unsigned k=0;pos<N;k++){size_t n=chunks[k%6];if(n>N-pos)n=N-pos;if(ubig_engine_process(b,il+pos,ir+pos,bl+pos,br+pos,n))return 12;pos+=n;}
    if(memcmp(al,bl,sizeof al)||memcmp(ar,br,sizeof ar)){fprintf(stderr,"engine chunk invariance mismatch\n");return 13;}
    ubig_engine_destroy(a);ubig_engine_destroy(b);
    puts("PASS engine exact Stage-A/profile-family lifecycle");return 0;
}
