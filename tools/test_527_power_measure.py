"""Exercise the real measurement sequencer's cleanup on every stage failure."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
source=(root/'epdiy-epub/src/boards/sf32lb527/power_measure.cpp').read_text(encoding='utf-8')
sequence=source[source.index('int reader527_power_measure('):source.index('int reader527_lcpu_measure(')]
lp_sequence=source[source.index('int reader527_lcpu_measure('):source.index('int reader527_amp_measure(')]
amp_sequence=source[source.index('int reader527_amp_measure('):]
stubs=r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
struct Renderer {void cancel_page_preview(){} void clear_screen(){} void draw_text(int,int,const char *){} void flush_display(){}};
struct Reader {void pause_background(){}};
static Reader instance,*reader=&instance;
static int failure,touch_sleep,touch_restores,network_paused,network_restores,awake;
static int lp_sleep,lp_restores;
static int amp_muted,amp_restores;
static std::vector<char> stages;
static int ui_queue;
#define RT_EOK 0
#define rt_kprintf(...) ((void)0)
static int rt_mq_recv(int,void *,unsigned,int){return -1;}
static void board527_power_auto(){}
static int board527_frontlight_set(int,int){return 0;}
static int board527_touch_test_sleep(int sleep){
    if(sleep){if(failure==1)return -1;touch_sleep=1;return 0;}
    ++touch_restores;if(failure==3)return -1;touch_sleep=0;return 0;
}
static int quiet_network(int quiet){
    if(quiet){if(failure==2)return -1;network_paused=1;return 0;}
    ++network_restores;if(failure==4)return -1;network_paused=0;return 0;
}
static int quiet_lcpu(int quiet){
    if(quiet){lp_sleep=1;return failure==1?-1:0;}
    ++lp_restores;if(failure==3)return -1;lp_sleep=0;return 0;
}
static int board527_amplifier_report(){return 0;}
static int board527_amplifier_test(int mute){
    if(mute){amp_muted=1;return failure==5?-1:0;}
    ++amp_restores;if(failure==6)return -1;amp_muted=0;return 0;
}
static void reader_power_busy(){++awake;}
static void card(Renderer *,const char *stage,const char *,const char *,int,const char *version=nullptr){stages.push_back(*stage);}
static void wait_stage(unsigned seconds){assert(seconds==120);assert(stages.size()<=4);}
'''
tests=r'''
int main(){
    Renderer r;
    for(failure=0;failure<=4;++failure){
        stages.clear();touch_sleep=touch_restores=network_paused=network_restores=awake=0;
        int rc=reader527_power_measure(&r,120);
        assert((rc==0)==(failure==0));assert(network_restores==1);
        assert(touch_restores==1);assert(awake==(failure==3));
        if(failure!=3)assert(!touch_sleep);
        if(failure!=4)assert(!network_paused);
        assert(stages[0]=='A'&&stages[1]=='B');
        if(!failure)assert(stages.size()==4&&stages[2]=='C'&&stages[3]=='D');
        else assert(stages.back()!='D');
    }
    for(failure=0;failure<=6;++failure){
        stages.clear();amp_muted=amp_restores=lp_sleep=lp_restores=network_paused=network_restores=awake=0;
        int rc=reader527_amp_measure(&r,120);
        assert((rc==0)==(failure==0));assert(network_restores==1&&lp_restores==1&&amp_restores==1);
        if(failure!=6)assert(!amp_muted);
        if(!failure)assert(stages.size()==4&&stages.back()=='D');
        else assert(stages.back()!='D');
    }
    for(failure=0;failure<=4;++failure){
        stages.clear();lp_sleep=lp_restores=network_paused=network_restores=awake=0;
        int rc=reader527_lcpu_measure(&r,120);
        assert((rc==0)==(failure==0));assert(network_restores==1&&lp_restores==1);
        if(failure!=3)assert(!lp_sleep);
        if(failure!=4)assert(!network_paused);
        if(!failure)assert(stages.size()==4&&stages.back()=='D');
        else assert(stages.back()!='D');
    }
    puts("POWER_MEASUREMENT_RESTORE_FAILURE_PATHS_PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='reader-power-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text(stubs+sequence+lp_sequence+amp_sequence+tests,encoding='utf-8')
    subprocess.run([a.cxx,'-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
