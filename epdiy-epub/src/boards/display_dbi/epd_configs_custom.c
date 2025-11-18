#include <rtthread.h>
#include "epd_configs.h"
#include "mem_section.h"
#ifdef LCD_USING_EPD_CUSTOM
#include "mem_map.h"
#include "epd_waveform_bin_reader.h"
#ifndef CUSTOM_EPD_WAVE_TABLE_START_ADDR
#error "CUSTOM_EPD_WAVE_TABLE_START_ADDR is not defined!!!"
#endif




#define PART_DISP_TIMES       10        // After PART_DISP_TIMES-1 partial refreshes, perform a full refresh once
static int reflesh_times = 0;


void epd_wave_table(void)
{
    int ret = waveform_bin_reader_init(CUSTOM_EPD_WAVE_TABLE_START_ADDR, CUSTOM_EPD_WAVE_TABLE_SIZE);

    if(ret != 0)
    {
        rt_kprintf("Failed to initialize custom EPD wave table reader! err=%d\n", ret);
    }
}

uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode)
{
    uint32_t frames = 0;
    if (reflesh_times % PART_DISP_TIMES == 0) {
        frames = waveform_bin_reader_get_frames(temperature, EPD_DRAW_MODE_FULL);
        reflesh_times = 0;
    } else {
        frames = waveform_bin_reader_get_frames(temperature, EPD_DRAW_MODE_PARTIAL);
    }
    reflesh_times++;

    return frames;
}

void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num)
{
    waveform_bin_reader_fill_lut(p_epic_lut, frame_num);
}


uint16_t epd_get_vcom_voltage(void)
{
    return 1000;
}


const EPD_TimingConfig *epd_get_timing_config(void)
{
    static const EPD_TimingConfig timing_config = {
        .sclk_freq = 24,
        .SDMODE = 0,
        .LSL = 0,
        .LBL = 0,
        .LDL = LCD_HOR_RES_MAX/4,
        .LEL = 1,

        .fclk_freq = 83,
        .FSL = 1,
        .FBL = 3,
        .FDL = LCD_VER_RES_MAX,
        .FEL = 5,
    };

    return &timing_config;
}

#endif /*LCD_USING_EPD_CUSTOM*/