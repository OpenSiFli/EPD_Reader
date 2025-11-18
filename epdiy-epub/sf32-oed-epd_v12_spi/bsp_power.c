#include "bsp_board.h"


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
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, /*enable=*/false, /*wait=*/true);//VDD33_VOUT2

}

__WEAK void BSP_PowerUpCustom(bool is_deep_sleep)
{
   HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, /*enable=*/true, /*wait=*/true);//VDD33_VOUT2

}

HAL_RAM_RET_CODE_SECT(PowerDownCustom, void PowerDownCustom(void))
{

    HAL_PMU_SelectWakeupPin(0, 10);   // PA34
    HAL_PMU_EnablePinWakeup(0, 0);

    HAL_PIN_Set(PAD_PA24, GPIO_A24, PIN_PULLDOWN, 1);
    for (uint32_t i = PAD_PA28; i <= PAD_PA44; i++)
    {
        HAL_PIN_Set(i, (pin_function)(i - PAD_PA28 + GPIO_A28), PIN_PULLDOWN, 1);
    }
    hwp_pmuc->PERI_LDO &=  ~(PMUC_PERI_LDO_EN_LDO18 | PMUC_PERI_LDO_EN_VDD33_LDO2 | PMUC_PERI_LDO_EN_VDD33_LDO3);
    hwp_pmuc->WKUP_CNT = 0x000F000F;

    

    HAL_DisableInterrupt();
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO2_3V3, false, false);
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO_1V8, false, false);
    HAL_PMU_EnterHibernate();

}

extern void *rt_flash_get_handle_by_addr(uint32_t addr);
void BSP_Power_Up(bool is_deep_sleep)
{
    BSP_PowerUpCustom(is_deep_sleep);
#ifdef SOC_BF0_HCPU
    if (!is_deep_sleep)
    {
#ifdef BSP_USING_PSRAM1
        bsp_psram_exit_low_power("psram1");
#endif /* BSP_USING_PSRAM1 */
#ifdef BSP_USING_NOR_FLASH2
        FLASH_HandleTypeDef *flash_handle;
        flash_handle = (FLASH_HandleTypeDef *)rt_flash_get_handle_by_addr(MPI2_MEM_BASE);
        HAL_FLASH_RELEASE_DPD(flash_handle);
        HAL_Delay_us(80);
#endif
    }
#endif  /* SOC_BF0_HCPU */
}



void BSP_IO_Power_Down(int coreid, bool is_deep_sleep)
{
    BSP_PowerDownCustom(coreid, is_deep_sleep);
#ifdef SOC_BF0_HCPU
    if (coreid == CORE_ID_HCPU)
    {
#ifdef BSP_USING_PSRAM1
        bsp_psram_enter_low_power("psram1");
#endif /* BSP_USING_PSRAM1 */
#ifdef BSP_USING_NOR_FLASH2
        FLASH_HandleTypeDef *flash_handle;
        flash_handle = (FLASH_HandleTypeDef *)rt_flash_get_handle_by_addr(MPI2_MEM_BASE);
        HAL_FLASH_DEEP_PWRDOWN(flash_handle);
        HAL_Delay_us(3);
#endif
    }
#endif  /* SOC_BF0_HCPU */
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


