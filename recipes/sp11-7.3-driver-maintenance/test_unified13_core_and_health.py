"""Exercise real link diagnostics and speaker health work with fake devices."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from test_unified13_audio_maintenance import COMMON, OLD, NEW, definitions, function

CORE_HEAD = COMMON + r'''
#define FWLINK_FLAG_IGNORE 1
#define FWNODE_FLAG_NEEDS_CHILD_BOUND_ON_ADD 2
#define FWNODE_FLAG_NOT_DEVICE 4
#define FWNODE_FLAG_INITIALIZED 8
#define DL_DEV_NO_DRIVER 0
#define DL_DEV_PROBING 1
#define DL_DEV_DRIVER_BOUND 2
#define DL_FLAG_SYNC_STATE_ONLY BIT(7)
#define DL_FLAG_INFERRED BIT(8)
#define FW_DEVLINK_FLAGS_PERMISSIVE (DL_FLAG_SYNC_STATE_ONLY|DL_FLAG_INFERRED)
#define dev_dbg(dev,...) ((void)(dev),debug_logs++)
#define dev_err(dev,...) ((void)(dev),error_logs++)
#define pr_debug(...) ((void)0)
struct fwnode_handle {unsigned int flags;};
struct device {struct {int status;} links;struct fwnode_handle *fwnode;};
struct fwnode_link {unsigned int flags;struct fwnode_handle *consumer;};
static struct fwnode_handle con_node,sup_node;
static struct device supplier;
static int debug_logs,error_logs,link_calls,put_calls,make_link,missing_supplier;
static bool fwnode_test_flag(struct fwnode_handle*n,unsigned int f){return n->flags&f;}
static bool fwnode_is_ancestor_of(struct fwnode_handle*a,struct fwnode_handle*b){(void)a;(void)b;return false;}
static void device_links_write_lock(void){}
static void device_links_write_unlock(void){}
static bool __fw_devlink_relax_cycles(struct fwnode_handle*a,struct fwnode_handle*b){(void)a;(void)b;return false;}
static void __fwnode_link_cycle(struct fwnode_link*l){(void)l;}
static unsigned int fw_devlink_get_flags(unsigned int f){return f;}
static struct device *get_dev_from_fwnode(struct fwnode_handle*f){(void)f;return missing_supplier?NULL:&supplier;}
static struct device *fwnode_get_next_parent_dev(struct fwnode_handle*f){return get_dev_from_fwnode(f);}
static void *device_link_add(struct device*c,struct device*s,unsigned int f){(void)c;(void)s;(void)f;link_calls++;return make_link?&supplier:NULL;}
static bool fwnode_init_without_drv(struct fwnode_handle*f){(void)f;return false;}
static bool fwnode_ancestor_init_without_drv(struct fwnode_handle*f){(void)f;return false;}
static void put_device(struct device*d){(void)d;put_calls++;}
'''
CORE_MAIN = r'''
int main(int argc,char**argv){
 (void)argc;struct device consumer={.fwnode=&con_node};
 consumer.links.status=atoi(argv[1]);unsigned int flags=strtoul(argv[2],NULL,0);
 make_link=atoi(argv[3]);missing_supplier=atoi(argv[4]);
 struct fwnode_link l={.flags=flags,.consumer=&con_node};
 int ret=fw_devlink_create_devlink(&consumer,&sup_node,&l);
 printf("%d %d %d %d %d\n",ret,error_logs,debug_logs,link_calls,put_calls);return 0;
}
'''

HEALTH_HEAD = COMMON + r'''
#define READ_ONCE(x) (x)
#define container_of(ptr,type,member) ((type*)((char*)(ptr)-offsetof(type,member)))
#define to_delayed_work(p) container_of(p,struct delayed_work,work)
#define dev_info(dev,...) do{(void)(dev);if(0)printf(__VA_ARGS__);}while(0)
struct work_struct {int unused;};
struct delayed_work {struct work_struct work;};
struct workqueue_struct {int id;};
static struct workqueue_struct legacy={.id=1},current={.id=2};
static struct workqueue_struct *system_wq __attribute__((unused))=&legacy;
static struct workqueue_struct *system_percpu_wq __attribute__((unused))=&current;
struct wsa884x_priv {
 struct delayed_work pa_health_work;void *dev,*regmap;int sp_lock;
 bool pa_on;unsigned int pa_recovery_failures,observe_remaining,observe_sequence;
};
static int pm_error,read_error,bad_state,recovery_error;
static int resumes,pm_puts,busy,locks,unlocks,recoveries,rearm_queue,initial_queue,reads;
static int pm_runtime_resume_and_get(void*d){(void)d;resumes++;return pm_error;}
static void pm_runtime_mark_last_busy(void*d){(void)d;busy++;}
static void pm_runtime_put_autosuspend(void*d){(void)d;pm_puts++;}
static void mutex_lock(int*l){(void)l;locks++;}
static void mutex_unlock(int*l){(void)l;unlocks++;}
static int regmap_read(void*m,unsigned int r,unsigned int*v){(void)m;(void)r;reads++;*v=bad_state?31:0;return read_error;}
static int wsa884x_recover_pa(struct wsa884x_priv*w,bool enable){(void)w;(void)enable;recoveries++;return recovery_error;}
static unsigned int msecs_to_jiffies(unsigned int t){return t;}
static void schedule_delayed_work(struct delayed_work*w,unsigned int t){(void)w;if(t!=100)exit(30);rearm_queue=current.id;}
static void mod_delayed_work(struct workqueue_struct*q,struct delayed_work*w,unsigned int t){(void)w;if(t!=100)exit(31);initial_queue=q->id;}
'''
HEALTH_MAIN = r'''
int main(int argc,char**argv){
 (void)argc;int scenario=atoi(argv[1]);struct wsa884x_priv state={.pa_on=true};
 struct wsa884x_priv *wsa884x=&state;
 INITIAL_ARM
 if(scenario==1)pm_error=-EIO;
 if(scenario==2)state.pa_on=false;
 if(scenario==3){bad_state=true;recovery_error=-EIO;}
 if(scenario==4){bad_state=true;state.pa_recovery_failures=WSA884X_PA_RECOVERY_FAILURES;}
 if(scenario==5){read_error=-EIO;}
 wsa884x_pa_health_work(&state.pa_health_work.work);
 printf("%d %d %d %d %d %d %d %d %d\n",initial_queue,rearm_queue,resumes,pm_puts,busy,locks,unlocks,recoveries,state.pa_recovery_failures);return 0;
}
'''


class CoreAndHealthCases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.bins = {}
        for label, tree in [('old', OLD), ('new', NEW)]:
            core = (tree/'drivers/base/core.c').read_text()
            health = (tree/'sound/soc/codecs/wsa884x.c').read_text()
            arm = re.search(r'mod_delayed_work\(system_\w+, &wsa884x->pa_health_work,\s*msecs_to_jiffies\(WSA884X_PA_HEALTH_INTERVAL_MS\)\);', health).group()
            sources = {
                'core': CORE_HEAD + function(core, 'fw_devlink_create_devlink') + CORE_MAIN,
                'health': HEALTH_HEAD + definitions(health) + '\n' + function(health, 'wsa884x_pa_health_work') + HEALTH_MAIN.replace('INITIAL_ARM', arm),
            }
            for kind, text in sources.items():
                c = Path(cls.tmp.name)/(label+'-'+kind+'.c')
                c.write_text(text)
                binary = c.with_suffix('')
                subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2', str(c), '-o', str(binary)], check=True)
                cls.bins[label, kind] = binary

    def run_c(self, kind, *args, label='new'):
        return tuple(map(int, subprocess.check_output([str(self.bins[label, kind]), *map(str, args)], text=True).split()))

    def test_link_intentional_skip_was_error(self):
        self.assertEqual(self.run_c('core', 2, 0x180, 0, 0, label='old'), (-22, 1, 0, 1, 1))
        self.assertEqual(self.run_c('core', 2, 0x180, 0, 0), (-22, 0, 1, 1, 1))

    def test_real_link_failures_remain_errors(self):
        for status, flags in [(0, 0x180), (1, 0x180), (2, 0x120), (3, 0x120)]:
            with self.subTest(status=status, flags=flags):
                self.assertEqual(self.run_c('core', status, flags, 0, 0), (-22, 1, 0, 1, 1))

    def test_success_and_probe_deferral_unchanged(self):
        for label in ['old', 'new']:
            self.assertEqual(self.run_c('core', 2, 0x180, 1, 0, label=label), (0, 0, 0, 1, 1))
            self.assertEqual(self.run_c('core', 0, 0x120, 0, 1, label=label), (-11, 0, 0, 0, 1))

    def test_health_initial_and_rearm_use_same_current_queue(self):
        self.assertEqual(self.run_c('health', 0, label='old')[:2], (1, 2))
        self.assertEqual(self.run_c('health', 0), (2, 2, 1, 1, 1, 1, 1, 0, 0))

    def test_health_pm_failure_retries_without_unbalanced_put(self):
        self.assertEqual(self.run_c('health', 1), (2, 2, 1, 0, 0, 0, 0, 0, 0))

    def test_stopped_amplifier_does_not_rearm(self):
        self.assertEqual(self.run_c('health', 2), (2, 0, 1, 1, 1, 1, 1, 0, 0))

    def test_health_error_still_attempts_recovery(self):
        self.assertEqual(self.run_c('health', 3), (2, 2, 1, 1, 1, 1, 1, 1, 1))

    def test_health_recovery_limit_preserved(self):
        r = self.run_c('health', 4)
        self.assertEqual(r[:8], (2, 2, 1, 1, 1, 1, 1, 0))
        self.assertGreater(r[8], 0)

    def test_health_read_error_rearms_without_false_recovery(self):
        self.assertEqual(self.run_c('health', 5), (2, 2, 1, 1, 1, 1, 1, 0, 0))


if __name__ == '__main__':
    unittest.main()
