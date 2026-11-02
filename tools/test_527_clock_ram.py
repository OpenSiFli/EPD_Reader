"""Run retained-clock loop with RTC/key events, fail injection and lock checks."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]/'epdiy-epub/src/boards/sf32lb527'
s=(root/'standby_clock.cpp').read_text(encoding='utf8')
actual=s[s.index('int reader527_standby_ram'):]
stubs=r'''
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#define RT_ENOMEM 12
#define RT_ERROR 1
#define RT_EIO 5
#define RT_ETIMEOUT 110
#define RT_EOK 0
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_CMD_RESET 0
#define RTC_WAKEUP_SEC 1
#define HAL_OK 0
#define AON_PIN_MODE_POS_EDGE 1
#define PIN_IRQ_DISABLE 0
#define HPSYS_AON_WER_RTC 1u
#define HPSYS_AON_WER_PIN10 (1u<<18)
#define rt_kprintf(...) ((void)0)
#define MAX_AREAS 30
struct AreaRect{int x;};static AreaRect g_area_array[MAX_AREAS];static int g_area_count;
struct Renderer{void cancel_page_preview(){}int get_margin_top(){return 35;}int get_margin_left(){return 10;}int get_page_height(){return 1115;}int get_page_width(){return 664;}void set_margin_top(int){}void set_margin_bottom(int){}void set_margin_left(int){}void set_margin_right(int){}};
struct Reader{void pause_background(){}void resume_background(){}};static Reader *reader;
using rt_mq_t=void *;static rt_mq_t ui_queue;
using RTC_cb=void(*)(int);static void original_callback(int){}static void ram_rtc_callback(int){}
static RTC_cb saved_rtc_callback;static struct {RTC_cb callback=original_callback;} RTC_Handler;
static struct {uint32_t WER=0x123;} aon;
#define hwp_hpsys_aon (&aon)
static int ram_event;static bool ram_event_initialized,ram_active,rtc_seen,key_seen;
static bool ram_restored;
static unsigned ram_wake_tick,now,reads,waits,votes=1,exclusive,quiet,restores,aborts,live_alloc;
static int failure,key_after;
static std::vector<int> fulls;static std::vector<unsigned> drawn;
static uint8_t frame[1216*684/2];
static uint32_t rt_tick_get_millisecond(){return now;}
static void *epub_mem_malloc(unsigned n){if(failure==1)return nullptr;++live_alloc;return malloc(n);}
static void epub_mem_free(void *p){assert(live_alloc);--live_alloc;free(p);}
static uint8_t *board527_framebuffer(){return frame;}
static void epd_font_ft_preheat_stop(){}
static void board527_frontlight_get(unsigned *c,unsigned *w){*c=*w=0;}
static void board527_storage_pm_enable(int){}
static void board527_power_auto(){}
static int font_manager_select(int,int){return failure==2?-1:0;}
static int board527_frontlight_set(unsigned,unsigned){return 0;}
static int board527_amplifier_test(int){return 0;}
static void board527_touch_power(int){}
static void watch_key(){key_seen=false;}
static bool read_clock(unsigned &m,unsigned &s){++reads;m=600;s=15;return failure!=3;}
static void clear_areas(){}
static void draw_clock(Renderer *,unsigned m){drawn.push_back(m);}
static void draw_clock_time(Renderer *,unsigned m){drawn.push_back(m);}
static int board527_clock_present(uint8_t *,int full){fulls.push_back(full);return failure==4&&fulls.size()==1?-1:0;}
static int board527_io_diagnostic_begin(){if(failure==5)return -1;exclusive=1;return 0;}
static void board527_io_diagnostic_end(){assert(exclusive);exclusive=0;}
static void ram_save_pins(){}
static void pm_disable_pin_wakeup(int){}
static int pm_enable_pin_wakeup(int,int){return failure==6?-1:0;}
static int pm_enable_rtc_wakeup(){return 0;}
static int reader527_steps_quiet_start(){quiet=1;return failure==7?-1:0;}
static int reader527_steps_quiet_finish(){quiet=0;return 0;}
static void rt_sem_init(int *,const char *,int,int){}
static void rt_sem_control(int *,int,void *){}
static void HAL_RTC_RegCallback(decltype(RTC_Handler) *r,RTC_cb cb){r->callback=cb;}
static void board527_steps_uart_idle(int){}
static void stop_rtc_wake(){}
static int HAL_RTC_SetWakeUpTimer(decltype(RTC_Handler) *,unsigned,unsigned){return failure==8?-1:0;}
static int ram_external_off(){assert(exclusive&&votes==1&&aon.WER==(HPSYS_AON_WER_RTC|HPSYS_AON_WER_PIN10));return failure==9?-1:0;}
static void board527_power_stats(uint32_t *n,uint32_t *t){*n=waits;*t=waits*5000;}
static void rt_thread_mdelay(unsigned n){now+=n;}
static unsigned rt_tick_from_millisecond(unsigned n){return n;}
static void reader_power_idle(){assert(votes==1);--votes;}
static void reader_power_busy(){assert(votes==0);++votes;}
static int rt_sem_take(int *,unsigned){assert(votes==0&&exclusive);++waits;now+=5000;ram_wake_tick=now;rtc_seen=true;if(key_after&&waits==unsigned(key_after))key_seen=true;return failure==10?-1:0;}
static int ram_display_power(){++restores;return 0;}
static bool clock_next_is_full(unsigned n){return n>=10;}
static void pm_disable_rtc_wakeup(){}
static void rt_pin_irq_enable(int,int){}
static int abort_clock(const char *){++aborts;return -1;}
static void reading_settings_load(Renderer *){assert(!exclusive&&!quiet);}
static int board527_touch_test_sleep(int){return 0;}
static int rt_mq_recv(void *,void *,unsigned,int){return -1;}
'''
tests=r'''
int main(){Renderer r;
 for(failure=0;failure<=10;++failure){
    now=reads=waits=exclusive=quiet=restores=aborts=live_alloc=0;votes=1;key_after=0;
    ram_active=false;ram_event_initialized=false;key_seen=false;fulls.clear();drawn.clear();aon.WER=0x123;RTC_Handler.callback=original_callback;
    int rc=reader527_standby_ram(&r,true);
    assert(votes==1&&!exclusive&&!quiet&&!live_alloc&&!ram_active&&aon.WER==0x123&&RTC_Handler.callback==original_callback);
    if(!failure){
      assert(!rc&&waits==11&&fulls.size()==13);assert(fulls[0]==1&&fulls[11]==1&&fulls[12]==1);
      for(unsigned n=1;n<=10;++n)assert(fulls[n]==0);
      for(unsigned n=0;n<drawn.size();++n)assert(drawn[n]==600+n);
      assert(reader527_standby_ram_take_restored());
      assert(!reader527_standby_ram_take_restored());
    } else {assert(rc!=0&&!aborts);assert(!reader527_standby_ram_take_restored());}
 }
 failure=0;key_after=1;waits=0;fulls.clear();drawn.clear();assert(!reader527_standby_ram(&r,false));assert(waits==1&&fulls.size()==2&&votes==1&&!exclusive&&reader527_standby_ram_take_restored());
 puts("CLOCK_RAM_RTC_KEY_CADENCE_FAILURE_RESTORE_LOCKS_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='clock-ram-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe';cpp.write_text(stubs+actual+tests,encoding='utf8')
    subprocess.run([a.cxx,'-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
