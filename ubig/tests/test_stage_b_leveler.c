#include "stage_b_leveler.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define NR 4
#define NW 20
static uint32_t rng=0x6a2d0123u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static float fb(uint32_t u){float f;memcpy(&f,&u,4);return f;}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
    UbigStageBLevelerState s;UbigStageBLevelerRecord p[NR],q[NR],obs[NR];
    float pv[NR][NW],qv[NR][NW],ov[NR][NW];memset(&s,0,sizeof s);
    s.base=fr(.75f,.999f);s.hold_count=ru()%6;s.adaptive_state=fr(0,.9f);s.primary=p;s.secondary=q;
    for(int j=0;j<NR;j++){p[j].values=pv[j];q[j].values=qv[j];obs[j].values=ov[j];p[j].scalar=fr(.35f,.95f);q[j].scalar=fr(.35f,.95f);p[j].reserved=q[j].reserved=obs[j].reserved=0;for(int k=0;k<NW;k++){pv[j][k]=fr(.2f,.95f);qv[j][k]=fr(.2f,.95f);}}
    for(int n=0;n<12;n++)ubig_stage_b_leveler_history_update(&s.history,fr(.03f,.12f),fr(.45f,.9f),fr(.2f,.9f));
    const UbigStageBLevelerConfig c={fb(0x3d5a740e),fb(0xba5939d7),fb(0xbb670610),375u,fb(0x3f7fe1b9),fb(0x3f7c3e0a)};
    for(unsigned n=0;n<20000;n++){
        for(int j=0;j<NR;j++){obs[j].scalar=fr(.25f,.99f);for(int k=0;k<NW;k++)ov[j][k]=fr(.1f,.99f);}
        ubig_stage_b_leveler_update(&s,&c,ru()%4u,NW,fr(0,.999f),fr(0,.999f),fb(0x3c23d70a),obs);
    }
    UbigStageBLevelerState canonical=s;canonical.primary=canonical.secondary=0;
    uint64_t h=h64(1469598103934665603ULL,&canonical,sizeof canonical);
    for(int j=0;j<NR;j++){h=h64(h,&p[j].scalar,8);h=h64(h,&q[j].scalar,8);h=h64(h,pv[j],sizeof pv[j]);h=h64(h,qv[j],sizeof qv[j]);}
    if(h!=0x3e549513f21d2250ULL){fprintf(stderr,"Stage-B leveler writer hash %016llx\n",(unsigned long long)h);return 2;}
    uint64_t rh=1469598103934665603ULL;
    for(unsigned n=0;n<10000;n++){
        UbigStageBLevelerState rs;uint32_t *words=(uint32_t*)&rs;for(size_t i=0;i<sizeof rs/4;i++)words[i]=ru();
        UbigStageBLevelerRecord rp[NR],rq[NR];float rpv[NR][NW],rqv[NR][NW];
        for(int j=0;j<NR;j++){rp[j].values=rpv[j];rq[j].values=rqv[j];rp[j].scalar=fb(ru());rq[j].scalar=fb(ru());rp[j].reserved=ru();rq[j].reserved=ru();for(int k=0;k<NW;k++){rpv[j][k]=fb(ru());rqv[j][k]=fb(ru());}}
        rs.primary=rp;rs.secondary=rq;ubig_stage_b_leveler_reset(&rs,ru()%(NR+1u),ru()%(NW+1u));
        UbigStageBLevelerState rc=rs;rc.primary=rc.secondary=0;rh=h64(rh,&rc,sizeof rc);for(int j=0;j<NR;j++){rh=h64(rh,&rp[j].scalar,8);rh=h64(rh,&rq[j].scalar,8);rh=h64(rh,rpv[j],sizeof rpv[j]);rh=h64(rh,rqv[j],sizeof rqv[j]);}
    }
    if(rh!=0x21e2c995a4ad21d7ULL){fprintf(stderr,"Stage-B leveler reset hash %016llx\n",(unsigned long long)rh);return 3;}
    puts("PASS Stage-B Leveler/DRC long-memory writer/reset regressions");return 0;
}
