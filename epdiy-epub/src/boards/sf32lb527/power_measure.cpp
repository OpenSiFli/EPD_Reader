/* Battery-side A/B diagnosis. All temporary settings are restored in this UI
 * task before returning; a reboot also restores normal startup configuration. */
#include "reader_ui.h"
#include "EpubList/EpubReader.h"
#include "reader_power.h"
#include "UIRegionsManager.h"
extern "C" {
#include "board527.h"
#include "pan.h"
#include "bf0_hal.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"
#include "lwip/netif.h"
}
extern EpubReader *reader;
extern rt_mq_t ui_queue;
extern bool bluetooth_ui_enabled;

static struct rt_semaphore network_done;
static int network_initialized,network_quiet,network_request,network_result;
static uint32_t lp_pmr,lp_wer,lp_anacr;
static int lp_changed;

static int quiet_lcpu(int quiet)
{
    if(quiet) {
        if(lp_changed||bluetooth_ui_enabled||pan_service_is_ready()||
           !(hwp_hpsys_aon->ISSR&HPSYS_AON_ISSR_LP_ACTIVE)||
           !(hwp_lpsys_aon->PMR&LPSYS_AON_PMR_CPUWAIT)||
           (hwp_lpsys_aon->PMR&LPSYS_AON_PMR_MODE)||
           (hwp_hpsys_aon->ISSR&HPSYS_AON_ISSR_HP2LP_REQ))return -RT_EBUSY;
        lp_pmr=hwp_lpsys_aon->PMR;lp_wer=hwp_lpsys_aon->WER;lp_anacr=hwp_lpsys_aon->ANACR;
        lp_changed=1;
        HAL_LPAON_Sleep();
        rt_thread_mdelay(3);
        // LPSYS registers are not accessible after its domain sleeps. Inspect
        // the cross-domain status in HPSYS until an explicit wake completes.
        rt_kprintf("PWR_LCPU sleep hp_issr=%08x\n",hwp_hpsys_aon->ISSR);
        // The request can complete after HCPU releases its own activity lock.
        // Do not reject a transient LP_ACTIVE before the measured idle window.
        return 0;
    }
    if(!lp_changed)return 0;
    /* Wake without an unbounded spin: the measurement must report a failed
     * restore rather than hang with the UI input disabled. No LCPU code runs. */
    hwp_hpsys_aon->ISSR|=HPSYS_AON_ISSR_HP2LP_REQ;
    HAL_Delay_us(260); // SDK WakeCore allows LP clock-domain synchronization.
    uint32_t start=rt_tick_get();
    while(!(hwp_hpsys_aon->ISSR&HPSYS_AON_ISSR_LP_ACTIVE)) {
        if(rt_tick_get()-start>rt_tick_from_millisecond(100))return -RT_ETIMEOUT;
        rt_thread_mdelay(1);
    }
    hwp_lpsys_aon->PMR=lp_pmr;
    hwp_lpsys_aon->ANACR=lp_anacr;
    hwp_lpsys_aon->WER=lp_wer;
    hwp_hpsys_aon->ISSR&=~HPSYS_AON_ISSR_HP2LP_REQ;
    lp_changed=0;
    rt_kprintf("PWR_LCPU restored pmr=%08x hp_issr=%08x\n",hwp_lpsys_aon->PMR,hwp_hpsys_aon->ISSR);
    return 0;
}

static void network_change(void *)
{
    network_result=0;
    if(network_request && !network_quiet) {
        // Check in the tcpip thread, where the netif list and timers are owned.
        for(struct netif *n=netif_list;n;n=n->next) {
            if(netif_is_up(n)||netif_is_link_up(n)) {network_result=-RT_EBUSY;break;}
        }
        if(!network_result) {sys_timeouts_uninit();network_quiet=1;}
    } else if(!network_request && network_quiet) {
        sys_timeouts_init();network_quiet=0;
    }
    rt_sem_release(&network_done);
}

