/* SPDX-License-Identifier: Apache-2.0 */
#include "rtconfig.h"
#if !defined(CFG_BOOTLOADER) && !defined(SOC_BF0_LCPU) && !defined(READER527_RECOVERY)
extern void board527_touch_power(int on);
extern int board_touch_reset(int high);
extern void board527_display_off(void);
void BSP_TP_PowerUp(void){board527_touch_power(1);}
void BSP_TP_PowerDown(void){board527_touch_power(0);}
void BSP_TP_Reset(unsigned char high){board_touch_reset(high);}
void BSP_LCD_Reset(unsigned char high){(void)high;}
void BSP_LCD_PowerUp(void){}
void BSP_LCD_PowerDown(void){board527_display_off();}
void SD_card_power_on(void){}
void SD_card_power_off(void){} /* No verified TF power switch. */
#else
void BSP_TP_PowerUp(void){}
void BSP_TP_PowerDown(void){}
void BSP_TP_Reset(unsigned char high){(void)high;}
void BSP_LCD_Reset(unsigned char high){(void)high;}
void BSP_LCD_PowerUp(void){}
void BSP_LCD_PowerDown(void){}
#endif
