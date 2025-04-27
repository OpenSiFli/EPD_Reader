/**
  ************************************************************************************
    * @function
    *    主要存放自定义的电子墨水屏基础显示驱动函数，比如显示一个英文、汉字与一页数据等。
    *
  ************************************************************************************
    */
#include <board.h>
#include "bsp_e-ink.h"
// #include "opm060e9_driver.h"
#include "GB2312_H24_SongTi.dat"
#include "GB2312_H32_KaiTi.dat"

uint16_t  LCD_X_LENGTH = EPD_WIDTH - 16;
uint16_t  LCD_Y_LENGTH = EPD_HEIGHT;

#define EPD_ARRAY  EPD_WIDTH*EPD_HEIGHT/8   // 全屏所需字节量

uint8_t  GetGBKCode_from_EXFlash(uint8_t *pBuffer, uint16_t c, uint8_t f_size, uint8_t myFont);
void SSD1677_DispChar_EN(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, const char cChar);
void SSD1677_DispChar_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint16_t usChar);
void SSD1677_DispString_EN_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint8_t z_space, char *pStr);
void SSD1677_Display_Page(uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t rowLedge, char *pStr);




unsigned char  BlackImage[EPD_ARRAY]; //Define canvas space

static rt_device_t lcd_device = NULL;

// extern const uint8_t ASCII8x16_Table [ ];
extern const uint8_t ASCII12x24_Table [ ];
extern const uint8_t ASCII16x32_Table [ ];
void read_buf(uint8_t *pBuffer, uint8_t *ReadAddr, uint16_t  numBytesToRead)
{
    // 确保目标数组和源数组指针有效
    if (pBuffer == NULL || ReadAddr == NULL)
    {
        return;
    }

    // 按照读取数量逐个从源数组读取到目标数组
    while (numBytesToRead--)
    {
        *pBuffer = *ReadAddr;  // 从源数组读取一个数据项到目标数组
        pBuffer++;                 // 移动到目标数组的下一个位置
        ReadAddr++;               // 移动到源数组的下一个位置
    }
}



/**
 * @brief  获取FLASH中文显示字库数据
 * @param  pBuffer：存储字库矩阵的缓冲区
 * @param  c ： 要获取的文字
 * @param  f_size：中文字体大小（16/24/32）
 * @param  myFont ：中文字体选择
 * @retval None.
 */
uint8_t GetGBKCode_from_EXFlash(uint8_t *pBuffer, uint16_t c, uint8_t f_size, uint8_t myFont)
{
    uint8_t         High8bit, Low8bit;
    uint32_t        pos;
    static uint8_t  everRead = 0;

    uint32_t        GBK_START_ADDRESS = 0;


    switch (myFont) // 选择哪种字体进行显示
    {
    case SongTi:    // 宋体
    {
        if (f_size == 24)  GBK_START_ADDRESS = (uint32_t)h24_songti;
        break;
    }
    case KaiTi:      // 楷体
    {
        if (f_size == 32)  GBK_START_ADDRESS = (uint32_t)h32_kaiti;
        break;
    }
    default:
        break;

    }
    RT_ASSERT(GBK_START_ADDRESS);
    // switch(myFont) // 选择哪种字体进行显示
    // {
    //  case SongTi:    // 宋体
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_SongTi;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_SongTi;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_SongTi;
    //       break;
    //  case KaiTi:      // 楷体
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_KaiTi;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_KaiTi;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_KaiTi;
    //       break;
    //  case HYMaqiduo:  // 汉仪玛奇朵
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_HYMaqiduo;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_HYMaqiduo;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_HYMaqiduo;
    //       break;
    //  case HYyoukai:     // 汉仪有楷
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_HYyoukai;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_HYyoukai;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_HYyoukai;
    //       break;
    //  case GongfanMFT:  // 龚帆免费体
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_GongfanMFT;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_GongfanMFT;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_GongfanMFT;
    //       break;
    //  case SanjiHKJT_C: // 三极行楷简体-粗
    //       if(f_size==16)  GBK_START_ADDRESS = ADD_GB2312_H1616_SanjiHKJT_C;
    //       if(f_size==24)  GBK_START_ADDRESS = ADD_GB2312_H2424_SanjiHKJT_C;
    //       if(f_size==32)  GBK_START_ADDRESS = ADD_GB2312_H3232_SanjiHKJT_C;
    //       break;
    //  default:break;
    // }


    High8bit = c >> 8;    /* 取高8位数据 */
    Low8bit = c & 0x00FF; /* 取低8位数据 */

//  pos = ((High8bit-0xa1)*94+(Low8bit-0xa1))*f_size*f_size/8; // OK
    if (High8bit <= 0xA9) // 显示01~09区特殊符号
    {
        pos = ((High8bit - 0xa1) * 94 + (Low8bit - 0xa1)) * f_size * f_size / 8; //计算特定汉字在字模中的偏移量
    }
    else // 显示16~87区汉字
    {
        pos = ((High8bit - 0xa7) * 94 + (Low8bit - 0xa1)) * f_size * f_size / 8; //计算特定汉字在字模中的偏移量
    }
    read_buf(pBuffer, (uint8_t *)GBK_START_ADDRESS + pos, f_size * f_size / 8);
    // SPI_FLASH_BufferRead(pBuffer,GBK_START_ADDRESS+pos,f_size*f_size/8); //读取字库数据
    return 0;
}

