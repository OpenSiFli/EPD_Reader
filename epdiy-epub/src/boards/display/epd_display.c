/**
  ******************************************************************************
  * @file   epd_display.c
  * @author Sifli software development team
  * @brief   This file includes the LCD driver for epd_display LCD.
  * @attention
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
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
#include "rtthread.h"
#include "string.h"
#include "board.h"
#include "drv_lcd.h"
#include "drv_epic.h"
#include "epd_pin_defs.h"
#include "epd_configs.h"
#include "epd_tps.h"
#include "mem_section.h"


#define  DBG_LEVEL            DBG_INFO  //DBG_LOG //

#define LOG_TAG                "epd_display"
#include "log.h"

#define LCD_ID                  0x85


#define DISPLAY_LINE_CLOCKS   (LCD_HOR_RES_MAX/4)     //每列刷新所需次数，362*4像素
#define DISPLAY_ROWS   LCD_VER_RES_MAX
#define DISPLAY_LINE_CLOCKS_ALIGNED  ((DISPLAY_LINE_CLOCKS + 3) & ~0x03) // align to 4 bytes


static LCDC_InitTypeDef lcdc_int_cfg =
{
    .lcd_itf = LCDC_INTF_DBI_8BIT_B,
    .freq = 0, //Overridden by epd_get_clk_freq()
    .color_mode = LCDC_PIXEL_FORMAT_RGB332,

    .cfg = {
        .dbi = {
            .syn_mode = HAL_LCDC_SYNC_DISABLE,
            .vsyn_polarity = 0,
            .vsyn_delay_us = 0,
            .hsyn_num = 0,
        },
    },
};

static uint8_t  lcdc_input_idx = 0;
ALIGN(4) static uint8_t  lcdc_input_buffer[2][DISPLAY_LINE_CLOCKS_ALIGNED];
static uint32_t wait_lcd_ticks;

static uint16_t epic_out_buffer_idx = 0;
static uint16_t epic_out_buffer[2][LCD_HOR_RES_MAX];
static uint32_t lut_copy_ticks;
/*
Define a mixed grey framebuffer on PSRAM
high 4 bits for old pixel and low 4 bits for new pixel in every byte.
*/
L2_NON_RET_BSS_SECT_BEGIN(frambuf)
L2_NON_RET_BSS_SECT(frambuf, ALIGN(4) static uint8_t mixed_framebuffer[LCD_HOR_RES_MAX * LCD_VER_RES_MAX];);
L2_NON_RET_BSS_SECT_END

/**
  * @brief  Power on the LCD.
  * @param  None
  * @retval None
  */
static void LCD_Init(LCDC_HandleTypeDef *hlcdc)
{
    uint8_t   parameter[14];

    lcdc_int_cfg.freq = epd_get_clk_freq();
    /* Initialize LCD low level bus layer ----------------------------------*/
    memcpy(&hlcdc->Init, &lcdc_int_cfg, sizeof(LCDC_InitTypeDef));
    HAL_LCDC_Init(hlcdc);

    //Initialize power supply chip
    oedtps_init(epd_get_vcom_voltage());


    hlcdc->Instance->LAYER0_CONFIG = (4   << LCD_IF_LAYER0_CONFIG_FORMAT_Pos) |       //RGB332
                                     (1   << LCD_IF_LAYER0_CONFIG_ALPHA_SEL_Pos) |     // use layer alpha
                                     (255 << LCD_IF_LAYER0_CONFIG_ALPHA_Pos) |         // layer alpha value is 255
                                     (0   << LCD_IF_LAYER0_CONFIG_FILTER_EN_Pos) |     // disable filter
                                     (DISPLAY_LINE_CLOCKS << LCD_IF_LAYER0_CONFIG_WIDTH_Pos) |         // layer line width
                                     (0   << LCD_IF_LAYER0_CONFIG_PREFETCH_EN_Pos) |   // prefetch enable
                                     (1   << LCD_IF_LAYER0_CONFIG_ACTIVE_Pos);         // layer is active

    hlcdc->Instance->LAYER0_TL_POS = (0  << LCD_IF_LAYER0_TL_POS_X0_Pos);
    hlcdc->Instance->LAYER0_BR_POS = ((DISPLAY_LINE_CLOCKS - 1) << LCD_IF_LAYER0_BR_POS_X1_Pos);
    // canvas area
    hlcdc->Instance->CANVAS_TL_POS = (0 << LCD_IF_CANVAS_TL_POS_X0_Pos);
    hlcdc->Instance->CANVAS_BR_POS = ((DISPLAY_LINE_CLOCKS - 1) << LCD_IF_CANVAS_BR_POS_X1_Pos);

    hlcdc->Instance->LCD_WR = 0;

    oedtps_vcom_disable();
    oedtps_source_gate_disable();

    EPD_LE_L_hs();
    EPD_CLK_L_hs();
    EPD_OE_L_hs();
    EPD_SPH_H_hs();
    EPD_STV_H_hs();
    EPD_CPV_L_hs();
    EPD_GMODE_H_hs();



    epd_wave_table();

}

