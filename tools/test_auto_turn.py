"""Check auto-turn deadline, persistence failure, UI entry and end-of-book gates."""
from pathlib import Path
import argparse,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cxx',default='g++');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
header=(root/'epdiy-epub/src/auto_turn.h').read_text(encoding='utf8').replace('#pragma once','')
settings=(root/'epdiy-epub/src/reading_settings.cpp').read_text(encoding='utf8')
setter=settings[settings.index('static unsigned g_auto_turn_seconds'):settings.index('/*==========================================================================',settings.index('static unsigned g_auto_turn_seconds'))]
reader=(root/'epdiy-epub/lib/Epub/EpubList/EpubReader.cpp').read_text(encoding='utf8')
gate=reader[reader.index('bool EpubReader::can_auto_advance'):reader.index('/*==========================================================================',reader.index('bool EpubReader::can_auto_advance'))]
stubs=r'''
#include <cassert>
#include <cstdio>
#define rt_kprintf(...) ((void)0)
static int save_calls;static bool save_ok=true;
static bool reading_settings_save(){++save_calls;return save_ok;}
struct Epub {int count=3;int get_spine_items_count(){return count;}};
struct Parser {bool done=true;int pages=4;bool is_layout_done(){return done;}int get_page_count(){return pages;}};
class EpubReader {
public:
    Epub *epub=nullptr;Parser *parser=nullptr;bool overlay_active=false;
    struct {int current_section=0,current_page=0;} state;
    bool can_auto_advance() const;
};
'''
tests=r'''
int main(){
    assert(!reading_auto_turn_seconds());
    assert(auto_turn_step(0,true)==10&&auto_turn_step(10,true)==15&&auto_turn_step(15,true)==20&&auto_turn_step(20,true)==0);
    assert(auto_turn_step(0,false)==20&&auto_turn_step(20,false)==15&&auto_turn_step(15,false)==10&&auto_turn_step(10,false)==0);
    for(unsigned value: {0u,10u,15u,20u})assert(auto_turn_valid(value));
    for(const char *bad: {"","-1","5","4294967306","10abc","10 ","30","9999999999999999999999"})assert(!auto_turn_parse(bad));
    assert(auto_turn_parse("0")==0&&auto_turn_parse("10")==10&&auto_turn_parse("15")==15&&auto_turn_parse("20")==20);
    assert(reading_auto_turn_set(10)&&save_calls==1&&reading_auto_turn_seconds()==10);
    assert(reading_auto_turn_set(10)&&save_calls==1);
    assert(!reading_auto_turn_set(12)&&save_calls==1);
    save_ok=false;assert(!reading_auto_turn_set(20)&&reading_auto_turn_seconds()==10);save_ok=true;
    AutoTurnClock clock;
    for(unsigned sec: {10u,15u,20u}) {
        clock.reset();assert(clock.wait(0,sec,true,5000)==5000);
        assert(!clock.take(sec*1000-1,sec,true));
        assert(clock.wait(sec*1000-200,sec,true,5000)==200);
        assert(clock.take(sec*1000,sec,true));assert(!clock.take(sec*1000,sec,true));
        // New interval starts after rendering ends, not from the old deadline.
        assert(clock.wait(sec*1000+2000,sec,true,5000)==5000);
        assert(!clock.take(sec*2000,sec,true));assert(clock.take(sec*2000+2000,sec,true));
    }
    clock.reset();clock.wait(0,10,true,5000);
    assert(clock.wait(8000,10,false,5000)==5000); // menu/nonreading/touch
    clock.wait(40000,10,true,5000);assert(!clock.take(40000,10,true));
    assert(clock.take(50000,10,true));
    clock.wait(0,10,true,5000);clock.reset(); // manual interaction
    clock.wait(9000,10,true,5000);assert(!clock.take(10000,10,true));
    assert(clock.take(19000,10,true));
    clock.wait(0,10,true,5000);assert(clock.wait(9000,20,true,5000)==5000);
    assert(!clock.take(10000,20,true));assert(clock.take(29000,20,true));
    clock.wait(0,10,true,5000);assert(!clock.take(50000,10,false));
    clock.wait(0,10,true,5000);assert(!clock.take(50000,0,true));
    clock.wait(0,10,true,5000);assert(clock.take(90000,10,true));assert(!clock.take(90000,10,true)); // no catchup burst
    clock.wait(0xfffffff0u,10,true,5000);assert(!clock.take(9983,10,true));assert(clock.take(9984,10,true));
    Epub epub;Parser parser;EpubReader r;
    assert(!r.can_auto_advance());r.epub=&epub;r.parser=&parser;assert(r.can_auto_advance());
    r.state.current_section=2;r.state.current_page=3;assert(!r.can_auto_advance());
    parser.done=false;assert(r.can_auto_advance());
    r.overlay_active=true;assert(!r.can_auto_advance());r.overlay_active=false;
    r.state.current_section=3;assert(!r.can_auto_advance());
    puts("AUTO_TURN_INTERVAL_PAUSE_RESET_WRAP_PERSISTENCE_END_GATE_PASS");
}
'''
# Check current source retains queue-first scheduling and bounds the hit areas.
main=(root/'epdiy-epub/src/main.cpp').read_text(encoding='utf8')
assert '!(ui_queue&&ui_queue->entry)' in main
assert main.index('rt_mq_recv(ui_queue, &msg_data')<main.index('auto_turn_clock.take(')
assert 'auto_turn_clock.reset(); // Restart after this action' in main
assert 'g_auto_turn_seconds=0; // Old/missing/invalid' in settings
with tempfile.TemporaryDirectory(prefix='auto-turn-') as d:
    d=Path(d);cpp=d/'test.cpp';exe=d/'test.exe'
    cpp.write_text('#include <initializer_list>\n'+header+stubs+setter+gate+tests,encoding='utf8')
    subprocess.run([a.cxx,'-std=c++11','-O2',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
