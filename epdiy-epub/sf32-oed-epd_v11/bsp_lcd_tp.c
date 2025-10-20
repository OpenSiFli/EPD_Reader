/*
 * SPDX-FileCopyrightText: 2019-2022 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bsp_board.h"
//#include "epd_tps.h"
extern void BSP_GPIO_Set(int pin, int val, int is_porta);
/***************************LCD ***********************************/
extern void BSP_PIN_LCD(void);
void BSP_LCD_Reset(uint8_t high1_low0)
{
}
void BSP_LCD_PowerDown(void)
{
    HAL_PIN_Set(PAD_PA00 + EPD_D0,    GPIO_A0 + EPD_D0,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D1,    GPIO_A0 + EPD_D1,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D2,    GPIO_A0 + EPD_D2,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D3,    GPIO_A0 + EPD_D3,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D4,    GPIO_A0 + EPD_D4,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D5,    GPIO_A0 + EPD_D5,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D6,    GPIO_A0 + EPD_D6,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D7,    GPIO_A0 + EPD_D7,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_LE,    GPIO_A0 + EPD_D0,    PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_OE,    GPIO_A0 + EPD_D1,    PIN_PULLDOWN, 1);
}
void BSP_LCD_PowerUp(void)
{
    //HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, /*enable=*/true, /*wait=*/true);//VDD33_VOUT2
    BSP_PIN_LCD();
}
/***************************Touch ***********************************/
extern void BSP_PIN_Touch(void);

void BSP_TP_PowerUp(void)
{
    BSP_GPIO_Set(32, 1, 1); // TP Power
    BSP_PIN_Touch();
    BSP_GPIO_Set(TOUCH_RESET_PIN,  1, 1);
}
void BSP_TP_PowerDown(void)
{
    //TODO: Setup TP power down pin
    HAL_PIN_Set(PAD_PA32, GPIO_A32, PIN_PULLDOWN, 1);   /* SENSOR_SDA */
    BSP_GPIO_Set(32, 0, 1); // TP Power

    //HAL_PIN_Set(PAD_PA31, GPIO_A31, PIN_PULLDOWN, 1);
    BSP_GPIO_Set(TOUCH_RESET_PIN,  0, 1);

    HAL_PIN_Set(PAD_PA30, GPIO_A30, PIN_PULLDOWN, 1);
    BSP_GPIO_Set(TOUCH_IRQ_PIN,  0, 1);

        
    //BSP_LCD_PowerDown();

}
void BSP_TP_Reset(uint8_t high1_low0)
{
    BSP_GPIO_Set(TOUCH_RESET_PIN, high1_low0, 1);
}
