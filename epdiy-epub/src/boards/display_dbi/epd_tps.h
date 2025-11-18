
#ifndef __EPD_TPS_H__
#define __EPD_TPS_H__
#include <rtthread.h>
#ifdef __cplusplus
extern "C" {
#endif
//模组的VCOM电压(2100代表-2.10V)
void oedtps_init(uint16_t vcom_voltage);
void oedtps_vcom_enable(void);
void oedtps_vcom_disable(void);
void oedtps_source_gate_enable(void);
void oedtps_source_gate_disable(void);


rt_err_t tps_enter_sleep(void);
#ifdef __cplusplus
}
#endif
#endif /*__EPD_TPS_H__*/