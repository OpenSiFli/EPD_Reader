#include "standby_clock.h"
#include "SF32Paper527Renderer.h"
#include "UIRegionsManager.h"
#include "reading_settings.h"
#include "reader_power.h"
#include "EpubList/EpubReader.h"
extern "C" {
#include "board527.h"
#include "font_manager.h"
#include "font_ft.h"
#include "board.h"
#include "bf0_pm.h"
#include "spi_msd.h"
extern RTC_HandleTypeDef RTC_Handler;
extern const EpdFont regular_font;
}
extern void reader527_close_book_for_power_test();
static bool resume_requested;
static volatile bool key_seen;
static struct rt_semaphore ram_event;
static bool ram_event_initialized;
static volatile bool ram_active,rtc_seen;
static volatile uint32_t ram_wake_tick;
static void clock_key_irq(void *)
{
    key_seen=true;
    if(ram_active){ram_wake_tick=rt_tick_get_millisecond();rt_sem_release(&ram_event);}
}
static void watch_key()
{
    HAL_PIN_Set(PAD_PA34,GPIO_A34,PIN_PULLDOWN,1);rt_pin_mode(34,PIN_MODE_INPUT);
    key_seen=rt_pin_read(34)!=0;
    rt_pin_attach_irq(34,PIN_IRQ_MODE_RISING,clock_key_irq,nullptr);
    rt_pin_irq_enable(34,PIN_IRQ_ENABLE);
}
bool reader527_standby_resume_requested(){return resume_requested;}

