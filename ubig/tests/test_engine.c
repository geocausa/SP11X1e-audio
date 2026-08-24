#include "ubig/ubig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng=0x9e3779b9u;
static float rf(void){rng=rng*1664525u+1013904223u;int32_t v=(int32_t)(rng>>8)-(1<<23);return(float)v/(float)(1<<24);}
static void fill(float *l,float *r,size_t n){for(size_t i=0;i<n;i++){l[i]=rf();r[i]=rf();}}

static ubig_engine *make_engine(ubig_profile p)
{
    ubig_engine_config cfg={UBIG_ABI_VERSION,UBIG_SAMPLE_RATE,UBIG_CHANNELS,p};
    return ubig_engine_create(&cfg);
}

static int cold_profile_equivalence(void)
{
    enum{N=2048};
    float il[N],ir[N],rl[N],rr[N],pl[N],pr[N];
    fill(il,ir,N);
    ubig_engine *ref=make_engine(UBIG_PROFILE_DYNAMIC);if(!ref)return 20;
    if(ubig_engine_process(ref,il,ir,rl,rr,N))return 21;
    ubig_engine_destroy(ref);
    for(int p=0;p<UBIG_PROFILE_COUNT;p++){
        ubig_engine *e=make_engine((ubig_profile)p);if(!e)return 22+p;
        if(ubig_engine_profile(e)!=(ubig_profile)p)return 40+p;
        if(ubig_engine_process(e,il,ir,pl,pr,N))return 60+p;
        if(memcmp(rl,pl,sizeof rl)||memcmp(rr,pr,sizeof rr)){
            fprintf(stderr,"cold Stage-A profile divergence: %s\n",ubig_profile_name((ubig_profile)p));
            return 80+p;
        }
        ubig_engine_destroy(e);
    }
    return 0;
}

static int transition_history_equivalence(void)
{
    enum{N=6144};
    static const ubig_profile seq[]={
        UBIG_PROFILE_DYNAMIC,UBIG_PROFILE_MOVIE,UBIG_PROFILE_MUSIC,
        UBIG_PROFILE_GAME,UBIG_PROFILE_VOICE,UBIG_PROFILE_COURSE,
        UBIG_PROFILE_CUSTOM,UBIG_PROFILE_DYNAMIC
    };
    static const size_t chunks[]={127,353,1,64,480,511,257,389};
    float il[N],ir[N],al[N],ar[N],bl[N],br[N];fill(il,ir,N);
    ubig_engine *a=make_engine(UBIG_PROFILE_DYNAMIC),*b=make_engine(UBIG_PROFILE_DYNAMIC);if(!a||!b)return 100;
    size_t pos=0;unsigned k=0;
    while(pos<N){
        const ubig_profile p=seq[k%(sizeof seq/sizeof seq[0])];
        if(ubig_engine_set_profile(a,p))return 101;
        if(ubig_engine_profile(a)!=p)return 102;
        size_t n=chunks[k%(sizeof chunks/sizeof chunks[0])];if(n>N-pos)n=N-pos;
        if(ubig_engine_process(a,il+pos,ir+pos,al+pos,ar+pos,n))return 103;
        if(ubig_engine_process(b,il+pos,ir+pos,bl+pos,br+pos,n))return 104;
        pos+=n;k++;
    }
    if(memcmp(al,bl,sizeof al)||memcmp(ar,br,sizeof ar)){
        fprintf(stderr,"profile transition reset/Stage-A divergence\n");return 105;
    }
    if(ubig_engine_set_profile(a,UBIG_PROFILE_COUNT)!=UBIG_EINVAL)return 106;
    ubig_engine_destroy(a);ubig_engine_destroy(b);return 0;
}

int main(void)
{
    int rc=cold_profile_equivalence();if(rc)return rc;
    rc=transition_history_equivalence();if(rc)return rc;
    ubig_engine *e=make_engine(UBIG_PROFILE_CUSTOM);if(!e)return 120;
    int32_t eq[UBIG_EQ_BANDS]={0};eq[0]=-192;eq[19]=192;if(ubig_engine_set_custom_eq(e,eq))return 121;
    eq[4]=193;if(ubig_engine_set_custom_eq(e,eq)!=UBIG_EINVAL)return 122;
    ubig_engine_destroy(e);
    puts("PASS engine exact Stage-A seven-profile lifecycle");return 0;
}
