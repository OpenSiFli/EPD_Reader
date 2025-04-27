#ifndef __GUI_PAINT_H
#define __GUI_PAINT_H

#include <board.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> //memset()
#include <math.h>

/**
 * Image attributes
**/



#define UBYTE   uint8_t
#define uint16_t   uint16_t
#define UDOUBLE uint32_t

typedef struct
{
    UBYTE *Image;
    uint16_t Width;
    uint16_t Height;
    uint16_t WidthMemory;
    uint16_t HeightMemory;
    uint16_t Color;
    uint16_t Rotate;
    uint16_t Mirror;
    uint16_t WidthByte;
    uint16_t HeightByte;
} PAINT;
extern PAINT Paint;

/**
 * Display rotate 显示旋转
**/
#define ROTATE_0            0
#define ROTATE_90           90
#define ROTATE_180          180
#define ROTATE_270          270

/**
 * Display Flip 显示翻转
**/
typedef enum
{
    MIRROR_NONE  = 0x00,       // 不翻转
    MIRROR_HORIZONTAL = 0x01,  // 水平翻转
    MIRROR_VERTICAL = 0x02,    // 垂直翻转
    MIRROR_ORIGIN = 0x03,      //
} MIRROR_IMAGE;
#define MIRROR_IMAGE_DFT MIRROR_NONE

/**
 * image color
**/
#define WHITE          0xFF
#define BLACK          0x00
#define RED            BLACK

#define IMAGE_BACKGROUND    WHITE
#define FONT_FOREGROUND     BLACK
#define FONT_BACKGROUND     WHITE

/**
 * The size of the point 点的大小
**/
typedef enum
{
    DOT_PIXEL_1X1  = 1,     // 1 x 1
    DOT_PIXEL_2X2,          // 2 X 2
    DOT_PIXEL_3X3,          // 3 X 3
    DOT_PIXEL_4X4,          // 4 X 4
    DOT_PIXEL_5X5,          // 5 X 5
    DOT_PIXEL_6X6,          // 6 X 6
    DOT_PIXEL_7X7,          // 7 X 7
    DOT_PIXEL_8X8,          // 8 X 8
} DOT_PIXEL;
#define DOT_PIXEL_DFT  DOT_PIXEL_1X1  //Default dot pilex 默认点堆

/**
 * Point size fill style 点大小填充样式
**/
typedef enum
{
    DOT_FILL_AROUND  = 1,       // dot pixel 1 x 1  环绕，围绕（显示面积最大）
    DOT_FILL_RIGHTUP,           // dot pixel 2 X 2  正直的
} DOT_STYLE;
#define DOT_STYLE_DFT  DOT_FILL_RIGHTUP  //Default dot pilex

/**
 * Line style, solid or dashed 线条样式，实线或虚线
**/
typedef enum
{
    LINE_STYLE_SOLID = 0,
    LINE_STYLE_DOTTED,
} LINE_STYLE;

/**
 * Whether the graphic is filled 是否填充图形
**/
typedef enum
{
    DRAW_FILL_EMPTY = 0,  // 不填充
    DRAW_FILL_FULL,       // 填充
} DRAW_FILL;

/**
 * Custom structure of a time attribute 时间属性的自定义结构
**/
typedef struct
{
    uint16_t Year;  //0000
    UBYTE  Month; //1 - 12
    UBYTE  Day;   //1 - 30
    UBYTE  Hour;  //0 - 23
    UBYTE  Min;   //0 - 59
    UBYTE  Sec;   //0 - 59
} PAINT_TIME;
extern PAINT_TIME sPaint_time;

//init and Clear
void Paint_NewImage(UBYTE *image, uint16_t Width, uint16_t Height, uint16_t Rotate, uint16_t Color);
void Paint_SelectImage(UBYTE *image);
void Paint_SetRotate(uint16_t Rotate);
void Paint_SetMirroring(UBYTE mirror);
void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color);

void Paint_Clear(uint16_t Color);
void Paint_ClearWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color);

//Drawing
void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color, DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_FillWay);
void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color, LINE_STYLE Line_Style, DOT_PIXEL Dot_Pixel);
void Paint_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color, DRAW_FILL Filled, DOT_PIXEL Dot_Pixel);
void Paint_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius, uint16_t Color, DRAW_FILL Draw_Fill, DOT_PIXEL Dot_Pixel);
void DrawArcRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t Color, DRAW_FILL Draw_Fill, int8_t r);

//pic
void Paint_DrawBitMap(const unsigned char *image_buffer);


#endif





