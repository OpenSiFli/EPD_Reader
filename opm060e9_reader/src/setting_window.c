/*
  ******************************************************************************
  * @file   reader_window.c
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

#include "bsp_board.h"
#include "bsp_e-ink.h"
#include "elm.h"
// #include "opm060e9_driver.h"
#include "window_mng.h"
#include "window_def.h"


typedef struct
{
    const char *name;
    enum Current_Font id;
    uint8_t size;
} font_desc_t;

static const font_desc_t font_db[] =
{
    {
        .name = "H24_SongTi",
        .id = SongTi,
        .size = 24,
    },
    {
        .name = "H32_KaiTi",
        .id = KaiTi,
        .size = 32,
    }
};

static uint8_t curr_font;

static void display_font(uint8_t myFont)
{
    uint16_t  i, j, k;
    uint16_t  usY = 0;
    bool      DD_Icon_Clear = false;

    char      Dir_Info_dis[56]; // 提示信息栏最后要显示的内容
    uint8_t   Dir_Info_Len;

    uint32_t  D_TXT_SIZE = 0;
    double    SIZE_RESULT = 0;
    char      SIZE_DIS[20];
    char      DH_dir_list[42];
    uint32_t font_num;

    // ------------------------------------------------------
//  printf("\r\nDSH_head=%d - DSH_trail=%d\r\n", DSH_head, DSH_trail);

    Paint_Clear(WHITE);

    font_num = sizeof(font_db) / sizeof(font_db[0]);
    // 显示文件名以及文件大小
    for (i = 0; i < font_num; i++)
    {
        SSD1677_DispString_EN_CH(32, 40 + usY, BLACK, 24, myFont, 0, (char *)font_db[i].name); // 显示文件名
        usY += 32;
    }
    // 图标移动
    for (i = curr_font * 32; i < (curr_font + 1) * 32; i += 32)
    {
        SSD1677_DispString_EN_CH(8, 40 + i, BLACK, 24, myFont, 0, "->");
        DrawArcRect(6, 38 + i, EPD_WIDTH - 20, 49 + 16 + i, BLACK, DRAW_FILL_EMPTY, 4);
    }
    Display_down_handle();
}



static void on_refresh(void)
{
    display_font(SongTi);
}

static void on_open(wnd_evt_open_t *evt_open)
{

}

static void on_close(void)
{

}

static void on_btn(wnd_evt_btn_t *evt_btn)
{
    uint32_t font_num;
    rt_kprintf("%s, %d, key=%d, action=%d\r\n", __FILE__, __LINE__, evt_btn->pin, evt_btn->action);

    font_num = sizeof(font_db) / sizeof(font_db[0]);

    if (BSP_KEY1_PIN == evt_btn->pin)
    {
        if (BUTTON_CLICKED == evt_btn->action) /* next font */
        {
            if ((curr_font + 1) >= font_num)
            {
                curr_font = 0;
            }
            else
            {
                curr_font++;
            }
            window_send_evt_refresh();
        }
        else if (BUTTON_LONG_PRESSED == evt_btn->action) /* select the font */
        {
            reader_set_font(font_db[curr_font].id, font_db[curr_font].size);
            window_load(READER_WND_ID, NULL);
        }
    }
    else if (BSP_KEY2_PIN == evt_btn->pin)
    {
        if (BUTTON_CLICKED == evt_btn->action) /* prev font */
        {
            if (0 == curr_font)
            {
                curr_font = font_num - 1;
            }
            else
            {
                curr_font--;
            }
            window_send_evt_refresh();
        }
    }
}

static void evt_handler(wnd_evt_t *evt)
{
    switch (evt->head.id)
    {
    case WINDOW_EVT_REFRESH:
    {
        on_refresh();
        break;
    }
    case WINDOW_EVT_BUTTON:
    {
        on_btn((wnd_evt_btn_t *)&evt->body[0]);
        break;
    }
    case WINDOW_EVT_OPEN:
    {
        on_open((wnd_evt_open_t *)&evt->body[0]);
        break;
    }
    case WINDOW_EVT_CLOSE:
    {
        on_close();
        break;
    }
    }
}

void setting_wnd_init(void)
{
    window_register_evt_handler(SETTING_WND_ID, evt_handler);
}