#if defined(SOLUTION_RES_USING_NAND)
#error Clock standby reserves the unused NOR-board NAND backup word
#endif
static RTC_HandleTypeDef rtc_handle()
{
    RTC_HandleTypeDef rtc={};rtc.Instance=hwp_rtc;return rtc;
}
static void stop_rtc_wake()
{
    RTC_HandleTypeDef rtc=rtc_handle();HAL_RTC_DeactivateWakeUpTimer(&rtc);
    HAL_PMU_DisableRtcWakeup();hwp_rtc->CR&=~(RTC_CR_ALRME|RTC_CR_ALRMIE);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc,RTC_FLAG_WUTF);
}
static void leave_clock()
{
    stop_rtc_wake();HAL_Set_backup(RTC_BACKUP_NAND_OTA_DES,0);
}
static int abort_clock(const char *why)
{
    rt_kprintf("CLOCK_ABORT %s; normal reboot\n",why);leave_clock();
    rt_thread_mdelay(30);drv_reboot();return -1;
}
static bool read_clock(unsigned &minute,unsigned &second)
{
    RTC_HandleTypeDef rtc=rtc_handle();RTC_TimeTypeDef t={};RTC_DateTypeDef d={};
    for(unsigned n=0;n<4;++n) {
        if(HAL_RTC_GetTime(&rtc,&t,RTC_FORMAT_BIN)==HAL_OK&&HAL_RTC_GetDate(&rtc,&d,RTC_FORMAT_BIN)==HAL_OK) {
            minute=t.Hours*60+t.Minutes;second=t.Seconds;return minute<1440&&second<60;
        }
    }
    return false;
}
static void center(Renderer *r,int y,int size,const char *text)
{
    epd_font_ft_set_size(size);int x=(684-r->get_text_width(text))/2;
    r->draw_text(x,y,text);
}
static void draw_clock_time(Renderer *r,unsigned minute)
{
    r->fill_rect(0,420,684,280,255);
    char text[8];rt_snprintf(text,sizeof(text),"%02u:%02u",minute/60,minute%60);
    center(r,430,178,text);
    uint8_t *fb=board527_framebuffer();
    for(unsigned n=0;n<1216*684/2;++n)fb[n]=((fb[n]&15)>=8?15:0)|((fb[n]>>4)>=8?240:0);
}
static void draw_clock(Renderer *r,unsigned minute)
{
    r->set_margin_top(0);r->set_margin_bottom(0);r->set_margin_left(0);r->set_margin_right(0);
    r->clear_screen();epd_font_ft_set_bold(0);epd_font_ft_set_line_spacing(1.0f);
    center(r,90,28,"S I F L I   R E A D E R");
    r->fill_rect(294,182,96,3,0);
    center(r,242,40,"休眠待机");
    r->fill_rect(62,706,560,2,0);
    center(r,754,28,"让时间慢下来");
    center(r,1022,32,"轻按电源键唤醒");
    center(r,1084,22,"触摸与前光已关闭");
    // Binary rendering ensures only changed time pixels need the DU waveform.
    // Draw the clock last so its cached glyphs survive the old/new transition.
    draw_clock_time(r,minute);
}
static int hibernate_clock(ClockStandbyState state)
{
    unsigned minute,second;
    if(!read_clock(minute,second))return abort_clock("rtc_read");
    unsigned seconds=state.test?5:(minute!=state.minute?1:60-second);
    stop_rtc_wake();
    hwp_pmuc->WER=0;
    HAL_PIN_Set(PAD_PA34,GPIO_A34,PIN_PULLDOWN,1);rt_pin_mode(34,PIN_MODE_INPUT);
    if(key_seen||rt_pin_read(34)) {
        // Keep the valid marker through the reboot so normal boot restores the
        // saved reading anchor even if the key was pressed during refresh.
        HAL_Set_backup(RTC_BACKUP_NAND_OTA_DES,clock_state_pack(state));
        rt_kprintf("CLOCK_EXIT key_before_sleep\n");drv_reboot();return -1;
    }
    HAL_PMU_SelectWakeupPin(0,10);HAL_PMU_EnablePinWakeup(0,AON_PIN_MODE_HIGH);
    for(unsigned n=0;!board527_logic_sleep_ready();++n) {
        if(n>=60)return abort_clock("discharge_guard");rt_thread_mdelay(10);
    }
    board527_minimum_park_buses();
    if(board527_minimum_external_off())return abort_clock("external_shutdown");
    RTC_HandleTypeDef rtc=rtc_handle();
    if(HAL_RTC_SetWakeUpTimer(&rtc,seconds-1,RTC_WAKEUP_SEC)!=HAL_OK)return abort_clock("rtc_arm");
    HAL_PMU_EnableRtcWakeup();
    HAL_Set_backup(RTC_BACKUP_NAND_OTA_DES,clock_state_pack(state));
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("CLOCK_SLEEP minute=%u partials=%u test=%u left=%u wake_s=%u WER=%08x\n",
        state.minute,state.partials,state.test,state.left,seconds,hwp_pmuc->WER);
#endif
    rt_thread_mdelay(20);board527_minimum_hibernate();return -1;
}
void reader527_standby_boot()
{
    ClockStandbyState state={};
    if(!clock_state_unpack(HAL_Get_backup(RTC_BACKUP_NAND_OTA_DES),state))return;
    unsigned reason=pm_get_pwron_wakeup_src();
    if(!clock_should_tick(SystemPowerOnModeGet()==PM_HIBERNATE_BOOT,reason&PMUC_WSR_RTC,reason&PMUC_WSR_PIN0,state)) {
        resume_requested=true;leave_clock();rt_kprintf("CLOCK_EXIT reason=%08x mode=%u\n",reason,SystemPowerOnModeGet());return;
    }
    stop_rtc_wake();
    watch_key();
    unsigned minute,second;if(!read_clock(minute,second)){abort_clock("rtc_boot_read");return;}
    if(tps_init(1100)||board527_display_init()||epd_font_ft_init_builtin(28)) {abort_clock("display_boot");return;}
    // Skip normal board init: no TF, touch, frontlight workers or UI on RTC ticks.
    SF32PaperRenderer r(&regular_font,&regular_font,&regular_font,&regular_font,nullptr,0,0);
    draw_clock(&r,state.minute);board527_clock_seed(board527_framebuffer());
    if(state.test)minute=(state.minute+1)%1440;
    bool full=clock_next_is_full(state.partials);
    draw_clock_time(&r,minute); // Static labels are already reconstructed.
    if(board527_clock_present(board527_framebuffer(),full)){abort_clock("refresh");return;}
    state.minute=minute;state.partials=full?0:state.partials+1;
    if(state.test)--state.left;
#if BOARD527_VERBOSE_LOGS
    rt_kprintf("CLOCK_REFRESH kind=%s minute=%u partials=%u\n",full?"FULL":"PARTIAL",minute,state.partials);
#endif
    hibernate_clock(state);
}
int reader527_standby_enter(Renderer *r,bool test)
{
    ClockStandbyState unused={};uint32_t backup=HAL_Get_backup(RTC_BACKUP_NAND_OTA_DES);
    if(backup&&!clock_state_unpack(backup,unused))return -RT_EBUSY;
    epd_font_ft_preheat_stop();
    if(reader527_standby_save())return -RT_EIO;
    watch_key();
    reader527_close_book_for_power_test();r->cancel_page_preview();board527_storage_pm_enable(0);
    if(font_manager_select(0,28))return abort_clock("builtin_font");
    if(board527_frontlight_set(0,0)||board527_amplifier_test(1))return abort_clock("peripheral_shutdown");
    board527_touch_power(0);
    unsigned minute,second;if(!read_clock(minute,second))return abort_clock("rtc_entry_read");
    clear_areas();draw_clock(r,minute);
    if(board527_clock_present(board527_framebuffer(),1))return abort_clock("entry_refresh");
    if(board527_storage_test_begin())return abort_clock("open_files");
    rt_kprintf("CLOCK_REFRESH kind=ENTRY_FULL minute=%u\n",minute);
    return hibernate_clock({minute,0,test?11u:0u,test});
}
void reader527_standby_preview(Renderer *r)
{
    reader527_close_book_for_power_test();r->cancel_page_preview();
    unsigned minute,second;
    if(!read_clock(minute,second)||font_manager_select(0,28))return;
    draw_clock(r,minute);int rc=board527_clock_present(board527_framebuffer(),1);
    reading_settings_load(r);
    rt_kprintf("CLOCK_PREVIEW minute=%u rc=%d\n",minute,rc);
}