/**
  * @brief  Disables the Display.
  * @param  None
  * @retval LCD Register Value.
  */
static uint32_t LCD_ReadID(LCDC_HandleTypeDef *hlcdc)
{
    return LCD_ID;
}

/**
  * @brief  Enables the Display.
  * @param  None
  * @retval None
  */
static void LCD_DisplayOn(LCDC_HandleTypeDef *hlcdc)
{
    /* Display On */
}

/**
  * @brief  Disables the Display.
  * @param  None
  * @retval None
  */
static void LCD_DisplayOff(LCDC_HandleTypeDef *hlcdc)
{
    /* Display Off */
}

static void LCD_SetRegion(LCDC_HandleTypeDef *hlcdc, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1)
{

}


static void lock_epic(void)
{
    EPIC_LayerConfigTypeDef input_layer;
    EPIC_LayerConfigTypeDef output_canvas;
    drv_gpu_open();

    //Configure layer registers by blending a dummy line.
    HAL_EPIC_LayerConfigInit(&input_layer);
    input_layer.data = (uint8_t *)&mixed_framebuffer[0];
    input_layer.color_mode = EPIC_INPUT_L8;
    input_layer.total_width = LCD_HOR_RES_MAX >> 1;
    input_layer.height = 2;
    input_layer.width = input_layer.total_width;
    input_layer.lookup_table = (uint8_t *)&epic_out_buffer[0]; //Any LUT for dummy blending

    HAL_EPIC_LayerConfigInit(&output_canvas);
    output_canvas.data = (uint8_t *)&epic_out_buffer[0];
    output_canvas.color_mode = EPIC_OUTPUT_RGB565;
    output_canvas.width = input_layer.width;
    output_canvas.total_width = input_layer.total_width;
    output_canvas.height = input_layer.height;

    //Flush cache
    mpu_dcache_clean(&mixed_framebuffer[0], sizeof(mixed_framebuffer));
    drv_gpu_take(500);

    RT_ASSERT(HAL_OK == HAL_EPIC_BlendStartEx(drv_get_epic_handle(), &input_layer, 1, &output_canvas));
}

static void unlock_epic(void)
{
    drv_gpu_release();
}


static int prepare_epic_lut(uint8_t frame)
{
    HAL_RCC_EnableModule(RCC_MOD_EPIC);
    hwp_epic->L0_CFG |= (1 << EPIC_L0_CFG_ACTIVE_Pos) | (1 << EPIC_L0_CFG_ALPHA_SEL_Pos);
    hwp_epic->L0_CFG &= ~(1 << EPIC_L0_CFG_ALPHA_BLEND_Pos);

    uint32_t start_tick = HAL_DBG_DWT_GetCycles();
    epd_wave_table_fill_lut((uint32_t *)drv_get_epic_handle()->LTab[0], frame);
    lut_copy_ticks += HAL_GetElapsedTick(start_tick, HAL_DBG_DWT_GetCycles());
    return 0;
}


