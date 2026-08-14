/*
 * SPDX-FileCopyrightText: 2019-2022 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include "board.h"
#include "gt967.h"
#include "drv_touch.h"

/* Define -------------------------------------------------------------------*/

#define DBG_LEVEL         DBG_INFO // DBG_ERROR  // 
#define LOG_TAG              "drv.gt967"
#include <drv_log.h>
#define TP_DEV_ADDR_DEFAULT     (0x5D)
#define TP_DEV_ADDR_ALTERNATE   (0x14)   /* 52 固定使用 0x14；57 根据复位/INT 时序在 0x5D 与 0x14 间选择 */
#define TP_TD_STATUS            (0x814e)
#define TP_P1_XL                (0x8150)
#define TP_P1_XH                (0x8151)
#define TP_P1_YL                (0x8152)
#define TP_P1_YH                (0x8153)

#define TP_ID_CONTROL           (0x8040)

// rotate to left with 90, 180, 270
// rotate to left with 360 for mirror
//#define TP_ROTATE_LEFT                 (90)

/* function and value-----------------------------------------------------------*/

static void correct_pos(touch_msg_t ppos);
static rt_err_t write_reg(uint16_t reg, rt_uint8_t data);
static rt_err_t read_regs(rt_uint16_t reg, rt_uint8_t len, rt_uint8_t *buf);

static struct rt_i2c_bus_device *ft_bus = NULL;
#ifdef SF32LB57X
static rt_uint8_t tp_dev_addr = TP_DEV_ADDR_DEFAULT;
#else
static rt_uint8_t tp_dev_addr = TP_DEV_ADDR_ALTERNATE;
#endif

static struct touch_drivers driver;

static rt_err_t write_reg(uint16_t reg, rt_uint8_t data)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;
    rt_uint8_t buf[3] = {(uint8_t)(reg >> 8), (uint8_t)reg, data};

    msgs.addr  = tp_dev_addr;    /* slave address */
    msgs.flags = RT_I2C_WR;        /* write flag */
    msgs.buf   = buf;              /* Send data pointer */
    msgs.len   = 3;

    if (rt_i2c_transfer(ft_bus, &msgs, 1) == 1)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

static rt_err_t read_regs(rt_uint16_t reg, rt_uint8_t len, rt_uint8_t *buf)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs[2];
    rt_uint8_t reg_w[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};

    msgs[0].addr  = tp_dev_addr;    /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = reg_w;              /* Slave register address */
    msgs[0].len   = 2;                /* Number of bytes sent */

    msgs[1].addr  = tp_dev_addr;    /* Slave address */
    msgs[1].flags = RT_I2C_RD;        /* Read flag */
    msgs[1].buf   = buf;              /* Read data pointer */
    msgs[1].len   = len;              /* Number of bytes read */

    if (rt_i2c_transfer(ft_bus, msgs, 2) == 2)
    {
        res = RT_EOK;
    }
    else
    {
        res = -RT_ERROR;
    }
    return res;
}

static void correct_pos(touch_msg_t ppos)
{
    int temp_x = ppos->x;
    ppos->x = ppos->y;
    ppos->y = LCD_VER_RES_MAX - (temp_x ) - 1;

    return;
}

static rt_err_t read_point(touch_msg_t p_msg)
{
    uint8_t buf[80] = {0};
    uint8_t point_num = 0, touch_down = 0;
    int ret = 0, retry = 2;
    uint8_t reg_value[8] = {0};
    reg_value[0] = 0x81;
    reg_value[1] = 0x40;
    //        reg_value[2] = 0x00;
    //        reg_value[3] = 0x00;
//        rt_kprintf("tp read_point\n");
    read_regs(0x814e, 1, buf);
    if ((buf[0] & 0x80) != 0x80)
    {
        rt_thread_delay(1); //delay 1ms if buffer status is not relay;
        LOG_D("tp\n");
        read_regs(0x814e, 1, buf);
    }
    rt_touch_irq_pin_enable(1);

    point_num = buf[0] & 0x0f;
    if (point_num) // the number of touch points
    {
        p_msg->event = TOUCH_EVENT_DOWN;
    }
    else
    {
        p_msg->event = TOUCH_EVENT_UP;
    }
    read_regs(0x8150, 6, buf);
    p_msg->x = buf[0] + ((uint16_t)(buf[1] & 0xff) << 8);
    p_msg->y = buf[2] + ((uint16_t)(buf[3] & 0xff) << 8);
    correct_pos(p_msg);
    LOG_D("piont:%d, x:%d, y:%d,event:%d,byte:%d\n", point_num, p_msg->x, p_msg->y, p_msg->event, buf[4]);

    write_reg(0x814e, 0); //clear tp interrupt

    return RT_EEMPTY;
}

