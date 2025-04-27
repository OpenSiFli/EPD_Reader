/*
  ******************************************************************************
  * @file   main.c
  * @author Sifli software development team
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2021 - 2021,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include "rtconfig.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "mem_section.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"

#ifdef EPD_DRIVER_LCDC

#define FRAME_END_LEN       32      //每次刷新所需帧数
#define DISPLAY_LINE_CLOCKS   362     //每列刷新所需次数，362*4像素
#define DISPLAY_ROWS   1072

#define  LOG_POINTS  rt_kprintf("%s, %d \r\n",__FUNCTION__, __LINE__)


static uint32_t epic_LTabs[FRAME_END_LEN][256];
static void epd_DMA_init(void);
static void epd_wave_table_epic_Ltab(void);


//反显刷图片 改善边缘扩散专用波形
const unsigned char wave_end[4][FRAME_END_LEN] =
{
    0, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, //GC3->GC0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //GC3->GC1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //GC3->GC2
    0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, //GC3->GC3
};


//1像素1bit表示，转为 1像素用2bit表示
unsigned char OneBitToTwoBit[16] =
{
    0x00,   //0000B
    0x03,   //0001B
    0x0c,   //0010B
    0x0f,   //0011B
    0x30,   //0100B
    0x33,   //0101B
    0x3c,   //0110B
    0x3f,   //0111B

    0xc0,   //1000B
    0xc3,   //1001B
    0xcc,   //1010B
    0xcf,   //1011B
    0xf0,   //1100B
    0xf3,   //1101B
    0xfc,   //1110B
    0xff,   //1111B
};




void epd_Init(void)
{
    oedtps_vcom_disable();
    oedtps_source_gate_disable();

    EPD_LE_L_hs();
    EPD_CLK_L_hs();
    EPD_OE_L_hs();
    EPD_SPH_H_hs();
    EPD_STV_H_hs();
    EPD_CPV_L_hs();
    EPD_GMODE_H_hs();

    epd_wave_table_epic_Ltab();
    epd_DMA_init();
}



#define EPIC_INPUT_1BPP
static LCDC_InitTypeDef lcdc_int_dbi_cfg =
{
    .lcd_itf = LCDC_INTF_DBI_8BIT_B,
    .freq = 24 * 1000 * 1000,
    .color_mode = LCDC_PIXEL_FORMAT_RGB332,

    .cfg = {
        .dbi = {
            .syn_mode = HAL_LCDC_SYNC_DISABLE, //HAL_LCDC_SYNC_VER, //HAL_LCDC_SYNC_DISABLE,
            .vsyn_polarity = 0,
            .vsyn_delay_us = 0,
            .hsyn_num = 0,
        },
    },
};
#ifdef LCD_OUTPUT_TO_RAM
static uint8_t lcd_output[DISPLAY_LINE_CLOCKS];
static LCDC_InitTypeDef lcdc_int_ahb_cfg =
{
    .freq = 240 * 1000 * 1000, //Non-zero frequency, avoid assertion only.
    .lcd_itf = LCDC_INTF_AHB,
    .color_mode = LCDC_PIXEL_FORMAT_RGB332,
    .cfg = {
        .ahb = {
            .lcd_mem = (uint32_t) lcd_output,
        },
    };

#endif /*LCD_OUTPUT_TO_RAM*/

    static uint8_t  cur_epic_ahb_out_idx = 0;

#ifdef EPIC_INPUT_1BPP
    static uint16_t epic_ahb_out[2][DISPLAY_LINE_CLOCKS >> 1];
#else
    static uint16_t epic_ahb_out[2][DISPLAY_LINE_CLOCKS];
    static uint8_t epic_input[DISPLAY_LINE_CLOCKS];
