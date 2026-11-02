#pragma once
#include <stdint.h>
#include <string.h>

inline bool auto_turn_valid(unsigned seconds)
{
    return seconds==0||seconds==10||seconds==15||seconds==20;
}
inline unsigned auto_turn_parse(const char *text)
{
    if(!strcmp(text,"10"))return 10;
    if(!strcmp(text,"15"))return 15;
    if(!strcmp(text,"20"))return 20;
    return 0;
}
inline unsigned auto_turn_step(unsigned seconds,bool forward)
{
    const unsigned values[]={0,10,15,20};
    unsigned index=0;
    for(unsigned n=0;n<4;++n)if(values[n]==seconds)index=n;
    return values[(index+(forward?1:3))%4];
}
/* Deadline lives in the UI thread; no timer callback queues stale page turns.
 * Millisecond subtraction is wrap-safe for these short intervals. */
class AutoTurnClock {
    bool armed=false;
    unsigned interval=0;
    uint32_t deadline=0;
public:
    void reset(){armed=false;}
    unsigned wait(uint32_t now,unsigned seconds,bool eligible,unsigned normal_wait)
    {
        if(!eligible||!seconds||!auto_turn_valid(seconds)){reset();return normal_wait;}
        if(!armed||interval!=seconds){armed=true;interval=seconds;deadline=now+seconds*1000u;}
        int32_t remaining=(int32_t)(deadline-now);
        unsigned delay=remaining>0?(unsigned)remaining:1;
        return delay<normal_wait?delay:normal_wait;
    }
    bool take(uint32_t now,unsigned seconds,bool eligible)
    {
        if(!eligible||!seconds||!auto_turn_valid(seconds)){reset();return false;}
        if(!armed||interval!=seconds)return false;
        if((int32_t)(now-deadline)<0)return false;
        reset();return true;
    }
};