static RTC_cb saved_rtc_callback;
static void ram_rtc_callback(int reason)
{
    if(ram_active&&reason==RTC_CBK_WAKEUP) {
        rtc_seen=true;ram_wake_tick=rt_tick_get_millisecond();rt_sem_release(&ram_event);
    } else if(saved_rtc_callback)saved_rtc_callback(reason);
}
static uint32_t ram_pads[45],ram_out[2],ram_enable[2];
static void ram_save_pins()
{
    GPIO1_TypeDef *g=(GPIO1_TypeDef *)hwp_gpio1;
    ram_out[0]=g->DOR0;ram_out[1]=g->DOR1;ram_enable[0]=g->DOER0;ram_enable[1]=g->DOER1;
    for(unsigned n=0;n<45;++n)ram_pads[n]=*(&hwp_pinmux1->PAD_PA00+n);
}
static int ram_display_power()
{
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,true,true);
    int rc=board527_steps_aw(0,1);if(rc)return rc;
    rt_thread_mdelay(10);
    for(unsigned n=0;n<45;++n) {
        if(n==3||(n>=12&&n<=20)||n==30||n==32||n==34)continue;
        unsigned bank=n/32,bit=1u<<(n%32);
        HAL_GPIO_WritePin(hwp_gpio1,n,(ram_out[bank]&bit)?GPIO_PIN_SET:GPIO_PIN_RESET);
        GPIO_InitTypeDef cfg={};cfg.Pin=n;cfg.Mode=(ram_enable[bank]&bit)?GPIO_MODE_OUTPUT:GPIO_MODE_INPUT;cfg.Pull=GPIO_NOPULL;
        HAL_GPIO_Init(hwp_gpio1,&cfg);*(&hwp_pinmux1->PAD_PA00+n)=ram_pads[n];
    }
    return 0;
}
static int ram_external_off()
{
    for(unsigned n=0;!board527_logic_sleep_ready();++n) {
        if(n>=60)return -RT_EBUSY;rt_thread_mdelay(10);
    }
    int present=board_tf_present();
    if(present<0)return -RT_EIO;
    if(present&&tf_msd_prepare_off())return -RT_EIO;
    tf_msd_power_lost();board527_minimum_park_buses();
    if(board527_minimum_external_off())return -RT_EIO;
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,false,true);
    return board527_steps_aw(1,0);
}
extern EpubReader *reader;
extern rt_mq_t ui_queue;
extern int reader527_steps_quiet_start();
extern int reader527_steps_quiet_finish();
static bool ram_restored;

