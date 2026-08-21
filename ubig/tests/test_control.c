#include "ubig/ubig_control.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char path[256];snprintf(path,sizeof(path),"/tmp/ubig-control-test-%lu",(unsigned long)getpid());
    unlink(path);
    ubig_control_handle h;if(ubig_control_open(&h,path,1))return 2;
    if(ubig_control_request_profile(&h,UBIG_PROFILE_MOVIE))return 3;
    int32_t eq[UBIG_EQ_BANDS]={0};eq[3]=48;eq[9]=-32;
    if(ubig_control_request_custom_eq(&h,eq))return 4;
    ubig_control_page p;if(ubig_control_snapshot(&h,&p))return 5;
    if(p.request_generation!=2 || p.desired_profile!=UBIG_PROFILE_CUSTOM || p.custom_eq[3]!=48 || p.custom_eq[9]!=-32)return 6;
    ubig_control_close(&h);unlink(path);
    puts("PASS control-page request/snapshot ABI");return 0;
}
