"""Actual APM/GPR probe and receiver C reproduce the impossible early query."""
from pathlib import Path
import os
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parent
SRC=Path(os.environ.get('SP11_KERNEL_SOURCE', '.'))
def function(text,name):
 marker=name+'(';start=text.index(marker);start=text.rfind('\n',0,start)+1;op=text.index('{',start);level=0
 for i in range(op,len(text)):
  if text[i]=='{':level+=1
  elif text[i]=='}':
   level-=1
   if level==0:return text[start:i+1]+'\n'
 raise AssertionError(name)
HEAD=r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#define __packed __attribute__((packed))
#define GFP_KERNEL 0
#define GPR_PKT_VER 0
#define GPR_PKT_HEADER_WORD_SIZE 6
#define GPR_PKT_HEADER_BYTE_SIZE 24
#define APM_CMD_GET_SPF_STATE 0x01001021
#define APM_CMD_RSP_GET_SPF_STATE 0x02001007
#define dev_err(...) ((void)0)
#define spin_lock_irqsave(p,f) do{(void)(p);(f)=0;}while(0)
#define spin_unlock_irqrestore(p,f) do{(void)(p);(void)(f);}while(0)
#define mutex_init(p) (*(p)=0)
#define atomic_set(p,v) (*(p)=(v))
#define init_waitqueue_head(p) (*(p)=0)
#define INIT_LIST_HEAD(p) (*(p)=0)
#define idr_init(p) (*(p)=0)
#define IS_ERR(p) ((p)==NULL)
#define PTR_ERR(p) (-ENOMEM)
#define __free(x) __attribute__((cleanup(cleanup_free)))
static void cleanup_free(void *p){free(*(void**)p);}
struct gpr_hdr{uint32_t version:4,hdr_size:4,pkt_size:24;uint32_t dest_domain:8,src_domain:8,reserved:16;uint32_t src_port,dest_port,token,opcode;} __packed;
struct gpr_pkt{struct gpr_hdr hdr;uint32_t payload[];};
struct gpr_resp_pkt{struct gpr_hdr hdr;size_t payload_size;void *payload;};
struct device{void *driver,*data,*of_node;};
struct pkt_router_svc{int(*callback)(const struct gpr_resp_pkt*,void*,int);void *priv;};
struct apr_device{struct device dev;struct pkt_router_svc svc;};
typedef struct apr_device gpr_device_t;
struct apr_driver{int(*probe)(struct apr_device*);int(*gpr_callback)(const struct gpr_resp_pkt*,void*,int);};
#define to_apr_device(d) ((struct apr_device*)(d))
#define to_apr_driver(d) ((struct apr_driver*)(d))
struct packet_router{int svcs_lock,svcs_idr;struct device *dev;};
struct apr_rx_buf{int len;uint8_t buf[64];};
struct q6apm{int lock,graph_mutex,command_token,wait,widget_list,graph_idr,graph_info_idr,sub_graphs_idr,containers_idr,modules_idr;struct device*dev;gpr_device_t*gdev;int state;};
static struct q6apm *g_apm;
static struct apr_device device;
static struct packet_router router;
static int mode,queries,timeouts,replies,probe_queries;
static int q6apm_audio_component;
static void *idr_find(int *idr,uint32_t id){(void)idr;return id==1?&device.svc:NULL;}
static void dev_set_drvdata(struct device*d,void*v){d->data=v;}
static void *devm_kzalloc(struct device*d,size_t n,int f){(void)d;(void)f;return mode==1?NULL:calloc(1,n);}
static int snd_soc_register_component(struct device*d,int*c,void*unused,int n){(void)d;(void)c;(void)unused;(void)n;return mode==2?-EINVAL:0;}
static void snd_soc_unregister_component(struct device*d){(void)d;}
static int of_platform_populate(void*a,void*b,void*c,struct device*d){(void)a;(void)b;(void)c;(void)d;return 0;}
static struct gpr_pkt *audioreach_alloc_apm_cmd_pkt(int n,uint32_t op,uint32_t token){(void)n;struct gpr_pkt*p=calloc(1,sizeof(*p));p->hdr.opcode=op;p->hdr.token=token;return p;}
'''
MIDDLE=r'''
static int reply(const struct gpr_resp_pkt*r,void*priv,int op){(void)priv;(void)op;replies++;g_apm->state=*(uint32_t*)r->payload;return 0;}
static int q6apm_send_cmd_sync(struct q6apm*apm,const struct gpr_pkt*p,uint32_t rsp){
 (void)apm;(void)p;queries++;if(mode==4)return -EIO;
 if(mode!=5){struct apr_rx_buf b={.len=28};struct gpr_hdr*h=(struct gpr_hdr*)b.buf;h->hdr_size=6;h->pkt_size=28;h->dest_port=1;h->opcode=rsp;uint32_t state=mode==3?0:1;memcpy(b.buf+24,&state,4);gpr_do_rx_callback(&router,&b);}
 if(replies==0){timeouts++;return -ETIMEDOUT;}return 0;
}
'''
MAIN=r'''
int main(int argc,char**argv){(void)argc;mode=atoi(argv[1]);struct apr_driver driver={.probe=apm_probe,.gpr_callback=reply};device.dev.driver=&driver;
 int ret=apr_device_probe(&device.dev);probe_queries=queries;int early=timeouts;bool ready=false;
 if(!ret){replies=0;ready=q6apm_is_adsp_ready();}
 printf("%d %d %d %d %d",ret,probe_queries,early,ready,timeouts-early);free(g_apm);return 0;}
