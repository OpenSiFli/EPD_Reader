/*
 * SPDX-FileCopyrightText: 2024-2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <string.h>
#include "lwip/api.h"
#include "lwip/dns.h"
#include <webclient.h>
#include <cJSON.h>
#include "pan.h"
#include "wheather.h"

#define GET_URL_LEN_MAX         256         //网址最大长度
#define GET_URI                 "http://%s/v3/weather/now.json?key=%s&location=%s&language=%s" //获取天气的 API
#define WEATHER_HOST                    "api.seniverse.com"
#define WEATHER_KEY_ID                  "SO23_Gmly2oK3kMf4"
#define WEATHER_CITY_ID_DEFAULT         "nanjing"
#define WEATHER_LANGUAGE_ID             "zh-Hans&unit=c" //
#define WEATHER_CITY_MAX_LEN            48

static char g_weather_city[WEATHER_CITY_MAX_LEN] = WEATHER_CITY_ID_DEFAULT;

static weather_snapshot_t g_weather_snapshot = {
    RT_FALSE,
    -1,
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "尚未获取天气"
};


void weather_copy_string(char *dst, rt_size_t dst_size, const char *src)
{
    if (dst == RT_NULL || dst_size == 0)
        return;

    if (src == RT_NULL)
        dst[0] = '\0';
    else
        rt_snprintf(dst, dst_size, "%s", src);
}

static void weather_copy_json_string(char *dst, rt_size_t dst_size, cJSON *item)
{
    if (item == RT_NULL || !cJSON_IsString(item))
    {
        weather_copy_string(dst, dst_size, RT_NULL);
        return;
    }

    weather_copy_string(dst, dst_size, item->valuestring);
}

const weather_snapshot_t *weather_get_snapshot(void)
{
    return &g_weather_snapshot;
}

const char *weather_get_city(void)
{
    return g_weather_city;
}

rt_err_t weather_set_city(const char *city)
{
    if (city == RT_NULL || city[0] == '\0')
        return -RT_EINVAL;

    rt_snprintf(g_weather_city, sizeof(g_weather_city), "%s", city);
    return RT_EOK;
}

void weather_set_status(const char *status)
{
    weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), status);
}

void svr_found_callback(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL)
    {
        rt_kprintf("DNS lookup succeeded, IP: %s\n", ipaddr_ntoa(ipaddr));
    }
}

int weather_check_internet_access(void)
{
    int r = 0;
    const char *hostname = WEATHER_HOST;
    ip_addr_t addr = {0};

    {
        err_t err = dns_gethostbyname(hostname, &addr, svr_found_callback, NULL);
        if (err != ERR_OK && err != ERR_INPROGRESS)
        {
            rt_kprintf("Coud not find %s, please check PAN connection\n", hostname);
        }
        else
            r = 1;
    }

    return r;
}

static int http_weather_data_parse(char *json_data);
static char *get_weather(void);

int weather_run_once(void)
{
    int result = -1;
    int retry;

    const int retry_count = 3;
    const int retry_delay_ms = 1500;

    for (retry = 0; retry < retry_count; ++retry)
    {
        if (!pan_service_is_pan_connected())
        {
            weather_set_status(pan_service_is_bt_connected() ? "网络未连接" : "蓝牙未连接");
            return -1;
        }

        {
            char *weather_data = get_weather();
            int local_result;

            if (weather_data == RT_NULL)
            {
                g_weather_snapshot.last_result = -1;
                weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气请求失败");
                local_result = -1;
            }
            else
            {
                local_result = http_weather_data_parse(weather_data);
                if (local_result != 0)
                {
                    g_weather_snapshot.last_result = local_result;
                    if (!g_weather_snapshot.status[0])
                        weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气解析失败");
                }
                web_free(weather_data);
            }

            result = local_result;
        }
        if (result == RT_EOK)
            return RT_EOK;

        if (retry + 1 < retry_count)
        {
            weather_set_status("等待网络就绪");
            rt_thread_mdelay(retry_delay_ms);
        }
    }

    return result;
}

int weather_refresh(void)
{
    rt_err_t connect_result;
    const int reconnect_retry = 12;
    const int reconnect_delay_ms = 250;
    const int pan_ready_delay_ms = 2500;
    const int network_ready_retry = 8;
    const int network_ready_delay_ms = 500;
    int retry;

    if (pan_service_is_pan_connected())
    {
            weather_set_status("天气更新中");
            return weather_run_once();
    }

    if (!pan_service_is_bt_connected())
    {
        weather_set_status("蓝牙未连接");
        return -1;
    }

    weather_set_status("正在连接网络");
    connect_result = pan_service_request_connect();
    if (connect_result != RT_EOK)
    {
        weather_set_status(pan_service_is_bt_connected() ? "网络连接失败" : "蓝牙未连接");
        return connect_result;
    }

    for (retry = 0; retry < reconnect_retry; ++retry)
    {
        int network_retry;

        if (pan_service_is_pan_connected())
        {
            rt_thread_mdelay(pan_ready_delay_ms);
            for (network_retry = 0; network_retry < network_ready_retry; ++network_retry)
            {
                if (weather_check_internet_access())
                    break;
                rt_thread_mdelay(network_ready_delay_ms);
            }

            weather_set_status("天气更新中");
                return weather_run_once();
        }

        if (!pan_service_is_bt_connected())
        {
            weather_set_status("蓝牙未连接");
            return -1;
        }

        rt_thread_mdelay(reconnect_delay_ms);
    }

    weather_set_status(pan_service_is_bt_connected() ? "网络未连接" : "蓝牙未连接");
    return -1;
}

static int http_weather_data_parse(char *json_data)
{
    cJSON *root = NULL;
    cJSON *result_item = NULL;
    cJSON *location = NULL;
    cJSON *now = NULL;
    weather_snapshot_t parsed_snapshot = g_weather_snapshot;

    root = cJSON_Parse(json_data);   /*json_data 为心知天气的原始数据*/
    if (!root)
    {
        rt_kprintf("Error before: [%s]\n", cJSON_GetErrorPtr());
        g_weather_snapshot.last_result = -1;
        weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气数据解析失败");
        return  -1;
    }

    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) <= 0)
    {
        cJSON_Delete(root);
        g_weather_snapshot.last_result = -1;
        weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气返回为空");
        return -1;
    }

    result_item = cJSON_GetArrayItem(results, 0);
    if (result_item == RT_NULL)
    {
        cJSON_Delete(root);
        g_weather_snapshot.last_result = -1;
        weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气字段不完整");
        return -1;
    }

    location = cJSON_GetObjectItem(result_item, "location");
    now = cJSON_GetObjectItem(result_item, "now");
    if (location == RT_NULL || now == RT_NULL)
    {
        cJSON_Delete(root);
        g_weather_snapshot.last_result = -1;
        weather_copy_string(g_weather_snapshot.status, sizeof(g_weather_snapshot.status), "天气字段不完整");
        return -1;
    }

    weather_copy_json_string(parsed_snapshot.location, sizeof(parsed_snapshot.location), cJSON_GetObjectItem(location, "name"));
    weather_copy_json_string(parsed_snapshot.country, sizeof(parsed_snapshot.country), cJSON_GetObjectItem(location, "country"));
    weather_copy_json_string(parsed_snapshot.path, sizeof(parsed_snapshot.path), cJSON_GetObjectItem(location, "path"));
    weather_copy_json_string(parsed_snapshot.timezone, sizeof(parsed_snapshot.timezone), cJSON_GetObjectItem(location, "timezone"));
    weather_copy_json_string(parsed_snapshot.timezone_offset, sizeof(parsed_snapshot.timezone_offset), cJSON_GetObjectItem(location, "timezone_offset"));
    weather_copy_json_string(parsed_snapshot.weather_text, sizeof(parsed_snapshot.weather_text), cJSON_GetObjectItem(now, "text"));
    weather_copy_json_string(parsed_snapshot.weather_code, sizeof(parsed_snapshot.weather_code), cJSON_GetObjectItem(now, "code"));
    weather_copy_json_string(parsed_snapshot.temperature, sizeof(parsed_snapshot.temperature), cJSON_GetObjectItem(now, "temperature"));
    weather_copy_json_string(parsed_snapshot.last_update, sizeof(parsed_snapshot.last_update), cJSON_GetObjectItem(result_item, "last_update"));
    weather_copy_string(parsed_snapshot.status, sizeof(parsed_snapshot.status), "更新成功");
    parsed_snapshot.valid = RT_TRUE;
    parsed_snapshot.last_result = 0;

    g_weather_snapshot = parsed_snapshot;

    rt_kprintf("name:%s\n", g_weather_snapshot.location);
    rt_kprintf("country:%s\n", g_weather_snapshot.country);
    rt_kprintf("path:%s\n", g_weather_snapshot.path);
    rt_kprintf("timezone:%s\n", g_weather_snapshot.timezone);
    rt_kprintf("timezone_offset:%s\n", g_weather_snapshot.timezone_offset);
    rt_kprintf("txt:%s\n", g_weather_snapshot.weather_text);
    rt_kprintf("code:%s\n", g_weather_snapshot.weather_code);
    rt_kprintf("temperature:%s\n", g_weather_snapshot.temperature);
    rt_kprintf("last_update:%s\n", g_weather_snapshot.last_update);
    cJSON_Delete(root);/*每次调用cJSON_Parse函数后，都要释放内存*/

    return  0;
}