static uint16_t *mixed_gray_to_epic_out(unsigned char *gray_buffer, uint32_t gray_buffer_size)
{
    uint16_t *p_epic_out = &epic_out_buffer[epic_out_buffer_idx][0];
    RT_ASSERT(gray_buffer_size <= sizeof(epic_out_buffer[0]));
    while (hwp_epic->STATUS != 0); //Wait for EPIC idle
    hwp_epic->AHB_MEM = (uint32_t)p_epic_out;
    hwp_epic->L0_SRC = (uint32_t)gray_buffer;
    hwp_epic->COMMAND = 1;

    epic_out_buffer_idx = !epic_out_buffer_idx;

    return p_epic_out;
}

static int epic_buf_to_wave_form_buffer(uint16_t *p_epic_out_buffer, uint32_t *wfm_buffer, uint32_t epic_out_buffer_len)
{
    int index = 0;

    for (int i = 0; i < epic_out_buffer_len; i += 16)
    {
        int out_v = 0;

        out_v |= p_epic_out_buffer[0] << 6;
        out_v |= p_epic_out_buffer[1] << 4;
        out_v |= p_epic_out_buffer[2] << 2;
        out_v |= p_epic_out_buffer[3] << 0;

        out_v |= p_epic_out_buffer[4] << 14;
        out_v |= p_epic_out_buffer[5] << 12;
        out_v |= p_epic_out_buffer[6] << 10;
        out_v |= p_epic_out_buffer[7] << 8;

        out_v |= p_epic_out_buffer[8] << 22;
        out_v |= p_epic_out_buffer[9] << 20;
        out_v |= p_epic_out_buffer[10] << 18;
        out_v |= p_epic_out_buffer[11] << 16;

        out_v |= p_epic_out_buffer[12] << 30;
        out_v |= p_epic_out_buffer[13] << 28;
        out_v |= p_epic_out_buffer[14] << 26;
        out_v |= p_epic_out_buffer[15] << 24;

        wfm_buffer[index++] = out_v;
        p_epic_out_buffer += 16;
    }

    return 0;
}

L1_RET_CODE_SECT(epd_codes, void epd_load_and_send_pic(LCDC_HandleTypeDef *hlcdc, const uint16_t *epic_buf, uint32_t epic_buf_len))
{

    uint8_t *p_lcdc_input = (uint8_t *) &lcdc_input_buffer[lcdc_input_idx][0];

    epic_buf_to_wave_form_buffer((uint16_t *)epic_buf, (uint32_t *)p_lcdc_input, epic_buf_len);

    //Wait previous LCDC done.
    uint32_t start_tick = HAL_DBG_DWT_GetCycles();
    while (hlcdc->Instance->STATUS & LCD_IF_STATUS_LCD_BUSY) {;}
    wait_lcd_ticks += HAL_GetElapsedTick(start_tick, HAL_DBG_DWT_GetCycles());

    EPD_CPV_L_hs();
    EPD_OE_L_hs();
    EPD_LE_H_hs();
    HAL_Delay_us(1);
    EPD_LE_L_hs();
    HAL_Delay_us(1);
    EPD_OE_H_hs();
    EPD_CPV_H_hs();


    hlcdc->Instance->LCD_SINGLE = LCD_IF_LCD_SINGLE_WR_TRIG;
    while (hlcdc->Instance->LCD_SINGLE & LCD_IF_LCD_SINGLE_LCD_BUSY) {;}

    hlcdc->Instance->LAYER0_SRC = (uint32_t)p_lcdc_input;
    hlcdc->Instance->COMMAND = 0x1;

    lcdc_input_idx = !lcdc_input_idx;
}