'''
class InitialStateOrder(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not (SRC/'sound/soc/qcom/qdsp6/q6apm.c').is_file():raise unittest.SkipTest('Set SP11_KERNEL_SOURCE to the pre-fix kernel source tree')
  cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup);root=Path(cls.tmp.name)
  apm=(SRC/'sound/soc/qcom/qdsp6/q6apm.c').read_text();fixed=root/'apm-fixed.c';fixed.write_text(apm);subprocess.run(['patch','--silent',str(fixed),str(ROOT/'patches/unified12-apm-initial-state-query.patch')],check=True)
  apr=(SRC/'drivers/soc/qcom/apr.c').read_text();cls.bins={}
  for label,text in [('old',apm),('fixed',fixed.read_text())]:
   code=HEAD+function(apr,'gpr_do_rx_callback')+MIDDLE+function(text,'q6apm_get_apm_state')+function(text,'q6apm_is_adsp_ready')+function(text,'apm_probe')+function(apr,'apr_device_probe')+MAIN
   c=root/(label+'.c');c.write_text(code);binary=root/label;subprocess.run(['gcc','-Wall','-Wextra','-Werror','-O2',str(c),'-o',str(binary)],check=True);cls.bins[label]=binary
 def result(self,mode=0,label='fixed'):return tuple(map(int,subprocess.check_output([str(self.bins[label]),str(mode)],text=True,timeout=2).split()))
 def test_old_probe_drops_reply(self):self.assertEqual(self.result(0,'old'),(0,1,1,1,0))
 def test_fixed_probe_and_later_readiness(self):self.assertEqual(self.result(),(0,0,0,1,0))
 def test_allocation_failure_remains_failure(self):self.assertEqual(self.result(1),(-12,0,0,0,0))
 def test_component_failure_remains_failure(self):self.assertEqual(self.result(2),(-22,0,0,0,0))
 def test_dsp_not_ready_is_not_bypassed(self):self.assertEqual(self.result(3),(0,0,0,0,0))
 def test_later_transport_failure_is_not_bypassed(self):self.assertEqual(self.result(4),(0,0,0,0,0))
 def test_later_real_timeout_remains_visible(self):self.assertEqual(self.result(5),(0,0,0,0,1))
