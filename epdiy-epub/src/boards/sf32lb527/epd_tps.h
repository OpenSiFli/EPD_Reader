/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EPD_TPS_H__
#define __EPD_TPS_H__
#include <rtthread.h>
#ifdef __cplusplus
extern "C" {
#endif
//模组的VCOM电压(2100代表-2.10V)
rt_err_t tps_init(uint16_t vcom_voltage);
rt_err_t tps_enter_sleep(void);
rt_err_t tps_exit_sleep(void);
int tps_bus_probe(void);
int board_touch_reset(int high);
int board_tf_present(void);
unsigned board_expander_generation(void);
int board_touch_gpio_report(void);
int board_frontlight_enable(int high);
int board_frontlight_gpio_report(void);
#ifdef __cplusplus
}
#endif
#endif /*__EPD_TPS_H__*/
