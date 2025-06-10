#ifndef __EPD_CONFIGS_H__
#define __EPD_CONFIGS_H__

typedef enum
{
    EPD_DRAW_MODE_INVALID = 0,
    EPD_DRAW_MODE_AUTO = 1,
} EpdDrawMode;

void epd_wave_table(void);//初始化wave table

uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode);

void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num);

uint32_t epd_get_clk_freq(void);

uint16_t epd_get_vcom_voltage(void);


#endif /* __EPD_CONFIGS_H__ */