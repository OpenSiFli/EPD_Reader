/*
 * SPDX-FileCopyrightText: 2024-2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pan.h"

#include <string.h>

#include "bts2_app_inc.h"
#include "ble_connection_manager.h"
#include "bt_connection_manager.h"
#include "ulog.h"
#include "wheather.h"



/*---------------------------------------------------------------------------*/
/* 配置常量 */
/*---------------------------------------------------------------------------*/
#define PAN_TIMER_MS           3000
#define PAN_THREAD_STACK_SIZE  4096
#define PAN_THREAD_PRIORITY    20
#define PAN_THREAD_TICK        20
#define PAN_LOCAL_NAME_MAX     32

/* wheather.c 提供的天气请求入口。 */
/* 使用新的统一接口 weather_refresh() 以包含 PAN 检查/重试逻辑 */

/*---------------------------------------------------------------------------*/
/* 模块状态 */
/*---------------------------------------------------------------------------*/
/**
 * @brief PAN 服务运行时上下文。
 */
typedef struct
{
    rt_bool_t initialized;
    rt_bool_t stack_ready;
    rt_bool_t bt_connected;
    rt_bool_t pan_connected;
    rt_bool_t auto_request_weather;
    rt_bool_t connect_pending;
    bt_notify_device_mac_t bd_addr;
    rt_mailbox_t mailbox;
    rt_timer_t pan_connect_timer;
    rt_thread_t worker;
    char local_name[PAN_LOCAL_NAME_MAX];
} pan_service_t;

static pan_service_t g_pan_service;

static rt_bool_t pan_service_has_peer_addr(void)
{
    uint8_t index;

    for (index = 0; index < sizeof(g_pan_service.bd_addr.addr); index++)
    {
        if (g_pan_service.bd_addr.addr[index] != 0x00 && g_pan_service.bd_addr.addr[index] != 0xFF)
            return RT_TRUE;
    }

    return RT_FALSE;
}

static void pan_service_reset_mailbox(void)
{
    if (g_pan_service.mailbox != RT_NULL)
        rt_mb_control(g_pan_service.mailbox, RT_IPC_CMD_RESET, RT_NULL);

    g_pan_service.connect_pending = RT_FALSE;
}

/*---------------------------------------------------------------------------*/
/* 内部工具函数 */
/*---------------------------------------------------------------------------*/
/**
 * @brief 向 PAN 工作线程投递内部事件。
 * @param event 内部事件类型。
 * @return RT-Thread 错误码。
 */
static rt_err_t pan_service_post(pan_msg_t event)
{
    if (g_pan_service.mailbox == RT_NULL)
        return -RT_ERROR;

    return rt_mb_send(g_pan_service.mailbox, (rt_ubase_t)event);
}

/**
 * @brief 停止延迟发起 PAN 连接的定时器。
 */
static void pan_service_stop_timer(void)
{
    if (g_pan_service.pan_connect_timer != RT_NULL)
        rt_timer_stop(g_pan_service.pan_connect_timer);
}

/**
 * @brief PAN 连接延迟定时器回调。
 * @param parameter 定时器参数，当前未使用。
 */
static void pan_service_connect_timeout(void *parameter)
{
    (void)parameter;

    if (g_pan_service.bt_connected)
    pan_service_request_connect();
}

/**
 * @brief 启动 PAN 延迟连接定时器。
 */
static void pan_service_start_timer(void)
{
    if (g_pan_service.pan_connect_timer == RT_NULL)
    {
        g_pan_service.pan_connect_timer = rt_timer_create("connect_pan",
                                                          pan_service_connect_timeout,
                                                          RT_NULL,
                                                          rt_tick_from_millisecond(PAN_TIMER_MS),
                                                          RT_TIMER_FLAG_SOFT_TIMER);
    }
    else
    {
        rt_timer_stop(g_pan_service.pan_connect_timer);
    }

    if (g_pan_service.pan_connect_timer != RT_NULL)
        rt_timer_start(g_pan_service.pan_connect_timer);
}

