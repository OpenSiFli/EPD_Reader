
#include <rtthread.h>
#include "string.h"
#include "board.h"
#include "drv_io.h"
#include "drv_lcd.h"
#include "bf0_hal.h"
#include "waveinit.h"
#include "epd_pin_defs.h"

#define DBG_TAG               "epd.spi"
#define DBG_LVL               DBG_INFO
#include "log.h"

#define EPD_LCD_ID            0x09ff
#define LCD_PIXEL_WIDTH       240
#define LCD_PIXEL_HEIGHT      416
#define LCD_HOR_RES_MAX_8    LCD_HOR_RES_MAX / 8

#define PICTURE_LENGTH        (LCD_HOR_RES_MAX / 8 * LCD_VER_RES_MAX) 
#define PIC_WHITE                   255  // 全白
#define PIC_BLACK                   254  // 全黑
#define PIC_LEFT_BLACK_RIGHT_WHITE  253  // 左黑右白
#define PIC_UP_BLACK_DOWN_WHITE     252  // 上黑下白
#define PART_DISP_TIMES       10 

#define REG_LUT_VCOM          0x20 
#define REG_LUT_W2W           0x21 
#define REG_LUT_K2W           0x22 
#define REG_LUT_W2K           0x23
#define REG_LUT_K2K           0x24 
#define REG_LUT_OPT           0x2A 
#define REG_WRITE_NEW_DATA   0x13  
#define REG_AUTO_REFRESH     0x17 
#define REG_PWR_ON_MEASURE    0x05   
#define REG_TEMP_CALIB        0x40 
#define REG_TEMP_SEL          0x41 
#define REG_TEMP_READ         0x43 
#define REG_PANEL_SETTING     0x00  
#define REG_POWER_SETTING     0x01  
#define REG_BOOSTER_SOFTSTART 0x06 
#define REG_PLL_CTRL          0x30 
#define REG_VCOM_DATA_INTERV  0x50 
#define REG_TCON_SETTING      0x60  
#define REG_RESOLUTION        0x61  
#define REG_REV               0x70 
#define REG_VDCS              0x82
#define REG_WRITE_NEW_DATA   0x13  

static int reflesh_times = 0;         
static uint8_t current_refresh_mode;
static unsigned char LUT_Flag = 0;    // LUT切换标志
static unsigned char Var_Temp = 0;    // 温度值
static int g_part_disp_times = 10;      // After g_part_disp_times-1 partial refreshes, perform a full refresh once

static LCDC_InitTypeDef lcdc_int_cfg = {
    .lcd_itf = LCDC_INTF_SPI_DCX_1DATA,
    .freq = 5000000,    
    .color_mode = LCDC_PIXEL_FORMAT_RGB332,
    .cfg = {
        .spi = {
            .dummy_clock = 0,
            .syn_mode = HAL_LCDC_SYNC_DISABLE,
            .vsyn_polarity = 1,
            .vsyn_delay_us = 0,
            .hsyn_num = 0,
        },
    },
};

LCDC_HandleTypeDef hlcdc;
static uint32_t LCD_ReadID(LCDC_HandleTypeDef *hlcdc);
static void LCD_WriteReg(LCDC_HandleTypeDef *hlcdc, uint16_t LCD_Reg, uint8_t *Parameters, uint32_t NbParameters);
static uint32_t LCD_ReadData(LCDC_HandleTypeDef *hlcdc, uint16_t RegValue, uint8_t ReadSize);
static void EPD_TemperatureMeasure(LCDC_HandleTypeDef *hlcdc);
static void EPD_EnterDeepSleep(LCDC_HandleTypeDef *hlcdc);
static void EPD_DisplayImage(LCDC_HandleTypeDef *hlcdc, uint8_t img_flag);
static void EPD_EnterDeepSleep(LCDC_HandleTypeDef *hlcdc);
static void EPD_LoadLUT(LCDC_HandleTypeDef *hlcdc, uint8_t lut_mode);

static rt_sem_t epd_busy_sem = RT_NULL; 

void set_part_disp_times(int val) 
{ 
    g_part_disp_times = val > 0 ? val : 1;
    reflesh_times = 1;
}
int get_part_disp_times(void) 
{ 
    return g_part_disp_times; 
}

