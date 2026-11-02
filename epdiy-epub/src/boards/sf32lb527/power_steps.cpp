/* First incremental rail ladder, with a constant retained-deep-sleep baseline.
 * Display rails are restored briefly ONLY to print the next stage card. */
#include "reader_ui.h"
#include "reader_power.h"
#include "UIRegionsManager.h"
extern "C" {
#include "board527.h"
#include "font_manager.h"
#include "board.h"
#include "bf0_pm.h"
}
extern void reader527_close_book_for_power_test();
extern int reader527_steps_quiet_start();

static uint32_t original_pads[45],original_out[2],original_enable[2];
static void save_pins()
{
    GPIO1_TypeDef *g=(GPIO1_TypeDef *)hwp_gpio1;
    original_out[0]=g->DOR0;original_out[1]=g->DOR1;
    original_enable[0]=g->DOER0;original_enable[1]=g->DOER1;
    for(unsigned n=0;n<45;++n)original_pads[n]=*(&hwp_pinmux1->PAD_PA00+n);
}
static void restore_display_pins()
{
    for(unsigned n=0;n<45;++n) {
        // Never disturb the running XIP bus or the independent AW bus/reset.
        if((n>=12&&n<=20)||n==3||n==30||n==32||n==34)continue;
        unsigned bank=n/32,bit=1u<<(n%32);
        HAL_GPIO_WritePin(hwp_gpio1,n,(original_out[bank]&bit)?GPIO_PIN_SET:GPIO_PIN_RESET);
        GPIO_InitTypeDef cfg={};cfg.Pin=n;cfg.Mode=(original_enable[bank]&bit)?GPIO_MODE_OUTPUT:GPIO_MODE_INPUT;cfg.Pull=GPIO_NOPULL;
        HAL_GPIO_Init(hwp_gpio1,&cfg);
        *(&hwp_pinmux1->PAD_PA00+n)=original_pads[n];
    }
}
static int display_power_on()
{
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,true,true);
    // I2C2 and AW reset were kept usable in all previous stages.
    int rc=board527_steps_aw(0,1);
    if(rc)return rc;
    rt_thread_mdelay(80);restore_display_pins();return 0;
}
static void stage_card(Renderer *r,unsigned stage,unsigned seconds)
{
    static const char *names[]={"A - MINIMUM RETAINED BASELINE","B - ADD AW9523B ACTIVE","C - ADD LDO3 SUPPLY","D - ADD GE14 SHARED 3V3"};
    clear_areas();r->clear_screen();
    r->draw_text(20,65,"POWER STEPS R35 / ROUND 1");
    r->draw_text(20,175,names[stage]);
    r->draw_text(20,285,"CPU: retained Deep Sleep in ALL stages");
    r->draw_text(20,355,stage?"AW: active, peripheral enables OFF":"AW: held RESET (outputs high-Z)");
    r->draw_text(20,425,stage>=2?"LDO3: ON":"LDO3: OFF");
    r->draw_text(20,495,stage>=3?"GE14: ON (TF + panel logic + pullups)":"GE14: OFF");
    r->draw_text(20,565,"No TF initialization/file reads.");
    r->draw_text(20,635,"No touch / light / HV / network timers.");
    r->draw_text(20,730,"Unplug USB + COM36. Wait 10 sec.");
    char text[70];rt_snprintf(text,sizeof(text),"Measure %u sec; auto next stage.",seconds);
    r->draw_text(20,800,text);
    r->draw_text(20,870,"After D: reboot to normal reader.");
    r->draw_text(20,950,"LDO2 / NOR / PSRAM remain supplied.");
    r->flush_display();
}
static void output(unsigned n,unsigned high)
{
    HAL_GPIO_WritePin(hwp_gpio1,n,high?GPIO_PIN_SET:GPIO_PIN_RESET);
    HAL_PIN_Set(PAD_PA00+n,(pin_function)(GPIO_A0+n),PIN_NOPULL,1);
    GPIO_InitTypeDef cfg={};cfg.Pin=n;cfg.Mode=GPIO_MODE_OUTPUT;cfg.Pull=GPIO_NOPULL;
    HAL_GPIO_Init(hwp_gpio1,&cfg);
}
static int apply_stage(unsigned stage)
{
    if(stage>3)return -RT_EINVAL;
    board527_minimum_park_buses(); // Shared rail was on for the stage card.
    int rc=board527_minimum_external_off();
    if(rc)return rc;
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,stage>=2,true);
    if(stage==0)rc=board527_steps_aw(1,0);
    else rc=board527_steps_aw(0,stage>=3);
    if(!rc&&stage>=3) {
        rt_thread_mdelay(80);
        // Powered panel inputs cannot float. No HV, no scan clocks. SPI CS high.
        const uint8_t panel[]={0,1,2,4,5,6,7,8,9,37,39,40,41,42,43};
        for(unsigned i=0;i<sizeof(panel);++i)output(panel[i],0);
        output(24,0);output(28,0);output(29,1);
    }
    return rc;
}
int reader527_power_steps(Renderer *r,unsigned seconds)
{
    int rc=0;
    rt_kprintf("POWER_STEPS_PREPARE seconds=%u\n",seconds);
    reader527_close_book_for_power_test();r->cancel_page_preview();board527_power_auto();
    if(font_manager_select(0,28)){rc=-RT_ERROR;goto steps_finish;}
    if(board527_frontlight_set(0,0)||board527_amplifier_test(1)){rc=-RT_ERROR;goto steps_finish;}
    board527_touch_power(0);
    if(tps_enter_sleep()!=RT_EOK){rc=-RT_ERROR;goto steps_finish;}
    if(board527_storage_test_begin()){rc=-RT_EBUSY;goto steps_finish;}
    save_pins();
    // AON touch wake disabled while its controller/pullups are intentionally off.
    pm_disable_pin_wakeup(20);
    rc=reader527_steps_quiet_start();
    if(!rc)board527_steps_uart_idle(1);
    for(unsigned stage=0;!rc&&stage<4;++stage) {
        rc=display_power_on();if(rc)break;
        stage_card(r,stage,seconds);
        rc=apply_stage(stage);if(rc)break;
        uint32_t before_count,before_ms,after_count,after_ms,reads=tf_msd_read_count();
        board527_power_stats(&before_count,&before_ms);
        rt_kprintf("POWER_STEP_BEGIN stage=%c seconds=%u reset=%d PERI_LDO=%08x\n",'A'+stage,seconds,rt_pin_read(3),hwp_pmuc->PERI_LDO);
        rt_thread_mdelay(20); // Drain UART before deep-sleep hook makes it high-Z.
        reader_power_idle();rt_thread_mdelay(seconds*1000u);reader_power_busy();
        board527_power_stats(&after_count,&after_ms);
        rt_kprintf("POWER_STEP_END stage=%c sleeps=%u sleep_ms=%u tf_sectors=%u\n",'A'+stage,after_count-before_count,after_ms-before_ms,tf_msd_read_count()-reads);
    }
steps_finish:
    board527_steps_uart_idle(0);
    rt_kprintf("POWER_STEPS_DONE rc=%d; rebooting to reinitialize card and peripherals\n",rc);
    rt_thread_mdelay(50);drv_reboot();return rc;
}