#endif

    static LCDC_HandleTypeDef lcdc_handler;

    static void epd_DMA_init(void)
    {
        /*Initialize LCDC*/
        LCDC_HandleTypeDef *hlcdc = &lcdc_handler;
        hlcdc->Instance = LCDC1;

#ifdef LCD_OUTPUT_TO_RAM
        memcpy(&hlcdc->Init, &lcdc_int_dbi_cfg, sizeof(LCDC_InitTypeDef));
#else
        memcpy(&hlcdc->Init, &lcdc_int_dbi_cfg, sizeof(LCDC_InitTypeDef));
#endif

        HAL_LCDC_Init(hlcdc);
#ifdef EPIC_INPUT_1BPP
        uint32_t lcdc_in_format = 4; // RGB332
#else
        uint32_t lcdc_in_format = 0; // RGB565
#endif
        hlcdc->Instance->LAYER0_CONFIG = (lcdc_in_format   << LCD_IF_LAYER0_CONFIG_FORMAT_Pos) |
                                         (1   << LCD_IF_LAYER0_CONFIG_ALPHA_SEL_Pos) |     // use layer alpha
                                         (255 << LCD_IF_LAYER0_CONFIG_ALPHA_Pos) |         // layer alpha value is 255
                                         (0   << LCD_IF_LAYER0_CONFIG_FILTER_EN_Pos) |     // disable filter
                                         (DISPLAY_LINE_CLOCKS << LCD_IF_LAYER0_CONFIG_WIDTH_Pos) |         // layer line width
                                         (0   << LCD_IF_LAYER0_CONFIG_PREFETCH_EN_Pos) |   // prefetch enable
                                         (1   << LCD_IF_LAYER0_CONFIG_ACTIVE_Pos);         // layer is active

        hlcdc->Instance->LAYER0_TL_POS = (0  << LCD_IF_LAYER0_TL_POS_X0_Pos);
        hlcdc->Instance->LAYER0_BR_POS = ((DISPLAY_LINE_CLOCKS - 1) << LCD_IF_LAYER0_BR_POS_X1_Pos);
        // canvas area
        hwp_lcdc1->CANVAS_TL_POS = (0 << LCD_IF_CANVAS_TL_POS_X0_Pos);
        hwp_lcdc1->CANVAS_BR_POS = ((DISPLAY_LINE_CLOCKS - 1) << LCD_IF_CANVAS_BR_POS_X1_Pos);

        /* Initialize EPIC*/
        HAL_RCC_EnableModule(RCC_MOD_EPIC);

        hwp_epic->SETTING = (0 << EPIC_SETTING_EOF_IRQ_MASK_Pos) | (1 << EPIC_SETTING_AUTO_GATE_EN_Pos);

        hwp_epic->CANVAS_BG = EPIC_CANVAS_BG_BG_BLENDING_BYPASS;


#ifdef EPIC_INPUT_1BPP
        uint32_t epic_in_pixels = DISPLAY_LINE_CLOCKS >> 1;
#else
        uint32_t epic_in_pixels = DISPLAY_LINE_CLOCKS;
#endif

        // ROI area
        hwp_epic->CANVAS_TL_POS = (0  << EPIC_CANVAS_TL_POS_X0_Pos);
        hwp_epic->CANVAS_BR_POS = ((epic_in_pixels - 1) << EPIC_CANVAS_BR_POS_X1_Pos);

        // set up L0
        hwp_epic->L0_CFG =  EPIC_L0_CFG_FMT_L8 |     // L8
                            (0   << EPIC_L0_CFG_ALPHA_SEL_Pos) |        // use format alpha
                            (255 << EPIC_L0_CFG_ALPHA_Pos) |            // alpha value
                            (0   << EPIC_L0_CFG_FILTER_EN_Pos) |        // enable filter
                            ((epic_in_pixels - 1) << EPIC_L0_CFG_WIDTH_Pos) |            // source image width
                            (1   << EPIC_L0_CFG_PREFETCH_EN_Pos) |      // enable prefetch
                            (1   << EPIC_L0_CFG_ACTIVE_Pos);            // L0 is active

        hwp_epic->L0_TL_POS = (0  << EPIC_L0_TL_POS_X0_Pos) ;
        hwp_epic->L0_BR_POS = ((epic_in_pixels - 1) << EPIC_L0_BR_POS_X1_Pos);

        hwp_epic->L0_MISC_CFG = (0 << EPIC_L0_MISC_CFG_CLUT_SEL_Pos);

        hwp_epic->AHB_CTRL = EPIC_AHB_CTRL_O_FMT_RGB565;


    }

    void epd_start_frame(uint8_t frame)
    {
        uint32_t *p_LTab = (uint32_t *)(((uint32_t)(EPIC)) + 0x400 * 1);
        memcpy(p_LTab, &epic_LTabs[frame][0], 0x400);
    }

    static uint32_t wave_table_2bpp_pixels(uint8_t num, int frame)
    {
        uint32_t tmp;

        tmp = 0;
        tmp = wave_end[(num >> 6) & 0x3][frame];

        tmp = tmp << 2;
        tmp &= 0xfffc;
        tmp |= wave_end[(num >> 4) & 0x3][frame];

        tmp = tmp << 2;
        tmp &= 0xfffc;
        tmp |= wave_end[(num >> 2) & 0x3][frame];

        tmp = tmp << 2;
        tmp &= 0xfffc;
        tmp |= wave_end[(num) & 0x3][frame];

        return tmp;
    }

    static void epd_wave_table_epic_Ltab(void)
    {
        int frame, num;

        //wave_end_table
        for (frame = 0; frame < FRAME_END_LEN; frame++)
        {
            for (num = 0; num < 256; num++)
            {
#ifdef EPIC_INPUT_1BPP
                uint8_t bits_7_4 = OneBitToTwoBit[num & 0x0f]; //1字节里低4位的像素点
                uint8_t bits_3_0 = OneBitToTwoBit[num >> 4];   //1字节里高4位的像素点

                uint32_t v_7_4 = wave_table_2bpp_pixels(bits_7_4, frame);
                uint32_t v_3_0 = wave_table_2bpp_pixels(bits_3_0, frame);
                epic_LTabs[frame][num] = 0xFF000000 |
                                         ((v_7_4 << 16) & 0x00F80000) |
                                         ((v_7_4 << 13) & 0x0000FC00) | ((v_3_0 << 5) & 0x0000FC00) |
                                         ((v_3_0 << 3) & 0x000000F8);
#else
                uint32_t v = wave_table_2bpp_pixels(num, frame);
                epic_LTabs[frame][num] = 0xFF000000 |
                                         ((v << 16) & 0x00E00000) |
                                         ((v << 11) & 0x0000E000) |
                                         ((v << 6) & 0x000000C0);
#endif
            }
        }
    }


    void epd_load_and_send_pic(uint8_t *new_pic_a1, uint8_t frame)
    {

        uint32_t epic_ahb_out_addr = (uint32_t) &epic_ahb_out[cur_epic_ahb_out_idx][0];
        LCDC_HandleTypeDef *hlcdc = &lcdc_handler;

#ifdef EPIC_INPUT_1BPP
        //Wait previous EPIC done.
        while (hwp_epic->STATUS != 0x0) {;}
        hwp_epic->L0_SRC  = (uint32_t)new_pic_a1;
        hwp_epic->AHB_MEM = (uint32_t)epic_ahb_out_addr;
        hwp_epic->COMMAND = 0x1;
#else
        for (int j = 0, i = 0; i < DISPLAY_LINE_CLOCKS / 2; i++)
        {
            uint8_t ret = new_pic_a1[i];
            epic_input[j++] = OneBitToTwoBit[(ret >> 4)];
            epic_input[j++] = OneBitToTwoBit[(ret & 0x0f)];
        }

        //Wait previous EPIC done.
        while (hwp_epic->STATUS != 0x0) {;}
        hwp_epic->L0_SRC  = (uint32_t)epic_input;
        hwp_epic->AHB_MEM = (uint32_t)epic_ahb_out_addr;
        hwp_epic->COMMAND = 0x1;
#endif

        //Wait previous LCDC done.
        while (hwp_lcdc1->STATUS & LCD_IF_STATUS_LCD_BUSY) {;}

        EPD_CPV_L_hs();
        EPD_OE_L_hs();
        EPD_LE_H_hs();
        HAL_Delay_us(1);
        EPD_LE_L_hs();
        HAL_Delay_us(1);
        EPD_OE_H_hs();
        EPD_CPV_H_hs();


        hwp_lcdc1->LCD_WR = 0;
        hwp_lcdc1->LCD_SINGLE = LCD_IF_LCD_SINGLE_WR_TRIG;
        while (hwp_lcdc1->LCD_SINGLE & LCD_IF_LCD_SINGLE_LCD_BUSY) {;}

        hwp_lcdc1->LAYER0_SRC = (uint32_t)epic_ahb_out_addr;
        hwp_lcdc1->COMMAND = 0x1;

        cur_epic_ahb_out_idx = !cur_epic_ahb_out_idx;


#ifdef LCD_OUTPUT_TO_RAM
        while (hwp_lcdc1->STATUS & LCD_IF_STATUS_LCD_BUSY) {;}

        for (uint32_t i = 0; i < DISPLAY_LINE_CLOCKS; i++)
        {
            if (lcd_output[i] !=
        }
    int j = 0;
    for (int i = 0; i < DISPLAY_LINE_CLOCKS / 2; i++)
        {
            uint8_t ret;
            ret = reverse_bits(new_pic[i]);
            g_dest_data[j++] = wave_end_table[OneBitToTwoBit[(ret & 0x0f)]][frame]; //1字节里低4位的像素点
            g_dest_data[j++] = wave_end_table[OneBitToTwoBit[(ret >> 4)]][frame];   //1字节里高4位的像素点
        }
#endif /*LCD_OUTPUT_TO_RAM*/
    }



    void epd_display_pic(unsigned char *ptr)
    {
        unsigned char frame;
        unsigned int line;
        unsigned char *ptrnext;

        // __disable_irq();
        uint32_t start_tick = rt_tick_get();
        oedtps_source_gate_enable();
        HAL_Delay(50);
        oedtps_vcom_enable();
        HAL_Delay(10);



        // epd_DMA_init();

        EPD_GMODE_H_hs();
        for (frame = 0; frame < FRAME_END_LEN; frame++)
        {
            EPD_STV_H_hs();
            EPD_STV_L_hs();
            HAL_Delay_us(1);
            EPD_CPV_L_hs();    //DCLK跑1个时钟
            HAL_Delay_us(1);
            EPD_CPV_H_hs();
            HAL_Delay_us(1);
            EPD_STV_H_hs();
            HAL_Delay_us(1);
            EPD_CPV_L_hs();    //DCLK跑1个时钟
            HAL_Delay_us(1);
            EPD_CPV_H_hs();
            HAL_Delay_us(1);
            EPD_CPV_L_hs();    //DCLK跑1个时钟
            HAL_Delay_us(1);
            EPD_CPV_H_hs();
            HAL_Delay_us(1);
            EPD_CPV_L_hs();
            EPD_LE_H_hs();
            HAL_Delay_us(1);
            EPD_LE_L_hs();
            HAL_Delay_us(1);
            EPD_OE_H_hs();
            EPD_CPV_H_hs();

            epd_start_frame(frame);
            for (line = 0; line < DISPLAY_ROWS; line++)                 //共有DISPLAY_ROWS列数据
            {
                epd_load_and_send_pic(ptr + (line * DISPLAY_LINE_CLOCKS / 2), frame); //(line*DISPLAY_LINE_CLOCKS/2)传完一列数据后传下一列，一列数据有
            }
            epd_load_and_send_pic(ptr + ((line - 1)*DISPLAY_LINE_CLOCKS / 2), frame); //最后一行还需GATE CLK,故再传一行没用数据

            //Wait previous LCDC done.
            while (hwp_lcdc1->STATUS & LCD_IF_STATUS_LCD_BUSY) {;}

            EPD_CPV_L_hs();
            HAL_Delay_us(1);
            EPD_OE_L_hs();
        }
        EPD_GMODE_L_hs();
        EPD_LE_L_hs();
        EPD_CLK_L_hs();
        EPD_OE_L_hs();
        EPD_SPH_H_hs();
        EPD_STV_H_hs();
        EPD_CPV_L_hs();

        HAL_Delay(10);
        oedtps_vcom_disable();
        HAL_Delay(10);
        oedtps_source_gate_disable();

        rt_kprintf("Cost time=%d \r\n", rt_tick_get() - start_tick);
        // __enable_irq();
    }



#endif




    /************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

