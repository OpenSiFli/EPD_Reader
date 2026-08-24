#ifndef SF32LB57X
#include "Battery.h"
#include <math.h>
extern "C" {
#include "rtdevice.h"
#include "board.h"
#include "battery_calculator.h"
#include "charge.h"
}


class ADCBattery : public Battery
{
private:
    rt_device_t s_adc_dev;
    int channel;
    rt_timer_t battery_check_timer;
    rt_mq_t ui_queue;
    uint8_t low_power; 

    battery_calculator_t battery_calc;

    static void battery_check_callback(void* parameter);
    static rt_err_t charge_event_callback(rt_device_t dev, rt_size_t size);
    void start_battery_monitor();

public:
    ADCBattery(rt_mq_t ui_queue);
    virtual void setup() override;
    
    virtual float get_voltage() override;
    virtual int get_percentage() override;
    virtual bool is_charging() override;
    
    virtual bool get_low_power_state() override;
    void set_low_power_state(uint8_t state); 
    void stop_battery_monitor(); 
    ~ADCBattery();

    
};
#endif /* !SF32LB57X */