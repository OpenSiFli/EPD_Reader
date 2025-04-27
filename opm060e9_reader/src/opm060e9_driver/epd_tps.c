#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <string.h>
#include "rtconfig.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "epd_pin_defs.h"


#define TI_REG_TMST_VALUE   0x00    //Thermistor value read by ADC
#define TI_REG_ENABLE       0x01    //Enable/disable bits for regulators
#define TI_REG_VADJ         0x02    //VPOS/VNEG voltage adjustment
#define TI_REG_VCOM1        0x03    //Voltage settings for VCOM
#define TI_REG_VCOM2        0x04    //Voltage settings for VCOM + control
#define TI_REG_INT_EN1      0x05    //Interrupt enable group1
#define TI_REG_INT_EN2      0x06    //Interrupt enable group2
#define TI_REG_INT1         0x07    //Interrupt group1
#define TI_REG_INT2         0x08    //Interrupt group2
#define TI_REG_DWNSEQ0      0x0B    //Power-down strobe assignment
#define TI_REG_TMST1        0x0D    //Thermistor configuration
#define TI_REG_TMST2        0x0E    //Thermistor hot temp set
#define TI_REG_PG           0x0F    //Power good status each rails
#define TI_REG_REVID        0x10    //Device revision ID information


#define I2C_NACK            0
#define I2C_M2S_ACK         1
#define I2C_S2M_ACK         2


#define TPS65185_ADDR       0x68    //TPS65185 device address 
#define TI_REG_UPSEQ0       0x09    //Power-up strobe assignment
#define TI_REG_UPSEQ1       0x0A    //Power-up sequence delay times
#define TI_REG_DWNSEQ1      0x0C    //Power-down sequence delay times


#define TI_STROBE1          0
#define TI_STROBE2          1
#define TI_STROBE3          2
#define TI_STROBE4          3

#define TI_UPLY3MS          0
#define TI_UPLY6MS          1
#define TI_UPLY9MS          2
#define TI_UPLY12MS         3

#define TI_DWNLY6MS         0
#define TI_DWNLY12MS        1
#define TI_DWNLY24MS        2
#define TI_DWNLY48MS        3

static I2C_HandleTypeDef i2c_Handle = {0};
static uint16_t oedopm_vcom    = 2100;                         //模组的VCOM电压(2100代表-2.10V)


void i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t dat)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2];

    __HAL_I2C_ENABLE(&i2c_Handle);  // for master, enable it before transmit

    // 3. write value to register
    buf[0] = reg_addr; // memory address to write
    buf[1] = dat; // data to write

    ret = HAL_I2C_Master_Transmit(&i2c_Handle, dev_addr, buf, 2, 1000);

    if (ret != 0)rt_kprintf("i2c_write ErrorCode:0x%x\n", i2c_Handle.ErrorCode);

    i2c_Handle.ErrorCode = 0;

    __HAL_I2C_DISABLE(&i2c_Handle); // for master, disable it after transmit to reduce error status
}


void oedtps_power_sequence_set(void)
{
    uint8_t dat;

    dat = ((TI_STROBE3 << 6) | (TI_STROBE4 << 4) | (TI_STROBE1 << 2) | (TI_STROBE2 << 0)); //set power up assignment VGL->VNEG->VGH->VPOS
    i2c_write(TPS65185_ADDR, TI_REG_UPSEQ0, dat);

    dat = ((TI_UPLY3MS << 6) | (TI_UPLY3MS << 4) | (TI_UPLY3MS << 2) | (TI_UPLY3MS << 0)); //set power up sequence delay times
    i2c_write(TPS65185_ADDR, TI_REG_UPSEQ1, dat);

    dat = ((TI_STROBE2 << 6) | (TI_STROBE1 << 4) | (TI_STROBE4 << 2) | (TI_STROBE3 << 0)); //set power down strobe assignment VPOS->VGH->VNEG->VGL
    i2c_write(TPS65185_ADDR, TI_REG_DWNSEQ0, dat);

    dat = ((TI_DWNLY24MS << 6) | (TI_DWNLY6MS << 4) | (TI_UPLY6MS << 2) | (0 << 1) | 0); //set power-down sequence delay times
    i2c_write(TPS65185_ADDR, TI_REG_DWNSEQ1, dat);
}


void oedtps_vposvneg_set(void)
{
    i2c_write(TPS65185_ADDR, TI_REG_VADJ, 0x03);    //set VPOS and VNEG voltage to 15V
}


void oedtps_vcom_set(uint16_t vcom)
{
    uint8_t dat;

    dat = ((vcom / 10) & 0x00FF);
    i2c_write(TPS65185_ADDR, TI_REG_VCOM1, dat);    //set voltage for VCOM

    dat = (((vcom / 10) >> 8) & 0x01);
    i2c_write(TPS65185_ADDR, TI_REG_VCOM2, dat);    //set voltage VCOM + control
}


//init TPS65185
void oedtps_init(void)
{
    //i2c init
    HAL_StatusTypeDef ret;

#define EXAMPLE_I2C I2C2 // i2c number of cpu
#define EXAMPLE_I2C_IRQ I2C2_IRQn // i2c number of interruput when using interrupte mode 

    HAL_RCC_EnableModule(RCC_MOD_I2C2); // enable i2c2


    i2c_Handle.Instance = EXAMPLE_I2C;
    i2c_Handle.Mode = HAL_I2C_MODE_MASTER; // i2c master mode
    i2c_Handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT; // i2c 7bits device address mode
    i2c_Handle.Init.ClockSpeed = 400000; // i2c speed (hz)
    i2c_Handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    ret = HAL_I2C_Init(&i2c_Handle);
    if (ret == 0)rt_kprintf("i2c_init success\n");
    else rt_kprintf("i2c_init success\n");

    TPS_WAKEUP_L_hs();
    TPS_PWRCOM_L_hs();
    TPS_PWRUP_L_hs();
    // HAL_Delay(10);
    TPS_WAKEUP_H_hs();
    // HAL_Delay(10);
    oedtps_power_sequence_set();
    oedtps_vposvneg_set();
    oedtps_vcom_set(oedopm_vcom);           //VCOM设置
}

void oedtps_vcom_enable(void)
{
    TPS_PWRCOM_H_hs();
}

void oedtps_vcom_disable(void)
{
    TPS_PWRCOM_L_hs();
}


void oedtps_source_gate_enable(void)
{
    TPS_PWRUP_H_hs();
}

void oedtps_source_gate_disable(void)
{
    TPS_PWRUP_L_hs();
}
