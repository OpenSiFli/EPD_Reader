#include "bsp_board.h"
#include "bf0_hal.h"
#include "string.h"
#include "bsp_io_expansion.h"

static I2C_HandleTypeDef i2c_Handle;
void AW9523B_I2C_Init(void)
{
    uint8_t slaveAddr = I2C_ADDR; 
    HAL_StatusTypeDef ret;

    
    HAL_RCC_EnableModule(RCC_MOD_I2C3);

    
    HAL_PIN_Set(PAD_PA38, I2C3_SCL, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA20, I2C3_SDA, PIN_PULLUP, 1);
    HAL_PIN_Set(PAD_PA02, GPIO_A2, PIN_PULLUP, 1);
    BSP_GPIO_Set(2, 1, 1);
    //
    i2c_Handle.Instance = I2C3;
    i2c_Handle.Mode = HAL_I2C_MODE_MASTER;
    i2c_Handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT; 
    i2c_Handle.Init.ClockSpeed = 400000;
    i2c_Handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;


    ret = HAL_I2C_Init(&i2c_Handle);
}

static void I2C_write_data(uint8_t addr, uint8_t data)
{
    HAL_StatusTypeDef ret;
    uint8_t buf[2] = {addr, data}; 

    __HAL_I2C_ENABLE(&i2c_Handle);


    ret = HAL_I2C_Master_Transmit(&i2c_Handle, I2C_ADDR, buf, 2, 1000);

    if (ret != HAL_OK)return;
    __HAL_I2C_DISABLE(&i2c_Handle);
}


static void I2C_read_data(uint8_t addr, uint8_t *pdata)
{
    if (pdata == NULL) return;

    HAL_StatusTypeDef ret;
    uint8_t buf = addr; 

    __HAL_I2C_ENABLE(&i2c_Handle);

    ret = HAL_I2C_Mem_Read(&i2c_Handle, I2C_ADDR, addr, 1, pdata, 1, 1000);

    if (ret != HAL_OK) return;

    __HAL_I2C_DISABLE(&i2c_Handle);
}

void AW9523B_Write(uint8_t port, uint8_t level)
{
    if (port == 0) {
        I2C_write_data(AW9523B_OUTPUT_PORT0, level);
    } else {
        I2C_write_data(AW9523B_OUTPUT_PORT1, level);
    }
}

void AW9523B_Write_Pin(uint8_t pin, uint8_t level)
{
    uint8_t data = 0;
    if (pin < 8) {

        I2C_read_data(AW9523B_OUTPUT_PORT0, &data);

        if (level) {
            data |= (0x01 << pin);
        } else {
            data &= ~(0x01 << pin);
        }

        I2C_write_data(AW9523B_OUTPUT_PORT0, data);
    } else {

        I2C_read_data(AW9523B_OUTPUT_PORT1, &data);

        if (level) {
            data |= (0x01 << (pin - 8));
        } else {
            data &= ~(0x01 << (pin - 8));
        }
        I2C_write_data(AW9523B_OUTPUT_PORT1, data);
    }
}

uint8_t AW9523B_Read(uint8_t port)
{
    uint8_t data = 0;
    if (port == 0) {
        I2C_read_data(AW9523B_INPUT_PORT0, &data);
    } else {
        I2C_read_data(AW9523B_INPUT_PORT1, &data);
    }
    return data;
}

uint8_t AW9523B_Read_Pin(uint8_t pin)
{
    uint8_t data = 0;
    uint8_t pin_val = 0;
    if (pin < 8) {
        I2C_read_data(AW9523B_INPUT_PORT0, &data);
        pin_val = data & (0x01 << pin);
    } else {
        I2C_read_data(AW9523B_INPUT_PORT1, &data);
        pin_val = data & (0x01 << (pin - 8));
    }
    return (pin_val > 0) ? 1 : 0;
}

  void AW9523B_Init(uint16_t dir)
  {
      uint8_t data = 0;

      AW9523B_I2C_Init();
      HAL_Delay(10);

      I2C_write_data(AW9523B_SOFT_RST, 0x00);
      HAL_Delay(5);
      data = 0x10;
      I2C_write_data(AW9523B_GLOBAL_CTL, data);
      HAL_Delay(1);

      data = dir & 0xFF;
      I2C_write_data(AW9523B_CONFIG_PORT0, data);

      data = (dir >> 8) & 0xFF;
      I2C_write_data(AW9523B_CONFIG_PORT1, data);
      HAL_Delay(1);

      I2C_write_data(AW9523B_OUTPUT_PORT0, 0x00);
      I2C_write_data(AW9523B_OUTPUT_PORT1, 0x00);
  }
