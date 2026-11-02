/* SPDX-License-Identifier: Apache-2.0 */
#ifndef EPD_READER_BOARD527_H
#define EPD_READER_BOARD527_H
#include <stdint.h>
#include "epd_tps.h"
/* Enable detailed per-refresh/per-minute timing only for diagnostics. */
#ifndef BOARD527_VERBOSE_LOGS
#define BOARD527_VERBOSE_LOGS 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
int board527_init(void);
int board527_display_init(void);
int board527_display_present(const uint8_t *gray4);
int board527_display_prepare(const uint8_t *gray4,int (*cancel)(void *),void *context);
void board527_display_cancel_prepare(void);
uint8_t *board527_framebuffer(void);
void board527_display_off(void);
void board527_clock_seed(const uint8_t *gray4);
int board527_clock_present(const uint8_t *gray4,int full);
int board527_touch_init(void);
void board527_touch_power(int on);
void board527_touch_lock(void);
void board527_touch_unlock(void);
int board527_touch_active(void);
int board527_touch_test_sleep(int sleeping);
int board527_touch_quiet_mode(unsigned mode);
void board527_io_init(void);
int board527_io_enter(void);
void board527_io_exit(void);
int board527_io_diagnostic_begin(void);
void board527_io_diagnostic_end(void);
int board527_logic_power(int on);
int board527_logic_sleep_ready(void);
int board527_storage_idle(int force);
unsigned board527_storage_wait_ms(unsigned normal_wait_ms);
void board527_storage_pm_enable(int enabled);
void board527_power_auto(void);
void board527_power_hold(void);
void board527_power_stats(uint32_t *enters,uint32_t *sleep_ms);
void board527_steps_uart_idle(int enabled);
int board527_steps_aw(int reset_hold,int logic_on);
int board527_logic_rail_test(int off);
int board527_storage_test_begin(void);
int board527_storage_test_end(void);
int board527_amplifier_report(void);
int board527_amplifier_test(int mute);
int board527_minimum_external_off(void);
void board527_minimum_park_buses(void);
void board527_minimum_hibernate(void);
uint32_t tf_msd_read_count(void);
int board527_storage_mount(void);
int board527_frontlight_set(unsigned cold,unsigned warm);
void board527_frontlight_get(unsigned *cold,unsigned *warm);
#ifdef __cplusplus
}
#endif
#endif
