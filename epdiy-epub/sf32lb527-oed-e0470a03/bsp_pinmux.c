
/*
 * SPDX-FileCopyrightText: 2019-2022 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "bsp_board.h"
extern void BSP_GPIO_Set(int pin, int val, int is_porta);
#ifdef BSP_USING_PSRAM1
/* APS 128p*/
static void board_pinmux_psram_func0()
{
    HAL_PIN_Set(PAD_SA01, MPI1_DIO0, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA02, MPI1_DIO1, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA03, MPI1_DIO2, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA04, MPI1_DIO3, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA05, MPI1_DIO4, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA06, MPI1_DIO5, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA07, MPI1_DIO6, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA08, MPI1_DIO7, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA09, MPI1_DQSDM, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA10, MPI1_CLK,  PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA11, MPI1_CS,   PIN_NOPULL, 1);
    HAL_PIN_Set_Analog(PAD_SA00, 1);
    HAL_PIN_Set_Analog(PAD_SA12, 1);
}
/* APS 1:64p 2:32P, 4:Winbond 32/64/128p*/
static void board_pinmux_psram_func1_2_4(int func)
{
    HAL_PIN_Set(PAD_SA01, MPI1_DIO0, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA02, MPI1_DIO1, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA03, MPI1_DIO2, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA04, MPI1_DIO3, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA08, MPI1_DIO4, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA09, MPI1_DIO5, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA10, MPI1_DIO6, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA11, MPI1_DIO7, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA07, MPI1_CLK,  PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA05, MPI1_CS,   PIN_NOPULL, 1);
#ifdef FPGA
    HAL_PIN_Set(PAD_SA00, MPI1_DM, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA06, MPI1_CLKB, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA12, MPI1_DQSDM, PIN_PULLDOWN, 1);
#else
    switch (func)
    {
    case 1:             // APS 64P XCELLA
        HAL_PIN_Set(PAD_SA12, MPI1_DQSDM, PIN_PULLDOWN, 1);
        HAL_PIN_Set_Analog(PAD_SA00, 1);
        HAL_PIN_Set_Analog(PAD_SA06, 1);
        break;
    case 2:             // APS 32P LEGACY
        HAL_PIN_Set(PAD_SA00, MPI1_DM, PIN_PULLDOWN, 1);
        HAL_PIN_Set(PAD_SA12, MPI1_DQS, PIN_PULLDOWN, 1);
        HAL_PIN_Set(PAD_SA06, MPI1_CLKB, PIN_NOPULL, 1);
        break;
    case 4:             // Winbond 32/64/128p
        //HAL_PIN_Set(PAD_SA06, MPI1_CLKB, PIN_NOPULL, 1);
        HAL_PIN_Set(PAD_SA12, MPI1_DQSDM, PIN_NOPULL, 1);
        HAL_PIN_Set_Analog(PAD_SA00, 1);
        HAL_PIN_Set_Analog(PAD_SA06, 1);
        break;
    }
#endif
}
/* APS 16p*/
static void board_pinmux_psram_func3()
{
    HAL_PIN_Set(PAD_SA09, MPI1_CLK, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA08, MPI1_CS,  PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_SA05, MPI1_DIO0, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA07, MPI1_DIO1, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_SA06, MPI1_DIO2, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_SA10, MPI1_DIO3, PIN_PULLUP, 1);
    HAL_PIN_Set_Analog(PAD_SA00, 1);
    HAL_PIN_Set_Analog(PAD_SA01, 1);
    HAL_PIN_Set_Analog(PAD_SA02, 1);
    HAL_PIN_Set_Analog(PAD_SA03, 1);
    HAL_PIN_Set_Analog(PAD_SA04, 1);
    HAL_PIN_Set_Analog(PAD_SA11, 1);
    HAL_PIN_Set_Analog(PAD_SA12, 1);
}
static void board_pinmux_mpi1_none(void)
{
    uint32_t i;
    for (i = 0; i <= 12; i++)
    {
        HAL_PIN_Set_Analog(PAD_SA00 + i, 1);
    }
}
#endif
static void BSP_PIN_Common(void)
{
#ifdef SOC_BF0_HCPU
    // HCPU pins
    uint32_t pid = (hwp_hpsys_cfg->IDR & HPSYS_CFG_IDR_PID_Msk) >> HPSYS_CFG_IDR_PID_Pos;
    pid &= 7;
#ifdef BSP_USING_PSRAM1
    switch (pid)
    {
    case 5: //BOOT_PSRAM_APS_16P:
        board_pinmux_psram_func3();         // 16Mb APM QSPI PSRAM
        break;
    case 4: //BOOT_PSRAM_APS_32P:
        board_pinmux_psram_func1_2_4(2);    // 32Mb APM LEGACY PSRAM
        break;
    case 6: //BOOT_PSRAM_WINBOND:                // Winbond HYPERBUS PSRAM
        board_pinmux_psram_func1_2_4(4);
        break;
    case 3: // BOOT_PSRAM_APS_64P:
        board_pinmux_psram_func1_2_4(1);    // 64Mb APM XCELLA PSRAM
        break;
    case 2: //BOOT_PSRAM_APS_128P:
        board_pinmux_psram_func0();         // 128Mb APM XCELLA PSRAM
        break;
    default:
        board_pinmux_mpi1_none();
        break;
    }
#endif /* BSP_USING_PSRAM1 */
#ifdef BSP_ENABLE_MPI2
    // MPI2
    HAL_PIN_Set(PAD_PA16, MPI2_CLK,  PIN_NOPULL,   1);
    HAL_PIN_Set(PAD_PA12, MPI2_CS,   PIN_NOPULL,   1);
    HAL_PIN_Set(PAD_PA15, MPI2_DIO0, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA13, MPI2_DIO1, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA14, MPI2_DIO2, PIN_PULLUP,   1);
    HAL_PIN_Set(PAD_PA17, MPI2_DIO3, PIN_PULLUP, 1);
    
#endif

#ifdef BSP_USING_SPI1
    /*****SPI TF card HCPU******************************START*/
     //SPI1(TF card)
    HAL_PIN_Set(PAD_PA24, SPI1_DIO, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA25, SPI1_DI,  PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA28, SPI1_CLK, PIN_NOPULL, 1);
    HAL_PIN_Set(PAD_PA29, SPI1_CS,  PIN_NOPULL, 1);
    /* Actual TF detect is AW9523B GE0, not the reference board PA27. */
    /*****SPI TF card HCPU******************************END***/
#endif

    // UART1 - debug
    HAL_PIN_Set(PAD_PA18, USART1_RXD, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA19, USART1_TXD, PIN_PULLUP, 1);


    /* Actual board uses AW9523B. Peripheral mapping is not yet known. */
#endif
}

