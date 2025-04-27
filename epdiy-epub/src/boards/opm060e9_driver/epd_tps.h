
#ifndef __EPD_TPS_H__
#define __EPD_TPS_H__
#include <rtthread.h>


void oedtps_init(void);
void oedtps_vcom_enable(void);
void oedtps_vcom_disable(void);
void oedtps_source_gate_enable(void);
void oedtps_source_gate_disable(void);

#endif /*__EPD_TPS_H__*/