static void epd_busy_callback(void *args)
{
    rt_sem_release(epd_busy_sem);
    rt_pin_irq_enable(EPD_BUSY, PIN_IRQ_DISABLE);
}

static void epd_sem_init(void)
{
    if (epd_busy_sem != RT_NULL)
    {
         return;
    }
    epd_busy_sem = rt_sem_create("epd_busy", 0, RT_IPC_FLAG_FIFO);
    if (epd_busy_sem == RT_NULL)
    {
        rt_kprintf("EPD busy semaphore create failed!\n");
        return;
    }
    rt_pin_mode(EPD_BUSY, PIN_MODE_INPUT_PULLUP);
    rt_err_t irq_ret = rt_pin_attach_irq(EPD_BUSY, PIN_IRQ_MODE_HIGH_LEVEL, epd_busy_callback, RT_NULL);
    if (irq_ret != RT_EOK)
    {
        rt_kprintf("EPD BUSY IRQ attach failed!\n");
        rt_sem_delete(epd_busy_sem);
        epd_busy_sem = RT_NULL;
        return;
    }
    rt_pin_irq_enable(EPD_BUSY, PIN_IRQ_ENABLE);
}

static void EPD_ReadBusy(void)
{
    rt_pin_irq_enable(EPD_BUSY, PIN_IRQ_ENABLE);
    rt_err_t result = rt_sem_take(epd_busy_sem, RT_TICK_PER_SECOND * 2);
    if (result != RT_EOK)
    {
        rt_kprintf("EPD busy wait timeout! (may cause display error)\n");
    }

    rt_sem_control(epd_busy_sem,RT_IPC_CMD_RESET, RT_NULL);

}
static uint8_t epd_get_refresh_mode(void)
{
    uint8_t mode;
    if (reflesh_times % g_part_disp_times == 0) 
    {
        rt_kprintf("cleared all \n");
        mode = 1; //全刷
    } 
    else 
    {
        rt_kprintf("executing partial refresh, this is the %dth partial refresh (there are %d partial refreshes left until the next full refresh)\n", 
               (reflesh_times % g_part_disp_times), 
               g_part_disp_times - (reflesh_times % g_part_disp_times));
        mode = 2; //局刷
    }
    current_refresh_mode = mode;
    return mode;
}
static void EPD_Reset(void)
{    
    nRST_H();
    rt_thread_delay(10);
    nRST_L();
    rt_thread_delay(15);
    nRST_H();
    rt_thread_delay(10);

    LUT_Flag = 0;
}
static void LCD_ReadMode(LCDC_HandleTypeDef *hlcdc, bool enable)
{
    if (HAL_LCDC_IS_SPI_IF(lcdc_int_cfg.lcd_itf))
    {
        if (enable)
        {
            HAL_LCDC_SetFreq(hlcdc, 2800000);
        }
        else
        {
            HAL_LCDC_SetFreq(hlcdc, lcdc_int_cfg.freq);
        }
    }
}