static int quiet_network(int quiet)
{
    if(quiet && (bluetooth_ui_enabled||pan_service_is_ready()||
       pan_service_is_bt_connected()||pan_service_is_pan_connected()))return -RT_EBUSY;
    if(!network_initialized) {
        rt_sem_init(&network_done,"pwrnet",0,RT_IPC_FLAG_FIFO);network_initialized=1;
    }
    network_request=quiet;
    if(tcpip_callback(network_change,RT_NULL)!=ERR_OK)return -RT_ERROR;
    rt_sem_take(&network_done,RT_WAITING_FOREVER);
    rt_kprintf("PWR_NETWORK quiet=%d rc=%d\n",network_quiet,network_result);
    return network_result;
}

int reader527_steps_quiet_start()
{
    int rc=quiet_lcpu(1);
    if(!rc)rc=quiet_network(1);
    return rc;
}
int reader527_steps_quiet_finish()
{
    int net=quiet_network(0),lp=quiet_lcpu(0);return net?net:lp;
}

static void card(Renderer *r,const char *stage,const char *touch,const char *network,int seconds,const char *version="POWER TEST R26")
{
    clear_areas();r->clear_screen();
    r->draw_text(24,90,version);
    r->draw_text(24,210,stage);
    r->draw_text(24,330,touch);
    r->draw_text(24,410,network);
    r->draw_text(24,490,"TF: powered, no reads during wait");
    r->draw_text(24,610,"Unplug USB and debug cable.");
    r->draw_text(24,690,"Read battery current after 10 seconds.");
    char duration[48];rt_snprintf(duration,sizeof(duration),"This stage lasts %d seconds.",seconds);
    r->draw_text(24,770,duration);
    r->draw_text(24,850,"Touch will restore automatically.");
    r->flush_display();
}

static void wait_stage(unsigned seconds)
{
    uint32_t reads=tf_msd_read_count();
    reader_power_idle();
    rt_thread_mdelay(seconds*1000u);
    reader_power_busy();
    rt_kprintf("PWR_MEASURE wait_s=%u tf_read_sectors=%u hp_issr=%08x\n",seconds,tf_msd_read_count()-reads,hwp_hpsys_aon->ISSR);
}

int reader527_power_measure(Renderer *r,int seconds)
{
    if(reader)reader->pause_background();
    r->cancel_page_preview();
    board527_power_auto(); // Release any serial diagnostic hold, keep the UI activity lock.
    board527_frontlight_set(0,0);
    int touch_changed=0,rc=0;
    card(r,"A - NORMAL BASELINE","Touch: normal","Network timers: normal",seconds);
    wait_stage(seconds);
    card(r,"B - TOUCH SLEEP","Touch: Sleep request","Network timers: normal",seconds);
    touch_changed=1; // Even a failed write/reset attempt must run final recovery.
    rc=board527_touch_test_sleep(1);
    if(!rc) {
        wait_stage(seconds);
        rc=quiet_network(1);
        if(!rc) {
            card(r,"C - TOUCH + NETWORK QUIET","Touch: Sleep request","Network timers: paused",seconds);
            wait_stage(seconds);
        }
    }
    // Restore every temporary change before reporting failure or completion.
    int net_rc=quiet_network(0);
    int touch_rc=touch_changed?board527_touch_test_sleep(0):0;
    if(!rc && !net_rc && !touch_rc) {
        card(r,"D - NORMAL RESTORED","Touch: normal","Network timers: normal",seconds);
        wait_stage(seconds);
    }
    rt_kprintf("PWR_MEASURE_DONE rc=%d net_restore=%d touch_restore=%d\n",rc,net_rc,touch_rc);
    // Discard gestures made on measurement cards so they cannot open a stale control.
    uint32_t msg;while(rt_mq_recv(ui_queue,&msg,sizeof(msg),0)==RT_EOK){}
    if(touch_rc) {
        reader_power_busy(); // Keep UART usable if touch could not be recovered.
    }
    if(rc||net_rc||touch_rc) {
        r->clear_screen();r->draw_text(20,250,"Power test failed. Reconnect COM36.");
        r->draw_text(20,350,touch_rc?"Touch reset needed.":"Touch restored.");r->flush_display();
        return -1;
    }
    return 0;
}

