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
    BSP_GPIO_Set(EPD_RES, high1_low0, 1);
}
void BSP_LCD_PowerDown(void)
{
    
    HAL_PIN_Set(PAD_PA00 + EPD_CS, GPIO_A0 + EPD_CS, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_SCK, GPIO_A0 + EPD_SCK, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_SDA,  GPIO_A0 + EPD_SDA, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_DC,  GPIO_A0 + EPD_DC, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_BUSY,  GPIO_A7, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_RES,  GPIO_A0, PIN_PULLDOWN, 1);
     }
void BSP_LCD_PowerUp(void)
{

    BSP_PIN_LCD();


}
/***************************Touch ***********************************/
extern void BSP_PIN_Touch(void);

void BSP_TP_PowerUp(void)
{
    BSP_GPIO_Set(32, 1, 1); // TP Power
    BSP_GPIO_Set(TOUCH_IRQ_PIN, 1, 1); 
    BSP_GPIO_Set(TOUCH_RESET_PIN, 1, 1); 
    HAL_PIN_Set(PAD_PA32, GPIO_A32, PIN_PULLUP, 1);   /* SENSOR_SDA */
    HAL_PIN_Set(PAD_PA00 + TOUCH_IRQ_PIN, GPIO_A0 + TOUCH_IRQ_PIN, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA00 + TOUCH_RESET_PIN, GPIO_A0 + TOUCH_RESET_PIN, PIN_NOPULL, 1);

}
void BSP_TP_PowerDown(void)
{
    //TODO: Setup TP power down pin
    BSP_GPIO_Set(32, 0, 1); // TP Power
    BSP_GPIO_Set(TOUCH_IRQ_PIN, 0, 1);
    BSP_GPIO_Set(TOUCH_RESET_PIN, 0, 1);
    HAL_PIN_Set(PAD_PA32, GPIO_A32, PIN_PULLDOWN, 1);   /* SENSOR_SDA */ 
    HAL_PIN_Set(PAD_PA00 + TOUCH_IRQ_PIN, GPIO_A0 + TOUCH_IRQ_PIN, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + TOUCH_RESET_PIN, GPIO_A0 + TOUCH_RESET_PIN, PIN_PULLDOWN, 1);
             


}
void BSP_TP_Reset(uint8_t high1_low0)
{
    BSP_GPIO_Set(TOUCH_RESET_PIN, high1_low0, 1);
}
void SD_card_power_off()
{
    HAL_PIN_Set(PAD_PA24, GPIO_A24, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA25, GPIO_A25, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA28, GPIO_A28, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA29, GPIO_A29, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA27, GPIO_A27, PIN_PULLDOWN, 1);
}
void SD_card_power_on()
{

    HAL_PIN_Set(PAD_PA24, SPI1_DIO, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA25, SPI1_DI,  PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA28, SPI1_CLK, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA29, SPI1_CS,  PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA27, GPIO_A27, PIN_PULLUP, 1);/*card detect pin*/
}