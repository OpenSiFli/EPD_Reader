#include "bsp_board.h"
#include "bsp_io_expansion.h"

#ifdef BSP_USING_LCD





/***************************LCD ***********************************/
extern void BSP_PIN_LCD(void);
void BSP_LCD_Reset(uint8_t high1_low0)
{

}

void BSP_LCD_PowerDown(void)
{

}

void BSP_LCD_PowerUp(void)
{
    BSP_PIN_LCD();
}


/***************************Touch ***********************************/
extern void BSP_PIN_Touch(void);
void BSP_TP_PowerUp(void)
{
    BSP_PIN_Touch();

    AW9523B_Write_Pin(P0_3_CTP_VDD_EN, 1);
    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 1);
}

void BSP_TP_PowerDown(void)
{
    // TODO: Setup TP power down pin
    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 0);
    AW9523B_Write_Pin(P0_3_CTP_VDD_EN, 1);
}
void BSP_TP_Reset(uint8_t high1_low0)
{
    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 1);
}

#endif
