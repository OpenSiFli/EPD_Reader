/* SPDX-License-Identifier: Apache-2.0 */
#include <rtthread.h>
#include "board527.h"
#include "frontlight.h"
#include "reader_power.h"
extern int board527_power_init(void);
int board527_init(void)
{
    static int initialized;
    if(initialized)return 0;
    if(tps_init(1100)||board527_display_init())return -1;
    board527_io_init();
    int light=frontlight_start(),touch=board527_touch_init();
    int power=board527_power_init(); // Diagnostics need timers even without touch.
    if (touch || power) {
        board527_power_hold(); /* Manual timed diagnostics may release this hold. */
        rt_kprintf("PWR_DISABLED touch wake initialization failed\n");
    }
    initialized=1;
    rt_kprintf("BOARD527_READY display=1 touch=%d frontlight=%d PSRAM=8MiB NOR=16MiB\n",!touch,!light);
    return touch||light?-1:0;
}
