typedef enum{
    EPD_WAVEFORM_ERR_OK = 0,
    EPD_WAVEFORM_ERR_INVALID_MAGIC_NUMBER = -1,
    EPD_WAVEFORM_ERR_INVALID_VERSION = -2,
    EPD_WAVEFORM_ERR_MALLOC_FAILED = -3,
    EPD_WAVEFORM_ERR_READ_FAILED = -4,
    EPD_WAVEFORM_ERR_INVALID_FIRST_TABLE_OFFSET = -10, //-10 ~ -25 for mode 0~15

    EPD_WAVEFORM_ERR_INVALID_TABLE_OFFSET       = -1000, //
}EPD_WAVEFORM_ERR_TYPEDEF;



/*
E0470A03 Waveform Table Mode:
mode 0: GC16 FULL   // 全刷
mode 1: DU PARTIAL  // 黑白刷新，黑白2阶
mode 3: INIT        // 刷新全白页面，初始化页面使用
mode 5: GL16        // 16灰阶
mode 6: A2          // 动画模式，黑白2阶
mode 7: DU4         // DU ，4级灰	
*/

typedef enum{
    WAVE_MODE_NONE    = 0,
    WAVE_MODE_PARTIAL = 5,   
    WAVE_MODE_FULL    = 0,   

    WAVE_MODE_MAX = 16
} WAVE_TABLE_MODE_T;

int waveform_bin_reader_init(uint32_t max_size);
uint32_t waveform_bin_reader_get_frames(int temperature, WAVE_TABLE_MODE_T mode);
void waveform_bin_reader_fill_lut(uint32_t *p_epic_lut, uint32_t frame_num);


/* �����ⲿ�ӿ� */
extern int waveform_bin_reader_read_data(uint32_t offset, uint8_t *buf, uint32_t size);
extern void *waveform_bin_reader_malloc(size_t size);
extern void waveform_bin_reader_free(void *ptr);