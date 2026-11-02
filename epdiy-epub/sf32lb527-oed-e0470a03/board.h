/*
 * SPDX-FileCopyrightText: 2019-2022 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "rtconfig.h"
#include "bf0_hal.h"
#include "bsp_board.h"
#define EPD_TPS_I2C_DEVICE_NAME "i2c1"
#define EPD_TPS_WAKEUP_PIN TPS_WAKEUP
#define EPD_TPS_PWRCOM_PIN TPS_PWRCOM
#define EPD_TPS_PWRUP_PIN TPS_PWRUP
#ifdef BSP_USING_RTTHREAD
    #include "drv_common.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* __BOARD_H__ */
