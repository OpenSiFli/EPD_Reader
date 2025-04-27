/*
  ******************************************************************************
  * @file   main.c
  * @author Sifli software development team
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2021 - 2021,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include "rtconfig.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "mem_section.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"

#ifdef EPD_DRIVER_SOFTWARE

#define FRAME_END_LEN       32      //每次刷新所需帧数
#define DISPLAY_LINE_CLOCKS   362     //每列刷新所需次数，362*4像素
#define DISPLAY_ROWS   1072


#define  LOG_POINTS  rt_kprintf("%s, %d \r\n",__FUNCTION__, __LINE__)


int page = 1;
unsigned int column = 0;                                //电子纸行驱动用 变量
unsigned int old_data;                                  //电子纸行驱动用 变量
unsigned int new_data;                                  //电子纸行驱动用 变量
unsigned char g_dest_data[DISPLAY_LINE_CLOCKS];                         //送到电子纸的一行数据缓存
unsigned char wave_end_table[256][FRAME_END_LEN] = {0};
uint16_t oedopm_vcom    = 2100;                         //模组的VCOM电压(2100代表-2.10V)
static I2C_HandleTypeDef i2c_Handle = {0};


//反显刷图片 改善边缘扩散专用波形
const unsigned char wave_end[4][FRAME_END_LEN] =
{
    0, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, //GC3->GC0
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //GC3->GC1
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //GC3->GC2
    0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, //GC3->GC3
};


//1像素1bit表示，转为 1像素用2bit表示
unsigned char OneBitToTwoBit[16] =
{
    0x00,   //0000B
    0x03,   //0001B
    0x0c,   //0010B
    0x0f,   //0011B
    0x30,   //0100B
    0x33,   //0101B
    0x3c,   //0110B
    0x3f,   //0111B

    0xc0,   //1000B
    0xc3,   //1001B
    0xcc,   //1010B
    0xcf,   //1011B
    0xf0,   //1100B
    0xf3,   //1101B
    0xfc,   //1110B
    0xff,   //1111B
};






// void pin_init(void)
// {
// //edp gpio pin
// const uint32_t pin_out[] =
// {
//     TPS_WAKEUP,
//     TPS_PWRCOM,
//     TPS_PWRUP,
//     EPD_LE,
//     EPD_OE,
//     EPD_STV,
//     EPD_CPV,
//     EPD_GMODE,
//     EPD_CLK,
//     EPD_SPH,
//     EPD_D0,
//     EPD_D1,
//     EPD_D2,
//     EPD_D3,
//     EPD_D4,
//     EPD_D5,
//     EPD_D6,
//     EPD_D7,
// };
//     int pin_num = sizeof(pin_out) / sizeof(pin_out[0]);

//     //epd pin init
//     for (int i = 0; i < pin_num; i++)
//     {
//         HAL_PIN_Set(PAD_PA00 + pin_out[i], GPIO_A0 + pin_out[i], PIN_NOPULL, 1);
//     }

//     HAL_RCC_EnableModule(RCC_MOD_GPIO1); // GPIO clock enable
//     GPIO_InitTypeDef GPIO_InitStruct;

//     for (int i = 0; i < pin_num; i++)
//     {
//         GPIO_InitStruct.Pin = pin_out[i];
//         GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT;
//         GPIO_InitStruct.Pull = GPIO_PULLDOWN;
//         HAL_GPIO_Init(hwp_gpio1, &GPIO_InitStruct);
//         HAL_GPIO_WritePin(hwp_gpio1, pin_out[i], GPIO_PIN_RESET);
//     }


//     //i2c init
//     HAL_StatusTypeDef ret;

// #define EXAMPLE_I2C I2C2 // i2c number of cpu
// #define EXAMPLE_I2C_IRQ I2C2_IRQn // i2c number of interruput when using interrupte mode

//     HAL_RCC_EnableModule(RCC_MOD_I2C2); // enable i2c2
//     HAL_PIN_Set(PAD_PA00 + TPS_SCL, I2C2_SCL, PIN_PULLUP, 1); // i2c io select
//     HAL_PIN_Set(PAD_PA00 + TPS_SDA, I2C2_SDA, PIN_PULLUP, 1);

//     i2c_Handle.Instance = EXAMPLE_I2C;
//     i2c_Handle.Mode = HAL_I2C_MODE_MASTER; // i2c master mode
//     i2c_Handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT; // i2c 7bits device address mode
//     i2c_Handle.Init.ClockSpeed = 400000; // i2c speed (hz)
//     i2c_Handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
//     ret = HAL_I2C_Init(&i2c_Handle);
//     if (ret == 0)rt_kprintf("i2c_init success\n");
//     else rt_kprintf("i2c_init success\n");

//     // key init
//     HAL_PIN_Set(PAD_PA00 + EPD_KEY, GPIO_A0 + EPD_KEY, PIN_PULLDOWN, 1);

//     GPIO_InitStruct.Pin = EPD_KEY;
//     GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING; //Set interrupt to trigger on raising and falling edge
//     GPIO_InitStruct.Pull = GPIO_PULLDOWN;
//     HAL_GPIO_Init(hwp_gpio1, &GPIO_InitStruct);
//     HAL_NVIC_SetPriority(GPIO1_IRQn, 2, 1); // Configure NVIC priority

//     HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);
//     HAL_NVIC_EnableIRQ(GPIO1_IRQn); //Enable GPIO interrupt in the NVIC interrupt controller.
// }



void epd_wave_table(void)
{
    int frame, num;
    unsigned char tmp, value;

    //wave_end_table
    for (frame = 0; frame < FRAME_END_LEN; frame++)
    {
        for (num = 0; num < 256; num++)
        {
            tmp = 0;
            tmp = wave_end[(num >> 6) & 0x3][frame];

            tmp = tmp << 2;
            tmp &= 0xfffc;
            tmp |= wave_end[(num >> 4) & 0x3][frame];

            tmp = tmp << 2;
            tmp &= 0xfffc;
            tmp |= wave_end[(num >> 2) & 0x3][frame];

            tmp = tmp << 2;
            tmp &= 0xfffc;
            tmp |= wave_end[(num) & 0x3][frame];

            value = 0;
            value = (tmp << 6) & 0xc0;
            value += (tmp << 2) & 0x30;
            value += (tmp >> 2) & 0x0c;
            value += (tmp >> 6) & 0x03;
            wave_end_table[num][frame] = value;
        }
    }
}


void epd_Init(void)
{
    oedtps_vcom_disable();
    oedtps_source_gate_disable();

    EPD_LE_L_hs();
    EPD_CLK_L_hs();
    EPD_OE_L_hs();
    EPD_SPH_H_hs();
    EPD_STV_H_hs();
    EPD_CPV_L_hs();
    EPD_GMODE_H_hs();

    epd_wave_table();
}



void epd_start_scan(void)
{
    EPD_STV_H_hs();
    EPD_STV_L_hs();
    HAL_Delay_us(1);
    EPD_CPV_L_hs();    //DCLK跑1个时钟
    HAL_Delay_us(1);
    EPD_CPV_H_hs();
    HAL_Delay_us(1);
    EPD_STV_H_hs();
    HAL_Delay_us(1);
    EPD_CPV_L_hs();    //DCLK跑1个时钟
    HAL_Delay_us(1);
    EPD_CPV_H_hs();
    HAL_Delay_us(1);
    EPD_CPV_L_hs();    //DCLK跑1个时钟
    HAL_Delay_us(1);
    EPD_CPV_H_hs();
    HAL_Delay_us(1);
    EPD_CPV_L_hs();
    EPD_LE_H_hs();
    HAL_Delay_us(1);
    EPD_LE_L_hs();
    HAL_Delay_us(1);
    EPD_OE_H_hs();
    EPD_CPV_H_hs();
}



uint8_t reverse_bits(uint8_t data)
{
    data = (data << 4) | (data >> 4);                         // 交换前四位和后四位
    data = ((data & 0xCC) >> 2) | ((data & 0x33) << 2);       // 交换每四位中的两位
    data = ((data & 0xAA) >> 1) | ((data & 0x55) << 1);       // 交换每两位中的一位
    return data;
}


void epd_load_pic(uint8_t *new_pic, uint8_t frame)
{
    int j = 0;
    for (int i = 0; i < DISPLAY_LINE_CLOCKS / 2; i++)
    {
        uint8_t ret;
        ret = reverse_bits(new_pic[i]);
        g_dest_data[j++] = wave_end_table[OneBitToTwoBit[(ret & 0x0f)]][frame]; //1字节里低4位的像素点
        g_dest_data[j++] = wave_end_table[OneBitToTwoBit[(ret >> 4)]][frame];   //1字节里高4位的像素点
    }
}

void epd_send_row_data(uint8_t *pic)                        //送数据
{
    EPD_CPV_L_hs();
    EPD_OE_L_hs();
    EPD_LE_H_hs();
    HAL_Delay_us(1);
    EPD_LE_L_hs();
    HAL_Delay_us(1);
    EPD_OE_H_hs();
    EPD_CPV_H_hs();
    EPD_SPH_L_hs();
    EPD_CLK_L_hs();                                             //STL信号中需要一个时钟信号
    EPD_CLK_H_hs();
    EPD_SPH_H_hs();

    old_data = (hwp_gpio1->DOR) & 0xFFFFFF00;

    for (column = 0; column < DISPLAY_LINE_CLOCKS; column++)                //写一行数据
    {
        (hwp_gpio1->DOR) = old_data  | pic[column];
        EPD_CLK_L_hs();
        EPD_CLK_H_hs();
    }
}

void epd_display_pic(unsigned char *ptr)
{
    unsigned char frame;
    unsigned int line;
    unsigned char *ptrnext;

    __disable_irq();

    oedtps_source_gate_enable();
    HAL_Delay(50);
    oedtps_vcom_enable();
    HAL_Delay(10);

    EPD_GMODE_H_hs();

    for (frame = 0; frame < FRAME_END_LEN; frame++)
    {
        epd_start_scan();
        for (line = 0; line < 1072; line++)                 //共有1072列数据
        {
            epd_load_pic(ptr + (line * DISPLAY_LINE_CLOCKS / 2), frame);   //(line*DISPLAY_LINE_CLOCKS/2)传完一列数据后传下一列，一列数据有
            epd_send_row_data(g_dest_data);
        }
        epd_load_pic(ptr + ((line - 1)*DISPLAY_LINE_CLOCKS / 2), frame);
        epd_send_row_data(g_dest_data);                      //最后一行还需GATE CLK,故再传一行没用数据

        EPD_CPV_L_hs();
        EPD_OE_L_hs();
    }

    EPD_GMODE_L_hs();
    EPD_LE_L_hs();
    EPD_CLK_L_hs();
    EPD_OE_L_hs();
    EPD_SPH_H_hs();
    EPD_STV_H_hs();
    EPD_CPV_L_hs();

    HAL_Delay(10);
    oedtps_vcom_disable();
    HAL_Delay(10);
    oedtps_source_gate_disable();

    __enable_irq();
}

#endif /*EPD_DRIVER_SOFTWARE*/


/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/

