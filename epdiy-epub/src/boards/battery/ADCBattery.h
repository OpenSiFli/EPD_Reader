#include "Battery.h"
#include <math.h>

class ADCBattery : public Battery
{
private:


public:
  ADCBattery()
  {
  }
  virtual void setup()
  {
    
  }
  virtual float get_voltage()
  {
    
    return 0;
  }

  int get_percentage()
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
    // see here for inspiration: https://github.com/G6EJD/ESP32-e-Paper-Weather-Display/issues/146
    return roundf(2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303);
  }
};