/*---------------------------------------------------------------------------*/
/* 天气请求流程 */
/*---------------------------------------------------------------------------*/
/**
 * @brief 在 PAN 网络可用后拉取天气，并在失败时做有限次重试。
 * @note 这里保留了同步延时重试逻辑，便于样例直接工作。
 */
static void pan_service_handle_weather_request(void)
{
    int retry;
    const int retry_count = 3;
    const int retry_delay_ms = 1000;
    const int ready_delay_ms = 2000;

    if (!g_pan_service.pan_connected)
        return;

    rt_thread_mdelay(ready_delay_ms);
    for (retry = 0; retry < retry_count; retry++)
    {
        if (!g_pan_service.pan_connected)
            break;

        if (weather_refresh() == 0)
            return;

        LOG_W("weather fetch failed, retry %d", retry + 1);
        rt_thread_mdelay(retry_delay_ms);
    }
}

/*---------------------------------------------------------------------------*/
/* 蓝牙事件回调 */
/*---------------------------------------------------------------------------*/
/**
 * @brief 处理蓝牙协议栈上报的公共事件和 PAN 事件。
 * @param type 事件大类。
 * @param event_id 事件编号。
 * @param data 事件负载。
 * @param data_len 事件负载长度。
 * @return 0。
 */
static int pan_service_bt_event_handle(uint16_t type, uint16_t event_id, uint8_t *data, uint16_t data_len)
{
    (void)data_len;

    /* 处理 BT 公共事件。 */
    if (type == BT_NOTIFY_COMMON)
    {
        rt_bool_t should_connect_pan = RT_FALSE;

        switch (event_id)
        {
        case BT_NOTIFY_COMMON_BT_STACK_READY:
            pan_service_post(PAN_MSG_STACK_READY);
            break;

        case BT_NOTIFY_COMMON_ACL_DISCONNECTED:
        {
            bt_notify_device_base_info_t *info = (bt_notify_device_base_info_t *)data;

            LOG_I("disconnected(0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x) res %d",
                  info->mac.addr[5], info->mac.addr[4], info->mac.addr[3],
                  info->mac.addr[2], info->mac.addr[1], info->mac.addr[0], info->res);
            g_pan_service.bt_connected = RT_FALSE;
            g_pan_service.pan_connected = RT_FALSE;
            pan_service_reset_mailbox();
            pan_service_stop_timer();
            break;
        }

        case BT_NOTIFY_COMMON_ENCRYPTION:
        {
            bt_notify_device_mac_t *mac = (bt_notify_device_mac_t *)data;

            LOG_I("Encryption competed");
            g_pan_service.bd_addr = *mac;
            should_connect_pan = RT_TRUE;
            break;
        }

        case BT_NOTIFY_COMMON_PAIR_IND:
        {
            bt_notify_device_base_info_t *info = (bt_notify_device_base_info_t *)data;

            LOG_I("Pairing completed %d", info->res);
            if (info->res == BTS2_SUCC)
            {
                g_pan_service.bd_addr = info->mac;
                should_connect_pan = RT_TRUE;
            }
            break;
        }

        case BT_NOTIFY_COMMON_KEY_MISSING:
        {
            bt_notify_device_base_info_t *info = (bt_notify_device_base_info_t *)data;

            LOG_I("Key missing %d", info->res);
            memset(&g_pan_service.bd_addr, 0xFF, sizeof(g_pan_service.bd_addr));
            bt_cm_delete_bonded_devs_and_linkkey(info->mac.addr);
            break;
        }

        default:
            break;
        }

        /* 配对或加密完成后，启动延迟定时器发起 PAN 连接。 */
        if (should_connect_pan)
        {
            LOG_I("bd addr 0x%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                  g_pan_service.bd_addr.addr[5], g_pan_service.bd_addr.addr[4],
                  g_pan_service.bd_addr.addr[3], g_pan_service.bd_addr.addr[2],
                  g_pan_service.bd_addr.addr[1], g_pan_service.bd_addr.addr[0]);
            g_pan_service.bt_connected = RT_TRUE;
            pan_service_start_timer();
        }
    }
    /* 处理 PAN profile 事件。 */
    else if (type == BT_NOTIFY_PAN)
    {
        switch (event_id)
        {
        case BT_NOTIFY_PAN_PROFILE_CONNECTED:
            LOG_I("pan connect successed");
            pan_service_stop_timer();
            g_pan_service.connect_pending = RT_FALSE;
            g_pan_service.pan_connected = RT_TRUE;
            if (g_pan_service.auto_request_weather)
                pan_service_request_weather();
            break;

        case BT_NOTIFY_PAN_PROFILE_DISCONNECTED:
            LOG_I("pan disconnect with remote device");
            g_pan_service.pan_connected = RT_FALSE;
            pan_service_reset_mailbox();
            break;

        default:
            break;
        }
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* 工作线程 */
/*---------------------------------------------------------------------------*/
/**
 * @brief PAN 服务工作线程。
 * @param parameter 线程参数，当前未使用。
 */
static void pan_service_thread_entry(void *parameter)
{
    rt_uint32_t value = 0;

    (void)parameter;
    /* 首次等待协议栈 ready 事件，并在 ready 后设置本地蓝牙名称。 */
    if (RT_EOK == rt_mb_recv(g_pan_service.mailbox, &value, 8000) && value == PAN_MSG_STACK_READY)
    {
        g_pan_service.stack_ready = RT_TRUE;
        LOG_I("BT/BLE stack and profile ready");
    }
    else
    {
        LOG_I("BT/BLE stack and profile init failed");
    }

    if (g_pan_service.local_name[0] != '\0')
        bt_interface_set_local_name(strlen(g_pan_service.local_name), g_pan_service.local_name);

    bt_interface_set_scan_mode(TRUE, TRUE);

    /* 后续循环只处理业务事件。 */
    while (1)
    {
        if (rt_mb_recv(g_pan_service.mailbox, &value, RT_WAITING_FOREVER) != RT_EOK)
            continue;

        switch (value)
        {
        case PAN_MSG_CONNECT_PAN:
            g_pan_service.connect_pending = RT_FALSE;
            if (g_pan_service.bt_connected)
                bt_interface_conn_ext((char *)&g_pan_service.bd_addr, BT_PROFILE_PAN);
            break;

        case PAN_MSG_GET_WEATHER:
            pan_service_handle_weather_request();
            break;

        default:
            break;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* 对外接口 */
/*---------------------------------------------------------------------------*/
/**
 * @brief 返回设备蓝牙 Class of Device，用于声明网络设备能力。
 * @return 蓝牙设备类别值。
 */
uint32_t bt_get_class_of_device(void)
{
    return (uint32_t)BT_SRVCLS_NETWORK | BT_COMPCLS_PALMSIZED;
}

/**
 * @brief 初始化 PAN 服务并启动内部工作线程。
 * @param local_name 本地蓝牙名称，为空时使用默认值。
 * @param auto_request_weather PAN 连通后是否自动请求天气。
 * @return RT-Thread 错误码。
 */
rt_err_t pan_service_init(const char *local_name, rt_bool_t auto_request_weather)
{
    if (g_pan_service.initialized)
        return RT_EOK;

    memset(&g_pan_service, 0, sizeof(g_pan_service));

    if (local_name == RT_NULL || local_name[0] == '\0')
        local_name = "sifli_pan";

    rt_strncpy(g_pan_service.local_name, local_name, sizeof(g_pan_service.local_name) - 1);
    g_pan_service.auto_request_weather = auto_request_weather;
    rt_kprintf("service init: %s\n", g_pan_service.local_name);
    g_pan_service.mailbox = rt_mb_create("bt_app", 4, RT_IPC_FLAG_FIFO);
    if (g_pan_service.mailbox == RT_NULL)
        return -RT_ENOMEM;

    g_pan_service.worker = rt_thread_create("pan_worker",
                                            pan_service_thread_entry,
                                            RT_NULL,
                                            PAN_THREAD_STACK_SIZE,
                                            PAN_THREAD_PRIORITY,
                                            PAN_THREAD_TICK);
    if (g_pan_service.worker == RT_NULL)
    {
        rt_mb_delete(g_pan_service.mailbox);
        g_pan_service.mailbox = RT_NULL;
        return -RT_ENOMEM;
    }
    rt_kprintf("service thread created\n");
    g_pan_service.initialized = RT_TRUE;

    bt_interface_register_bt_event_notify_callback(pan_service_bt_event_handle);
    rt_thread_startup(g_pan_service.worker);

    /* LCPU probe: check whether LCPU is actually running before enabling BLE */
    rt_kprintf("[BLE_DBG] LCPU CPUWAIT = 0x%08x (0=running, non-0=stalled)\n",
               hwp_lpsys_aon->PMR & LPSYS_AON_PMR_CPUWAIT);
    sifli_ble_enable();
    rt_kprintf("[BLE_DBG] sifli_ble_enable() returned\n");
   

    return RT_EOK;
}

/**
 * @brief 请求发起一次 PAN 连接。
 * @return RT-Thread 错误码。
 */
rt_err_t pan_service_request_connect(void)
{
    if (!g_pan_service.initialized || !g_pan_service.bt_connected)
        return -RT_ERROR;

    if (g_pan_service.pan_connected || g_pan_service.connect_pending)
        return RT_EOK;

    g_pan_service.connect_pending = RT_TRUE;
    if (pan_service_post(PAN_MSG_CONNECT_PAN) != RT_EOK)
    {
        g_pan_service.connect_pending = RT_FALSE;
        return -RT_ERROR;
    }

    return RT_EOK;
}

/**
 * @brief 请求通过当前 PAN 网络拉取一次天气。
 * @return RT-Thread 错误码。
 */
rt_err_t pan_service_request_weather(void)
{
    if (!g_pan_service.initialized || !g_pan_service.pan_connected)
        return -RT_ERROR;

    return pan_service_post(PAN_MSG_GET_WEATHER);
}

rt_err_t pan_service_set_enabled(rt_bool_t enabled)
{
    if (!g_pan_service.initialized || !g_pan_service.stack_ready)
        return -RT_ERROR;

    if (enabled)
    {
        if (g_pan_service.local_name[0] != '\0')
            bt_interface_set_local_name(strlen(g_pan_service.local_name), g_pan_service.local_name);
        bt_interface_set_scan_mode(TRUE, TRUE);
        return RT_EOK;
    }

    bt_interface_set_scan_mode(FALSE, FALSE);

    if (pan_service_has_peer_addr())
    {
        if (g_pan_service.pan_connected)
            bt_interface_disc_ext((unsigned char *)&g_pan_service.bd_addr, BT_PROFILE_PAN);

        if (g_pan_service.bt_connected)
            bt_interface_disconnect_req((unsigned char *)&g_pan_service.bd_addr);
    }

    g_pan_service.bt_connected = RT_FALSE;
    g_pan_service.pan_connected = RT_FALSE;
    pan_service_reset_mailbox();
    pan_service_stop_timer();
    return RT_EOK;
}

/**
 * @brief 查询蓝牙协议栈是否已经 ready。
 * @return RT_TRUE 表示 ready。
 */
rt_bool_t pan_service_is_ready(void)
{
    return g_pan_service.stack_ready;
}

/**
 * @brief 查询 ACL 蓝牙链路是否已建立。
 * @return RT_TRUE 表示已建立。
 */
rt_bool_t pan_service_is_bt_connected(void)
{
    return g_pan_service.bt_connected;
}

/**
 * @brief 查询 PAN profile 是否已连通。
 * @return RT_TRUE 表示已连通。
 */
rt_bool_t pan_service_is_pan_connected(void)
{
    return g_pan_service.pan_connected;
}