
#include "bsp_board.h"
#if defined(BSP_USING_PSRAM1)
    uint32_t rt_psram_enter_low_power(char *name);
    uint32_t rt_psram_exit_low_power(char *name);
#endif
#if defined(BSP_USING_NOR_FLASH2)
    extern void rt_flash_power_down(uint32_t addr, int pd);
    extern void rt_flash_enable_lock(uint8_t en);
#endif
#if defined(BSP_USING_NOR_FLASH2)
    extern void rt_flash_power_down(uint32_t addr, int pd);
    extern void rt_flash_enable_lock(uint8_t en);
#endif
#ifdef BSP_USING_LCDC
    extern uint8_t lcd_get_idle_status(void);
#endif
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
__WEAK void BSP_PowerDownCustom(int coreid, bool is_deep_sleep)
{
}
__WEAK void BSP_PowerUpCustom(bool is_deep_sleep)
{
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, /*enable=*/true, /*wait=*/true);//VDD33_VOUT2
    BSP_GPIO_Set(32, 1, 1); // TP Power
}
/**
  * @brief when HDK52X close sensor, needf config sensor pin for low power
  * @param  void
  * @retval void.
  */
void BSP_Sensor_PowerDown_IO_Config()
{
    // HAL_PIN_Set(PAD_PA35, GPIO_A35, PIN_NOPULL, 1);   /* SENSOR_SCL */
    // HAL_PIN_Set(PAD_PA36, GPIO_A36, PIN_NOPULL, 1);   /* SENSOR_SDA */
}
/**
 * @brief 
 * 
 *
  * @param  void
  * @retval void.
  */
void BSP_Sensor_PowerUp()
{
}
/**
  * @brief Config sensor power down
  * @param  void
  * @retval void.
  */
void BSP_Sensor_PowerDown()
{
}
/**
  * @brief when HDK52X power on or wake up , rboot, config power supply or sleep
  * @param  is_deep_sleep   true: wake from  deep sleep; false: other;
  * @retval void.
  */
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
#ifdef SOC_BF0_HCPU
    //HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, /*enable=*/false, /*wait=*/true);//VDD33_VOUT2
    if (coreid == CORE_ID_HCPU)
    {
        BSP_PowerDownCustom(coreid, is_deep_sleep);
#if  defined(BSP_USING_NOR_FLASH2)
#ifdef BSP_QSPI2_DUAL_MODE
        rt_flash_enable_lock(0);
        rt_flash_power_down(0x13000000, 1); //  deep flash 2 bus flash B
        HAL_Delay_us(50);
#endif
        rt_flash_power_down(0x12000000, 1); //  deep flash 2 bus flash A
        HAL_Delay_us(50);
#endif
#if defined(BSP_USING_PSRAM1)
        rt_psram_enter_low_power("psram1");
#endif
    }
#else
    {
        ;
    }
#endif
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
