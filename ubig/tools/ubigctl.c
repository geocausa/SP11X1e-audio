#include "ubig/ubig_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage:\n"
        "  %s status\n"
        "  %s profile <Dynamic|Movie|Music|Game|Voice|Course|Custom>\n"
        "  %s eq v1,v2,...,v20   (raw values -192..192)\n"
        "  %s postgain <-1200..0>  (raw 1/16-dB units)\n", argv0, argv0, argv0, argv0);
}

static int parse_eq(const char *s, int32_t out[UBIG_EQ_BANDS])
{
    for (unsigned i=0;i<UBIG_EQ_BANDS;++i) {
        char *end=NULL; long v=strtol(s,&end,10);
        if (end==s || v < -192 || v > 192) return -1;
        out[i]=(int32_t)v;
        if (i+1<UBIG_EQ_BANDS) { if (*end != ',') return -1; s=end+1; }
        else if (*end) return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(argv[0]); return 2; }
    ubig_control_handle h;
    int rc=ubig_control_open(&h,NULL,1);
    if (rc) { fprintf(stderr,"UbiG control open failed: %d\n",rc); return 1; }

    if (!strcmp(argv[1],"status")) {
        ubig_control_page p;
        rc=ubig_control_snapshot(&h,&p);
        if (!rc) {
            printf("path=%s\nabi=%u\nrequest_generation=%u\nack_generation=%u\npostgain_request_generation=%u\npostgain_ack_generation=%u\ndesired_profile=%s\nactive_profile=%s\ndesired_postgain=%d\nactive_postgain=%d\nlast_error=%d\n",
                   h.path,p.abi_version,p.request_generation,p.ack_generation,
                   p.postgain_request_generation,p.postgain_ack_generation,
                   ubig_profile_name((ubig_profile)p.desired_profile),
                   ubig_profile_name((ubig_profile)p.active_profile),
                   p.desired_postgain,p.active_postgain,p.last_error);
        }
    } else if (!strcmp(argv[1],"profile") && argc==3) {
        ubig_profile p;
        if (ubig_profile_parse(argv[2],&p)) rc=UBIG_EINVAL;
        else rc=ubig_control_request_profile(&h,p);
        if (!rc) printf("queued profile %s generation=%u\n",ubig_profile_name(p),h.page->request_generation);
    } else if (!strcmp(argv[1],"eq") && argc==3) {
        int32_t eq[UBIG_EQ_BANDS];
        if (parse_eq(argv[2],eq)) rc=UBIG_EINVAL;
        else rc=ubig_control_request_custom_eq(&h,eq);
        if (!rc) printf("queued Custom EQ generation=%u\n",h.page->request_generation);
    } else if (!strcmp(argv[1],"postgain") && argc==3) {
        char *end=NULL; long raw=strtol(argv[2],&end,10);
        if(end==argv[2] || *end || raw < -1200 || raw > 0) rc=UBIG_EINVAL;
        else rc=ubig_control_request_postgain(&h,(int32_t)raw);
        if(!rc) printf("queued postgain %ld generation=%u\n",raw,h.page->postgain_request_generation);
    } else {
        usage(argv[0]); rc=UBIG_EINVAL;
    }
    ubig_control_close(&h);
    return rc ? 2 : 0;
}
