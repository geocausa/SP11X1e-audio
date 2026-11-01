"""Compile real old/new C paths; do not access the Surface audio hardware."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
OLD = Path(os.environ.get('SP11_KERNEL_BEFORE', '/path/to/unified12'))
NEW = Path(os.environ.get('SP11_KERNEL_SOURCE', '/path/to/unified13'))


def function(text, name):
    m = re.search(r'^.*\b' + name + r'\([^;]*?\n\{', text, re.M)
    assert m, name
    start = m.start()
    depth = 1
    end = m.end()
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def definitions(text):
    lines = iter(text.splitlines())
    result = []
    for line in lines:
        if line.startswith('#define'):
            result.append(line)
            while line.endswith('\\'):
                line = next(lines)
                result.append(line)
    return '\n'.join(result)


COMMON = r'''
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
typedef uint32_t u32; typedef uint32_t __le32; typedef uint8_t u8;
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define BIT(n) (1U << (n))
#define GENMASK(h,l) (((~0U) << (l)) & (~0U >> (31-(h))))
#define EXPORT_SYMBOL(x)
'''

APM_HEAD = COMMON + r'''
#define SND_SOC_AR_TPLG_GRAPH_CAL_CFG_TYPE 1
#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define IS_ERR(p) ((intptr_t)(p) < 0 && (intptr_t)(p) >= -4095)
#define PTR_ERR(p) ((intptr_t)(p))
#define dev_warn(...) ((void)0)
#define pr_info(...) do { if (0) printf(__VA_ARGS__); } while (0)
struct audioreach_module_priv_data { u32 size; u8 data[10464]; };
struct audioreach_graph { void *info; };
static struct audioreach_module_priv_data cal;
static int scenario, calls, diagnostics, inband;
static struct audioreach_module_priv_data *audioreach_find_stage(void *g,int type) {
 (void)g;(void)type;
 if(scenario==3)return NULL;
 if(scenario==4)return (void*)(intptr_t)-ENOMEM;
 return &cal;
}
static int audioreach_validate_stage_modules(void *g,void *c) {
 (void)g;(void)c;return scenario==6?-EINVAL:0;
}
static int q6apm_send_oob_config(struct audioreach_graph*g,const void*p,size_t n) {
 (void)g;calls++;
 if(calls==1 && (p!=cal.data || n!=sizeof(cal.data)))return -EFAULT;
 return scenario==1?-EOPNOTSUPP:scenario==2?-EIO:0;
}
static __attribute__((unused)) int q6apm_send_inband_config(struct audioreach_graph*g,const void*p,size_t n) {
 (void)g;(void)p;(void)n;inband++;return -EIO;
}
static __attribute__((unused)) void audioreach_diagnose_oob_frames(struct audioreach_graph*g,const u8*p,size_t n) {
 (void)g;(void)p;(void)n;diagnostics++;
}
'''
APM_MAIN = r'''
int main(int argc,char**argv) {
 (void)argc;scenario=atoi(argv[1]);cal.size=scenario==5?1:10464;
 struct audioreach_graph graph={0};
 int ret=audioreach_send_protected_graph_calibration(&graph);
 printf("%d %d %d %d\n",ret,calls,diagnostics,inband);return 0;
}
'''

WSA_HEAD = COMMON + r'''
#define LPASS_CODEC_VERSION_2_1 0x201
#define LPASS_CODEC_VERSION_2_5 0x205
#define WSA_MACRO_SPKR_MODE_1 1
struct wsa_macro {int codec_version,spkr_mode;};
struct snd_soc_component {struct wsa_macro *wsa;};
static struct {unsigned int reg,mask,value;} writes[8];
static unsigned int count;
static struct wsa_macro *snd_soc_component_get_drvdata(struct snd_soc_component*c){return c->wsa;}
static int snd_soc_component_update_bits(struct snd_soc_component*c,unsigned int r,unsigned int m,unsigned int v){
 (void)c;writes[count].reg=r;writes[count].mask=m;writes[count++].value=v;return 0;
}
'''
WSA_MAIN = r'''
int main(int argc,char**argv){
 (void)argc;int version=atoi(argv[1]),mode=atoi(argv[2]);
 struct wsa_macro wsa={.codec_version=version};struct snd_soc_component c={.wsa=&wsa};
 int ret=wsa_macro_set_spkr_mode(&c,mode);if(ret||count!=6||wsa.spkr_mode!=mode)return 1;
 unsigned int c3=version>=LPASS_CODEC_VERSION_2_5?0x5ec:0x5cc;
 unsigned int c7=version>=LPASS_CODEC_VERSION_2_5?0x5fc:0x5dc;
 if(writes[1].reg!=c3||writes[3].reg!=c7)return 2;
 unsigned int regs[]={CDC_WSA_COMPANDER0_CTL3,c3,CDC_WSA_COMPANDER0_CTL7,c7,CDC_WSA_BOOST0_BOOST_CTL,CDC_WSA_BOOST1_BOOST_CTL};
 unsigned int masks[]={0x80,0x80,1,1,0x7c,0x7c};
 for(unsigned int i=0;i<6;i++) {
  unsigned int val=i<2?(mode==1?0:0x80):i<4?(mode==1?0:1):(mode==1?0x44:0x58);
  if(writes[i].reg!=regs[i]||writes[i].mask!=masks[i]||writes[i].value!=val)return 3;
 }
 return 0;
}
'''


class MaintenanceCases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.bins = {}
        for label, tree in [('old', OLD), ('new', NEW)]:
            apm = (tree/'sound/soc/qcom/qdsp6/audioreach.c').read_text()
            wsa = (tree/'sound/soc/codecs/lpass-wsa-macro.c').read_text()
            for kind, source in [('apm', APM_HEAD + function(apm, 'audioreach_send_protected_graph_calibration') + APM_MAIN),
                                 ('wsa', WSA_HEAD + definitions(wsa) + '\n' + function(wsa, 'wsa_macro_set_spkr_mode') + WSA_MAIN)]:
                c = Path(cls.tmp.name)/(label+'-'+kind+'.c')
                c.write_text(source)
                binary = c.with_suffix('')
                subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2', str(c), '-o', str(binary)], check=True)
                cls.bins[label, kind] = binary

    def apm(self, scenario, label='new'):
        return tuple(map(int, subprocess.check_output([str(self.bins[label, 'apm']), str(scenario)], text=True).split()))

    def test_unsupported_calibration_has_no_active_resend(self):
        self.assertEqual(self.apm(1, 'old'), (0, 1, 1, 0))
        self.assertEqual(self.apm(1), (0, 1, 0, 0))

    def test_transport_failure_returns_original_without_mutating_diagnostics(self):
        self.assertEqual(self.apm(2, 'old'), (-5, 3, 1, 1))
        self.assertEqual(self.apm(2), (-5, 1, 0, 0))

    def test_success_preserves_full_aggregate(self):
        self.assertEqual(self.apm(0), (0, 1, 0, 0))

    def test_absent_calibration_does_not_send(self):
        self.assertEqual(self.apm(3), (0, 0, 0, 0))

    def test_find_stage_error_propagates(self):
        self.assertEqual(self.apm(4), (-12, 0, 0, 0))

    def test_wrong_calibration_size_refused(self):
        self.assertEqual(self.apm(5), (-22, 0, 0, 0))

    def test_invalid_modules_refused(self):
        self.assertEqual(self.apm(6), (-22, 0, 0, 0))

    def test_compander_new_silicon_regression_reproduced(self):
        self.assertEqual(subprocess.run([str(self.bins['old', 'wsa']), str(0x205), '1']).returncode, 2)

    def test_compander_generation_and_mode_matrix(self):
        for version in [0x201, 0x205, 0x209]:
            for mode in [0, 1, 99]:
                with self.subTest(version=version, mode=mode):
                    subprocess.run([str(self.bins['new', 'wsa']), str(version), str(mode)], check=True)


if __name__ == '__main__':
    unittest.main()
