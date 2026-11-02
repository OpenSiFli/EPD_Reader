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

#define MPI2_POWER_PIN  (11) /* unused; PA11 is TPS SDA on OED v1.2 */
static int steps_uart_idle,steps_uart_parked;
static uint32_t steps_uart_pads[2];
void board527_steps_uart_idle(int enabled){steps_uart_idle=enabled;}

__WEAK void BSP_PowerDownCustom(int coreid, bool is_deep_sleep)
{
    /* Keep touch, AW9523B and frontlight rails stable across retained sleep. */
    (void)coreid; (void)is_deep_sleep;
    if(steps_uart_idle) {
        steps_uart_pads[0]=hwp_pinmux1->PAD_PA18;
        steps_uart_pads[1]=hwp_pinmux1->PAD_PA19;
        HAL_PIN_Set_Analog(PAD_PA18,1);HAL_PIN_Set_Analog(PAD_PA19,1);
        steps_uart_parked=1;
    }
}

__WEAK void BSP_PowerUpCustom(bool is_deep_sleep)
{
    /* Cold boot still establishes LDO3; retained sleep never switches it off. */
    if (is_deep_sleep) {
        // Called from pre-init too: do not interpret uninitialized SRAM flags.
        steps_uart_idle=0;steps_uart_parked=0;
        HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, true, true);
    } else if(steps_uart_parked) {
        hwp_pinmux1->PAD_PA18=steps_uart_pads[0];hwp_pinmux1->PAD_PA19=steps_uart_pads[1];
        steps_uart_parked=0;
    }
}

void PowerDownCustom(void)
{
    /* HCPU retained sleep uses BSP_IO_Power_Down/BSP_Power_Up below. */
}

extern void *rt_flash_get_handle_by_addr(uint32_t addr);
void BSP_Power_Up(bool is_deep_sleep)
{
    BSP_PowerUpCustom(is_deep_sleep);
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO2_3V3, true, true);
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

/* This entire object and all called HAL/Flash entry points are in internal
 * SRAM. Nothing after Flash DPD may execute or fetch constants from XIP. */
#if defined(SOC_BF0_HCPU) && defined(RT_USING_PM)
void board527_minimum_park_buses(void)
{
    for(unsigned n=0;n<=44;++n) {
        // Keep NOR/UART, AW I2C/reset, touch power low and wake key available.
        if(n==3||(n>=12&&n<=20)||n==30||n==32||n==34)continue;
        GPIO_InitTypeDef cfg={.Pin=n,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_NOPULL};
        HAL_GPIO_Init(hwp_gpio1,&cfg);
        HAL_PIN_Set_Analog(PAD_PA00+n,1);
    }
}

__attribute__((noreturn)) void board527_minimum_hibernate(void)
{
    HAL_DisableInterrupt();
    SysTick->CTRL=0;
    SCB_CleanDCache();__DSB();
    FLASH_HandleTypeDef *flash=(FLASH_HandleTypeDef *)rt_flash_get_handle_by_addr(MPI2_MEM_BASE);
    bsp_psram_enter_low_power("psram1");
    HAL_FLASH_DEEP_PWRDOWN(flash);HAL_Delay_us(10);

    /* All external buses, audio, USB and UART are high-Z/no pull. PA3 keeps
     * AW in reset and PA32 holds the touch switch off until pads power down. */
    for(unsigned n=0;n<=44;++n) {
        GPIO_InitTypeDef cfg={.Pin=n,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_NOPULL};
        HAL_GPIO_Init(hwp_gpio1,&cfg);
        HAL_PIN_Set_Analog(PAD_PA00+n,1);
    }
    HAL_PIN_Set(PAD_PA03,GPIO_A3,PIN_NOPULL,1);BSP_GPIO_Set(3,0,1);
    HAL_PIN_Set(PAD_PA32,GPIO_A32,PIN_NOPULL,1);BSP_GPIO_Set(32,0,1);
    HAL_PIN_Set(PAD_PA34,GPIO_A34,PIN_PULLDOWN,1); // PMU power-key wake only.
    for(unsigned pad=PAD_SA00;pad<=PAD_SA12;++pad)HAL_PIN_Set_Analog(pad,1);
    __DSB();

    /* No PSRAM retention is needed: Hibernate always boots from reset. The
     * SDK Hibernate entry also disables LDO2 and resets/stops the LCPU. */
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3,false,false);
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO_1V8,false,false);
    HAL_PMU_EnterHibernate();
    while(1){};
}
#endif


