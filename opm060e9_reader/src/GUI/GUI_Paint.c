#include "GUI_Paint.h"



PAINT Paint;
/******************************************************************************
function:   Create Image
parameter:
    image   :   Pointer to the image cache
    width   :   The width of the picture
    Height  :   The height of the picture
    Color   :   Whether the picture is inverted
******************************************************************************/

void Paint_NewImage(UBYTE *image, uint16_t Width, uint16_t Height, uint16_t Rotate, uint16_t Color)
{
    Paint.Image = NULL;
    Paint.Image = image;

    Paint.WidthMemory = Width;
    Paint.HeightMemory = Height;
    Paint.Color = Color;
    Paint.WidthByte = (Width % 8 == 0) ? (Width / 8) : (Width / 8 + 1);
    Paint.HeightByte = Height;
    //printf("WidthByte = %d, HeightByte = %d\r\n", Paint.WidthByte, Paint.HeightByte);
    //printf(" EPD_WIDTH / 8 = %d\r\n",  122 / 8);

    Paint.Rotate = Rotate;
    Paint.Mirror = MIRROR_NONE;

    if (Rotate == ROTATE_0 || Rotate == ROTATE_180)
    {
        Paint.Width = Width;
        Paint.Height = Height;
    }
    else
    {
        Paint.Width = Height;
        Paint.Height = Width;
    }
}

/******************************************************************************
function:   Select Image
parameter:
    image   :   Pointer to the image cache
******************************************************************************/
void Paint_SelectImage(UBYTE *image)
{
    Paint.Image = image;
}

/******************************************************************************
function:   Select Image Rotate
parameter:
    Rotate   :   0,90,180,270
******************************************************************************/
void Paint_SetRotate(uint16_t Rotate)
{
    if (Rotate == ROTATE_0 || Rotate == ROTATE_90 || Rotate == ROTATE_180 || Rotate == ROTATE_270)
    {
        //Debug("Set image Rotate %d\r\n", Rotate);
        Paint.Rotate = Rotate;
    }
    else
    {
        //Debug("rotate = 0, 90, 180, 270\r\n");
    }
}

/******************************************************************************
function:   Select Image mirror
parameter:
    mirror   :       Not mirror,Horizontal mirror,Vertical mirror,Origin mirror
******************************************************************************/
void Paint_SetMirroring(UBYTE mirror)
{
    if (mirror == MIRROR_NONE || mirror == MIRROR_HORIZONTAL ||
            mirror == MIRROR_VERTICAL || mirror == MIRROR_ORIGIN)
    {
        //Debug("mirror image x:%s, y:%s\r\n",(mirror & 0x01)? "mirror":"none", ((mirror >> 1) & 0x01)? "mirror":"none");
        Paint.Mirror = mirror;
    }
    else
    {
        //Debug("mirror should be MIRROR_NONE, MIRROR_HORIZONTAL, \
        MIRROR_VERTICAL or MIRROR_ORIGIN\r\n");
    }
}

/******************************************************************************
function:   Draw Pixels
parameter:
    Xpoint  :   At point X
    Ypoint  :   At point Y
    Color   :   Painted colors
******************************************************************************/
void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color)
{
    uint16_t X, Y;
    UDOUBLE Addr;
    UBYTE Rdata;
    if (Xpoint > Paint.Width || Ypoint > Paint.Height)
    {
        //Debug("Exceeding display boundaries\r\n");
        return;
    }


    switch (Paint.Rotate)  // 设置屏幕是否旋转
    {
    case 0:
        X = Xpoint;
        Y = Ypoint;
        break;
    case 90:
        X = Paint.WidthMemory - Ypoint - 1;
        Y = Xpoint;
        break;
    case 180:
        X = Paint.WidthMemory - Xpoint - 1;
        Y = Paint.HeightMemory - Ypoint - 1;
        break;
    case 270:
        X = Ypoint;
        Y = Paint.HeightMemory - Xpoint - 1;
        break;

    default:
        return;
    }

    switch (Paint.Mirror)  // 控制是否翻转
    {
    case MIRROR_NONE:
        break;
    case MIRROR_HORIZONTAL:
        X = Paint.WidthMemory - X - 1;
        break;
    case MIRROR_VERTICAL:
        Y = Paint.HeightMemory - Y - 1;
        break;
    case MIRROR_ORIGIN:
        X = Paint.WidthMemory - X - 1;
        Y = Paint.HeightMemory - Y - 1;
        break;
    default:
        return;
    }

    if (X > Paint.WidthMemory || Y > Paint.HeightMemory)
    {
        //Debug("Exceeding display boundaries\r\n");
        return;
    }

    Addr = X / 8 + Y * Paint.WidthByte;
    Rdata = Paint.Image[Addr];
    if (Color == BLACK)
        Paint.Image[Addr] = Rdata & ~(0x80 >> (X % 8));
    else
        Paint.Image[Addr] = Rdata | (0x80 >> (X % 8));
}

