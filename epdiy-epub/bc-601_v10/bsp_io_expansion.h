#ifndef AW9523B_H
#define AW9523B_H

#include <stdio.h> 

#define I2C_ADDR       0x59 
#define AW9523B_CONFIG_PORT0  0x04
#define AW9523B_CONFIG_PORT1  0x05
#define AW9523B_OUTPUT_PORT0  0x02
#define AW9523B_OUTPUT_PORT1  0x03 
#define AW9523B_INPUT_PORT0   0x00
#define AW9523B_INPUT_PORT1   0x01
#define AW9523B_GLOBAL_CTL    0x11
#define AW9523B_SOFT_RST      0x7F

#define P0_1_CHG_STATUS   0x01
#define P0_2_VBUS_DET     0x02 
#define P0_3_CTP_VDD_EN   0x04
#define P0_5_EBC_EN_GOOD  0x05
#define P0_6_EBC_PMIC_ON  0x06
#define P0_7_EBC_EN_ST    0x07
#define P1_4_AU_PA_EN     0x04
#define P1_5_CTP_RST      0x05


void AW9523B_Init(uint16_t dir);
void AW9523B_Write(uint8_t port, uint8_t level);
void AW9523B_Write_Pin(uint8_t pin, uint8_t level);
uint8_t AW9523B_Read(uint8_t port);
uint8_t AW9523B_Read_Pin(uint8_t pin);
void EBC_PMIC_Control(uint8_t enable);
void EBC_EN_ST_Control(uint8_t enable);
void AU_PA_Control(uint8_t enable);
#endif