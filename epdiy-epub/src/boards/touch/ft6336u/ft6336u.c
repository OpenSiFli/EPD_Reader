/*
 * SPDX-FileCopyrightText: 2019-2022 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include "board.h"
#include "ft6336u.h"
#include "drv_touch.h"

/* Define -------------------------------------------------------------------*/

#define DBG_LEVEL          DBG_ERROR  // DBG_LOG //
#define LOG_TAG              "drv.ft6336"
#include <drv_log.h>

#define FT_DEV_ADDR             (0x39)
#define FT_TD_STATUS            (0x02)
#define FT_P1_XH                (0x03)
#define FT_P1_XL                (0x04)
#define FT_P1_YH                (0x05)
#define FT_P1_YL                (0x06)
#define FT_ID_G_MODE            (0xA4)

#define FT_MAX_WIDTH                   (1032)
#define FT_MAX_HEIGHT                  (758)

// rotate to left with 90, 180, 270
// rotate to left with 360 for mirror
//#define FT_ROTATE_LEFT                 (90)

/* function and value-----------------------------------------------------------*/

static void ft6336_correct_pos(touch_msg_t ppos);
static rt_err_t write_reg(uint8_t reg, rt_uint8_t data);
static rt_err_t read_regs(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *buf);

static struct rt_i2c_bus_device *ft_bus = NULL;

static struct touch_drivers ft6336_driver;

static rt_err_t write_reg(uint8_t reg, rt_uint8_t data)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs;
    rt_uint8_t buf[2] = {reg, data};

    msgs.addr  = FT_DEV_ADDR;    /* slave address */
    msgs.flags = RT_I2C_WR;        /* write flag */
    msgs.buf   = buf;              /* Send data pointer */
    msgs.len   = 2;

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

static rt_err_t read_regs(rt_uint8_t reg, rt_uint8_t len, rt_uint8_t *buf)
{
    rt_int8_t res = 0;
    struct rt_i2c_msg msgs[2];

    msgs[0].addr  = FT_DEV_ADDR;    /* Slave address */
    msgs[0].flags = RT_I2C_WR;        /* Write flag */
    msgs[0].buf   = &reg;             /* Slave register address */
    msgs[0].len   = 1;                /* Number of bytes sent */

    msgs[1].addr  = FT_DEV_ADDR;    /* Slave address */
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

static void ft6336_correct_pos(touch_msg_t ppos)
{
    int temp_x = ppos->x;
    ppos->x = ppos->y;
    ppos->y = LCD_VER_RES_MAX - (temp_x * 2 / 3)- 1;

    return;
}

static rt_err_t read_point(touch_msg_t p_msg)
{
    int res = 0;
    uint8_t buf[4] = {0}; // 存储XH, XL, YH, YL
    uint8_t tp_num;
    rt_err_t err;

    LOG_D("ft6336 read_point");
    rt_touch_irq_pin_enable(1);

    if (ft_bus && p_msg)
    {
        // 1. 读取触摸点数（屏蔽FT_TD_STATUS高4位）
        err = read_regs(FT_TD_STATUS, 1, &tp_num);
        if (RT_EOK != err) goto ERROR_HANDLE;
        tp_num &= 0x0F; // 仅保留低4位（有效触摸点数）

        if (tp_num > 0)
        {
            // 2. 读取XH(0x03)、XL(0x04)、YH(0x05)、YL(0x06)共4字节
            err = read_regs(FT_P1_XH, 4, buf);
            if (RT_EOK != err)
            {
                LOG_I("read X/Y fail\n");
                goto ERROR_HANDLE;
            }

            // 3. 组合16位坐标（处理XH/YH低4位为高字节）
            // 组合16位坐标（原有代码保留）
            uint16_t raw_x = ((buf[0] & 0x0F) << 8) | buf[1]; 
            uint16_t raw_y = ((buf[2] & 0x0F) << 8) | buf[3]; 

            // 新增：过滤超出合理范围的异常坐标（1.2倍分辨率为阈值）
            if (raw_x > FT_MAX_WIDTH * 1.2 || raw_y > FT_MAX_HEIGHT * 1.2)
            {
                LOG_W("abnormal coordinate: raw_x=%d, raw_y=%d (skip)\n", raw_x, raw_y);
                goto ERROR_HANDLE;
            }

            // 4. 赋值原始坐标（后续校正）
            p_msg->x = raw_x;
            p_msg->y = raw_y;
            p_msg->event = TOUCH_EVENT_DOWN;

            // 5. 坐标校正（需结合实际屏幕调整，见步骤2）
            ft6336_correct_pos(p_msg);

            LOG_D("Down event, x = %d, y = %d\n", p_msg->x, p_msg->y);
            return (tp_num > 1) ? RT_EOK : RT_EEMPTY;
        }
        else
        {
            // 6. Touch Up：保留上次坐标（仅改事件类型，避免固定0值）
            p_msg->event = TOUCH_EVENT_UP;
            LOG_D("Up event, x = %d, y = %d\n", p_msg->x, p_msg->y);
            return RT_EEMPTY;
        }
    }

ERROR_HANDLE:
    p_msg->event = TOUCH_EVENT_UP;
    LOG_E("Error, return Up event, x = %d, y = %d\n", p_msg->x, p_msg->y);
    return RT_ERROR;
}

void ft6336_irq_handler(void *arg)
{
    rt_err_t ret = RT_ERROR;

    int value = (int)arg;
    LOG_D("ft6336 touch_irq_handler\n");

    rt_touch_irq_pin_enable(0);

    ret = rt_sem_release(ft6336_driver.isr_sem);
    RT_ASSERT(RT_EOK == ret);
}

static rt_err_t init(void)
{
    rt_err_t err;
    struct touch_message msg;

    LOG_D("ft6336 init");

    rt_touch_irq_pin_attach(PIN_IRQ_MODE_FALLING, ft6336_irq_handler, NULL);
    rt_touch_irq_pin_enable(1); //Must enable before read I2C

    err = write_reg(FT_ID_G_MODE, 1);
    if (RT_EOK != err)
    {
        LOG_E("G_MODE set fail\n");
        //return RT_FALSE;
    }

    LOG_D("ft6336 init OK");
    return RT_EOK;

}

static rt_err_t deinit(void)
{
    LOG_D("ft6336 deinit");

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

    LOG_I("ft6336 probe OK");

    return RT_TRUE;
}

static struct touch_ops ops =
{
    read_point,
    init,
    deinit
};

static int rt_ft6336_init(void)
{

    ft6336_driver.probe = probe;
    ft6336_driver.ops = &ops;
    ft6336_driver.user_data = RT_NULL;
    ft6336_driver.isr_sem = rt_sem_create("ft6336", 0, RT_IPC_FLAG_FIFO);

    rt_touch_drivers_register(&ft6336_driver);

    return 0;

}
INIT_COMPONENT_EXPORT(rt_ft6336_init);

//#define FT6336_FUNC_TEST
#ifdef FT6336_FUNC_TEST

int cmd_ft_test(int argc, char *argv[])
{
    touch_data_t post = {0};
    int res, looper;

    if (argc > 1)
    {
        looper = atoi(argv[1]);
    }
    else
    {
        looper = 0x0fffffff;
    }

    if (NULL == ft_bus)
    {
        ft6336_init();
    }
    while (looper != 0)
    {
        res = touch_read(&post);
        if (post.state)
        {
            LOG_I("x = %d, y = %d", post.point.x, post.point.y);
        }

        looper--;
        rt_thread_delay(100);
    }
    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_ft_test, __cmd_ft_test, Test hw ft6336);
#endif  /* ADS7846_FUNC_TEST */

