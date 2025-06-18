#include <rtthread.h>
#include "epd_configs.h"
#include "mem_section.h"
#include "wfmlib.h"
#include "string.h"
#define PART_DISP_TIMES       10        // After PART_DISP_TIMES-1 partial refreshes, perform a full refresh once
static int reflesh_times = 0;

// 8bit lookup table for the current frame (high 4 bits: old data, low 4 bits: new data).
// The output is 2-bit data.
static uint8_t wave_forms[32][256];

static void get_all_waveform(uint32_t total_frames)
{
    uint8_t gray_buffer[256];
    uint8_t wave_form[64];

    for (int i = 0; i < 256; ++i)     gray_buffer[i] = i;

    for (uint8_t f = 0; f < total_frames; f++)
    {
        get_waveform(&gray_buffer[0], (int *)&wave_form, 256, 1, f);

        for (uint32_t i = 0; i < 256; ++i)
        {
            uint8_t shift = 2 * (3 - (i & 3));
            wave_forms[f][i] = (wave_form[i / 4] >> shift) & 0x3;
        }
    }
}

void epd_wave_table(void)
{

}


uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode)
{
    uint32_t total_frames = 0;
    if (reflesh_times % PART_DISP_TIMES == 0)
    {
        total_frames = get_frame(2, temperature); //Full refresh
    }
    else
    {
        total_frames = get_frame(1, temperature); //Partial refresh
    }
    reflesh_times++;

    //Initialize the wave_forms array with the waveform data
    get_all_waveform(total_frames);


    return total_frames;
}

void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num)
{

    uint8_t *p_frame_wave = &wave_forms[frame_num][0];

    //Convert the 8-bit waveforms to 32-bit epic LUT values
    for (uint16_t i = 0; i < 256; i++)
        p_epic_lut[i] = p_frame_wave[i] << 3;
}


uint32_t epd_get_clk_freq(void)
{
    return 24 * 1000 * 1000; // 24MHz
}

uint16_t epd_get_vcom_voltage(void)
{
#ifdef LCD_USING_EPD_YZC085_V100
    return 1050;
#else
    return 2100;
#endif
}