static void LCD_Drv_Init(LCDC_HandleTypeDef *hlcdc)
{
    memcpy(&hlcdc->Init, &lcdc_int_cfg, sizeof(LCDC_InitTypeDef));
    HAL_LCDC_Init(hlcdc);

    epd_sem_init();
    EPD_Reset();

    uint8_t parameter[5];
    parameter[0] = 0x3F;
    parameter[1] = 0x01;
    LCD_WriteReg(hlcdc, REG_PANEL_SETTING, parameter, 2);

    parameter[0] = 0x03;
    parameter[1] = 0x10;
    parameter[2] = 0x3F;
    parameter[3] = 0x3F;
    parameter[4] = 0x03;
    LCD_WriteReg(hlcdc, REG_POWER_SETTING, parameter, 5);
    
    parameter[0] = 0x37;
    parameter[1] = 0x3D;
    parameter[2] = 0x3D;
    LCD_WriteReg(hlcdc, REG_BOOSTER_SOFTSTART, parameter, 3);

    parameter[0] = 0x22;
    LCD_WriteReg(hlcdc, REG_TCON_SETTING, parameter, 1);

    parameter[0] = 0x07;
    LCD_WriteReg(hlcdc, REG_VDCS, parameter, 1);

    parameter[0] = 0x09;
    LCD_WriteReg(hlcdc, REG_PLL_CTRL, parameter, 1);

    parameter[0] = 0x88;
    LCD_WriteReg(hlcdc, 0xE3, parameter, 1);
    
    parameter[0] = LCD_PIXEL_WIDTH % 256;   
    parameter[1] = LCD_PIXEL_HEIGHT /256;
    parameter[2] = LCD_PIXEL_HEIGHT % 256;
    LCD_WriteReg(hlcdc, REG_RESOLUTION, parameter, 3);

    parameter[0] = 0xB7;
    LCD_WriteReg(hlcdc, REG_VCOM_DATA_INTERV, parameter, 1);

}
static void LCD_Init(LCDC_HandleTypeDef *hlcdc)
{
    uint8_t parameter[5];
    LCD_Drv_Init(hlcdc);
    EPD_LoadLUT(hlcdc, 0);
    EPD_DisplayImage(hlcdc,PIC_WHITE);  

    parameter[0] = 0x17;
    LCD_WriteReg(hlcdc, REG_VCOM_DATA_INTERV, parameter, 1);

    EPD_TemperatureMeasure(hlcdc);
}

static uint32_t LCD_ReadID(LCDC_HandleTypeDef *hlcdc)
{
    uint32_t epd_id = LCD_ReadData(hlcdc, REG_REV, 3);
    // rt_kprintf("EPD ID: 0x%x (240x416 mono EPD)", epd_id);
    return epd_id;
}
static void LCD_DisplayOn(LCDC_HandleTypeDef *hlcdc)
{
}

static void LCD_DisplayOff(LCDC_HandleTypeDef *hlcdc)
{
}
static void LCD_SetRegion(LCDC_HandleTypeDef *hlcdc, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1)
{
    HAL_LCDC_SetROIArea(hlcdc, 0, 0, LCD_HOR_RES_MAX_8 - 1, LCD_VER_RES_MAX - 1);

}

static void LCD_WritePixel(LCDC_HandleTypeDef *hlcdc, uint16_t Xpos, uint16_t Ypos, const uint8_t *RGBCode)
{
    uint8_t data = 0;
    /* Set Cursor */
    LCD_SetRegion(hlcdc, Xpos, Ypos, Xpos, Ypos);
    LCD_WriteReg(hlcdc, REG_WRITE_NEW_DATA, (uint8_t *)RGBCode, 2);
}


static void LCD_WriteMultiplePixels(LCDC_HandleTypeDef *hlcdc, const uint8_t *RGBCode, uint16_t Xpos0, uint16_t Ypos0, uint16_t Xpos1, uint16_t Ypos1)
{
    uint8_t parameter[5];
    if (RGBCode == NULL || Xpos0 > Xpos1 || Ypos0 > Ypos1)
    {
        rt_kprintf("EPD multiple pixels param error\n"); 
        return;
    }
    
    uint8_t lut_mode = epd_get_refresh_mode();
    EPD_LoadLUT(hlcdc, lut_mode);

    //Force the layer format to RGB332
    HAL_LCDC_LayerSetFormat(hlcdc, HAL_LCDC_LAYER_DEFAULT, LCDC_PIXEL_FORMAT_RGB332);

    HAL_LCDC_LayerSetData(hlcdc, HAL_LCDC_LAYER_DEFAULT,(uint8_t *)RGBCode,0, 0, LCD_HOR_RES_MAX_8 - 1, LCD_VER_RES_MAX - 1);
    if (HAL_LCDC_SendLayerData2Reg_IT(hlcdc, REG_WRITE_NEW_DATA, 1) != HAL_OK) return;

    parameter[0] = 0xA5;
    LCD_WriteReg(hlcdc, REG_AUTO_REFRESH, parameter, 1);

    reflesh_times++;
}
static void LCD_WriteReg(LCDC_HandleTypeDef *hlcdc, uint16_t LCD_Reg, uint8_t *Parameters, uint32_t NbParameters)
{
    EPD_ReadBusy();
    HAL_LCDC_WriteU8Reg(hlcdc, LCD_Reg, Parameters, NbParameters);
}
static uint32_t LCD_ReadData(LCDC_HandleTypeDef *hlcdc, uint16_t RegValue, uint8_t ReadSize)
{
    uint32_t rd_data = 0;
    EPD_ReadBusy();
    LCD_ReadMode(hlcdc, true);
    HAL_LCDC_ReadU8Reg(hlcdc, RegValue, (uint8_t *)&rd_data, ReadSize);
    LCD_ReadMode(hlcdc, false);

    return rd_data;
}

