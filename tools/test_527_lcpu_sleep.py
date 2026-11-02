"""Replay the actual domain transition with forbidden sleeping-domain accesses."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
s=(root/'epdiy-epub/src/boards/sf32lb527/power_measure.cpp').read_text(encoding='utf8')
s=s[s.index('static int quiet_lcpu'):s.index('static void network_change')]
stubs=r'''
#include <cstdint>
#include <cstdio>
#include <cassert>
#define RT_EBUSY 16
#define RT_ERROR 1
#define RT_ETIMEOUT 110
#define LPSYS_AON_PMR_CPUWAIT 4
#define LPSYS_AON_PMR_MODE 3
#define HPSYS_AON_ISSR_HP2LP_REQ 1
#define HPSYS_AON_ISSR_LP_ACTIVE 32
#define rt_kprintf(...) ((void)0)
static bool domain_awake=true, service, wake_failure, bluetooth_ui_enabled;
static uint32_t tick,lp_pmr,lp_wer,lp_anacr;
static int lp_changed;
struct ProtectedRegister {
    uint32_t value=0;
    operator uint32_t()const{assert(domain_awake);return value;}
    void operator=(uint32_t v){assert(domain_awake);value=v;}
};
static struct LP {ProtectedRegister PMR,WER,ANACR;} lp;
static struct HP {uint32_t ISSR;} hp;
#define hwp_lpsys_aon (&lp)
#define hwp_hpsys_aon (&hp)
static bool pan_service_is_ready(){return service;}
static uint32_t rt_tick_get(){return tick;}
static unsigned rt_tick_from_millisecond(unsigned n){return n;}
static void rt_thread_mdelay(unsigned n){tick+=n;}
static void HAL_Delay_us(unsigned){
    if((hp.ISSR&1)&&!wake_failure){hp.ISSR|=32;domain_awake=true;}
}
static void HAL_LPAON_Sleep(){
    lp.PMR=0x80000007;lp.WER=lp.WER.value|2;lp.ANACR=lp.ANACR.value|3;
    domain_awake=false;hp.ISSR&=~32;
}
'''
tests=r'''
static void reset(){
    domain_awake=true;lp_changed=0;service=wake_failure=bluetooth_ui_enabled=false;
    lp.PMR=4;lp.WER=0x40;lp.ANACR=0x80;hp.ISSR=0x30;tick=0xfffffff0;
}
int main(){
    reset();assert(quiet_lcpu(1)==0);assert(!domain_awake);
    assert(quiet_lcpu(1)==-RT_EBUSY); // No read from sleeping LPSYS.
    assert(quiet_lcpu(0)==0);assert(domain_awake&&!lp_changed);
    assert(lp.PMR.value==4&&lp.WER.value==0x40&&lp.ANACR.value==0x80&&hp.ISSR==0x30);
    reset();bluetooth_ui_enabled=true;assert(quiet_lcpu(1)==-RT_EBUSY);
    reset();service=true;assert(quiet_lcpu(1)==-RT_EBUSY);
    reset();lp.PMR=0;assert(quiet_lcpu(1)==-RT_EBUSY);
    reset();hp.ISSR|=1;assert(quiet_lcpu(1)==-RT_EBUSY);
    reset();domain_awake=false;hp.ISSR&=~32;assert(quiet_lcpu(1)==-RT_EBUSY);
    reset();assert(quiet_lcpu(1)==0);wake_failure=true;
    assert(quiet_lcpu(0)==-RT_ETIMEOUT);assert(lp_changed&&!domain_awake);
    wake_failure=false;assert(quiet_lcpu(0)==0);assert(domain_awake);
    puts("LCPU_SLEEP_NO_OFF_DOMAIN_ACCESS_RESTORE_TIMEOUT_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='lp-sleep-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+s+tests,encoding='utf8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
