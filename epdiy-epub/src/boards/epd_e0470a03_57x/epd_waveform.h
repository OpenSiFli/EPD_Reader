/*
 * SPDX-FileCopyrightText: 2026 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EPD_WAVEFORM_H__
#define __EPD_WAVEFORM_H__

typedef enum
{
    EPD_DRAW_MODE_INVALID = 0,
    EPD_DRAW_MODE_AUTO = 1,
    EPD_DRAW_MODE_FULL = 2,
    EPD_DRAW_MODE_PARTIAL = 3,
} EpdDrawMode;

void epd_wave_table(void);//初始化wave table

uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode);

void epd_wave_table_fill_lut(uint32_t *p_argb8888_lut, uint32_t frame_num);

/* 手动设置 WAVE_MODE (0~9)，直接传给 waveform_bin_reader_get_frames。设置为 -1 表示使用 AUTO 模式 */
void epd_wave_set_mode(int mode);

/* 获取当前波形模式 */
int epd_wave_get_mode(void);

/* 手动设置温度区间 (0~13)，设置为 -1 表示自动遍历 */
void epd_wave_set_tempzone(int zone);

/* 获取当前温度区间索引 */
int epd_wave_get_tempzone(void);

#endif /* __EPD_WAVEFORM_H__ */