/******************************************************************************
function:   Clear the color of the picture
parameter:
    Color   :   Painted colors
******************************************************************************/
void Paint_Clear(uint16_t Color)
{
    uint16_t X, Y;
    UDOUBLE Addr;
    for (Y = 0; Y < Paint.HeightByte; Y++)
    {
        for (X = 0; X < Paint.WidthByte; X++)   //8 pixel =  1 byte
        {
            Addr = X + Y * Paint.WidthByte;
            Paint.Image[Addr] = Color;
        }
    }
}

/******************************************************************************
function:   Clear the color of a window
parameter:
    Xstart :   x starting point
    Ystart :   Y starting point
    Xend   :   x end point
    Yend   :   y end point
******************************************************************************/
void Paint_ClearWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color)
{
    uint16_t X, Y;
    for (Y = Ystart; Y < Yend; Y++)
    {
        for (X = Xstart; X < Xend; X++)  //8 pixel =  1 byte
        {
            Paint_SetPixel(X, Y, Color);
        }
    }
}

/******************************************************************************
function:   Draw Point(Xpoint, Ypoint) Fill the color
parameter:
    Xpoint      :   The Xpoint coordinate of the point
    Ypoint      :   The Ypoint coordinate of the point
    Color       :   Set color
    Dot_Pixel   :   point size
******************************************************************************/
void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color,
                     DOT_PIXEL Dot_Pixel, DOT_STYLE DOT_STYLE)
{
    int16_t XDir_Num, YDir_Num;
    if (Xpoint > Paint.Width || Ypoint > Paint.Height)
    {
        //Debug("Paint_DrawPoint Input exceeds the normal display range\r\n");
        return;
    }


    if (DOT_STYLE == DOT_FILL_AROUND)
    {
        for (XDir_Num = 0; XDir_Num < 2 * Dot_Pixel - 1; XDir_Num++)
        {
            for (YDir_Num = 0; YDir_Num < 2 * Dot_Pixel - 1; YDir_Num++)
            {
                if (Xpoint + XDir_Num - Dot_Pixel < 0 || Ypoint + YDir_Num - Dot_Pixel < 0)
                    break;
                Paint_SetPixel(Xpoint + XDir_Num - Dot_Pixel, Ypoint + YDir_Num - Dot_Pixel, Color);
            }
        }
    }
    else
    {
        for (XDir_Num = 0; XDir_Num <  Dot_Pixel; XDir_Num++)
        {
            for (YDir_Num = 0; YDir_Num <  Dot_Pixel; YDir_Num++)
            {
                Paint_SetPixel(Xpoint + XDir_Num - 1, Ypoint + YDir_Num - 1, Color);
            }
        }
    }
}

/******************************************************************************
function:   Draw a line of arbitrary slope
parameter:
    Xstart ：Starting Xpoint point coordinates
    Ystart ：Starting Xpoint point coordinates
    Xend   ：End point Xpoint coordinate
    Yend   ：End point Ypoint coordinate
    Color  ：The color of the line segment
******************************************************************************/
void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,
                    uint16_t Color, LINE_STYLE Line_Style, DOT_PIXEL Dot_Pixel)
{
    uint16_t Xpoint, Ypoint;
    int dx, dy;
    int XAddway, YAddway;
    int Esp;
    char Dotted_Len;
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
            Xend > Paint.Width || Yend > Paint.Height)
    {
        //Debug("Paint_DrawLine Input exceeds the normal display range\r\n");
        return;
    }

    Xpoint = Xstart;
    Ypoint = Ystart;
    dx = (int)Xend - (int)Xstart >= 0 ? Xend - Xstart : Xstart - Xend;
    dy = (int)Yend - (int)Ystart <= 0 ? Yend - Ystart : Ystart - Yend;

    // Increment direction, 1 is positive, -1 is counter;
    XAddway = Xstart < Xend ? 1 : -1;
    YAddway = Ystart < Yend ? 1 : -1;

    //Cumulative error
    Esp = dx + dy;
    Dotted_Len = 0;

    for (;;)
    {
        Dotted_Len++;
        //Painted dotted line, 2 point is really virtual
        if (Line_Style == LINE_STYLE_DOTTED && Dotted_Len % 3 == 0)
        {
            //Debug("LINE_DOTTED\r\n");
            Paint_DrawPoint(Xpoint, Ypoint, IMAGE_BACKGROUND, Dot_Pixel, DOT_STYLE_DFT);
            Dotted_Len = 0;
        }
        else
        {
            Paint_DrawPoint(Xpoint, Ypoint, Color, Dot_Pixel, DOT_STYLE_DFT);
        }
        if (2 * Esp >= dy)
        {
            if (Xpoint == Xend)
                break;
            Esp += dy;
            Xpoint += XAddway;
        }
        if (2 * Esp <= dx)
        {
            if (Ypoint == Yend)
                break;
            Esp += dx;
            Ypoint += YAddway;
        }
    }
}