// /**
//  * @brief  电子墨水屏初始化
//  * @retval 无
//  */
// void EPD_First_Init(void)
// {
//  EPD_HW_Init();               // E-paper 初始化
//  EPD_WhiteScreen_White();     // 清屏功能。
//  EPD_DeepSleep();             // 进入休眠模式，请不要删除，否则会降低屏幕寿命。
// }

// /**
//  * @brief  屏幕显示前处理
//  * @retval 无
//  */
// extern uint8_t Update_Counter; // 刷新次数
// extern const unsigned char gImage_basemap[]; // 快刷全白效果
// extern unsigned char BlackImage[];

void Display_up_handle(bool DisImage_Control)
{
#if 0
    if (Update_Counter == 0) // 第一次时执行的操作
    {
        EPD_HW_Init();
        if (DisImage_Control == false)   EPD_SetRAMValue_BaseMap(gImage_basemap); // 刷白再重新显示
        else                          EPD_SetRAMValue_BaseMap(BlackImage);     // 在当前内容上显示
    }

    Paint_NewImage(BlackImage, EPD_HEIGHT, EPD_WIDTH, ROTATE_90, WHITE); // 结构体填参
    Paint_SetMirroring(MIRROR_HORIZONTAL); // 水平翻转
    Paint_SelectImage(BlackImage); // 选择要显示的图片
#endif

    lcd_device = rt_device_find("lcd");
    if (rt_device_open(lcd_device, RT_DEVICE_OFLAG_RDWR) == RT_EOK)
    {
        struct rt_device_graphic_info info;
        if (rt_device_control(lcd_device, RTGRAPHIC_CTRL_GET_INFO, &info) == RT_EOK)
        {
            rt_kprintf("Lcd info w:%d, h%d, bits_per_pixel %d, draw_align:%d\r\n",
                       info.width, info.height, info.bits_per_pixel, info.draw_align);
        }
    }
    else
    {
        rt_kprintf("Lcd open error!\n");
        return;
    }
    uint16_t framebuffer_color_format = RTGRAPHIC_PIXEL_FORMAT_MONO;
    rt_device_control(lcd_device, RTGRAPHIC_CTRL_SET_BUF_FORMAT, &framebuffer_color_format);

    memset(BlackImage, 0xff, sizeof(BlackImage));
    Paint_NewImage(BlackImage, EPD_HEIGHT, EPD_WIDTH, ROTATE_90, WHITE);

    // if(DisImage_Control==false)
    //  Paint_Clear(WHITE); // 清除储存数组历史记录
}

static rt_err_t lcd_flush_done(rt_device_t dev, void *buffer)
{
    rt_kprintf("lcd_flush_done!\n");
    return RT_EOK;
}

/**
 * @brief  屏幕显示后处理
 * @retval 无
 */