/* Touch state comparison: supplies and parked buses are identical in A-D. */
static void touch_card(Renderer *r,unsigned stage,unsigned seconds)
{
    static const char *names[]={"A - TOUCH HELD IN RESET","B - TOUCH AUTOMATIC IDLE","C - TOUCH SLEEP REQUEST","D - TOUCH HELD IN RESET AGAIN"};
    clear_areas();r->clear_screen();
    r->draw_text(20,65,"TOUCH POWER R36");
    r->draw_text(20,175,names[stage]);
    r->draw_text(20,285,"LDO3 ON / GE14 ON / PA32 HIGH");
    r->draw_text(20,355,"Same supplies and bus pins in A-D.");
    r->draw_text(20,425,"Touch IRQ / wake disabled in ALL stages.");
    r->draw_text(20,495,"TF absent. No file reads.");
    r->draw_text(20,565,"Frontlight / HV / amplifier OFF.");
    r->draw_text(20,635,"CPU Deep Sleep; network timers paused.");
    r->draw_text(20,730,"Unplug USB + COM36. Wait 10 sec.");
    char text[70];rt_snprintf(text,sizeof(text),"Measure %u sec; auto next stage.",seconds);
    r->draw_text(20,800,text);
    r->draw_text(20,870,"After D: reboot restores touch.");
    r->draw_text(20,950,"Please do not touch during measurement.");
    r->flush_display();
}
int reader527_touch_states(Renderer *r,unsigned seconds)
{
    int rc=0;
    rt_kprintf("TOUCH_STATES_PREPARE seconds=%u\n",seconds);
    reader527_close_book_for_power_test();r->cancel_page_preview();board527_power_auto();
    if(board_tf_present()!=0){rc=-RT_EBUSY;goto touch_finish;}
    if(font_manager_select(0,28)){rc=-RT_ERROR;goto touch_finish;}
    if(board527_frontlight_set(0,0)||board527_amplifier_test(1)){rc=-RT_ERROR;goto touch_finish;}
    board527_touch_power(0);
    if(tps_enter_sleep()!=RT_EOK){rc=-RT_ERROR;goto touch_finish;}
    if(board527_storage_test_begin()){rc=-RT_EBUSY;goto touch_finish;}
    save_pins();pm_disable_pin_wakeup(20);
    rc=reader527_steps_quiet_start();
    if(!rc)board527_steps_uart_idle(1);
    for(unsigned stage=0;!rc&&stage<4;++stage) {
        rc=display_power_on();if(rc)break;
        touch_card(r,stage,seconds);
        rc=apply_stage(3);if(rc)break;
        // Restore only I2C1 for the mode command; identical pins for all stages.
        HAL_PIN_Set(PAD_PA10,I2C1_SCL,PIN_NOPULL,1);
        HAL_PIN_Set(PAD_PA11,I2C1_SDA,PIN_NOPULL,1);
        unsigned mode=stage==3?0:stage;
        rc=board527_touch_quiet_mode(mode);if(rc)break;
        uint32_t before_count,before_ms,after_count,after_ms,reads=tf_msd_read_count();
        board527_power_stats(&before_count,&before_ms);
        rt_kprintf("TOUCH_STATE_BEGIN stage=%c mode=%u seconds=%u PERI_LDO=%08x\n",'A'+stage,mode,seconds,hwp_pmuc->PERI_LDO);
        rt_thread_mdelay(20);
        reader_power_idle();rt_thread_mdelay(seconds*1000u);reader_power_busy();
        board527_power_stats(&after_count,&after_ms);
        rt_kprintf("TOUCH_STATE_END stage=%c sleeps=%u sleep_ms=%u tf_sectors=%u\n",'A'+stage,after_count-before_count,after_ms-before_ms,tf_msd_read_count()-reads);
    }
touch_finish:
    board527_steps_uart_idle(0);
    rt_kprintf("TOUCH_STATES_DONE rc=%d; rebooting\n",rc);
    rt_thread_mdelay(50);drv_reboot();return rc;
}