/******************************************************************************
function:   Draw a rectangle
parameter:
    Xstart ：Rectangular  Starting Xpoint point coordinates
    Ystart ：Rectangular  Starting Xpoint point coordinates
    Xend   ：Rectangular  End point Xpoint coordinate
    Yend   ：Rectangular  End point Ypoint coordinate
    Color  ：The color of the Rectangular segment
    Filled : Whether it is filled--- 1 solid 0：empty
******************************************************************************/
void Paint_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,
                         uint16_t Color, DRAW_FILL Filled, DOT_PIXEL Dot_Pixel)
{
    uint16_t Ypoint;
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
            Xend > Paint.Width || Yend > Paint.Height)
    {
        //Debug("Input exceeds the normal display range\r\n");
        return;
    }

    if (Filled)
    {
        for (Ypoint = Ystart; Ypoint < Yend; Ypoint++)
        {
            Paint_DrawLine(Xstart, Ypoint, Xend, Ypoint, Color, LINE_STYLE_SOLID, Dot_Pixel);
        }
    }
    else
    {
        Paint_DrawLine(Xstart, Ystart, Xend, Ystart, Color, LINE_STYLE_SOLID, Dot_Pixel);
        Paint_DrawLine(Xstart, Ystart, Xstart, Yend, Color, LINE_STYLE_SOLID, Dot_Pixel);
        Paint_DrawLine(Xend, Yend, Xend, Ystart, Color, LINE_STYLE_SOLID, Dot_Pixel);
        Paint_DrawLine(Xend, Yend, Xstart, Yend, Color, LINE_STYLE_SOLID, Dot_Pixel);
    }
}

