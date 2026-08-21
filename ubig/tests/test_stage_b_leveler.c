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
    rng=0x82e28a55u;uint64_t ph=1469598103934665603ULL;
    for(unsigned n=0;n<14000;n++){
        UbigStageBLevelerPairCoefficients pc;float *cp=(float*)&pc;for(int i=0;i<6;i++)cp[i]=fr(-.3f,1.3f);
        UbigStageBLevelerPairControl ctl={fr(-.3f,1.3f),fr(-.3f,1.3f),fr(-.3f,1.3f),ru()&1u,ru()&1u,ru()&1u};
        float tav[NW],tb[NW+1],mixv[NW],sav[NW],sbv[NW];for(int i=0;i<NW;i++){tav[i]=fr(-1.2f,1.2f);mixv[i]=fr(-.2f,1.2f);sav[i]=fr(-1.2f,1.2f);sbv[i]=fr(-1.2f,1.2f);}for(int i=0;i<NW+1;i++)tb[i]=fr(-1.2f,1.2f);
        UbigStageBLevelerRecord ta={tav,fr(-1.2f,1.2f),ru()};float sa=fr(-1.2f,1.2f),sb=fr(-1.2f,1.2f);
        ubig_stage_b_leveler_pair_row(&pc,tb,&ctl,ru()%(NW+1u),mixv,&ta,&sa,sav,&sb,sbv,fr(-.2f,1.2f));
        ph=h64(ph,&sa,4);ph=h64(ph,&sb,4);ph=h64(ph,sav,sizeof sav);ph=h64(ph,sbv,sizeof sbv);
    }
    if(ph!=0x21c3a30f08a6290eULL){fprintf(stderr,"Stage-B leveler pair-row hash %016llx\n",(unsigned long long)ph);return 4;}
    rng=0x83440a55u;uint64_t ch=1469598103934665603ULL;
    for(unsigned n=0;n<12000;n++){float cv[17];float t0=fr(-1,1),t1=fr(-1,t0),t2=fr(-1,t1),t3=fr(-1,t2),t4=fr(-1,t3),t5=fr(-1,t4);cv[0]=t0;cv[1]=t1;cv[2]=t2;cv[3]=t3;cv[4]=t4;cv[5]=t5;for(int i=6;i<17;i++)cv[i]=fr(-2,2);UbigStageBLevelerRecord a[5],b[5];float av[5][NW],bv[5][NW],out[5*21];for(int r=0;r<5;r++){a[r].values=av[r];b[r].values=bv[r];a[r].scalar=fr(-1.2f,1.2f);b[r].scalar=fr(-1.2f,1.2f);a[r].reserved=b[r].reserved=0;for(int k=0;k<NW;k++){av[r][k]=fr(-1.2f,1.2f);bv[r][k]=fr(-1.2f,1.2f);}}for(int i=0;i<5*21;i++)out[i]=fr(-3,3);ubig_stage_b_leveler_curve_rows(cv,a,b,ru()%(NW+1u),ru()%5u,out,fr(-1,1),fr(-1,1));ch=h64(ch,out,sizeof out);}
    if(ch!=0x057492a9dafa8981ULL){fprintf(stderr,"Stage-B leveler curve-rows hash %016llx\n",(unsigned long long)ch);return 5;}
    rng=0x837f0a55u;uint64_t cbh=1469598103934665603ULL;for(unsigned n=0;n<12000;n++){float cv[17];float t0=fr(-1,1),t1=fr(-1,t0),t2=fr(-1,t1),t3=fr(-1,t2),t4=fr(-1,t3),t5=fr(-1,t4);cv[0]=t0;cv[1]=t1;cv[2]=t2;cv[3]=t3;cv[4]=t4;cv[5]=t5;for(int i=6;i<17;i++)cv[i]=fr(-2,2);float lim[5];UbigStageBLevelerRecord a[5],b[5];float av[5][NW],bv[5][NW],rows[5*21];for(int r=0;r<5;r++){lim[r]=fr(-1.2f,1.2f);a[r].values=av[r];b[r].values=bv[r];a[r].scalar=fr(-1.2f,1.2f);b[r].scalar=fr(-1.2f,1.2f);a[r].reserved=b[r].reserved=0;for(int k=0;k<NW;k++){av[r][k]=fr(-1.2f,1.2f);bv[r][k]=fr(-1.2f,1.2f);}}for(int i=0;i<5*21;i++)rows[i]=fr(-1.5f,1.5f);ubig_stage_b_leveler_curve_bounds(lim,a,b,cv,ru()%(NW+1u),ru()%5u,rows,fr(-1,1),fr(-1,1));cbh=h64(cbh,rows,sizeof rows);}if(cbh!=0x6a59181925d2fe72ULL){fprintf(stderr,"Stage-B curve-bounds hash %016llx\n",(unsigned long long)cbh);return 6;}
    rng=0x83b48a55u;uint64_t lch=1469598103934665603ULL;for(unsigned n=0;n<12000;n++){UbigStageBLevelerRecord a[5],b[5];float av[5][NW],bv[5][NW],thr[5],rows[5*21];for(int r=0;r<5;r++){a[r].values=av[r];b[r].values=bv[r];a[r].scalar=fr(-1.2f,1.2f);b[r].scalar=fr(-1.2f,1.2f);a[r].reserved=b[r].reserved=0;thr[r]=fr(-.2f,.3f);for(int k=0;k<NW;k++){av[r][k]=fr(-1.2f,1.2f);bv[r][k]=fr(-1.2f,1.2f);}}for(int i=0;i<5*21;i++)rows[i]=fr(-1.5f,1.5f);ubig_stage_b_leveler_link_ceiling(a,b,thr,ru()%(NW+1u),ru()%5u,rows);lch=h64(lch,rows,sizeof rows);}if(lch!=0x4c3f1ca5efe06547ULL){fprintf(stderr,"Stage-B link-ceiling hash %016llx\n",(unsigned long long)lch);return 7;}
    rng=0x83e58a55u;uint64_t cph=1469598103934665603ULL;for(unsigned n=0;n<10000;n++){float cv[17];float t0=fr(-1,1),t1=fr(-1,t0),t2=fr(-1,t1),t3=fr(-1,t2),t4=fr(-1,t3),t5=fr(-1,t4);cv[0]=t0;cv[1]=t1;cv[2]=t2;cv[3]=t3;cv[4]=t4;cv[5]=t5;for(int i=6;i<17;i++)cv[i]=fr(-2,2);float lim[5],thr[5];UbigStageBLevelerRecord a[5],b[5];float av[5][NW],bv[5][NW],rows[5*21];for(int r=0;r<5;r++){lim[r]=fr(-1.2f,1.2f);thr[r]=fr(-.2f,.3f);a[r].values=av[r];b[r].values=bv[r];a[r].scalar=fr(-1.2f,1.2f);b[r].scalar=fr(-1.2f,1.2f);a[r].reserved=b[r].reserved=0;for(int k=0;k<NW;k++){av[r][k]=fr(-1.2f,1.2f);bv[r][k]=fr(-1.2f,1.2f);}}for(int i=0;i<5*21;i++)rows[i]=fr(-1.2f,1.2f);ubig_stage_b_leveler_curve_pipeline(cv,a,b,lim,thr,ru()%(NW+1u),ru()%5u,rows,ru()&1u,fr(-.8f,.8f),fr(-.8f,.8f));cph=h64(cph,rows,sizeof rows);}
    if(cph!=0x0689d8092fcb91aaULL){fprintf(stderr,"Stage-B curve-pipeline hash %016llx\n",(unsigned long long)cph);return 8;}
    rng=0x696b2026u;uint64_t pph=1469598103934665603ULL;UbigStageBLevelerProducerState ps;memset(&ps,0,sizeof ps);
    for(unsigned n=0;n<12000;n++){
        float cf[5],post[NW],thr[NR],cv[17];for(unsigned i=0;i<5;i++)cf[i]=fr(-.25f,1.1f);for(unsigned i=0;i<NW;i++)post[i]=fr(.1f,1.4f);for(unsigned i=0;i<NR;i++)thr[i]=fr(-.15f,.25f);
        float a0=fr(-1,1);for(int i=0;i<6;i++){float next=fr(-1,a0);cv[i]=a0;a0=next;}for(int i=6;i<17;i++)cv[i]=fr(-2,2);
        UbigStageBLevelerSymmetricFilter filter={cf,post,1u+ru()%5u,0u};UbigStageBLevelerProducerConfig pcfg;pcfg.filter=&filter;float *pp=(float*)&pcfg.pair;for(unsigned i=0;i<6;i++)pp[i]=fr(-.2f,1.15f);pcfg.exp_drive=fr(-.05f,.05f);pcfg.hold_limit=ru()%8u;
        UbigStageBLevelerPairCoefficients pov;float *pop=(float*)&pov;for(unsigned i=0;i<6;i++)pop[i]=fr(-.2f,1.15f);const UbigStageBLevelerPairCoefficients *povp=(ru()&1u)?&pov:0;
        UbigStageBLevelerRecord pin[NR],panch[NR],pcmp[NR];float piv[NR][NW],pav[NR][NW],pbv[NR][NW];for(unsigned r=0;r<NR;r++){pin[r].values=piv[r];panch[r].values=pav[r];pcmp[r].values=pbv[r];pin[r].scalar=fr(-1,1);panch[r].scalar=fr(-1,1);pcmp[r].scalar=fr(-1,1);pin[r].reserved=panch[r].reserved=pcmp[r].reserved=0;for(unsigned k=0;k<NW;k++){piv[r][k]=fr(-1,1);pav[r][k]=fr(-1,1);pbv[r][k]=fr(-1,1);}}
        float prows[NR][NW],*prp[NR];for(unsigned r=0;r<NR;r++){prp[r]=prows[r];for(unsigned k=0;k<NW;k++)prows[r][k]=fr(-1,1);}UbigStageBLevelerProducerRows pout={prp,pcmp};float perr[NR][NW];for(unsigned r=0;r<NR;r++)for(unsigned k=0;k<NW;k++)perr[r][k]=fr(-1,1);
        unsigned pwidth=ru()%(NW+1u),pidx=ru()%NR;ubig_stage_b_leveler_producer_process(&ps,&pcfg,pin,panch,ru()&1u,pwidth,pidx,cv,fr(.01f,.95f),fr(0,.999f),fr(-.6f,.6f),fr(-.6f,.6f),povp,ru()&1u,&perr[0][0],&pout,ru()&1u,thr);
        pph=h64(pph,&ps,sizeof ps);pph=h64(pph,prows,sizeof prows);pph=h64(pph,perr,sizeof perr);
    }
    if(pph!=0x4b55c0c0974ae190ULL){fprintf(stderr,"Stage-B Leveler producer hash %016llx\n",(unsigned long long)pph);return 9;}
    rng=0x67ac8b26u;uint64_t afh=1469598103934665603ULL;float aff[NW]={0},afs[NW]={0};UbigStageBLevelerAdaptiveState ast={aff,afs};
    float aris[NW],abld[NW],awgt[NW],asgv[NW],aref[NW],afc[5],apost[NW];float arecv[NR][NW],arows[NR][NW];UbigStageBLevelerRecord arec[NR];float *arowp[NR];int32_t atele[NW]={0};
    for(unsigned i=0;i<NW;i++)awgt[i]=0.115f+0.0185f*(float)i+0.003f*(float)(i%4u);
    for(unsigned n=0;n<16000;n++){for(unsigned i=0;i<NW;i++){aris[i]=fr(.08f,.999f);abld[i]=fr(.08f,.999f);asgv[i]=fr(.05f,1.45f);aref[i]=fr(-1.15f,.35f);apost[i]=fr(.12f,.85f);}for(unsigned i=0;i<5;i++)afc[i]=fr(.01f,.48f);for(unsigned r=0;r<NR;r++){arec[r].values=arecv[r];arec[r].scalar=fr(-.2f,.2f);arec[r].reserved=0;arowp[r]=arows[r];for(unsigned i=0;i<NW;i++){arecv[r][i]=fr(-1.18f,.18f);arows[r][i]=fr(-.22f,.22f);}}UbigStageBLevelerAdaptiveControl actl={aris,abld,fr(.08f,.999f),0};UbigStageBLevelerSymmetricFilter afilt={afc,apost,1u+ru()%5u,0};UbigStageBLevelerSourceGate asg={asgv,(ru()%11u==0u)?fr(-.08f,0.0f):fr(.004f,.18f),0};UbigStageBLevelerProducerRows aout={arowp,arec};int32_t *atp=(ru()&1u)?atele:0;ubig_stage_b_leveler_adaptive_filter_process(&ast,&actl,&afilt,&asg,aref,awgt,ru()&1u,ru()&1u,ru()&1u,&aout,atp,fr(-.025f,.025f),fr(.25f,.75f),fr(.001f,.04f),fr(0,.999f),fr(0,1));afh=h64(afh,aff,sizeof aff);afh=h64(afh,afs,sizeof afs);afh=h64(afh,arows,sizeof arows);afh=h64(afh,atele,sizeof atele);}
    if(afh!=0x744a6bdc1ec1dd38ULL){fprintf(stderr,"Stage-B adaptive-filter hash %016llx\n",(unsigned long long)afh);return 10;}
    puts("PASS Stage-B Leveler/DRC long-memory writer/reset/pair-row/curve-pipeline/producer/adaptive-filter regressions");return 0;
}