static char *get_weather(void)
{
    char *buffer = RT_NULL;
    char *weather_url = RT_NULL;
    size_t resp_len = 0;
    int request_result;

    if (weather_check_internet_access() == 0)
        return buffer;

    /* 为 weather_url 分配空间 */
    weather_url = rt_calloc(1, GET_URL_LEN_MAX);
    if (weather_url == RT_NULL)
    {
        rt_kprintf("No memory for weather_url!\n");
        goto __exit;
    }
    /* 拼接 GET 网址 */
    rt_snprintf(weather_url, GET_URL_LEN_MAX, GET_URI,
                WEATHER_HOST, WEATHER_KEY_ID, g_weather_city, WEATHER_LANGUAGE_ID);

    request_result = webclient_request(weather_url, RT_NULL, RT_NULL, 0, (void **)&buffer, &resp_len);
    if (request_result < 0 || buffer == RT_NULL || resp_len == 0)
    {
        rt_kprintf("weather request failed, result=%d len=%d\n", request_result, (int)resp_len);
        if (buffer != RT_NULL)
        {
            web_free(buffer);
            buffer = RT_NULL;
        }
        goto __exit;
    }

__exit:
    /* 释放网址空间 */
    if (weather_url != RT_NULL)
    {
        rt_free(weather_url);
        weather_url = RT_NULL;
    }

    return buffer;
}

static void weather_city(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("weather city: %s\n", weather_get_city());
        rt_kprintf("usage: weather_city <location>\n");
        return;
    }

    if (weather_set_city(argv[1]) != RT_EOK)
    {
        rt_kprintf("invalid city\n");
        return;
    }

    rt_kprintf("weather city set to: %s\n", weather_get_city());
}

FINSH_FUNCTION_EXPORT_ALIAS(weather_city, __cmd_weather_city, Show or set weather city);

