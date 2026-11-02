#pragma once
#include <stdint.h>
class Renderer;
void reader527_standby_boot();
int reader527_standby_enter(Renderer *renderer,bool test);
int reader527_standby_ram(Renderer *renderer,bool test);
bool reader527_standby_ram_take_restored();
void reader527_standby_preview(Renderer *renderer);
bool reader527_standby_restore();
bool reader527_standby_render_saved(Renderer *renderer);
int reader527_standby_save();

/* One NOR-board-unused backup word, no Flash writes on minute ticks.
 * Payload: previous displayed minute, partial count, test ticks left, test bit.
 * No date is shown, so the old image is reconstructed exactly from HH:MM. */
struct ClockStandbyState {unsigned minute,partials,left;bool test;};
inline uint32_t clock_state_pack(ClockStandbyState s)
{
    uint32_t p=s.minute|(s.partials<<11)|(s.left<<15)|(uint32_t(s.test)<<19);
    unsigned check=(p^(p>>8)^(p>>16)^0x6d)&255;
    return 0xd0000000u|(check<<20)|p;
}
inline bool clock_state_unpack(uint32_t word,ClockStandbyState &s)
{
    uint32_t p=word&0xfffffu;
    if((word>>28)!=13||((word>>20)&255)!=((p^(p>>8)^(p>>16)^0x6d)&255))return false;
    s={p&2047,(p>>11)&15,(p>>15)&15,((p>>19)&1)!=0};
    return s.minute<1440&&s.partials<=10&&s.left<=11&&(s.test||!s.left);
}
inline bool clock_next_is_full(unsigned partials){return partials>=10;}
inline bool clock_should_tick(bool hibernate,bool rtc,bool key,ClockStandbyState s)
{
    return hibernate&&rtc&&!key&&(!s.test||s.left!=0);
}