#ifdef SOC_BF0_HCPU
void BSP_PIN_Touch(void) {} /* Touch driver owns PA32/PA44 and AW GE13. */
void BSP_PIN_LCD(void)
{
    //edp gpio pin
    const uint32_t pin_out[] =
    {
        EPD_LE,
        EPD_OE,
        EPD_STV,
        EPD_CPV,
        EPD_GMODE,
    };
    int pin_num = sizeof(pin_out) / sizeof(pin_out[0]);
    //epd pin init
    for (int i = 0; i < pin_num; i++)
    {
        HAL_PIN_Set(PAD_PA00 + pin_out[i], GPIO_A0 + pin_out[i], PIN_NOPULL, 1);
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
    HAL_PIN_Set(PAD_PA00 + TPS_SCL, I2C1_SCL, PIN_NOPULL, 1); // CTP
    HAL_PIN_Set(PAD_PA00 + TPS_SDA, I2C1_SDA, PIN_NOPULL, 1);
}
void BSP_PIN_Init(void)
{
    BSP_PIN_Common();
    BSP_PIN_LCD();
    HAL_PIN_Set(PAD_PA20, I2C2_SCL, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA30, I2C2_SDA, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA03, GPIO_A3, PIN_NOPULL, 1);
    BSP_GPIO_Set(3, 0, 1); /* AW9523B held in reset until software init. */
    HAL_PIN_Set(PAD_PA38, GPIO_A38, PIN_PULLUP, 1);
    HAL_GPIO_WritePin(hwp_gpio1, 38, GPIO_PIN_RESET);
    GPIO_InitTypeDef irq = {.Pin=38,.Mode=GPIO_MODE_INPUT,.Pull=GPIO_PULLUP};
    HAL_GPIO_Init(hwp_gpio1, &irq);
}
#else
void BSP_PIN_Init(void) {} /* HCPU owns board peripherals. */
#endif
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
