/* Write an ASoC bytes-TLV control. amixer cset cannot do this: it attempts a
 * read first and fails with "Cannot read the given element". */
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv){
    if(argc<4){fprintf(stderr,"usage: %s <card> <numid> <hexbytes>\n",argv[0]);return 2;}
    unsigned numid=atoi(argv[2]); const char *hex=argv[3];
    unsigned char buf[2048]; int n=0;
    for(const char *p=hex; *p && n<(int)sizeof(buf); ){
        if(*p==','||*p==' '){p++;continue;}
        unsigned v; if(sscanf(p,"%2x",&v)!=1) break; buf[n++]=v; p+=2;
    }
    snd_ctl_t *ctl; int err;
    if((err=snd_ctl_open(&ctl,argv[1],0))<0){fprintf(stderr,"open: %s\n",snd_strerror(err));return 1;}
    unsigned int *tlv=calloc(1,2*sizeof(unsigned)+n+8);
    tlv[0]=0; tlv[1]=n; memcpy(&tlv[2],buf,n);
    err=snd_ctl_elem_tlv_write(ctl,({snd_ctl_elem_id_t *i;snd_ctl_elem_id_malloc(&i);snd_ctl_elem_id_set_numid(i,numid);i;}),tlv);
    printf("tlv_write rc=%d (%s)  payload=%d bytes\n",err,err<0?snd_strerror(err):"OK",n);
    snd_ctl_close(ctl); return err<0?1:0;
}
