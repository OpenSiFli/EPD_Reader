#ifndef __BSP_EINK_H
#define __BSP_EINK_H



#include <board.h>
#include "GUI_Paint.h"
#include "fonts.h"

#if (LCD_VER_RES_MAX <= LCD_HOR_RES_MAX)
    #define  EPD_LESS_PIXEL                     LCD_VER_RES_MAX            //液晶屏较短方向的像素宽度   480/16=30   480/32=15
    #define  EPD_MORE_PIXEL                     LCD_HOR_RES_MAX        //液晶屏较长方向的像素宽度   800/16=50   800/32=25
#else
    #define  EPD_LESS_PIXEL                     LCD_HOR_RES_MAX            //液晶屏较短方向的像素宽度   480/16=30   480/32=15
    #define  EPD_MORE_PIXEL                     LCD_VER_RES_MAX        //液晶屏较长方向的像素宽度   800/16=50   800/32=25
#endif
#define EPD_WIDTH   EPD_LESS_PIXEL
#define EPD_HEIGHT  EPD_MORE_PIXEL



//   480/16=30    480/24=20   480/32=15
//   800/16=50    800/24=33   800/32=25
//   30*48=1440   20*31=620   15*24=360
//   2880         1240        720
//   720*2        310*2       150*2

extern   uint16_t   LCD_X_LENGTH, LCD_Y_LENGTH;

/********************************************************************************************
*   文件名称                 字体       文本大小           起始地址          长度（分配空间）
* GB2312_H1616_SongTi        宋体          256Kb             1*4096          65*4096(260Kb)
* GB2312_H2424_SongTi        宋体          576Kb            67*4096         145*4096(580Kb)
* GB2312_H3232_SongTi        宋体         1023Kb           213*4096         256*4096(1024Kb)

* GB2312_H1616_HeiTi         黑体          256Kb           470*4096          65*4096(260Kb)
* GB2312_H2424_HeiTi         黑体          576Kb           536*4096         145*4096(580Kb)
* GB2312_H3232_HeiTi         黑体         1023Kb           682*4096         256*4096(1024Kb)

* GB2312_H1616_KaiTi         楷体          256Kb           939*4096          65*4096(260Kb)
* GB2312_H2424_KaiTi         楷体          576Kb          1005*4096         145*4096(580Kb)
* GB2312_H3232_KaiTi         楷体         1023Kb          1151*4096         256*4096(1024Kb)

* GB2312_H1616_FangSong      仿宋          256Kb          1408*4096          65*4096(260Kb)
* GB2312_H2424_FangSong      仿宋          576Kb          1474*4096         145*4096(580Kb)
* GB2312_H3232_FangSong      仿宋         1023Kb          1620*4096         256*4096(1024Kb)

* GB2312_H1616_XinSong       新宋体        256Kb          1877*4096          65*4096(260Kb)
* GB2312_H2424_XinSong       新宋体        576Kb          1943*4096         145*4096(580Kb)
* GB2312_H3232_XinSong       新宋体       1023Kb          2089*4096         256*4096(1024Kb)

* GB2312_H1616_YaHei         微软雅黑      256Kb          2346*4096          65*4096(260Kb)
* GB2312_H2424_YaHei         微软雅黑      576Kb          2412*4096         145*4096(580Kb)
* GB2312_H3232_YaHei         微软雅黑     1023Kb          2558*4096         256*4096(1024Kb)
********************************************************************************************/

/**
* @note
*        1Byte = 8bit ,  1Kb = 1024Byte
* 16M字节(Byte)，128Mbit，空间范围0X000000 ~ 0XFFFFFF
* 分256块:(64Kb)
*         每块16个扇区(4Kb)
*                     每个扇区16页(256B)
*/
// 字体控制
enum Current_Font
{
    SongTi,     // 0 宋体 ----（默认字体-调为0级）
    KaiTi,      // 1 楷体
    HYMaqiduo,  // 2 汉仪玛奇朵 65简
    HYyoukai,   // 3 汉仪有楷 65简
    GongfanMFT, // 4 龚帆免费体2.0
    SanjiHKJT_C,// 5 三极行楷简体-粗
    Font_Number,// 6 总字体数量
};


//// 字体控制
//enum Current_Font
//{
//  SongTi,     // 0 宋体 ----（默认字体-调为0级）
//  HeiTi,      // 1 黑体
//  KaiTi,      // 2 楷体
//  FangSong,   // 3 仿宋
//  XinSong,    // 4 新宋体
//  YaHei,      // 5 微软雅黑
//  Font_Number,// 6 总字体数量
//};


#define ADD_GB2312_H1616_SongTi           1*4096
#define ADD_GB2312_H2424_SongTi          67*4096
#define ADD_GB2312_H3232_SongTi         213*4096

#define ADD_GB2312_H1616_KaiTi          470*4096
#define ADD_GB2312_H2424_KaiTi          536*4096
#define ADD_GB2312_H3232_KaiTi          682*4096

#define ADD_GB2312_H1616_HYMaqiduo      939*4096
#define ADD_GB2312_H2424_HYMaqiduo     1005*4096
#define ADD_GB2312_H3232_HYMaqiduo     1151*4096

#define ADD_GB2312_H1616_HYyoukai      1408*4096
#define ADD_GB2312_H2424_HYyoukai      1474*4096
#define ADD_GB2312_H3232_HYyoukai      1620*4096

#define ADD_GB2312_H1616_GongfanMFT    1877*4096
#define ADD_GB2312_H2424_GongfanMFT    1943*4096
#define ADD_GB2312_H3232_GongfanMFT    2089*4096

#define ADD_GB2312_H1616_SanjiHKJT_C   2346*4096
#define ADD_GB2312_H2424_SanjiHKJT_C   2412*4096
#define ADD_GB2312_H3232_SanjiHKJT_C   2558*4096



/*获取字库的函数*/
//定义获取中文字符字模数组的函数名，ucBuffer为存放字模数组名，usChar为中文字符（国标码）
uint8_t  GetGBKCode_from_EXFlash(uint8_t *pBuffer, uint16_t c, uint8_t f_size, uint8_t myFont);


void EPD_First_Init(void);
void Display_up_handle(bool DisImage_Control);
void Display_down_handle(void);

void SSD1677_DispChar_EN(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, const char cChar);
void SSD1677_DispChar_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint16_t usChar);
void SSD1677_DispString_EN_CH(uint16_t usX, uint16_t usY, uint16_t Color, uint8_t f_size, uint8_t myFont, uint8_t z_space, char *pStr);
void SSD1677_Display_Page(uint16_t usX, uint16_t usY, uint8_t f_size, uint8_t myFont, uint8_t wordspace, uint8_t rowLedge, char *pStr);

#endif /* BSP_E-INK */

