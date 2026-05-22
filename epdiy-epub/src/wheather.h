#ifndef WHEATHER_H
#define WHEATHER_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    rt_bool_t valid;
    int last_result;
    char location[32];
    char country[32];
    char path[48];
    char timezone[24];
    char timezone_offset[8];
    char weather_text[24];
    char weather_code[8];
    char temperature[8];
    char last_update[24];
    char status[48];
} weather_snapshot_t;

const weather_snapshot_t *weather_get_snapshot(void);
const char *weather_get_city(void);
rt_err_t weather_set_city(const char *city);
void weather_set_status(const char *status);
int weather_check_internet_access(void);
int weather_refresh(void);
void weather_copy_string(char *dst, rt_size_t dst_size, const char *src);
#ifdef __cplusplus
}
#endif

#endif
