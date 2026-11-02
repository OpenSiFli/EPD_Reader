#include "reader_ui.h"
#include "reader_power.h"
#include "UIRegionsManager.h"
extern "C" {
#include "board527.h"
#include "font_manager.h"
#include "bf0_hal.h"
#include "pan.h"
#include "drv_flash.h"
#include "board.h"
}
extern void reader527_close_book_for_power_test();

static int minimum_abort(const char *reason)
{
    rt_kprintf("MINPOWER_ABORT %s; rebooting to restore normal rails\n",reason);
    hwp_rtc->CR&=~(RTC_CR_WUTE|RTC_CR_WUTIE);
    rt_thread_mdelay(100);
    drv_reboot();
    return -1;
}

/* Explicit diagnostic entry, never automatic at boot or on normal inactivity.
 * File data and the protected USB loader are untouched; RAM is discarded. */
int reader527_minimum_power(Renderer *r,bool timed_test)
{
    reader527_close_book_for_power_test();r->cancel_page_preview();
    if(font_manager_select(0,28))return minimum_abort("builtin_font");
    clear_areas();r->clear_screen();
    r->draw_text(20,80,"MINIMUM POWER TEST R32");
    r->draw_text(20,210,"CPU HIBERNATE - NO RAM RETENTION");
    r->draw_text(20,310,"Panel HV / frontlight / touch: OFF");
    r->draw_text(20,380,"Amplifier / TF / shared 3V3: OFF");
    r->draw_text(20,450,"PSRAM / NOR / AW supply: OFF");
    r->draw_text(20,520,"USB / UART / buses: high impedance");
    r->draw_text(20,650,"Unplug USB and COM36 before measuring.");
    r->draw_text(20,720,timed_test?"RTC test: reboot in about 10 seconds.":"No automatic wake. Touch is disabled.");
    r->draw_text(20,790,"Exit: remove ALL power, including battery.");
    r->draw_text(20,860,"Power on again to return to the reader.");
    r->flush_display();

    // No storage power may be removed until every card descriptor is closed.
    if(board527_storage_test_begin()) {
        return minimum_abort("storage_busy");
    }
    if(board527_frontlight_set(0,0)||board527_amplifier_test(1)) {
        return minimum_abort("peripheral_shutdown");
    }
    board527_touch_power(0); // GE13 reset low and PA32 power control low.
    if(tps_enter_sleep()!=RT_EOK) {
        return minimum_abort("HV_shutdown");
    }

    RTC_HandleTypeDef rtc={};rtc.Instance=hwp_rtc;
    if(HAL_RTC_DeactivateWakeUpTimer(&rtc)!=HAL_OK) {
        return minimum_abort("rtc_stop");
    }
    hwp_rtc->CR&=~(RTC_CR_ALRME|RTC_CR_ALRMIE);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&rtc,RTC_FLAG_WUTF);
    hwp_pmuc->WER=0;
    if(rt_pin_read(34))return minimum_abort("power_key_held");
    HAL_PMU_SelectWakeupPin(0,10); // SF32LB527 PA34 = AON PIN10.
    HAL_PMU_EnablePinWakeup(0,AON_PIN_MODE_HIGH);
    if(timed_test) {
        if(HAL_RTC_SetWakeUpTimer(&rtc,9,RTC_WAKEUP_SEC)!=HAL_OK) {
            return minimum_abort("rtc_start");
        }
        HAL_PMU_EnableRtcWakeup();
    }
    board527_minimum_park_buses(); // Avoid back-power before the shared rail falls.
    if(board527_minimum_external_off())return minimum_abort("shared_enable_readback");
    rt_kprintf("MINPOWER_ENTER timed_test=%d TF_unmounted=1 HV=OFF PMU_WER=%08x PERI_LDO_before=%08x\n",
               timed_test,hwp_pmuc->WER,hwp_pmuc->PERI_LDO);
    rt_thread_mdelay(30); // Drain the final UART message before its pins go high-Z.
    board527_minimum_hibernate();
    return -1;
}