/******************************************************************************
function:   Use the 8-point method to draw a circle of the
            specified size at the specified position->
parameter:
    X_Center  ：Center X coordinate
    Y_Center  ：Center Y coordinate
    Radius    ：circle Radius
    Color     ：The color of the ：circle segment
    Filled    : Whether it is filled: 1 filling 0：Do not
******************************************************************************/
void Paint_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius,
                      uint16_t Color, DRAW_FILL  Draw_Fill, DOT_PIXEL Dot_Pixel)
{
    int16_t Esp, sCountY;
    int16_t XCurrent, YCurrent;
    if (X_Center > Paint.Width || Y_Center >= Paint.Height)
    {
        //Debug("Paint_DrawCircle Input exceeds the normal display range\r\n");
        return;
    }

    //Draw a circle from(0, R) as a starting point
    XCurrent = 0;
    YCurrent = Radius;

    //Cumulative error,judge the next point of the logo
    Esp = 3 - (Radius << 1);
    if (Draw_Fill == DRAW_FILL_FULL)
    {
        while (XCurrent <= YCurrent)    //Realistic circles
        {
            for (sCountY = XCurrent; sCountY <= YCurrent; sCountY ++)
            {
                Paint_DrawPoint(X_Center + XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//1
                Paint_DrawPoint(X_Center - XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//2
                Paint_DrawPoint(X_Center - sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//3
                Paint_DrawPoint(X_Center - sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//4
                Paint_DrawPoint(X_Center - XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//5
                Paint_DrawPoint(X_Center + XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//6
                Paint_DrawPoint(X_Center + sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//7
                Paint_DrawPoint(X_Center + sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            }
            if (Esp < 0)
                Esp += 4 * XCurrent + 6;
            else
            {
                Esp += 10 + 4 * (XCurrent - YCurrent);
                YCurrent --;
            }
            XCurrent ++;
        }
    }
    else     //Draw a hollow circle
    {
        while (XCurrent <= YCurrent)
        {
            Paint_DrawPoint(X_Center + XCurrent, Y_Center + YCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//1
            Paint_DrawPoint(X_Center - XCurrent, Y_Center + YCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//2
            Paint_DrawPoint(X_Center - YCurrent, Y_Center + XCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//3
            Paint_DrawPoint(X_Center - YCurrent, Y_Center - XCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//4
            Paint_DrawPoint(X_Center - XCurrent, Y_Center - YCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//5
            Paint_DrawPoint(X_Center + XCurrent, Y_Center - YCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//6
            Paint_DrawPoint(X_Center + YCurrent, Y_Center - XCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//7
            Paint_DrawPoint(X_Center + YCurrent, Y_Center + XCurrent, Color, Dot_Pixel, DOT_STYLE_DFT);//0

            if (Esp < 0)
                Esp += 4 * XCurrent + 6;
            else
            {
                Esp += 10 + 4 * (XCurrent - YCurrent);
                YCurrent --;
            }
            XCurrent ++;
        }
    }
}


/**
 * @brief  在指定位置画指定弧度和半径的圆弧
 * @note   无
 * @retval 无
 */
void Drawarc(int x, int y, int a, int b, uint16_t Color, DRAW_FILL Draw_Fill, uint16_t r)
{
    double rad, x_tp, y_tp, i;
    i = a;
    for (; i < b; i = i + 0.8) //此处写0.2是为了提高精度 不然半径过大有时会有虚线
    {
        rad = 0.01745 * i;
        x_tp = r * cos(rad) + x;
        y_tp = -r * sin(rad) + y;

        if (Draw_Fill == DRAW_FILL_EMPTY) // 不填充图形
        {
            Paint_DrawPoint((uint16_t)x_tp, (uint16_t)y_tp, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
        }
        else // 填充图形
        {
            if (x_tp > x)
                Paint_DrawLine(x, (uint16_t)y_tp, (uint16_t)x_tp, (uint16_t)y_tp, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
            if (x_tp < x)
                Paint_DrawLine((uint16_t)x_tp, (uint16_t)y_tp, x, (uint16_t)y_tp, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
            if (x_tp == x)
                Paint_DrawPoint((uint16_t)x_tp, (uint16_t)y_tp, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
        }
    }
}


/**
 * @brief  在xy矩形内 画半径r的圆角矩形 画笔颜色为c
 * @note   无
 * @retval 无
 */
void DrawArcRect(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t Color, DRAW_FILL Draw_Fill, int8_t r)
{
    uint16_t i;

    if (Draw_Fill == DRAW_FILL_EMPTY) // 不填充图形
    {
        //先画4个没有圆角的矩形
        Paint_DrawLine(x1 + r, y1, x2 - r, y1, Color, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
        Paint_DrawLine(x1, y1 + r, x1, y2 - r, Color, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
        Paint_DrawLine(x1 + r, y2, x2 - r, y2, Color, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
        Paint_DrawLine(x2, y1 + r, x2, y2 - r, Color, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
    }
    else  // 填充图形
    {
        for (i = 0; i < y2 - y1; i++)
        {
            Paint_DrawLine(x1 + r, y1 + i, x2 - r, y1 + i, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
        }
        for (i = 0; i < y2 - r - (y1 + r); i++)
        {
            Paint_DrawLine(x1, y1 + r + i, x1 + r, y1 + r + i, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
            Paint_DrawLine(x2 - r, y1 + r + i, x2, y1 + r + i, BLACK, LINE_STYLE_SOLID, DOT_PIXEL_DFT);
        }
    }

    //再画四个圆角
    Drawarc(x1 + r, y1 + r, 90, 180, Color, Draw_Fill, r);
    Drawarc(x2 - r, y1 + r,  0, 90, Color, Draw_Fill, r);
    Drawarc(x1 + r, y2 - r, 180, 270, Color, Draw_Fill, r);
    Drawarc(x2 - r, y2 - r, 270, 360, Color, Draw_Fill, r);
}

/******************************************************************************
function:   Display monochrome bitmap
parameter:
    image_buffer ：A picture data converted to a bitmap
info:
    Use a computer to convert the image into a corresponding array,
    and then embed the array directly into Imagedata.cpp as a .c file.
******************************************************************************/
void Paint_DrawBitMap(const unsigned char *image_buffer)
{
    uint16_t x, y;
    UDOUBLE Addr = 0;

    for (y = 0; y < Paint.HeightByte; y++)
    {
        for (x = 0; x < Paint.WidthByte; x++)  //8 pixel =  1 byte
        {
            Addr = x + y * Paint.WidthByte;
            Paint.Image[Addr] = (unsigned char)image_buffer[Addr];
        }
    }
}





