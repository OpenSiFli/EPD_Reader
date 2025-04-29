#include "SF32PaperRenderer.h"

#ifdef __cplusplus
extern "C" {
#endif


#include "rtthread.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"
// #include "opm060e9_driver.h"
#include <rtdevice.h>
#include "bf0_hal.h"
// #define EPD_KEY 11


void HAL_PostMspInit(void)
{
    //edp gpio pin
    const uint32_t pin_out[] =
    {
        TPS_WAKEUP,
        TPS_PWRCOM,
        TPS_PWRUP,
        EPD_LE,
        EPD_OE,
        EPD_STV,
        EPD_CPV,
        EPD_GMODE,
#ifdef EPD_DRIVER_SOFTWARE
        EPD_CLK,
        EPD_SPH,
        EPD_D0,
        EPD_D1,
        EPD_D2,
        EPD_D3,
        EPD_D4,
        EPD_D5,
        EPD_D6,
        EPD_D7,
#endif /*EPD_DRIVER_SOFTWARE*/
    };
    int pin_num = sizeof(pin_out) / sizeof(pin_out[0]);

    //epd pin init
    for (int i = 0; i < pin_num; i++)
    {
        HAL_PIN_Set(PAD_PA00 + pin_out[i], (pin_function) (GPIO_A0 + pin_out[i]), PIN_NOPULL, 1);
    }

    HAL_RCC_EnableModule(RCC_MOD_GPIO1); // GPIO clock enable
    GPIO_InitTypeDef GPIO_InitStruct;

    for (int i = 0; i < pin_num; i++)
    {
        GPIO_InitStruct.Pin = pin_out[i];
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        HAL_GPIO_Init(hwp_gpio1, &GPIO_InitStruct);
        HAL_GPIO_WritePin(hwp_gpio1, pin_out[i], GPIO_PIN_RESET);
    }

#ifdef EPD_DRIVER_LCDC
    HAL_PIN_Set(PAD_PA00 + EPD_CLK, LCDC1_8080_WR, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_SPH, LCDC1_8080_DC, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D0,  LCDC1_8080_DIO0, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D1,  LCDC1_8080_DIO1, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D2,  LCDC1_8080_DIO2, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D3,  LCDC1_8080_DIO3, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D4,  LCDC1_8080_DIO4, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D5,  LCDC1_8080_DIO5, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D6,  LCDC1_8080_DIO6, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA00 + EPD_D7,  LCDC1_8080_DIO7, PIN_NOPULL, 1);
#endif /*EPD_DRIVER_LCDC*/

    HAL_PIN_Set(PAD_PA00 + TPS_SCL, I2C2_SCL, PIN_PULLUP, 1); // i2c io select
    HAL_PIN_Set(PAD_PA00 + TPS_SDA, I2C2_SDA, PIN_PULLUP, 1);

    // key init
    // HAL_PIN_Set(PAD_PA00 + EPD_KEY, (pin_function) (GPIO_A0 + EPD_KEY), PIN_PULLDOWN, 1);
}
#ifdef __cplusplus
}
#endif