void Display_down_handle(void)
{
    // epd_display_pic(BlackImage);

    rt_graphix_ops(lcd_device)->set_window(0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    // rt_device_set_tx_complete(lcd_device, lcd_flush_done);
    // rt_graphix_ops(lcd_device)->draw_rect_async((const char *)BlackImage, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);
    rt_graphix_ops(lcd_device)->draw_rect((const char *)BlackImage, 0, 0, LCD_HOR_RES_MAX - 1, LCD_VER_RES_MAX - 1);

#if 0
//  printf("\r\n局部全刷次数累计 = %d\r\n",Update_Counter);
    EPD_Dis_PartAll(BlackImage);
    Update_Counter++; // 刷新次数累计
    if (Update_Counter == 10) // 每7次全屏局部刷新后，全屏刷新一次
    {
        Update_Counter = 0;
    }
    EPD_DeepSleep();   // 屏幕进入休眠模式，延长屏幕寿命。
#endif
}

/**
 * @brief  在 SSD1677 显示器上显示一个英文字符
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下该点的起始Y坐标
 * @param  Color ：BLACK，WHITE颜色控制
 * @param  f_size：英文字体大小（16/24/32）
 * @param  cChar ：要显示的英文字符
 * @retval 无
 */
void SSD1677_DispChar_EN(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, const char cChar)
{
    uint32_t   byteCount, bitCount, fontLength;
    uint32_t   ucRelativePositon;
    uint32_t   usTemp;
    uint8_t    *Pfont;
    uint16_t      Forecolor, BackColor;
    // ---------------------------------------------

    if (Color == BLACK)
    {
        Forecolor = BLACK; // 前景色
        BackColor = WHITE; // 背景色
    }
    else
    {
        Forecolor = WHITE; // 前景色
        BackColor = BLACK; // 背景色
    }

    ucRelativePositon = cChar - ' '; //对ascii码表偏移（字模表不包含ASCII表的前32个非图形符号）
//----------------------------------------------------------------------------------------------------------
    // //按字节读取字模数据
    // if(f_size==16)
    // {
    //  fontLength = f_size*f_size/2/8;
    //  Pfont = (uint8_t *)&ASCII8x16_Table[ucRelativePositon * fontLength];  /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/

    //  for ( byteCount = 0; byteCount < fontLength; byteCount++ )  // 控制字节数循环
    //  {
    //      for ( bitCount = 0; bitCount < 8; bitCount++ )  //一位一位处理要显示的颜色
    //      {
    //          if ( Pfont[byteCount] & (0x80>>bitCount) )
    //              Paint_DrawPoint( usX+bitCount, usY+byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
    //          else
    //              Paint_DrawPoint( usX+bitCount, usY+byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
    //      }
    //  }
    // }
//----------------------------------------------------------------------------------------------------------
    if (f_size == 24)
    {
        Pfont = (uint8_t *)&ASCII12x24_Table[ucRelativePositon * 48]; /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/

        for (byteCount = 0; byteCount < 24; byteCount++)
        {
            /* 取出两个字节的数据，在lcd上即是一个汉字的一行 */
            usTemp = Pfont [ byteCount * 2 + 0 ];
            usTemp = (usTemp << 8);
            usTemp |= Pfont [ byteCount * 2 + 1 ];

            for (bitCount = 0; bitCount < 12; bitCount ++)
            {
                if (usTemp & (0x8000 >> bitCount))      //高位在前
                    Paint_DrawPoint(usX + bitCount, usY + byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                else
                    Paint_DrawPoint(usX + bitCount, usY + byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
        }
    }

//----------------------------------------------------------------------------------------------------------
    if (f_size == 32)
    {
        fontLength = f_size * f_size / 2 / 8;
        Pfont = (uint8_t *)&ASCII16x32_Table[ucRelativePositon * fontLength];  /*ascii码表偏移值乘以每个字模的字节数，求出字模的偏移位置*/

        for (byteCount = 0; byteCount < (fontLength / 2); byteCount++)  // 控制字节数循环
        {
            for (bitCount = 0; bitCount < 8; bitCount++)    //一位一位处理要显示的颜色
            {
                if (Pfont[byteCount * 2] & (0x80 >> bitCount))
                    Paint_DrawPoint(usX + bitCount, usY + byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                else
                    Paint_DrawPoint(usX + bitCount, usY + byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                //--------------------------------------------------
                if (Pfont[byteCount * 2 + 1] & (0x80 >> bitCount))
                    Paint_DrawPoint(usX + bitCount + 8, usY + byteCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                else
                    Paint_DrawPoint(usX + bitCount + 8, usY + byteCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
        }
    }
}

/**
 * @brief  在 SSD1677 显示器上显示一个中文字符
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  Color ：BLACK，WHITE颜色控制
 * @param  f_size：中文字体大小（16/24/32）
 * @param  myFont ：中文字体选择
 * @param  usChar ：要显示的中文字符（国标码）
 * @retval 无
 */
void SSD1677_DispChar_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint16_t usChar)
{
    uint8_t   rowCount, bitCount;
//  uint8_t   ucBuffer [ f_size*f_size/8 ];  // 使用此条会导致不定时触发HardFault_Handler
    uint8_t   ucBuffer[128];
    uint32_t  usTemp1, usTemp2;
    uint16_t     Forecolor, BackColor;

    if (Color == BLACK)
    {
        Forecolor = BLACK; // 前景色
        BackColor = WHITE; // 背景色
    }
    else
    {
        Forecolor = WHITE; // 前景色
        BackColor = BLACK; // 背景色
    }

    //取字模数据
    GetGBKCode_from_EXFlash(ucBuffer, usChar, f_size, myFont);

    for (rowCount = 0; rowCount < f_size; rowCount++)
    {
        /* 取出两个字节的数据，在lcd上即是一个汉字的一行 */
        usTemp1 = ucBuffer  [ rowCount * (f_size / 8) + 0 ];
        usTemp1 = (usTemp1 << 8);
        usTemp1 |= ucBuffer [ rowCount * (f_size / 8) + 1 ];

        if (f_size == 24) // 24字号独有代码
        {
            usTemp2 = ucBuffer  [ rowCount * (f_size / 8) + 2 ];
        }
        if (f_size == 32) // 32字号独有代码
        {
            usTemp2 = ucBuffer  [ rowCount * (f_size / 8) + 2 ];
            usTemp2 = (usTemp2 << 8);
            usTemp2 |= ucBuffer [ rowCount * (f_size / 8) + 3 ];
        }

        //数据逐行处理
        for (bitCount = 0; bitCount < 16; bitCount ++)
        {
            if (usTemp1 & (0x8000 >> bitCount))      // 16/24/32字号代码通用部分
                Paint_DrawPoint(usX + bitCount, usY + rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            else
                Paint_DrawPoint(usX + bitCount, usY + rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);

            if (f_size == 32) // 32字号独有代码
            {
                if (usTemp2 & (0x8000 >> bitCount))
                    Paint_DrawPoint(usX + bitCount + 16, usY + rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                else
                    Paint_DrawPoint(usX + bitCount + 16, usY + rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
        }

        if (f_size == 24) // 24字号独有代码
        {
            for (bitCount = 0; bitCount < 8; bitCount ++)
            {
                if (usTemp2 & (0x80 >> bitCount))      //高位在前
                    Paint_DrawPoint(usX + bitCount + 16, usY + rowCount, Forecolor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                else
                    Paint_DrawPoint(usX + bitCount + 16, usY + rowCount, BackColor, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
        }
    }
}


/**
 * @brief  在 ST7789V 显示器上显示中英文
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  Color ：BLACK，WHITE颜色控制
 * @param  f_size：中英文字体大小（16/24/32）
 * @param  myFont ：中文字体选择
 * @param  z_space：字间距
 * @param  pStr ：要显示的字符串的首地址
 * @retval 无
 */
void SSD1677_DispString_EN_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint8_t z_space, char *pStr)
{
    uint16_t  usCh;

    while (* pStr != '\0')
    {
        if ((* pStr <= 126) && (* pStr != '\r'))  //英文字符只读取ASCII值前126的
        {
            if ((usX + f_size / 2 + z_space) <= LCD_X_LENGTH)    // x轴坐标超出屏幕宽度
            {
                SSD1677_DispChar_EN(usX, usY, Color, f_size, * pStr);
                pStr ++;
                usX += (f_size / 2 + z_space);
            }
            else
            {
                pStr ++;
            }
        }
        else  // 显示汉字字符
        {
            if ((usX + f_size + z_space) <= LCD_X_LENGTH)     // x轴坐标超出屏幕宽度
            {
                usCh = * (uint16_t *) pStr;                   // 码值转换
                usCh = (usCh << 8) + (usCh >> 8);

                SSD1677_DispChar_CH(usX, usY, Color, f_size, myFont, usCh);

                pStr += 2;           //一个汉字两个字节
                usX += (f_size + z_space);
            }
            else
            {
                pStr += 2;
            }
        }
    }
}

/**
 * @brief  在 SSD1677 显示器上显示1页文字
 * @param  usX ：在特定扫描方向下字符的起始X坐标
 * @param  usY ：在特定扫描方向下字符的起始Y坐标
 * @param  Color ：BLACK，WHITE颜色控制
 * @param  f_size：中英文字体大小（16/24/32）
 * @param  myFont ：中文字体选择
 * @param  rowLedge ：行间距控制
 * @param  pStr ：要显示的字符串的首地址
 * @retval 无
 */
extern uint32_t Page_Counter_EN_CH_Space; //  引用全局变量

void SSD1677_Display_Page(uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t rowLedge, char *pStr)
{
    uint32_t EN_Counter = 0; // 存放当前页显示的英文字符总数
    uint32_t CH_Counter = 0; // 存放当前页显示的汉字总数
    uint32_t Space_Counter = 0; // 存放当前页显示的空格总数

    uint16_t usCh = 0;
    bool Page_Display_Limit = false; // 页面显示限制，防止超出屏幕宽度

    while (* pStr != '\0')
    {
        if (* pStr == '\r')   // 遇到换行时 y轴坐标增加一个单位，另起一行
        {
            usY += (f_size + rowLedge);
            usX = 16;
//          printf("\r\n>>>>>>>>遇到了换行符\r\n");
        }

        if ((* pStr <= 126) && (* pStr != '\r'))  //英文字符只读取ASCII值前126的
        {
            if ((usX + f_size / 2) > LCD_X_LENGTH)    // x轴坐标超出屏幕宽度
            {
                usX = 16; //左边距
                usY += (f_size + rowLedge); // Y换行
            }
            if ((usY + f_size) > (LCD_Y_LENGTH - 32))   // y轴坐标超出屏幕宽度
            {
                usX = 0; // X = 0;
                usY = 0; // Y = 0;
                Page_Display_Limit = true;
            }

            if (Page_Display_Limit == false) // 不超出范围时显示
            {
                SSD1677_DispChar_EN(usX, usY, BLACK, f_size, * pStr);    // 显示英文字符
                EN_Counter++; // 统计显示的英文字符数
            }
            pStr ++;
            usX += (f_size / 2 + wordspace); // X轴坐标增加
        }
        else  // 显示汉字字符
        {
            if ((usX + f_size) > LCD_X_LENGTH)      // x轴坐标超出屏幕宽度
            {
                usX = 16;//左边距
                usY += (f_size + rowLedge);
            }
            if ((usY + f_size) > (LCD_Y_LENGTH - 32))   // y轴坐标超出屏幕宽度
            {
                usX = 0;
                usY = 0;
                Page_Display_Limit = true;
            }

            usCh = * (uint16_t *) pStr;                   // 码值转换
            usCh = (usCh << 8) + (usCh >> 8);
            // usCh = (uint8_t)*pStr ;   // 将pStr的第一个字节赋值给usCh的高八位
            // usCh |= (uint8_t)*(pStr + 1)<< 8; // 将pStr的第二个字节赋值给usCh的低八位
            if (Page_Display_Limit == false) // 超出范围不显示
            {
                if (usCh == 3338) // ' '空格符号的国标码值
                {
                    Space_Counter++;  // 统计显示的空格符总数
                }
                else
                {
                    if (* pStr != '\r')
                    {
                        SSD1677_DispChar_CH(usX, usY, BLACK, f_size, myFont, usCh);    // 显示一个汉字
                        CH_Counter++; // 统计显示的汉字总数
                        usX += (f_size + wordspace);
                    }
                }
            }
            pStr += 2;           //一个汉字两个字节
        }
    }

//  printf("输出的空格数量：%03d \r\n",Space_Counter);
//  printf("输出的英文数量：%03d \r\n",EN_Counter);
//  printf("输出的汉字数量：%03d \r\n",CH_Counter);

    Page_Counter_EN_CH_Space = CH_Counter * 2 + EN_Counter + Space_Counter * 2; // 显示的总字节数
//  printf("显示的总字节数：%03d \r\n",Page_Counter_EN_CH_Space);

    Space_Counter = 0;
    EN_Counter = 0;
    CH_Counter = 0;

    Page_Display_Limit = false; // 页面显示限制变量清零，防止下次判断失误
}

//----------------------------------------- END Files -------------------------------------------

