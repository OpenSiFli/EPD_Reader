"""Run actual touch-state diagnostic code against failing I2C and power fakes."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
touch=(root/'epdiy-epub/src/boards/sf32lb527/ft5446.c').read_text(encoding='utf8')
steps=(root/'epdiy-epub/src/boards/sf32lb527/power_steps.cpp').read_text(encoding='utf8')
stubs=r'''
#include <cstdint>
#include <cassert>
#include <cstdio>
#include <vector>
#define RT_EINVAL 22
#define RT_ENOSYS 38
#define RT_ERROR 1
#define RT_EBUSY 16
#define RT_EOK 0
#define RT_EIO 5
static int board527_io_enter(){return 0;}
static void board527_io_exit(){}
#define RT_WAITING_FOREVER -1
#define RT_IPC_CMD_RESET 0
#define RT_NULL nullptr
#define rt_kprintf(...) ((void)0)
static unsigned initialized=1,ready=1;
static int lock,sem,bus;
static int lock_depth,pa32,reset_high,sleep_sent,irq_enabled=1,reads_after_sleep;
static int operation,fail_operation,wrong_id,waits,pm_depth=1,reboots,card_present;
static int prep_fail,stage_fail=-1,stage;
static std::vector<unsigned> modes;
static int step_failure(){return ++operation==fail_operation;}
static void rt_mutex_take(int *,int){assert(lock_depth==0);++lock_depth;}
static void rt_mutex_release(int *){assert(lock_depth==1);--lock_depth;}
static void rt_sem_control(int *,int,void *){}
static void rt_pin_write(int pin,int high){assert(pin==32);pa32=high;}
static int board_touch_reset(int high){reset_high=high;if(!high)sleep_sent=0;return step_failure()?-1:0;}
static void disable(){ready=0;irq_enabled=0;reset_high=pa32=0;sleep_sent=0;}
static void rt_thread_mdelay(unsigned ms){if(ms==5000)++waits;}
static int read_regs(unsigned reg,uint8_t *v,unsigned){
    if(sleep_sent)++reads_after_sleep;
    if(step_failure())return -1;
    *v=reg==0xa3?(wrong_id?0:0x54):reg==0?0:1;return 0;
}
static int rt_i2c_mem_write(int,unsigned,unsigned reg,unsigned,const uint8_t *v,unsigned){
    if(step_failure())return 0;
    if(reg==0xa5){assert(*v==3);sleep_sent=1;}else assert(reg==0xa4&&*v==1);
    return 1;
}
'''
actual=touch[touch.index('int board527_touch_quiet_mode'):]
tests=r'''
static void test_modes(){
    for(unsigned mode=0;mode<3;++mode){
        operation=fail_operation=wrong_id=0;ready=irq_enabled=1;
        assert(board527_touch_quiet_mode(mode)==0);
        int count=operation;
        assert(pa32==1&&ready==0&&irq_enabled==0&&lock_depth==0);
        assert(reset_high==(mode!=0)&&sleep_sent==(mode==2));
        assert(reads_after_sleep==0);
        for(int failure=1;failure<=count;++failure){
            operation=0;fail_operation=failure;
            assert(board527_touch_quiet_mode(mode)!=0);
            assert(!reset_high&&!ready&&!irq_enabled&&lock_depth==0);
        }
    }
    fail_operation=0;wrong_id=1;
    assert(board527_touch_quiet_mode(1)!=0&&!reset_high);wrong_id=0;
    initialized=0;assert(board527_touch_quiet_mode(0)==-RT_ENOSYS);initialized=1;
    assert(board527_touch_quiet_mode(3)==-RT_EINVAL);
}
struct Renderer {void cancel_page_preview(){}};
static struct {uint32_t PERI_LDO;} pmu;
#define hwp_pmuc (&pmu)
#define PAD_PA10 10
#define PAD_PA11 11
#define I2C1_SCL 100
#define I2C1_SDA 101
#define PIN_NOPULL 0
static void reader527_close_book_for_power_test(){}
static void board527_power_auto(){}
static int board_tf_present(){return card_present;}
static int font_manager_select(int,int){return prep_fail==1?-1:0;}
static int board527_frontlight_set(int,int){return prep_fail==2?-1:0;}
static int board527_amplifier_test(int){return prep_fail==3?-1:0;}
static void board527_touch_power(int){}
static int tps_enter_sleep(){return prep_fail==4?-1:0;}
static int board527_storage_test_begin(){return prep_fail==5?-1:0;}
static void save_pins(){}
static void pm_disable_pin_wakeup(int){}
static int reader527_steps_quiet_start(){return prep_fail==6?-1:0;}
static void board527_steps_uart_idle(int){}
static int display_power_on(){return stage_fail==stage?-1:0;}
static void touch_card(Renderer *,unsigned n,unsigned){stage=n+1;}
static int apply_stage(unsigned n){assert(n==3);return prep_fail==7?-1:0;}
static void HAL_PIN_Set(int pin,int func,int,int){assert((pin==10&&func==100)||(pin==11&&func==101));}
static int probe(unsigned mode){modes.push_back(mode);return prep_fail==8?-1:0;}
#define board527_touch_quiet_mode probe
static unsigned tf_msd_read_count(){return 0;}
static void board527_power_stats(uint32_t *n,uint32_t *ms){*n=waits;*ms=waits*5000;}
static void reader_power_idle(){assert(pm_depth==1);--pm_depth;}
static void reader_power_busy(){assert(pm_depth==0);++pm_depth;}
static void drv_reboot(){++reboots;}
'''
sequence=steps[steps.index('int reader527_touch_states'):]
main=r'''
int main(){
    test_modes();Renderer r;
    for(prep_fail=0;prep_fail<=8;++prep_fail){
        waits=reboots=stage=0;pm_depth=1;modes.clear();
        int rc=reader527_touch_states(&r,5);
        assert(reboots==1&&pm_depth==1);
        if(!prep_fail){assert(rc==0&&waits==4);assert(modes==std::vector<unsigned>({0,1,2,0}));}
        else assert(rc!=0&&waits==0);
    }
    prep_fail=0;
    for(stage_fail=0;stage_fail<4;++stage_fail){
        waits=reboots=stage=0;pm_depth=1;
        assert(reader527_touch_states(&r,5)!=0);
        assert(reboots==1&&waits==stage_fail&&pm_depth==1);
    }
    stage_fail=-1;card_present=1;waits=reboots=0;
    assert(reader527_touch_states(&r,5)!=0&&waits==0&&reboots==1);
    puts("TOUCH_POWER_CONSTANT_RAILS_NO_SLEEP_READS_ABORT_REBOOT_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='touch-power-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+actual+tests+sequence+main,encoding='utf8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
