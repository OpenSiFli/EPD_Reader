"""Check the actual minimum-power preparation order and every abort gate."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
s=(root/'epdiy-epub/src/boards/sf32lb527/power_minimum.cpp').read_text(encoding='utf8')
s=s[s.index('int reader527_minimum_power('):]
stubs=r'''
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <vector>
#include <string>
static std::vector<std::string> events;
static int fail_at;
struct End {bool slept;};
struct Renderer {void cancel_page_preview(){} void clear_screen(){} void draw_text(int,int,const char*){} void flush_display(){events.push_back("draw");}};
static struct RTC {uint32_t CR=0;} rtc_regs;
static struct PMU {uint32_t WER=0,PERI_LDO=0;} pmu;
#define hwp_rtc (&rtc_regs)
#define hwp_pmuc (&pmu)
#define RTC_CR_ALRME 1
#define RTC_CR_ALRMIE 2
#define RTC_FLAG_WUTF 4
#define AON_PIN_MODE_HIGH 0
#define RTC_WAKEUP_SEC 0
#define HAL_OK 0
#define RT_EOK 0
#define rt_kprintf(...) ((void)0)
#define __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(...) ((void)0)
struct RTC_HandleTypeDef {RTC *Instance;};
static int step(const char *name,int index){events.push_back(name);return fail_at==index?-1:0;}
static void reader527_close_book_for_power_test(){events.push_back("close_books");}
static int font_manager_select(int,int){return step("builtin_font",1);}
static void clear_areas(){}
static int board527_storage_test_begin(){return step("unmount",2);}
static int board527_frontlight_set(int,int){return step("frontlight_off",3);}
static int board527_amplifier_test(int){return step("amplifier_off",4);}
static void board527_touch_power(int){events.push_back("touch_off");}
static int tps_enter_sleep(){return step("hv_off",5);}
static int HAL_RTC_DeactivateWakeUpTimer(RTC_HandleTypeDef *){return step("rtc_stop",6);}
static int rt_pin_read(int){return fail_at==7;}
static void HAL_PMU_SelectWakeupPin(int,int){}
static void HAL_PMU_EnablePinWakeup(int,int){pmu.WER|=8;}
static int HAL_RTC_SetWakeUpTimer(RTC_HandleTypeDef *,int counter,int clock){assert(counter==9&&clock==0);return step("rtc_test",8);}
static void HAL_PMU_EnableRtcWakeup(){pmu.WER|=1;}
static void board527_minimum_park_buses(){events.push_back("park_buses");}
static int board527_minimum_external_off(){return step("shared_rail_off",9);}
static void rt_thread_mdelay(int){events.push_back("drain_uart");}
static int minimum_abort(const char *){throw End{false};}
static void board527_minimum_hibernate(){events.push_back("ram_hibernate");throw End{true};}
'''
tests=r'''
int main(){
    Renderer r;
    for(int timed=0;timed<2;++timed)for(fail_at=0;fail_at<=9;++fail_at){
        events.clear();bool slept=false;
        try{reader527_minimum_power(&r,timed);}catch(End end){slept=end.slept;}
        bool pass=!fail_at||(!timed&&fail_at==8);assert(slept==pass);
        if(slept){
            assert(events[0]=="close_books"&&events[1]=="builtin_font"&&events[2]=="draw"&&events[3]=="unmount");
            auto n=events.size();assert(events[n-4]=="park_buses"&&events[n-3]=="shared_rail_off"&&events[n-2]=="drain_uart"&&events[n-1]=="ram_hibernate");
            assert(pmu.WER==(timed?9u:8u));
        }else{for(auto e:events)assert(e!="ram_hibernate");}
    }
    puts("MINIMUM_POWER_PREPARE_ORDER_ABORT_GATES_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='minpower-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+s+tests,encoding='utf8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
