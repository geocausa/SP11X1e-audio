#include "stage_b_rt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t q=0x3a31ca55u;static uint32_t ru(void){q=q*1664525u+1013904223u;return q;}static float rf(float a,float b){return a+(b-a)*(float)(ru()>>8)*(1.0f/16777216.0f);}static uint64_t hh(uint64_t h,const void*p,size_t n){const unsigned char*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;}return h;}
int main(void){uint64_t h=1469598103934665603ULL;for(unsigned t=0;t<100000;t++){float coeff[4],input[20];int32_t code[20];UbigStageBRtTelemetrySmoothState s;for(int j=0;j<4;j++)coeff[j]=rf(-.2f,.2f);s.coeff=coeff;for(int i=0;i<20;i++){s.code[i]=(int32_t)ru();s.scaled[i]=(int32_t)ru();s.value[i]=rf(-.5f,.5f);input[i]=rf(-.5f,.5f);code[i]=(int32_t)(ru()%1401u)-500;}ubig_stage_b_rt_telemetry_smooth(&s,code,input,1u+ru()%20u);h=hh(h,s.code,sizeof s.code);h=hh(h,s.scaled,sizeof s.scaled);h=hh(h,s.value,sizeof s.value);}if(h!=0x36e53311bbfee0ecULL){fprintf(stderr,"Stage-B RT telemetry-smooth hash %016llx\n",(unsigned long long)h);return 2;}puts("PASS Stage-B RT telemetry smoother regression");return 0;}
