#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include "rtconfig.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "epd_pin_defs.h"
#include "bsp_io_expansion.h"


static struct rt_i2c_bus_device *p_i2c_bus = NULL;



#define FP9931_ADDR         0x18
#define FP9931_REG_TMST     0x00
#define FP9931_REG_VCOM     0x01
#define FP9931_REG_VP_VN    0x02
#define FP9931_REG_PWR_DELAY 0x03
#define FP9931_REG_CTRL1    0x0B
#define FP9931_REG_CTRL2    0x0C

#define FP9931_SS_TIME_3MS  0x00 
#define FP9931_VPOS_15V     0x28 
#define FP9931_VNEG_15V     0x28
#define FP9931_PWR_DELAY_DEF 0x00
#define FP9931_V3P3_EN      0x01
#define FP9931_VP_VN_CL_5A  0x0F 


static rt_err_t i2c_write(uint8_t reg, rt_uint8_t data)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;
    rt_uint8_t buf[2] = {reg, data};

    msgs.addr  = FP9931_ADDR;    /* slave address */
    msgs.flags = RT_I2C_WR;        /* write flag */
    msgs.buf   = buf;              /* Send data pointer */
    msgs.len   = 2;

    if (rt_i2c_transfer(p_i2c_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}


void fp9931_power_sequence_set(void)
{
    uint8_t dat;

    dat = FP9931_PWR_DELAY_DEF; 
    i2c_write(FP9931_REG_PWR_DELAY, dat);
    dat = (FP9931_SS_TIME_3MS << 6) | FP9931_V3P3_EN;
    i2c_write(FP9931_REG_CTRL1, dat);
    dat = FP9931_VP_VN_CL_5A; 
    i2c_write(FP9931_REG_CTRL2, dat);
}


void fp9931_vposvneg_set(void)
{
    i2c_write(FP9931_REG_VP_VN, FP9931_VPOS_15V);    //set VPOS and VNEG voltage to 15V
}


void fp9931_vcom_set(uint16_t vcom_voltage)
{

    if (vcom_voltage > 5000) vcom_voltage = 5000;
    if (vcom_voltage < 0)   vcom_voltage = 0;
    uint8_t dat = (vcom_voltage * 255) / 5000;

    i2c_write(FP9931_REG_VCOM, dat);
    rt_kprintf("FP9931 VCOM set: %dmV → dat=0x%02X\n", vcom_voltage, dat);
}

void fp9931_enable(void)
{
    EBC_PMIC_Control(1);
}

void fp9931_disable(void)
{
    EBC_PMIC_Control(0);
}

void fp9931_init(uint16_t vcom_voltage)
{
    uint16_t dir = (0x00 << 8) | 0x26;
    uint8_t chg_status = 0, vbus_status = 0, ebc_good = 0;

    AW9523B_Init(dir);
    AW9523B_Write_Pin(P0_3_CTP_VDD_EN, 1);

    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 1);
    HAL_Delay(10);
    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 0);
    HAL_Delay(50);
    AW9523B_Write_Pin(8 + P1_5_CTP_RST, 1);

    struct rt_i2c_configuration configuration =
    {
        .mode = 0,
        .addr = 0,
        .timeout = 500,
        .max_hz  = 400000,
    };
    p_i2c_bus = (struct rt_i2c_bus_device *)rt_device_find("i2c1");

    if (rt_device_open((rt_device_t)p_i2c_bus, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("FP9931 init: Open I2C bus failed\n");
        return;
    }
    rt_i2c_configure(p_i2c_bus, &configuration);

    fp9931_disable();
    HAL_Delay(10);


    fp9931_power_sequence_set();
    fp9931_vposvneg_set();
    fp9931_vcom_set(vcom_voltage);
}