static uint32_t LCD_ReadPixel(LCDC_HandleTypeDef *hlcdc, uint16_t Xpos, uint16_t Ypos)
{
    if (Xpos >= LCD_PIXEL_WIDTH || Ypos >= LCD_PIXEL_HEIGHT)
    {
        LOG_W("EPD read pixel out of range");
        return 0;
    }

    LCD_SetRegion(hlcdc, Xpos, Ypos, Xpos, Ypos);
    uint8_t read_data = (uint8_t)LCD_ReadData(hlcdc, 0x2E, 1);
    uint32_t color = (read_data & 0x80) ? 0xFFFFFF : 0x000000; 
    LOG_D("EPD read pixel: (%d,%d), color: 0x%x", Xpos, Ypos, color);
    return color;
}

static void LCD_SetColorMode(LCDC_HandleTypeDef *hlcdc, uint16_t color_mode)
{
    if (color_mode != RTGRAPHIC_PIXEL_FORMAT_RGB332 && color_mode != LCDC_PIXEL_FORMAT_RGB332)
    {
        rt_kprintf("EPD only support mono-color, ignore mode: %d\n", color_mode);
        return;
    }
    lcdc_int_cfg.color_mode = LCDC_PIXEL_FORMAT_RGB332;
    HAL_LCDC_SetOutFormat(hlcdc, lcdc_int_cfg.color_mode);
    // rt_kprintf("EPD set color mode: mono-color (1bit)\n");
}

static void LCD_SetBrightness(LCDC_HandleTypeDef *hlcdc, uint8_t br)
{
    LOG_W("EPD has no brightness adjustment, ignore br: %d", br);
}

static void LCD_IdleModeOn(LCDC_HandleTypeDef *hlcdc)
{
    EPD_EnterDeepSleep(hlcdc);
    BSP_LCD_PowerDown();
    BSP_LCD_Reset(0);
}

static void LCD_IdleModeOff(LCDC_HandleTypeDef *hlcdc)
{
    BSP_LCD_PowerUp();
    BSP_LCD_Reset(1);
    HAL_Delay(1);
    LCD_Drv_Init(hlcdc);
}
static void EPD_LoadLUT(LCDC_HandleTypeDef *hlcdc, uint8_t lut_mode)
{
    uint16_t count;
    switch (lut_mode)
    {
        case 0: // 5S模式（清屏）
            LCD_WriteReg(hlcdc, REG_LUT_VCOM, lut_R20_5S, 56);
            LCD_WriteReg(hlcdc, REG_LUT_W2W, lut_R21_5S, 42);
            LCD_WriteReg(hlcdc, REG_LUT_K2K, lut_R24_5S, 42);
            
            if (LUT_Flag == 0)
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R22_5S, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R23_5S, 42);
                LUT_Flag = 1;
            }
            else
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R23_5S, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R22_5S, 42);
                LUT_Flag = 0;
            }
            break;

        case 1: // GC模式（全刷无残影）
            LCD_WriteReg(hlcdc, REG_LUT_VCOM, lut_R20_GC, 56);
            LCD_WriteReg(hlcdc, REG_LUT_W2W, lut_R21_GC, 42);
            LCD_WriteReg(hlcdc, REG_LUT_K2K, lut_R24_GC, 42);
            if (LUT_Flag == 0)
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R22_GC, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R23_GC, 42);
                LUT_Flag = 1;
            }
            else
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R23_GC, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R22_GC, 42);
                LUT_Flag = 0;
            }
            break;
        case 2: // DU模式（局部刷有残影）
            LCD_WriteReg(hlcdc, REG_LUT_VCOM, lut_R20_DU, 56);
            LCD_WriteReg(hlcdc, REG_LUT_W2W, lut_R21_DU, 42);
            LCD_WriteReg(hlcdc, REG_LUT_K2K, lut_R24_DU, 42);
            if (LUT_Flag == 0)
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R22_DU, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R23_DU, 42);
                LUT_Flag = 1;
            }
            else
            {
                LCD_WriteReg(hlcdc, REG_LUT_K2W, lut_R23_DU, 56);
                LCD_WriteReg(hlcdc, REG_LUT_W2K, lut_R22_DU, 42);
                LUT_Flag = 0;
            }
            break;
        default:
            break;
    }
}