int reader527_lcpu_measure(Renderer *r,int seconds)
{
    if(reader)reader->pause_background();
    r->cancel_page_preview();board527_power_auto();board527_frontlight_set(0,0);
    int rc=0;
    card(r,"A - NORMAL BASELINE","Touch: normal throughout","LCPU: original / net: normal",seconds,"POWER TEST R29");
    wait_stage(seconds);
    card(r,"B - LCPU SLEEP","Touch: normal throughout","LCPU: sleeping / net: normal",seconds,"POWER TEST R29");
    rc=quiet_lcpu(1);
    if(!rc) {
        wait_stage(seconds);
        rc=quiet_network(1);
        if(!rc) {
            card(r,"C - LCPU + NETWORK QUIET","Touch: normal throughout","LCPU: sleeping / net: paused",seconds,"POWER TEST R29");
            wait_stage(seconds);
        }
    }
    int net_rc=quiet_network(0),lp_rc=quiet_lcpu(0);
    if(!rc&&!net_rc&&!lp_rc) {
        card(r,"D - NORMAL RESTORED","Touch: normal throughout","LCPU: original / net: normal",seconds,"POWER TEST R29");
        wait_stage(seconds);
    }
    rt_kprintf("PWR_LCPU_MEASURE_DONE rc=%d net_restore=%d lcpu_restore=%d\n",rc,net_rc,lp_rc);
    uint32_t msg;while(rt_mq_recv(ui_queue,&msg,sizeof(msg),0)==RT_EOK){}
    if(rc||net_rc||lp_rc) {
        r->clear_screen();r->draw_text(20,250,"LCPU test failed. Reconnect COM36.");r->flush_display();
        return -1;
    }
    return 0;
}

int reader527_amp_measure(Renderer *r,int seconds)
{
    if(reader)reader->pause_background();
    r->cancel_page_preview();board527_power_auto();board527_frontlight_set(0,0);
    int rc=0;
    board527_amplifier_report();
    card(r,"A - ORIGINAL AMP PIN","Touch: normal throughout","AMP GE7: original input mode",seconds,"AMP TEST R31");
    wait_stage(seconds);
    card(r,"B - AMPLIFIER MUTED","Touch: normal throughout","AMP GE7: output LOW",seconds,"AMP TEST R31");
    rc=board527_amplifier_test(1);
    if(!rc) {
        wait_stage(seconds);
        rc=quiet_lcpu(1);
        if(!rc)rc=quiet_network(1);
        if(!rc) {
            card(r,"C - AMP + LCPU + NET QUIET","Touch: normal throughout","AMP GE7: output LOW",seconds,"AMP TEST R31");
            wait_stage(seconds);
        }
    }
    int net_rc=quiet_network(0),lp_rc=quiet_lcpu(0),amp_rc=board527_amplifier_test(0);
    if(!rc&&!net_rc&&!lp_rc&&!amp_rc) {
        card(r,"D - ORIGINAL STATE RESTORED","Touch: normal throughout","AMP GE7: original input mode",seconds,"AMP TEST R31");
        wait_stage(seconds);
    }
    rt_kprintf("PWR_AMP_DONE rc=%d network=%d lcpu=%d amp=%d\n",rc,net_rc,lp_rc,amp_rc);
    uint32_t msg;while(rt_mq_recv(ui_queue,&msg,sizeof(msg),0)==RT_EOK){}
    if(rc||net_rc||lp_rc||amp_rc) {
        r->clear_screen();r->draw_text(20,250,"Amp test error. Reconnect COM36.");r->flush_display();
        return -1;
    }
    return 0;
}