static void irq_handler(void *arg)
{
    rt_err_t ret = RT_ERROR;

    int value = (int)arg;
    LOG_D("gt967 touch_irq_handler\n");

    rt_touch_irq_pin_enable(0);

    ret = rt_sem_release(driver.isr_sem);
    RT_ASSERT(RT_EOK == ret);
}
#ifdef SF32LB57X
static rt_err_t probe_addr(rt_uint8_t dev_addr)
{
    struct rt_i2c_msg msgs[2];
    rt_uint8_t reg_w[2] = {0x81, 0x44};
    rt_uint8_t firmware[4];

    msgs[0].addr  = dev_addr;
    msgs[0].flags = RT_I2C_WR;
    msgs[0].buf   = reg_w;
    msgs[0].len   = sizeof(reg_w);

    msgs[1].addr  = dev_addr;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf   = firmware;
    msgs[1].len   = sizeof(firmware);

    return (rt_i2c_transfer(ft_bus, msgs, 2) == 2) ? RT_EOK : -RT_ERROR;
}

static rt_err_t select_device_address(void)
{
    if (RT_EOK == probe_addr(TP_DEV_ADDR_DEFAULT))
    {
        tp_dev_addr = TP_DEV_ADDR_DEFAULT;
    }
    else if (RT_EOK == probe_addr(TP_DEV_ADDR_ALTERNATE))
    {
        tp_dev_addr = TP_DEV_ADDR_ALTERNATE;
    }
    else
    {
        LOG_E("GT967 not found at 0x%02x or 0x%02x", TP_DEV_ADDR_DEFAULT, TP_DEV_ADDR_ALTERNATE);
        return -RT_ERROR;
    }

    LOG_I("GT967 I2C address = 0x%02x", tp_dev_addr);
    return RT_EOK;
}
#endif /* SF32LB57X */

static rt_err_t init(void)
{
    rt_err_t err;
    struct touch_message msg;

    LOG_D("gt967 init");

    rt_pin_mode(TOUCH_IRQ_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(TOUCH_IRQ_PIN, 0);
    BSP_TP_Reset(0);
    rt_thread_delay(1);
    rt_pin_write(TOUCH_IRQ_PIN, 1);
    rt_thread_delay(1);
    BSP_TP_Reset(1);
    rt_thread_delay(8);
    rt_pin_write(TOUCH_IRQ_PIN, 0);
    rt_thread_delay(60);

    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, irq_handler, NULL);
    rt_touch_irq_pin_enable(1);

#ifdef SF32LB57X
    err = select_device_address();
    if (RT_EOK != err)
    {
        return RT_FALSE;
    }
#endif

    uint8_t buf[6] = {0};
    err = read_regs(0x8144, 4, buf);
    if (RT_EOK != err)
    {
        LOG_E("Read firmware version fail");
        return RT_FALSE;
    }
    uint16_t firmware_version;
    firmware_version = buf[0] + ((uint16_t)(buf[1] & 0xff) << 8);

    LOG_I("Firmware version = 0x%x(%d)", firmware_version, firmware_version);

    //Soft reset
    err = write_reg(TP_ID_CONTROL, 2);
    if (RT_EOK != err)
    {
        LOG_E("SoftReset fail\n");
        return RT_FALSE;
    }
    err = write_reg(TP_ID_CONTROL, 0);
    if (RT_EOK != err)
    {
        LOG_E("SoftReset stop fail\n");
        return RT_FALSE;
    }

    LOG_D("gt967 init OK");
    return RT_EOK;

}

static rt_err_t deinit(void)
{
    LOG_D("gt967 deinit");

    rt_touch_irq_pin_enable(0);
    return RT_EOK;

}

static rt_bool_t probe(void)
{

    ft_bus = (struct rt_i2c_bus_device *)rt_device_find(TOUCH_DEVICE_NAME);
    if (RT_Device_Class_I2CBUS != ft_bus->parent.type)
    {
        ft_bus = NULL;
    }
    if (ft_bus)
    {
        rt_device_open((rt_device_t)ft_bus, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    }
    else
    {
        LOG_I("bus not find\n");
        return RT_FALSE;
    }

    {
        struct rt_i2c_configuration configuration =
        {
            .mode = 0,
            .addr = 0,
            .timeout = 500,
            .max_hz  = 400000,
        };

        rt_i2c_configure(ft_bus, &configuration);
    }

    LOG_I("probe OK");

    return RT_TRUE;
}

static struct touch_ops ops =
{
    read_point,
    init,
    deinit
};

static int rt_tp_device_init(void)
{

    driver.probe = probe;
    driver.ops = &ops;
    driver.user_data = RT_NULL;
    driver.isr_sem = rt_sem_create("gt967", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&driver);

    return 0;

}
INIT_COMPONENT_EXPORT(rt_tp_device_init);