L1_RET_CODE_SECT(epd_codes, static void CopyToMixedGrayBuffer(LCDC_HandleTypeDef *hlcdc, const uint8_t *RGBCode, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1))
{
    uint32_t total_pixels = LCD_HOR_RES_MAX * LCD_VER_RES_MAX;
    RT_ASSERT((total_pixels % 4) == 0); // 必须是4像素的倍数

    //Convert layer data to 4bit gray data
    if (hlcdc->Layer[HAL_LCDC_LAYER_DEFAULT].data_format == LCDC_PIXEL_FORMAT_MONO)
    {
        RT_ASSERT(0);//Not implemented yet
    }
    else if (hlcdc->Layer[HAL_LCDC_LAYER_DEFAULT].data_format == LCDC_PIXEL_FORMAT_A4)
    {
        uint32_t n = total_pixels / 4; // 每次处理4像素（4字节）
        uint32_t *p_dst = (uint32_t *)mixed_framebuffer;
        const uint8_t *p_src = RGBCode;

        while (n--)
        {
            uint8_t byte0 = *p_src++;
            uint8_t byte1 = *p_src++;

            // 生成4像素的新值
            uint32_t src_v = ((byte1 << 20) | (byte1 << 16) | (byte0 << 4) | byte0) & 0x0F0F0F0F;

            // 读取原像素，旧像素清零，新像素移入老像素
            uint32_t dst_v = (*p_dst & 0x0F0F0F0F) << 4;

            // 合并新像素
            *p_dst++ = dst_v | src_v;
        }
    }
    else if (hlcdc->Layer[HAL_LCDC_LAYER_DEFAULT].data_format == LCDC_PIXEL_FORMAT_RGB565)
    {
        uint32_t n = total_pixels / 4; // 每次处理4像素（4字节）
        uint32_t *p_dst = (uint32_t *)mixed_framebuffer;
        const uint16_t *p_src = (const uint16_t *)RGBCode;

        // 计算灰度值
        // 0.299*R + 0.587*G + 0.114*B
#define RGB565_TO_GRAY4(rgb)  ( \
        (uint8_t)(( \
        ((((rgb) >> 8) & 0xF8) * 77 + \
         (((rgb) >> 3) & 0xFC) * 150 + \
         (((rgb) << 3) & 0xF8) * 29) >> 8) >> 4) \
        )

        while (n--)
        {
            uint8_t pixel0 = RGB565_TO_GRAY4(*p_src);
            p_src++;
            uint8_t pixel1 = RGB565_TO_GRAY4(*p_src);
            p_src++;
            uint8_t pixel2 = RGB565_TO_GRAY4(*p_src);
            p_src++;
            uint8_t pixel3 = RGB565_TO_GRAY4(*p_src);
            p_src++;


            // 生成4像素的新值
            uint32_t src_v = ((pixel3 << 24) | (pixel2 << 16) | (pixel1 << 8) | pixel0) & 0x0F0F0F0F;

            // 读取原像素，旧像素清零，新像素移入老像素
            uint32_t dst_v = (*p_dst & 0x0F0F0F0F) << 4;

            // 合并新像素
            *p_dst++ = dst_v | src_v;
        }
    }
    else
        RT_ASSERT(0);
}



