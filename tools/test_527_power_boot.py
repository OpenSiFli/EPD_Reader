"""Run board power initialization and lock release with missing wake hardware."""
from pathlib import Path
import argparse,subprocess,tempfile

p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
power=(root/'epdiy-epub/src/boards/sf32lb527/reader_power.c').read_text(encoding='utf8')
board=(root/'epdiy-epub/src/boards/sf32lb527/board527.c').read_text(encoding='utf8')
def function(source,name):
    start=source.index(name);brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]+'\n'

stubs=r'''
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>
using rt_base_t=int;
#define RT_NULL nullptr
#define RT_TIMER_FLAG_ONE_SHOT 0
#define PM_HIBERNATE_BOOT 2
#define RT_ENOSYS 38
#define AON_PIN_MODE_NEG_EDGE 1
#define rt_kprintf(...) ((void)0)
static int forced_awake,locks,timer_ready,notify_ready,power_calls,test_timer_ready;
static int touch_error,wake_error,display_error,light_error;
static unsigned entered,exits,slept_ms,longest_ms;
static int test_timer;
struct RTC_HandleTypeDef {void *Instance;};
static void *hwp_rtc;
static rt_base_t rt_hw_interrupt_disable(){return 0;}
static void rt_hw_interrupt_enable(rt_base_t){}
static void reader_power_busy(){++locks;}
static void reader_power_idle(){assert(locks>0);--locks;}
static void test_finished(void *){}
static void notify(unsigned char,unsigned char,void *){}
static unsigned rt_tick_from_millisecond(unsigned n){return n;}
static void rt_timer_init(int *,const char *,void (*)(void *),void *,unsigned,unsigned){timer_ready=1;}
static void rt_timer_stop(int *){assert(timer_ready);}
static void rt_timer_start(int *){assert(timer_ready);}
static unsigned rt_tick_get(){return 0;}
static void rt_pm_notify_set(void (*)(unsigned char,unsigned char,void *),void *){notify_ready=1;}
static int pm_enable_pin_wakeup(int,int){assert(timer_ready&&notify_ready);++power_calls;return wake_error;}
static int SystemPowerOnModeGet(){return 0;}
static void HAL_RTC_DeactivateWakeUpTimer(RTC_HandleTypeDef *){}
static void HAL_PMU_DisableRtcWakeup(){}
static int tps_init(int){return 0;}
static int board527_display_init(){return display_error;}
static void board527_io_init(){}
static int frontlight_start(){return light_error;}
static int board527_touch_init(){return touch_error;}
'''
source=stubs
for signature in ('static void diagnostic_hold','void board527_power_hold','void board527_power_auto','static int pwr','int board527_power_init'):
    source+=function(power,signature)
source+=function(board,'int board527_init')
source+=r'''
int main(int argc,char **argv){
    assert(argc==2);int mode=std::atoi(argv[1]);
    touch_error=(mode==1||mode==3)?-1:0;
    wake_error=(mode==2||mode==3)?-1:0;
    display_error=mode==4?-1:0;light_error=mode==5?-1:0;
    char command[]="off";char *command_args[]={nullptr,command};
    assert(pwr(2,command_args)==-RT_ENOSYS&&locks==0&&!timer_ready);
    board527_power_auto();assert(locks==0);
    int rc=board527_init();
    if(display_error){assert(rc!=0&&power_calls==0&&locks==0);return 0;}
    assert(power_calls==1&&timer_ready&&notify_ready);
    assert(locks==((touch_error||wake_error)?1:0));
    assert((rc!=0)==bool(touch_error||light_error));
    board527_init();assert(power_calls==1);
    board527_power_hold();board527_power_hold();assert(locks==1);
    board527_power_auto();assert(locks==0);
    assert(!pwr(2,command_args)&&locks==1);strcpy(command,"on");
    assert(!pwr(2,command_args)&&locks==0);
    board527_power_auto();assert(locks==0);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='power-boot-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe';cpp.write_text(source,encoding='utf8')
    subprocess.run([args.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    for mode in range(6):subprocess.run([str(exe),str(mode)],check=True,timeout=10)
print('POWER_BOOT_MISSING_TOUCH_WAKE_FAILURE_TIMER_INIT_BALANCED_HOLD_PASS')
