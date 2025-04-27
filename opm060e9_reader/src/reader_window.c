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
#include "dfs_posix.h"


#define READER_PREV_PIN     (BSP_KEY1_PIN)
#define READER_SETTING_PIN  (BSP_KEY1_PIN)
#define READER_NEXT_PIN     (BSP_KEY2_PIN)
#define READER_HOME_PIN     (BSP_KEY2_PIN)

#define READ_BUFFER_SIZE  (4096)
#define FILE_READ_SIZE    (2880)

#if FILE_READ_SIZE > READ_BUFFER_SIZE
    #error "file_read_size must be less than read_buffer_size"
#endif

extern uint32_t   Page_Counter_EN_CH_Space;

static uint8_t read_buffer[READ_BUFFER_SIZE];

typedef struct
{
    char *file_path;
    int fid;
    int32_t curr_pos;
    int32_t next_pos;
    int32_t prev_pos;
    uint8_t font_size;
    enum Current_Font font;

    uint8_t h_space;
    uint8_t z_space;
    uint32_t file_size;
} reader_window_ctx_t;

static reader_window_ctx_t reader_wnd_ctx;

static void BSP_DispPage_EPD(uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t usRowL)
{
    uint32_t    TXT_SIZE;
    double      Reading_Percentage; // 阅读进度百分比计算结果
    char        Reading_Percentage_Save[20]; // 阅读进度最终显示结果
    bool        F_READ_END = false;

    char        BDE_pBuffer[50];
    uint8_t     file_name_length;
    reader_window_ctx_t *ctx = &reader_wnd_ctx;
    int rd_size;

    //----------------------------------------------------------------------------------------
    Paint_Clear(WHITE);
    if ((ctx->fid > 0) && (ctx->curr_pos < ctx->file_size))
    {
        lseek(ctx->fid, ctx->curr_pos, SEEK_SET);  // 移动到要读取的位置

        rt_kprintf("Now Reading Address = %d\r\n", ctx->curr_pos);

        memset((void *)read_buffer, 0, sizeof(read_buffer));
        rd_size = read(ctx->fid, read_buffer, FILE_READ_SIZE);  //从文件中取一页的数据
        SSD1677_Display_Page(usX, usY, f_size, myFont, wordspace, usRowL, (char *)read_buffer); //在屏幕上显示

        ctx->next_pos = ctx->curr_pos + Page_Counter_EN_CH_Space; // 记录位置变量更新
        if (ctx->next_pos >= ctx->file_size)  // 读取到了结尾
        {
            ctx->next_pos = ctx->file_size;
        }

        Display_down_handle();
    }
}


static void on_refresh(void)
{
    reader_window_ctx_t *ctx = &reader_wnd_ctx;
    BSP_DispPage_EPD(16, 30, ctx->font_size, ctx->font, ctx->z_space, ctx->h_space);
}

static void on_open(wnd_evt_open_t *evt_open)
{
    // window_send_evt_refresh();
}

static void on_close(void)
{

}

static void on_btn(wnd_evt_btn_t *evt_btn)
{
    // int rd_size;
    uint32_t prev_pos = -1;
    reader_window_ctx_t *ctx = &reader_wnd_ctx;
    rt_kprintf("%s, %d, key=%d, action=%d\r\n", __FILE__, __LINE__, evt_btn->pin, evt_btn->action);
    if (BSP_KEY1_PIN == evt_btn->pin)
    {
        if (ctx->next_pos == ctx->file_size)  // 读取到了结尾
        {

        }
        else
        {
            if (BUTTON_CLICKED == evt_btn->action) /* next page */
            {
                ctx->curr_pos = ctx->next_pos;
                window_send_evt_refresh();
            }
            else if (BUTTON_LONG_PRESSED == evt_btn->action) /* load setting window */
            {
                window_load(SETTING_WND_ID, NULL);
            }
        }
    }
    else if (BSP_KEY2_PIN == evt_btn->pin)
    {
        if (BUTTON_CLICKED == evt_btn->action) /* prev page */
        {
            if (ctx->curr_pos > 0)
            {
                // ctx->curr_pos -= sizeof(read_buffer);
                ctx->curr_pos = ctx->curr_pos - (ctx->next_pos - ctx->curr_pos); /* 假设当前页的长度和上一页相同 */
                if (ctx->curr_pos < 0)
                {
                    ctx->curr_pos = 0;
                }

                window_send_evt_refresh();
            }

        }
        else if (BUTTON_LONG_PRESSED == evt_btn->action) /* load dir window */
        {
            window_load(DIR_WND_ID, NULL);
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

void reader_set_file_path(const char *file_path)
{
    reader_window_ctx_t *ctx = &reader_wnd_ctx;
    struct stat buf;

    if (ctx->fid > 0)
    {
        close(ctx->fid);
    }
    ctx->fid = open((char *)file_path, O_RDONLY, 0777);
    ctx->curr_pos = 0;
    ctx->next_pos = -1;
    ctx->file_size = 0;

    if (ctx->fid > 0)
    {
        if (RT_EOK == fstat(ctx->fid, &buf))
        {
            ctx->file_size = buf.st_size;
        }
    }

}

void reader_set_font(enum Current_Font font, uint8_t font_size)
{
    reader_window_ctx_t *ctx = &reader_wnd_ctx;

    ctx->font_size = font_size;
    ctx->font = font;
}

void reader_wnd_init(void)
{
    reader_wnd_ctx.font_size = 24;
    reader_wnd_ctx.font = SongTi;
    reader_wnd_ctx.h_space = 10;
    reader_wnd_ctx.z_space = 0;

    window_register_evt_handler(READER_WND_ID, evt_handler);
}