L1_RET_CODE_SECT(epd_codes, static void LCD_WriteMultiplePixels(LCDC_HandleTypeDef *hlcdc, const uint8_t *RGBCode, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1))
{
    uint32_t line, line_bytes;
    //波形帧数量，用于局刷和全刷控制
    unsigned int frame_times = 0;



    uint32_t start_tick = rt_tick_get();
    oedtps_source_gate_enable();
    LCD_DRIVER_DELAY_MS(50);
    oedtps_vcom_enable();
    LCD_DRIVER_DELAY_MS(10);

    LOG_I("LCD_WriteMultiplePixels ColorMode=%d", hlcdc->Layer[HAL_LCDC_LAYER_DEFAULT].data_format);


    uint8_t temperature = 26;


    frame_times = epd_wave_table_get_frames(temperature, EPD_DRAW_MODE_AUTO);

    CopyToMixedGrayBuffer(hlcdc, RGBCode, Xpos0, Ypos0, Xpos1, Ypos1);
    LOG_I("Convert layer data take=%d(ms) \r\n", rt_tick_get() - start_tick);

    lock_epic();

    line_bytes = LCD_HOR_RES_MAX;
    wait_lcd_ticks = 0;
    lut_copy_ticks = 0;
    EPD_GMODE_H_hs();
    for (uint32_t frame = 0; frame < frame_times; frame++)
    {
        prepare_epic_lut(frame);
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

        uint16_t *cur_line_epic_out = NULL;
        uint16_t *next_line_epic_out = NULL;
        for (line = 0; line < DISPLAY_ROWS; line++)                 //共有DISPLAY_ROWS列数据
        {
            if (NULL == next_line_epic_out)
                cur_line_epic_out = mixed_gray_to_epic_out(&mixed_framebuffer[line * line_bytes], LCD_HOR_RES_MAX);
            else
            {
                cur_line_epic_out = next_line_epic_out; //如果下一行已经转换过了，就复用它
                next_line_epic_out = NULL; //复用后就清空下一行
            }

            if (line < DISPLAY_ROWS - 1)
                next_line_epic_out = mixed_gray_to_epic_out(&mixed_framebuffer[(line + 1) * line_bytes], LCD_HOR_RES_MAX);
            else
                next_line_epic_out = NULL; //最后一行没有下一行了


            epd_load_and_send_pic(hlcdc, cur_line_epic_out, LCD_HOR_RES_MAX); //传完一列数据后传下一列，一列数据有
        }
        epd_load_and_send_pic(hlcdc, cur_line_epic_out, LCD_HOR_RES_MAX); //最后一行还需GATE CLK,故再传一行没用数据
        while (hlcdc->Instance->STATUS & LCD_IF_STATUS_LCD_BUSY) {;}


        EPD_CPV_L_hs();
        HAL_Delay_us(1);
        EPD_OE_L_hs();
    }
    unlock_epic();
    EPD_GMODE_L_hs();
    EPD_LE_L_hs();
    EPD_CLK_L_hs();
    EPD_OE_L_hs();
    EPD_SPH_H_hs();
    EPD_STV_H_hs();
    EPD_CPV_L_hs();

    LCD_DRIVER_DELAY_MS(10);
    oedtps_vcom_disable();
    LCD_DRIVER_DELAY_MS(10);
    oedtps_source_gate_disable();
    LOG_I("Total %d frames, take time=%dms wait_lcd=%d(us), lut_copy=%d(us)\r\n", frame_times,
          rt_tick_get() - start_tick, wait_lcd_ticks / 240,
          lut_copy_ticks / 240);


    /* Simulate LCDC IRQ handler, call user callback */
    if (hlcdc->XferCpltCallback)
    {
        hlcdc->XferCpltCallback(hlcdc);
    }

    HAL_LCDC_SendLayerDataCpltCbk(hlcdc);
}


static void LCD_SetColorMode(LCDC_HandleTypeDef *hlcdc, uint16_t color_mode)
{

}

static void LCD_SetBrightness(LCDC_HandleTypeDef *hlcdc, uint8_t br)
{
    rt_device_t device = rt_device_find("lcdlight");
    if (device)
    {
        rt_err_t err = rt_device_open(device, RT_DEVICE_OFLAG_RDWR);
        uint8_t val = br;
        rt_device_write(device, 0, &val, 1);
        rt_device_close(device);
    }
    else
    {
        LOG_E("Can't find device lcdlight!");
    }
}


static const LCD_DrvOpsDef LCD_drv =
{
    LCD_Init,
    LCD_ReadID,
    LCD_DisplayOn,
    LCD_DisplayOff,

    LCD_SetRegion,
    NULL,
    LCD_WriteMultiplePixels,

    NULL,

    LCD_SetColorMode,
    LCD_SetBrightness,
    NULL,
    NULL
};

LCD_DRIVER_EXPORT2(epd_display, LCD_ID, &lcdc_int_cfg, &LCD_drv, 1);
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
