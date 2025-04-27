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

#define WND_INVALID_ID   (UINT8_MAX)
#define WND_MAX_NUM      (5)

static rt_mailbox_t wnd_mng_mb;
static uint8_t wnd_curr_id = WND_INVALID_ID;
static wnd_evt_handler_t wnd_evt_handler_tbl[WND_MAX_NUM];


static void handle_evt(wnd_evt_t *evt)
{
    if ((wnd_curr_id < WND_INVALID_ID)
            && (wnd_evt_handler_tbl[wnd_curr_id]))
    {
        wnd_evt_handler_tbl[wnd_curr_id](evt);
    }
    rt_free(evt);
}


void window_send_evt(wnd_evt_t *evt)
{
    rt_err_t err;

    err = rt_mb_send(wnd_mng_mb, (rt_uint32_t)evt);
    RT_ASSERT(RT_EOK == err);
}

void window_send_evt_sync(wnd_evt_t *evt)
{
    handle_evt(evt);
}

void *window_create_evt(wnd_evt_id_t id, void *arg, uint32_t size)
{
    wnd_evt_t *evt;

    evt = rt_malloc(sizeof(wnd_evt_t) + size);
    RT_ASSERT(evt);

    evt->head.id = id;
    if (arg && (size > 0))
    {
        memcpy((void *)evt->body, arg, size);
    }

    return evt;
}


void window_send_evt_refresh(void)
{
    wnd_evt_t *evt;

    evt = window_create_evt(WINDOW_EVT_REFRESH, NULL, 0);
    window_send_evt(evt);
}

void window_send_evt_open(void *arg)
{
    wnd_evt_t *evt;
    wnd_evt_open_t evt_open;

    evt_open.arg = arg;
    evt = window_create_evt(WINDOW_EVT_OPEN, &evt_open, sizeof(evt_open));
    window_send_evt_sync(evt);
}

void window_send_evt_close(void)
{
    wnd_evt_t *evt;

    evt = window_create_evt(WINDOW_EVT_CLOSE, NULL, 0);
    window_send_evt_sync(evt);
}

void window_load(uint8_t wnd_id, void *arg)
{
    if (wnd_id != wnd_curr_id)
    {
        if (wnd_curr_id != WND_INVALID_ID)
        {
            window_send_evt_close();
        }
        wnd_curr_id = wnd_id;
        window_send_evt_open(arg);
        window_send_evt_refresh();
    }
}

void window_loop(void)
{
    wnd_evt_t *evt;
    rt_err_t err;

    while (1)
    {
        err = rt_mb_recv(wnd_mng_mb, (rt_uint32_t *)&evt, RT_WAITING_FOREVER);
        RT_ASSERT(RT_EOK == err);

        handle_evt(evt);
    }
}

void window_register_evt_handler(uint8_t wnd_id, wnd_evt_handler_t handler)
{
    RT_ASSERT(wnd_id < WND_MAX_NUM);
    RT_ASSERT(!wnd_evt_handler_tbl[wnd_id]);

    wnd_evt_handler_tbl[wnd_id] = handler;
}

void window_mng_init(void)
{
    wnd_mng_mb = rt_mb_create("wnd", 20, RT_IPC_FLAG_FIFO);
    RT_ASSERT(wnd_mng_mb);

}