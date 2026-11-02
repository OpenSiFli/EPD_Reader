"""Validate Hibernate clock state/cadence and actual shutdown failure gates."""
from pathlib import Path
import argparse,subprocess,tempfile,struct
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
folder=root/'epdiy-epub/src/boards/sf32lb527'
h=(folder/'standby_clock.h').read_text(encoding='utf8').replace('#pragma once','')
s=(folder/'standby_clock.cpp').read_text(encoding='utf8')
shutdown=s[s.index('static int hibernate_clock'):s.index('void reader527_standby_boot')]
stubs=r'''
#include <cassert>
#include <cstdio>
using rt_tick_t=unsigned;
struct RTC_HandleTypeDef{};
struct PMU {unsigned WER;};static PMU pmu;
#define hwp_pmuc (&pmu)
#define HAL_OK 0
#define PAD_PA34 34
#define GPIO_A34 34
#define PIN_PULLDOWN 0
#define PIN_MODE_INPUT 0
#define AON_PIN_MODE_HIGH 1
#define RTC_WAKEUP_SEC 1
#define RTC_BACKUP_NAND_OTA_DES 8
#define rt_kprintf(...) ((void)0)
static unsigned fail,hibernate_calls,abort_calls,reboot_calls,now_minute=750,now_second=41,wut_value,saved,parks,guard_waits;
static bool key_seen;
static bool read_clock(unsigned &m,unsigned &s){m=now_minute;s=now_second;return fail!=1;}
static int abort_clock(const char *){++abort_calls;return -1;}
static void stop_rtc_wake(){}
static void HAL_PIN_Set(int,int,int,int){}
static void rt_pin_mode(int,int){}
static int rt_pin_read(int){return fail==2;}
static void leave_clock(){saved=0;}
static void drv_reboot(){++reboot_calls;}
static void HAL_PMU_SelectWakeupPin(int n,int pin){assert(n==0&&pin==10);}
static void HAL_PMU_EnablePinWakeup(int,int){pmu.WER|=8;}
static bool board527_logic_sleep_ready(){++guard_waits;return fail!=3;}
static void rt_thread_mdelay(unsigned){}
static void board527_minimum_park_buses(){++parks;}
static int board527_minimum_external_off(){assert(parks);return fail==4?-1:0;}
static RTC_HandleTypeDef rtc_handle(){return {};}
static int HAL_RTC_SetWakeUpTimer(RTC_HandleTypeDef *,unsigned count,unsigned mode){assert(mode==RTC_WAKEUP_SEC);wut_value=count;return fail==5?-1:0;}
static void HAL_PMU_EnableRtcWakeup(){pmu.WER|=1;}
static void HAL_Set_backup(unsigned index,unsigned value){assert(index==8);saved=value;}
static void board527_minimum_hibernate(){assert(pmu.WER==9&&saved);++hibernate_calls;}
'''
tests=r'''
int main(){
    ClockStandbyState s={};
    for(unsigned minute=0;minute<1440;++minute)for(unsigned n=0;n<=10;++n){
        ClockStandbyState in={minute,n,0,false};uint32_t w=clock_state_pack(in);
        assert(clock_state_unpack(w,s)&&s.minute==minute&&s.partials==n&&!s.test);
        for(unsigned bit=0;bit<32;++bit)assert(!clock_state_unpack(w^(1u<<bit),s));
    }
    assert(!clock_state_unpack(0,s));
    for(unsigned left=0;left<=11;++left){ClockStandbyState in={1439,10,left,true};assert(clock_state_unpack(clock_state_pack(in),s)&&s.left==left);}
    unsigned partials=0,fulls=0;
    for(unsigned n=1;n<=22;++n){bool full=clock_next_is_full(partials);assert(full==(n%11==0));partials=full?0:partials+1;fulls+=full;}assert(fulls==2);
    s={750,0,0,false};assert(clock_should_tick(true,true,false,s));
    assert(!clock_should_tick(true,true,true,s)&&!clock_should_tick(false,true,false,s)&&!clock_should_tick(true,false,false,s));
    s.test=true;assert(!clock_should_tick(true,true,false,s));s.left=1;assert(clock_should_tick(true,true,false,s));
    for(fail=0;fail<=5;++fail){
        hibernate_calls=abort_calls=reboot_calls=parks=guard_waits=saved=0;
        hibernate_clock({750,0,0,false});
        if(!fail){assert(hibernate_calls==1&&wut_value==18);assert(clock_state_unpack(saved,s));}
        else assert(!hibernate_calls&&(abort_calls||reboot_calls));
    }
    fail=0;hibernate_clock({750,0,11,true});assert(wut_value==4);
    now_minute=751;hibernate_clock({750,0,0,false});assert(wut_value==0);
    key_seen=true;hibernate_calls=0;hibernate_clock({750,0,0,false});assert(!hibernate_calls);
    puts("CLOCK_STATE_CRC_CADENCE_RTC_KEY_PRIORITY_SHUTDOWN_GATES_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='clock-standby-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe';cpp.write_text(h+stubs+shutdown+tests,encoding='utf8')
    subprocess.run([a.cxx,'-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=15)
# For binary clocks, unchanged black/white must remain neutral in DU phases.
wave=(root/'epdiy-epub/waveform/e0470a03/epd_waveform.bin').read_bytes()
zones,off=struct.unpack_from('<II',wave,16)
for z in range(zones):
    low,high,count,table=struct.unpack_from('<iiII',wave,off+16*z)
    if low<=28<high:break
for phase in range(count):
    for transition in (0,255):assert ((wave[table+phase*64+transition//4]>>(6-2*(transition%4)))&3)==0
print('CLOCK_DU_UNCHANGED_BLACK_WHITE_NEUTRAL_PASS')
