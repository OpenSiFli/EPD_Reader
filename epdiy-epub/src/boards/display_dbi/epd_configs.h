#ifndef __EPD_CONFIGS_H__
#define __EPD_CONFIGS_H__

typedef enum
{
    EPD_DRAW_MODE_INVALID = 0,
    EPD_DRAW_MODE_AUTO = 1,
    EPD_DRAW_MODE_FULL = 2,
    EPD_DRAW_MODE_PARTIAL = 3,
} EpdDrawMode;

typedef struct
{
    uint32_t sclk_freq; //Line clock frequency in MHz
    uint32_t SDMODE; //Source driver mode
    uint32_t LSL; //Line start length
    uint32_t LBL; //Line begin length
    uint32_t LDL; //Line data length
    uint32_t LEL; //Line end length
    uint32_t GSTA; //Gate STA length

    uint32_t fclk_freq; //Frame clock frequency in KHz
    uint32_t FSL; //Frame sync length
    uint32_t FBL; //Frame begin length
    uint32_t FDL; //Frame data length
    uint32_t FEL; //Frame end length
} EPD_TimingConfig;
void epd_wave_table(void);//初始化wave table

uint32_t epd_wave_table_get_frames(int temperature, EpdDrawMode mode);

void epd_wave_table_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num);

uint16_t epd_get_vcom_voltage(void);

const EPD_TimingConfig *epd_get_timing_config(void);
#endif /* __EPD_CONFIGS_H__ */