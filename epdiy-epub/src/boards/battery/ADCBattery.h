#include "Battery.h"
#include <math.h>
extern "C" {
#include "rtdevice.h"
#include "board.h"
}

class ADCBattery : public Battery
{
private:
    rt_device_t s_adc_dev;
    int channel;

public:
    ADCBattery()
  {
      s_adc_dev = rt_device_find("bat1"); 
      channel = 7;              
      if (s_adc_dev)
      {
          rt_adc_enable((rt_adc_device_t)s_adc_dev, channel);
      }

  }
  virtual void setup() override
  {
      rt_kprintf("[ADCBattery] setup completed\n");
  }
    
  virtual float get_voltage() override
  {
      if (!s_adc_dev) {
          rt_kprintf("[ADCBattery] get_voltage: s_adc_dev is NULL\n");
          return 0;
      }
      rt_uint32_t value = rt_adc_read((rt_adc_device_t)s_adc_dev, channel);
      float voltage = value / 10.0f; 
      return voltage;
  }

  virtual int get_percentage() override
  {
    auto voltage = get_voltage() / 1000.0f;
    if (voltage >= 4.20)
    {
      return 100;
    }
    if (voltage <= 3.50)
    {
      return 0;
    }
    return roundf(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
  }
  
  virtual bool is_charging() override
  {
      int pin_val = rt_pin_read(CHG_STATUS);
      return pin_val == 0;
  }
};