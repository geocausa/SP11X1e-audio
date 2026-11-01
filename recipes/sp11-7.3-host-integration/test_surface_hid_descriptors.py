"""Exercise the actual descriptor C function against captured and malformed replies."""
from pathlib import Path
import os
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parent
SOURCE=Path(os.environ.get('SP11_KERNEL_SOURCE', '.'))/'drivers/hid/surface-hid/surface_hid.c'
PATCH=ROOT/'patches/unified12-surface-hid-empty-descriptor.patch'
def extract(text):
 start=text.index('static int ssam_hid_get_descriptor(');opening=text.index('{',start);depth=0
 for i in range(opening,len(text)):
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if depth==0:return text[start:i+1]
 raise AssertionError('unterminated real function')
HEADER=r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
typedef uint8_t u8; typedef uint32_t u32; typedef uint8_t __u8; typedef uint32_t __le32;
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define SURFACE_HID_CID_GET_DESCRIPTOR 4
#define SSAM_REQUEST_HAS_RESPONSE 1
#define dev_err(...) ((void)0)
#define ssam_retry(fn,...) fn(__VA_ARGS__)
struct surface_hid_buffer_slice { __u8 entry; __le32 offset,length; __u8 end; __u8 data[];} __attribute__((packed));
struct surface_hid_device {void *ctrl,*dev;struct {u8 category,target,instance;} uid;};
struct ssam_request {u8 target_category,target_id,command_id,instance_id,flags;size_t length;void *payload;};
struct ssam_response {size_t capacity,length;void *pointer;};
static u32 get_unaligned_le32(const void *p){u32 v;memcpy(&v,p,4);return v;}
static void put_unaligned_le32(u32 v,void *p){memcpy(p,&v,4);}
static int scenario,calls;
static int ssam_request_do_sync_onstack(void *ctrl,struct ssam_request *req,struct ssam_response *rsp,size_t header){
 (void)ctrl;(void)req;(void)header;
 struct surface_hid_buffer_slice *s=rsp->pointer;memset(s,0,rsp->capacity);
 if(scenario==4)return -EIO;
 if(scenario==5 && calls++)return -EIO;
 u32 offset=0,length=9;u8 end=1;rsp->length=19;
 if(scenario==1){length=0;end=0xff;rsp->length=10;}
 if(scenario==2)rsp->length=9;
 if(scenario==3)rsp->length=11;
 if(scenario==5){length=0;end=0;rsp->length=10;}
 if(scenario==6){offset=calls?4:0;length=calls?5:4;end=calls?1:0;rsp->length=10+length;calls++;}
 if(scenario==9){offset=0;length=calls?0:4;end=calls?0xff:0;rsp->length=10+length;calls++;}
 if(scenario==7){length=119;rsp->length=128;}
 if(scenario==8)offset=10;
 put_unaligned_le32(offset,&s->offset);put_unaligned_le32(length,&s->length);s->end=end;
 if(length<=118)for(u32 i=0;i<length;i++)s->data[i]=(u8)(offset+i+1);
 return 0;
}
'''
MAIN=r'''
int main(int argc,char **argv){(void)argc;scenario=atoi(argv[1]);struct surface_hid_device d={0};u8 out[9]={0};
 int ret=ssam_hid_get_descriptor(&d,0,out,sizeof(out));printf("%d",ret);
 if(!ret)for(int i=0;i<9;i++)if(out[i]!=i+1)return 2;
 return 0;}
'''
class DescriptorCases(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if not SOURCE.is_file():raise unittest.SkipTest('Set SP11_KERNEL_SOURCE to the pre-fix kernel source tree')
  cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup);root=Path(cls.tmp.name)
  original=SOURCE.read_text();fixed=root/'fixed.c';fixed.write_text(original)
  subprocess.run(['patch','--silent',str(fixed),str(PATCH)],check=True)
  cls.bins={}
  for label,text in [('old',original),('fixed',fixed.read_text())]:
   source=root/(label+'.c');source.write_text(HEADER+extract(text)+MAIN);binary=root/label
   subprocess.run(['gcc','-Wall','-Wextra','-Werror','-O2',str(source),'-o',str(binary)],check=True);cls.bins[label]=binary
 def result(self,scenario,label='fixed'):
  return int(subprocess.check_output([str(self.bins[label]),str(scenario)],text=True,timeout=2))
 def test_valid_keyboard_descriptor(self):self.assertEqual(self.result(0),0)
 def test_unavailable_function(self):
  self.assertEqual(self.result(1,'old'),-71);self.assertEqual(self.result(1),-19)
 def test_short_header(self):self.assertEqual(self.result(2),-71)
 def test_truncated_payload(self):
  self.assertEqual(self.result(3,'old'),0);self.assertEqual(self.result(3),-71)
 def test_transport_failure(self):self.assertEqual(self.result(4),-5)
 def test_no_progress_is_rejected(self):
  self.assertEqual(self.result(5,'old'),-5);self.assertEqual(self.result(5),-71)
 def test_multiple_valid_chunks(self):self.assertEqual(self.result(6),0)
 def test_oversized_chunk(self):self.assertEqual(self.result(7),-71)
 def test_invalid_offset(self):self.assertEqual(self.result(8),-71)
 def test_empty_later_reply_is_not_absence(self):self.assertEqual(self.result(9),-71)