static void EPD_DisplayImage(LCDC_HandleTypeDef *hlcdc, uint8_t img_flag)
{
    uint16_t row, col;
    uint16_t pcnt = 0;
    uint8_t *temp_buf = rt_malloc(PICTURE_LENGTH);
    
    if (temp_buf == RT_NULL)
    {
        rt_kprintf("EPD image buf malloc failed\n");
        return;
    }

    for (col = 0; col < LCD_VER_RES_MAX; col++)
    {
        for (row = 0; row < LCD_HOR_RES_MAX / 8; row++)
        {
            switch (img_flag)
            {
                case PIC_BLACK:
                    temp_buf[pcnt] = 0x00;
                    break;
                case PIC_WHITE:
                    temp_buf[pcnt] = 0xFF;
                    break;
                case PIC_LEFT_BLACK_RIGHT_WHITE:
                    temp_buf[pcnt] = (col >= LCD_VER_RES_MAX/2) ? 0xFF : 0x00;
                    break;
                case PIC_UP_BLACK_DOWN_WHITE:
                    temp_buf[pcnt] = (row > LCD_HOR_RES_MAX/16) ? 0xFF : (row == LCD_HOR_RES_MAX/16) ? 0x0F : 0x00;
                    break;
                default:
                    temp_buf[pcnt] = 0x00;
                    break;
            }
            pcnt++;
        }
    }

    LCD_WriteReg(hlcdc, REG_WRITE_NEW_DATA, temp_buf, PICTURE_LENGTH);
    rt_free(temp_buf);

    uint8_t parameter[5];
    parameter[0] = 0xA5;
    LCD_WriteReg(hlcdc, REG_AUTO_REFRESH, parameter, 1);
    EPD_ReadBusy();

}
static void EPD_EnterDeepSleep(LCDC_HandleTypeDef *hlcdc)
{
    uint8_t parameter[5];
    parameter[0] = 0xA5;
    LCD_WriteReg(hlcdc, 0x07, parameter, 1);
}
static void EPD_TemperatureMeasure(LCDC_HandleTypeDef *hlcdc)
{
    uint8_t parameter[5];
    LCD_WriteReg(hlcdc, REG_PWR_ON_MEASURE, RT_NULL, 0);  
    parameter[0] = 0xA5;
    LCD_WriteReg(hlcdc, REG_TEMP_SEL, parameter, 1);
    LCD_WriteReg(hlcdc, REG_TEMP_CALIB, RT_NULL, 0);
    EPD_ReadBusy();

    HAL_LCDC_ReadDatas(hlcdc, REG_TEMP_READ, 0, &Var_Temp, 1);
    // rt_kprintf("EPD internal temp: %d °C", (int8_t)Var_Temp);
}

static const LCD_DrvOpsDef epd_spi_drv = {
    LCD_Init,
    LCD_ReadID,
    LCD_DisplayOn,
    LCD_DisplayOff,
    LCD_SetRegion,    
    LCD_WritePixel,          
    LCD_WriteMultiplePixels,  
    LCD_ReadPixel,      
    LCD_SetColorMode,   
    LCD_SetBrightness, 
    LCD_IdleModeOn,         
    LCD_IdleModeOff   
};

LCD_DRIVER_EXPORT(epd_spi, EPD_LCD_ID, &lcdc_int_cfg, &epd_spi_drv, LCD_PIXEL_WIDTH, LCD_PIXEL_HEIGHT, 8); 