int reader527_standby_ram(Renderer *r,bool test)
{
    ram_restored=false;
    // Same visible clock/cadence as Hibernate; memory is deliberately retained.
    uint8_t *saved_frame=(uint8_t *)epub_mem_malloc(1216*684/2);
    if(!saved_frame)return -RT_ENOMEM;
    int rc=0,exclusive=0,quiet=0,pads_saved=0;
    int top=r->get_margin_top(),left=r->get_margin_left();
    int bottom=1216-top-r->get_page_height(),right=684-left-r->get_page_width();
    AreaRect areas[MAX_AREAS];memcpy(areas,g_area_array,sizeof(areas));int area_count=g_area_count;
    unsigned cold=0,warm=0,minute=0,second=0,shown_minute=0,partials=0,updates=0;
    uint32_t wer=hwp_hpsys_aon->WER;
    if(reader)reader->pause_background();r->cancel_page_preview();epd_font_ft_preheat_stop();
    memcpy(saved_frame,board527_framebuffer(),1216*684/2);
    board527_frontlight_get(&cold,&warm);board527_storage_pm_enable(0);board527_power_auto();
    if(font_manager_select(0,28)||board527_frontlight_set(0,0)||board527_amplifier_test(1)) {rc=-RT_ERROR;goto ram_done;}
    board527_touch_power(0);watch_key();
    if(!read_clock(minute,second)){rc=-RT_ERROR;goto ram_done;}
    clear_areas();draw_clock(r,minute);
    shown_minute=minute;
    if(board527_clock_present(board527_framebuffer(),1)){rc=-RT_ERROR;goto ram_done;}
    if(board527_io_diagnostic_begin()){rc=-RT_EIO;goto ram_done;}exclusive=1;
    ram_save_pins();pads_saved=1;
    pm_disable_pin_wakeup(20);
    if(pm_enable_pin_wakeup(10,AON_PIN_MODE_POS_EDGE)||pm_enable_rtc_wakeup()){rc=-RT_ERROR;goto ram_done;}
    // The stock PM enables GPIO1 and LPTIM1 wake sources. This retained
    // clock session accepts only its RTC minute event and PA34/AON PIN10.
    hwp_hpsys_aon->WER=HPSYS_AON_WER_RTC|HPSYS_AON_WER_PIN10;
    quiet=1;rc=reader527_steps_quiet_start();if(rc)goto ram_done;
    if(!ram_event_initialized){rt_sem_init(&ram_event,"clkram",0,RT_IPC_FLAG_FIFO);ram_event_initialized=true;}
    saved_rtc_callback=RTC_Handler.callback;HAL_RTC_RegCallback(&RTC_Handler,ram_rtc_callback);ram_active=true;
    board527_steps_uart_idle(1);
    rt_kprintf("CLOCK_RAM_START test=%u SRAM_PSRAM_retained=1 WER=%08x\n",test,hwp_hpsys_aon->WER);
    while(!rc&&!key_seen&&(!test||updates<11)) {
        if(!read_clock(minute,second)){rc=-RT_EIO;break;}
        unsigned seconds=test?5:60-second;
        rtc_seen=false;rt_sem_control(&ram_event,RT_IPC_CMD_RESET,nullptr);
        stop_rtc_wake();
        if(HAL_RTC_SetWakeUpTimer(&RTC_Handler,seconds-1,RTC_WAKEUP_SEC)!=HAL_OK){rc=-RT_EIO;break;}
        rc=ram_external_off();if(rc)break;
#if BOARD527_VERBOSE_LOGS
        rt_kprintf("CLOCK_RAM_SLEEP next_s=%u partials=%u\n",seconds,partials);
#endif
        rt_thread_mdelay(20);
        if(!key_seen) {
            reader_power_idle();int wait=rt_sem_take(&ram_event,rt_tick_from_millisecond((seconds+3)*1000));reader_power_busy();
            if(wait!=RT_EOK){rc=-RT_ETIMEOUT;break;}
        }
        stop_rtc_wake();
        if(key_seen)break;
        rc=ram_display_power();if(rc)break;
        if(!read_clock(minute,second)){rc=-RT_EIO;break;}
        if(test)minute=(shown_minute+1)%1440;
        bool full=clock_next_is_full(partials);draw_clock_time(r,minute);
#if BOARD527_VERBOSE_LOGS
        uint32_t prepare_ms=rt_tick_get_millisecond()-ram_wake_tick;
#endif
        rc=board527_clock_present(board527_framebuffer(),full);if(rc)break;
        shown_minute=minute;partials=full?0:partials+1;++updates;
#if BOARD527_VERBOSE_LOGS
        rt_kprintf("CLOCK_RAM_REFRESH kind=%s minute=%u partials=%u prepare_ms=%u total_ms=%u\n",
            full?"FULL":"PARTIAL",minute,partials,prepare_ms,rt_tick_get_millisecond()-ram_wake_tick);
#else
        if(full)rt_kprintf("CLOCK_RAM_FULL minute=%u total_ms=%u\n",minute,rt_tick_get_millisecond()-ram_wake_tick);
#endif
    }
ram_done:
    stop_rtc_wake();
    if(ram_active){ram_active=false;HAL_RTC_RegCallback(&RTC_Handler,saved_rtc_callback);}
    pm_disable_rtc_wakeup();rt_pin_irq_enable(34,PIN_IRQ_DISABLE);hwp_hpsys_aon->WER=wer;
    board527_steps_uart_idle(0);
    if(pads_saved&&ram_display_power())return abort_clock("ram_restore_rails");
    if(exclusive)board527_io_diagnostic_end();
    if(quiet&&reader527_steps_quiet_finish())return abort_clock("ram_restore_quiet");
    reading_settings_load(r);r->set_margin_top(top);r->set_margin_bottom(bottom);r->set_margin_left(left);r->set_margin_right(right);
    memcpy(board527_framebuffer(),saved_frame,1216*684/2);epub_mem_free(saved_frame);
    if(board527_clock_present(board527_framebuffer(),1))return abort_clock("ram_restore_frame");
    if(board527_touch_test_sleep(0))return abort_clock("ram_restore_touch");
    if(board527_frontlight_set(cold,warm))return abort_clock("ram_restore_light");
    memcpy(g_area_array,areas,sizeof(areas));g_area_count=area_count;
    board527_storage_pm_enable(1);if(reader)reader->resume_background();
    if(ui_queue){uint32_t msg;while(rt_mq_recv(ui_queue,&msg,sizeof(msg),0)==RT_EOK){}}
    ram_restored=(rc==0);
    rt_kprintf("CLOCK_RAM_DONE rc=%d updates=%u key=%u return_ms=%u\n",rc,updates,key_seen,rt_tick_get_millisecond()-ram_wake_tick);
    return rc;
}
bool reader527_standby_ram_take_restored()
{
    bool restored=ram_restored;ram_restored=false;return restored;
}
