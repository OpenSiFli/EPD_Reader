/*
  ******************************************************************************
  * @file   dir_window.c
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
#include "dfs_file.h"

#define BOOK_NAME_LIST_MAX  (23)
#define BOOK_NAME_MAX       (80)

typedef struct
{
    uint8_t curr_book;
    uint8_t book_num;
    char book_name_list[BOOK_NAME_LIST_MAX][BOOK_NAME_MAX];
} dir_wnd_ctx_t;

static dir_wnd_ctx_t dir_wnd_ctx;

static struct dfs_fd fd;
static struct dirent dirent;
static void read_dir(const char *pathname)
{
    dir_wnd_ctx_t *ctx = &dir_wnd_ctx;
    struct stat stat;
    int length;
    char *fullpath, *path;

    fullpath = NULL;
    if (pathname == NULL)
    {
        path = rt_strdup("/");
        RT_ASSERT(path);
    }
    else
    {
        path = (char *)pathname;
    }

    ctx->book_num = 0;
    ctx->curr_book = 0;

    /* list directory */
    if (dfs_file_open(&fd, path, O_DIRECTORY) == 0)
    {
        rt_kprintf("Directory %s:\n", path);
        do
        {
            memset(&dirent, 0, sizeof(struct dirent));
            length = dfs_file_getdents(&fd, &dirent, sizeof(struct dirent));
            if (length > 0)
            {
                memset(&stat, 0, sizeof(struct stat));

                /* build full path for each file */
                fullpath = dfs_normalize_path(path, dirent.d_name);
                if (fullpath == NULL)
                    break;

                if (dfs_file_stat(fullpath, &stat) == 0)
                {
                    if (!S_ISDIR(stat.st_mode))
                    {
                        if (strstr(dirent.d_name, ".txt"))
                        {
                            strncpy(ctx->book_name_list[ctx->book_num], dirent.d_name, BOOK_NAME_MAX - 1);
                            ctx->book_name_list[ctx->book_num][BOOK_NAME_MAX - 1] = 0;
                            ctx->book_num++;


                            rt_kprintf("1\n");

                            rt_kprintf("%s\n", dirent.d_name);

                        }
                    }
                }
                rt_free(fullpath);
            }
        }
        while (length > 0);

        dfs_file_close(&fd);
    }
    else
    {
        rt_kprintf("No such directory\n");
    }
    if (pathname == NULL)
        rt_free(path);
}

static void display_dir(uint8_t myFont)
{
    uint16_t  i, j, k;
    uint16_t  usY = 0;
    dir_wnd_ctx_t *ctx = &dir_wnd_ctx;
    // ------------------------------------------------------
    // rt_kprintf("\r\nDSH_head=%d - DSH_trail=%d\r\n", DSH_head, DSH_trail);



    Paint_Clear(WHITE);


    if (1)
    {
        for (i = 0; i < ctx->book_num; i++)
        {
            SSD1677_DispString_EN_CH(32, 40 + usY, BLACK, 24, myFont, 0, ctx->book_name_list[i]); // ï¿½ï¿½Ê¾ï¿½Ä¼ï¿½ï¿½ï¿½
            usY += 32;
        }
        for (i = ctx->curr_book * 32; i < (ctx->curr_book + 1) * 32; i += 32)
        {
            SSD1677_DispString_EN_CH(8, 40 + i, BLACK, 24, myFont, 0, "->");
            DrawArcRect(6, 38 + i, EPD_WIDTH - 20, 49 + 16 + i, BLACK, DRAW_FILL_EMPTY, 4);
        }
    }

    Display_down_handle();
}



static void on_refresh(void)
{
    display_dir(SongTi);
}

static void on_open(wnd_evt_open_t *evt_open)
{

}

static void on_close(void)
{

}

static void on_btn(wnd_evt_btn_t *evt_btn)
{
    dir_wnd_ctx_t *ctx = &dir_wnd_ctx;

    rt_kprintf("%s, %d, key=%d, action=%d\r\n", __FILE__, __LINE__, evt_btn->pin, evt_btn->action);
    if (BSP_KEY1_PIN == evt_btn->pin)
    {
        if (BUTTON_CLICKED == evt_btn->action) /* next book */
        {
            if ((ctx->curr_book + 1) >= ctx->book_num)
            {
                ctx->curr_book = 0;
            }
            else
            {
                ctx->curr_book++;
            }
            window_send_evt_refresh();
        }
        else if (BUTTON_LONG_PRESSED == evt_btn->action) /* select the book */
        {
            reader_set_file_path(ctx->book_name_list[ctx->curr_book]);
            window_load(READER_WND_ID, NULL);
        }
    }
    else if (BSP_KEY2_PIN == evt_btn->pin)
    {
        if (BUTTON_CLICKED == evt_btn->action) /* prev book */
        {
            if (0 == ctx->curr_book)
            {
                ctx->curr_book = ctx->book_num - 1;
            }
            else
            {
                ctx->curr_book--;
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

void dir_wnd_init(void)
{
    read_dir("/");

    window_register_evt_handler(DIR_WND_ID, evt_handler);
}

