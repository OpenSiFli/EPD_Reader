/*
 * SPDX-FileCopyrightText: 2024-2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PAN_H
#define PAN_H

#include <rtthread.h>

typedef enum
{
    PAN_MSG_STACK_READY = 1,
    PAN_MSG_CONNECT_PAN = 2,
    PAN_MSG_GET_WEATHER = 3,
} pan_msg_t;
/* Initialize the PAN service and start its internal worker thread. */
#ifdef __cplusplus
extern "C" {
#endif
rt_err_t pan_service_init(const char *local_name, rt_bool_t auto_request_weather);

/* Request a PAN connection after the ACL link is available. */
rt_err_t pan_service_request_connect(void);

/* Request one weather fetch after PAN network is ready. */
rt_err_t pan_service_request_weather(void);

/* Toggle discoverable/connectable mode after initial stack bring-up. */
rt_err_t pan_service_set_enabled(rt_bool_t enabled);

/* Query runtime status for menu/UI logic. */
rt_bool_t pan_service_is_ready(void);
rt_bool_t pan_service_is_bt_connected(void);
rt_bool_t pan_service_is_pan_connected(void);

#ifdef __cplusplus
}
#endif

#endif
