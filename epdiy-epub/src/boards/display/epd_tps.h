
#ifndef __EPD_TPS_H__
#define __EPD_TPS_H__
#include <rtthread.h>


void fp9931_init(uint16_t vcom_voltage);
void fp9931_enable(void);
void fp9931_disable(void);

#endif /*__EPD_TPS_H__*/