"""Exercise the actual ladder policy and abort/reboot sequence with HAL fakes."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
s=(root/'epdiy-epub/src/boards/sf32lb527/power_steps.cpp').read_text(encoding='utf8')
policy=s[s.index('static int apply_stage'):s.index('int reader527_power_steps')]
sequence=s[s.index('int reader527_power_steps'):s.index('/* Touch state comparison:')]
stubs=r'''
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>
#define RT_EINVAL 22
#define RT_ERROR 1
#define RT_EBUSY 16
#define RT_EOK 0
#define PMU_PERI_LDO3_3V3 16
#define rt_kprintf(...) ((void)0)
static int ldo3,aw_reset,logic,parked,outputs;
static int fail_at,active_stage,fail_stage=-1,waits,reboots,busy_depth=1;
static std::vector<unsigned> stages;
static struct {uint32_t PERI_LDO;} pmu;
#define hwp_pmuc (&pmu)
static void board527_minimum_park_buses(){parked=1;}
static int board527_minimum_external_off(){logic=0;return fail_at==9?-1:0;}
static void HAL_PMU_ConfigPeriLdo(int,int enable,int){ldo3=enable;}
static int board527_steps_aw(int reset,int on){aw_reset=reset;logic=on;return 0;}
static void output(unsigned,unsigned){++outputs;}
static void rt_thread_mdelay(unsigned ms){if(ms==5000)++waits;}
static int rt_pin_read(int){return !aw_reset;}
static unsigned tf_msd_read_count(){return 0;}
static void board527_power_stats(uint32_t *count,uint32_t *ms){*count=waits;*ms=waits*5000;}
struct Renderer {void cancel_page_preview(){}};
static void reader527_close_book_for_power_test(){}
static void board527_power_auto(){}
static int font_manager_select(int,int){return fail_at==1?-1:0;}
static int board527_frontlight_set(int,int){return fail_at==2?-1:0;}
static int board527_amplifier_test(int){return fail_at==3?-1:0;}
static void board527_touch_power(int){}
static int tps_enter_sleep(){return fail_at==4?-1:0;}
static int board527_storage_test_begin(){return fail_at==5?-1:0;}
static void save_pins(){}
static void pm_disable_pin_wakeup(int){}
static int reader527_steps_quiet_start(){return fail_at==6?-1:0;}
static void board527_steps_uart_idle(int){}
static int display_power_on(){return active_stage==fail_stage?-1:0;}
static void stage_card(Renderer *,unsigned n,unsigned){stages.push_back(n);active_stage=n+1;}
static void reader_power_idle(){assert(busy_depth==1);--busy_depth;}
static void reader_power_busy(){assert(busy_depth==0);++busy_depth;}
static void drv_reboot(){++reboots;}
'''
tests=r'''
int main(){
    for(unsigned n=0;n<4;++n){
        parked=outputs=0;assert(apply_stage(n)==0);
        assert(parked&&aw_reset==(n==0)&&ldo3==(n>=2)&&logic==(n>=3));
        assert(outputs==(n==3?18:0));
    }
    parked=0;assert(apply_stage(4)==-RT_EINVAL);assert(!parked);
    Renderer r;
    for(fail_at=0;fail_at<=9;++fail_at){
        stages.clear();waits=reboots=active_stage=0;fail_stage=-1;busy_depth=1;
        int rc=reader527_power_steps(&r,5);
        assert(reboots==1&&busy_depth==1);
        if(fail_at==0||fail_at==7||fail_at==8){assert(rc==0&&waits==4&&stages.size()==4);}
        else assert(rc!=0&&waits==0);
    }
    fail_at=0;
    for(fail_stage=0;fail_stage<4;++fail_stage){
        stages.clear();waits=reboots=active_stage=0;busy_depth=1;
        assert(reader527_power_steps(&r,5)!=0);assert(waits==fail_stage&&reboots==1&&busy_depth==1);
    }
    puts("POWER_STEPS_POLICY_INCREMENT_ORDER_AND_ABORT_REBOOT_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='power-steps-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+policy+sequence+tests,encoding='utf8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
