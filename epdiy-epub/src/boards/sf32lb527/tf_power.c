/* GE14 feeds TF, panel logic and I2C1 pullups. Serialize their hardware users
 * before parking these buses; keep LDO3, touch RESET and IRQ/wake unchanged.
 * Hardware lock order: shared -> SPI/frontlight/touch -> AW/I2C.
 * Filesystem/command locks are above shared; display must never acquire them.
 * Never call from an ISR or a PM hook. Filesystem/RAM state stays retained. */
#include "board527.h"
#include "board.h"
#include "reader_power.h"
#include "spi_msd.h"
#include <finsh.h>
#include <string.h>

static struct rt_mutex shared_lock;
static int initialized,automatic,rail_off;
static rt_tick_t last_use;
static unsigned sleeps,wakes,errors;
static const uint8_t pins[]={0,1,2,4,5,6,7,8,9,10,11,24,25,28,29,37,39,40,41,42,43};
static uint32_t pads[sizeof(pins)],out[2],enable[2];

static void park(void)
{
    GPIO1_TypeDef *g=(GPIO1_TypeDef *)hwp_gpio1;
    out[0]=g->DOR0;out[1]=g->DOR1;enable[0]=g->DOER0;enable[1]=g->DOER1;
    for(unsigned i=0;i<sizeof(pins);++i) {
        unsigned n=pins[i];pads[i]=*(&hwp_pinmux1->PAD_PA00+n);
        GPIO_InitTypeDef cfg={.Pin=n,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_NOPULL};
        HAL_GPIO_Init(hwp_gpio1,&cfg);HAL_PIN_Set_Analog(PAD_PA00+n,1);
    }
}
static void restore(void)
{
    for(unsigned i=0;i<sizeof(pins);++i) {
        unsigned n=pins[i],bank=n/32,bit=1u<<(n%32);
        HAL_GPIO_WritePin(hwp_gpio1,n,(out[bank]&bit)?GPIO_PIN_SET:GPIO_PIN_RESET);
        GPIO_InitTypeDef cfg={.Pin=n,.Mode=(enable[bank]&bit)?GPIO_MODE_OUTPUT:GPIO_MODE_INPUT,.Pull=GPIO_NOPULL};
        HAL_GPIO_Init(hwp_gpio1,&cfg);*(&hwp_pinmux1->PAD_PA00+n)=pads[i];
    }
}
static int wake_locked(void)
{
    if(!rail_off)return 0;
    if(board527_logic_power(1)){++errors;return -RT_EIO;}
    rt_thread_mdelay(10);restore();rail_off=0;++wakes;
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("TF_PM_WAKE rail=1 card_reinit=deferred count=%u\n",wakes);
#endif
    return 0;
}
void board527_io_init(void)
{
    if(initialized)return;
    rt_mutex_init(&shared_lock,"io527",RT_IPC_FLAG_PRIO);
    last_use=rt_tick_get();initialized=1;
}
int board527_io_enter(void)
{
    if(!initialized)return 0;
    rt_mutex_take(&shared_lock,RT_WAITING_FOREVER);reader_power_busy();
    int rc=wake_locked();
    if(rc){reader_power_idle();rt_mutex_release(&shared_lock);}
    return rc;
}
void board527_io_exit(void)
{
    if(!initialized)return;
    last_use=rt_tick_get();reader_power_idle();rt_mutex_release(&shared_lock);
}
int board527_io_diagnostic_begin(void)
{
    int rc=board527_io_enter();
    if(!rc&&initialized)reader_power_idle(); // Keep mutex, not an awake vote.
    return rc;
}
void board527_io_diagnostic_end(void)
{
    if(initialized)reader_power_busy();
    board527_io_exit();
}
void board527_storage_pm_enable(int enabled)
{
    if(!initialized)return;
    if(board527_io_enter())return;
    automatic=enabled!=0;board527_io_exit();
}
int board527_storage_idle(int force)
{
    if(!initialized)return -RT_ENOSYS;
    if(rt_mutex_take(&shared_lock,0)!=RT_EOK)return -RT_EBUSY;
    int rc=0;
    if(!automatic||rail_off||
       (!force&&(rt_tick_t)(rt_tick_get()-last_use)<rt_tick_from_millisecond(500)))goto done;
    if(board527_touch_active()||!board527_logic_sleep_ready()){rc=-RT_EBUSY;goto done;}
    reader_power_busy();
    // Wait for card programming to finish and check status before removing VDD.
    rc=tf_msd_prepare_off();
    if(!rc) {
        park();
        // Mark card context lost even if gate verification later fails.
        tf_msd_power_lost();
        rc=board527_logic_power(0);
        rail_off=1;
        if(!rc){
            ++sleeps;
#if BOARD527_VERBOSE_LOGS
            rt_kprintf("TF_PM_SLEEP rail=0 count=%u mounted_state=retained idle_ms=%u tick_ms=%u\n",
                sleeps,(unsigned)((uint64_t)(rt_tick_t)(rt_tick_get()-last_use)*1000/RT_TICK_PER_SECOND),rt_tick_get_millisecond());
#endif
        }
        else {automatic=0;++errors;(void)wake_locked();}
    } else {automatic=0;++errors;}
    if(rc)rt_kprintf("TF_PM_SUSPEND_FAIL rc=%d auto=0\n",rc);
    reader_power_idle();
done:
    rt_mutex_release(&shared_lock);return rc;
}
/* Called after attempting idle, only when layout/preheat/preview are finished.
 * Let the existing UI queue timeout reach the idle deadline instead of waiting
 * another five seconds. Once off/disabled, keep the normal long sleep. */
unsigned board527_storage_wait_ms(unsigned normal_wait_ms)
{
    if(!initialized)return normal_wait_ms;
    unsigned delay=50;
    if(rt_mutex_take(&shared_lock,0)!=RT_EOK)return delay<normal_wait_ms?delay:normal_wait_ms;
    if(!automatic||rail_off)delay=normal_wait_ms;
    else {
        rt_tick_t elapsed=rt_tick_get()-last_use,threshold=rt_tick_from_millisecond(500);
        if(elapsed<threshold)
            delay=(unsigned)(((uint64_t)(threshold-elapsed)*1000+RT_TICK_PER_SECOND-1)/RT_TICK_PER_SECOND);
        // An expired deadline with active hardware/gesture retries boundedly.
    }
    rt_mutex_release(&shared_lock);
    return delay<normal_wait_ms?delay:normal_wait_ms;
}
static int tfpm(int argc,char **argv)
{
    int rc=0;
    if(argc==2&&!strcmp(argv[1],"on"))board527_storage_pm_enable(1);
    else if(argc==2&&!strcmp(argv[1],"off"))board527_storage_pm_enable(0);
    else if(argc==2&&!strcmp(argv[1],"sleep"))rc=board527_storage_idle(1);
    else if(argc!=2||strcmp(argv[1],"status"))return -RT_EINVAL;
    rt_kprintf("TF_PM_STATUS auto=%d rail_off=%d sleeps=%u wakes=%u errors=%u rc=%d\n",automatic,rail_off,sleeps,wakes,errors,rc);
    return rc;
}
MSH_CMD_EXPORT(tfpm, TF shared rail power: status/on/off/sleep);
