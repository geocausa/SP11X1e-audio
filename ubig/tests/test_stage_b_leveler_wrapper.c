#include "stage_b_leveler.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define R 2u
#define W 20u
#define PR 4u
static uint32_t rng=0x34b78a55u;
static uint32_t ru(void){rng=rng*1664525u+1013904223u;return rng;}
static float fr(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}
static uint64_t h64(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){
    float lookup_data[8][96];UbigStageBLevelerLookupTables lookup;
    for(unsigned g=0;g<8;g++){float v=-.92f+.012f*g;for(unsigned i=0;i<96;i++){v+=.0125f+.00007f*(float)(((i+3u)*(g+5u))%23u);lookup_data[g][i]=v;}lookup.table[g]=lookup_data[g];}
    float slopes[8][40],knots[8][40];UbigStageBLevelerInverseLookupTables inv;
    for(unsigned g=0;g<8;g++){inv.slopes[g]=slopes[g];inv.knots[g]=knots[g];inv.length[g]=40u;inv.baseline[g]=-.45f+.025f*g;for(unsigned j=0;j<40;j++){knots[g][j]=-1.4f+.075f*j+.001f*g;slopes[g][j]=.18f+.006f*j+.002f*g;}}
    UbigStageBLevelerNormalizedCubic cubic={.06f,0,.72f,-1,.41f,-2,.13f,-3};
    float offsets[W],thresholds[PR],band_weights[W],tail[8];
    for(unsigned i=0;i<W;i++){offsets[i]=-.48f+.011f*i;band_weights[i]=.11f+.017f*i+.002f*(i%3u);}for(unsigned i=0;i<PR;i++)thresholds[i]=.03f+.012f*i;for(unsigned i=0;i<8;i++)tail[i]=.025f*(i+1u);

    UbigStageBLevelerTransitionRecord large[W],normal[W],lookup_tr[W],matrix_tr[W];
    for(unsigned i=0;i<W;i++){large[i]=(UbigStageBLevelerTransitionRecord){-.015f,-.01f,.72f,.28f,.88f,.12f};normal[i]=(UbigStageBLevelerTransitionRecord){-.012f,-.008f,.82f,.18f,.93f,.07f};lookup_tr[i]=(UbigStageBLevelerTransitionRecord){-.01f,-.01f,.8f,.2f,.9f,.1f};matrix_tr[i]=(UbigStageBLevelerTransitionRecord){-.02f,-.015f,.76f,.24f,.9f,.1f};}
    UbigStageBLevelerRowConfig life_cfg={0,.06f,.985f,9u};
    UbigStageBLevelerLookupConfig lookup_cfg={lookup_tr,.45f,.985f,.01f,.16f,.45f,0};
    float rise_mix[W],blend[W],fc[5]={.48f,.14f,.075f,.035f,.015f},post[W];
    for(unsigned i=0;i<W;i++){rise_mix[i]=.25f+.012f*i;blend[i]=.78f+.006f*(i%7u);post[i]=.72f+.008f*(i%9u);}
    UbigStageBLevelerAdaptiveControl adaptive_cfg={rise_mix,blend,.91f,0};
    UbigStageBLevelerSymmetricFilter filter={fc,post,5u,0};
    UbigStageBLevelerConfig writer_cfg={.055f,-.0017f,-.0024f,12u,.992f,.965f};
    UbigStageBLevelerProducerConfig producer_cfg={&filter,{.83f,.74f,.79f,.69f,.61f,.53f},.018f,7u};
    float base_row[W];for(unsigned i=0;i<W;i++)base_row[i]=-.22f+.007f*i;
    UbigStageBLevelerParentConfig cfg={base_row,&life_cfg,&lookup_cfg,large,normal,&adaptive_cfg,&filter,matrix_tr,&writer_cfg,&producer_cfg};
    UbigStageBLevelerParentTuning tuning={&lookup,&inv,&cubic,offsets,thresholds,band_weights,tail};

    float matrix_state[R][W]={{0}},transition_state[PR][W]={{0}};float *matrix_ptr[R]={matrix_state[0],matrix_state[1]},*transition_ptr[PR]={transition_state[0],transition_state[1],transition_state[2],transition_state[3]};
    float life_prev[W]={0},life_cur[W]={0};UbigStageBLevelerRowState life={life_prev,life_cur,0u,-1,.97f,0};
    float lookup_ts[W]={0},lookup_cs[W]={0};UbigStageBLevelerLookupState lookup_state={0,0,lookup_ts,lookup_cs,0,.08f};
    UbigStageBLevelerProducerState producer_state;memset(&producer_state,0,sizeof producer_state);
    UbigStageBLevelerRecord primary[PR],secondary[PR];float primary_v[PR][W],secondary_v[PR][W];
    UbigStageBLevelerState writer;memset(&writer,0,sizeof writer);writer.base=.99f;writer.primary=primary;writer.secondary=secondary;
    for(unsigned r=0;r<PR;r++){primary[r].values=primary_v[r];secondary[r].values=secondary_v[r];primary[r].scalar=.28f+.035f*r;secondary[r].scalar=-.999f;primary[r].reserved=secondary[r].reserved=0;for(unsigned i=0;i<W;i++){primary_v[r][i]=.18f+.009f*i+.015f*r;secondary_v[r][i]=-.999f;}}
    ubig_stage_b_leveler_history_init(&writer.history);
    float adaptive_fast[W]={0},adaptive_slow[W]={0};UbigStageBLevelerAdaptiveState adaptive_state={adaptive_fast,adaptive_slow};
    UbigStageBLevelerParentState state={matrix_ptr,&lookup_state,&life,transition_ptr,&producer_state,&writer,&adaptive_state};

    float prev_curve[18]={.78f,.55f,.31f,.12f,-.08f,-.35f,.1f,.18f,.05f,-.02f,.12f,-.08f,.04f,.2f,-.1f,.06f,.03f,.0f};
    float tmpl_curve[18]={.74f,.50f,.27f,.09f,-.11f,-.39f,.08f,.22f,.04f,-.03f,.10f,-.07f,.05f,.18f,-.09f,.07f,.02f,.0f};
    float source_vec[W];for(unsigned i=0;i<W;i++)source_vec[i]=.22f+.021f*i;UbigStageBLevelerSourceGate source={source_vec,.055f,0};
    float inrows[R][W],outrows[R][W];float *inp[R]={inrows[0],inrows[1]},*outp[R]={outrows[0],outrows[1]};UbigStageBLevelerInputRows in={R,W,inp},out={R,W,outp};
    int32_t tele[W]={0};uint64_t h=1469598103934665603ULL;
    UbigStageBLevelerWrapperState ws={-.15f,.0f,1u,1u};
    UbigStageBLevelerWrapperConfig wc={1u,1u,1u,1u,1u,.025f,-.18f,-.12f,.62f,.48f,.71f};
    for(unsigned n=0;n<6000;n++){
        for(unsigned r=0;r<R;r++)for(unsigned i=0;i<W;i++){inrows[r][i]=fr(-.18f,.16f);outrows[r][i]=fr(-.08f,.08f);}
        for(unsigned i=0;i<W;i++)source_vec[i]=fr(.08f,1.05f);
        source.gate=(n%23u==0u)?-.01f:fr(.015f,.12f);
        wc.enabled=(n%17u)!=0u;
        wc.adaptive_emit=(n%3u)==0u;
        wc.target_scale_override=(n&1u)!=0u;
        wc.lookup_control_override=(n%4u)!=0u;
        wc.preserve_rows=(n%5u)!=0u;
        wc.smoothing_step=fr(.001f,.075f);
        wc.base_limit=fr(-.45f,.12f);
        wc.target_limit=fr(-.55f,.18f);
        wc.adaptive_output_scale=fr(.25f,.85f);
        wc.adaptive_target_scale=fr(.15f,.95f);
        wc.lookup_control=fr(.15f,.95f);
        if(n%29u==0u)ws.force_target=1u;
        if(n%7u==0u)ws.adaptive_direct=1u;
        float a=fr(-.32f,.28f),b=fr(-.22f,.22f);
        ubig_stage_b_leveler_wrapper_process(&ws,&wc,&state,&cfg,&tuning,prev_curve,tmpl_curve,&source,&in,&out,tele,&a,&b);
        h=h64(h,&ws,sizeof ws);h=h64(h,&a,4);h=h64(h,&b,4);h=h64(h,inrows,sizeof inrows);h=h64(h,outrows,sizeof outrows);h=h64(h,tele,sizeof tele);
    }
    UbigStageBLevelerLookupState lc=lookup_state;lc.transition_state=lc.cubic_state=0;UbigStageBLevelerRowState rc=life;rc.previous=rc.current=0;UbigStageBLevelerState wcanon=writer;wcanon.primary=wcanon.secondary=0;UbigStageBLevelerAdaptiveState ac=adaptive_state;ac.fast=ac.slow=0;
    h=h64(h,matrix_state,sizeof matrix_state);h=h64(h,transition_state,sizeof transition_state);h=h64(h,&lc,sizeof lc);h=h64(h,lookup_ts,sizeof lookup_ts);h=h64(h,lookup_cs,sizeof lookup_cs);h=h64(h,&rc,sizeof rc);h=h64(h,life_prev,sizeof life_prev);h=h64(h,life_cur,sizeof life_cur);h=h64(h,&producer_state,sizeof producer_state);h=h64(h,&wcanon,sizeof wcanon);h=h64(h,primary_v,sizeof primary_v);h=h64(h,secondary_v,sizeof secondary_v);for(unsigned r=0;r<PR;r++){h=h64(h,&primary[r].scalar,8);h=h64(h,&secondary[r].scalar,8);}h=h64(h,&ac,sizeof ac);h=h64(h,adaptive_fast,sizeof adaptive_fast);h=h64(h,adaptive_slow,sizeof adaptive_slow);
    if(h!=0xc03c38ccf02019e9ULL){fprintf(stderr,"Stage-B Leveler wrapper hash %016llx\n",(unsigned long long)h);return 2;}
    puts("PASS Stage-B deployed Leveler control-wrapper regression");
    return 0;
}
