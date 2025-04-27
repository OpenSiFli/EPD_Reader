/*
  ******************************************************************************
  * @file   window_msg.h
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
#include "button.h"

typedef enum
{
    WINDOW_EVT_REFRESH,
    WINDOW_EVT_BUTTON,
    WINDOW_EVT_CLOSE,
    WINDOW_EVT_OPEN,
} wnd_evt_id_t;

typedef struct
{
    uint8_t pin;
    button_action_t action;
} wnd_evt_btn_t;

typedef struct
{
    void *arg;
} wnd_evt_open_t;

typedef struct
{
    wnd_evt_id_t id;
} wnd_evt_head_t;

typedef struct
{
    wnd_evt_head_t head;
    uint8_t body[0];
} wnd_evt_t;

typedef void (*wnd_evt_handler_t)(wnd_evt_t *evt);

void *window_create_evt(wnd_evt_id_t id, void *arg, uint32_t size);
void window_send_evt(wnd_evt_t *evt);
void window_send_evt_refresh(void);
void window_send_evt_open(void *arg);
void window_send_evt_close(void);

void window_load(uint8_t wnd_id, void *arg);
void window_loop(void);
void window_register_evt_handler(uint8_t wnd_id, wnd_evt_handler_t handler);
void window_mng_init(void);

void reader_set_font(enum Current_Font font, uint8_t font_size);
void reader_set_file_path(const char *file_path);
void reader_wnd_init(void);
void dir_wnd_init(void);
void setting_wnd_init(void);