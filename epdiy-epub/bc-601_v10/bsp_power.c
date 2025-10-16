#include "bsp_board.h"
#include "bsp_io_expansion.h"

void BSP_GPIO_Set(int pin, int val, int is_porta)
{
    GPIO_TypeDef *gpio = (is_porta) ? hwp_gpio1 : hwp_gpio2;
    GPIO_InitTypeDef GPIO_InitStruct;

    // set sensor pin to output mode
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(gpio, &GPIO_InitStruct);

    // set sensor pin to high == power on sensor board
    HAL_GPIO_WritePin(gpio, pin, (GPIO_PinState)val);
}

#define MPI2_POWER_PIN  (11)

__WEAK void BSP_PowerDownCustom(int coreid, bool is_deep_sleep)
{
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, false, false);
    BSP_GPIO_Set(MPI2_POWER_PIN, 0, 1);
}

__WEAK void BSP_PowerUpCustom(bool is_deep_sleep)
{
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, true, false);
    BSP_GPIO_Set(MPI2_POWER_PIN, 1, 1);
}


void BSP_Power_Up(bool is_deep_sleep)
{
    BSP_PowerUpCustom(is_deep_sleep);
}



void BSP_IO_Power_Down(int coreid, bool is_deep_sleep)
{
    BSP_PowerDownCustom(coreid, is_deep_sleep);
}

void BSP_SDIO_Power_Up(void)
{
#ifdef RT_USING_SDIO
    // TODO: Add SDIO power up

#endif

}
void BSP_SDIO_Power_Down(void)
{
#ifdef RT_USING_SDIO
    // TODO: Add SDIO power down
#endif
}

void EBC_PMIC_Control(uint8_t enable)
{
    AW9523B_Write_Pin(P0_6_EBC_PMIC_ON, enable);
}

void EBC_EN_ST_Control(uint8_t enable)
{
    AW9523B_Write_Pin(P0_7_EBC_EN_ST, enable);
}

void AU_PA_Control(uint8_t enable)
{
    AW9523B_Write_Pin(8 + P1_4_AU_PA_EN, enable);
}

uint8_t EBC_Read_Good_State(void)
{
    return AW9523B_Read_Pin(P0_5_EBC_EN_GOOD);
}


