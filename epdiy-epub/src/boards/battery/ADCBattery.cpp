#include "battery/ADCBattery.h"
#include "Battery.h"
#include "boards/controls/TouchControls.h"
#include <math.h>
extern "C" {
#include "rtdevice.h"
#include "board.h"
#include "battery_calculator.h"
}

rt_err_t ADCBattery::charge_event_callback(rt_device_t dev, rt_size_t size)
{
    rt_charge_event_t event = (rt_charge_event_t)size;
    
    switch (event)
    {
        case RT_CHARGE_EVENT_DETECT:
        {
            rt_kprintf("Charge detect event\n");
            extern rt_mq_t ui_queue;
            if (ui_queue) 
            {
                // 发送刷新充电状态的消息
                UIAction msg = MSG_UPDATE_CHARGE_STATUS;
                rt_mq_send(ui_queue, &msg, sizeof(UIAction));
            }
            break;
        }
        case RT_CHARGE_EVENT_END:
            rt_kprintf("Charge end event\n");
            break;
        default:
            break;
    }
    
    return RT_EOK;
}
ADCBattery::ADCBattery(rt_mq_t ui_queue)
{
    this->ui_queue = ui_queue;  
    s_adc_dev = rt_device_find("bat1");
    channel = 7;
    low_power = 0; // 初始化low_power为0
    if (s_adc_dev)
    {
        rt_adc_enable((rt_adc_device_t)s_adc_dev, channel);
    }
    battery_check_timer = RT_NULL;

    // 初始化电池计算器
    static const battery_calculator_config_t config = {
        .charging_table = charging_curve_table,
        .charging_table_size = charging_curve_table_size,
        .discharging_table = discharge_curve_table,
        .discharging_table_size = discharge_curve_table_size,
        .charge_filter_threshold = 50,          // 充电状态下的电压变化滤波阈值(mV)
        .discharge_filter_threshold = 50,       // 放电状态下的电压变化滤波阈值(mV)
        .filter_count = 3,                      // 滤波计数阈值
        .secondary_filter_enabled = true,       // 启用二级滤波
        .secondary_filter_weight_pre = 90,      // 上次电压权重
        .secondary_filter_weight_cur = 10       // 当前电压权重
    };
    
    int result = battery_calculator_init(&battery_calc, &config);
    if (result != BATTERY_CALC_SUCCESS) 
    {
        rt_kprintf("[ADCBattery] Failed to initialize battery calculator: %d\n", result);
    }

   
}



void ADCBattery::start_battery_monitor()
{
    battery_check_timer = rt_timer_create("battery_check", 
                                        battery_check_callback, 
                                        this,  
                                        rt_tick_from_millisecond(10000), // 10秒
                                        RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_SOFT_TIMER);

    if (battery_check_timer != RT_NULL) 
    {
        rt_timer_start(battery_check_timer);
        rt_kprintf("[ADCBattery] Battery check timer started\n");
    } 
    else 
    {
        rt_kprintf("[ADCBattery] Failed to create battery check timer\n");
    }
}
void ADCBattery::setup()
{
    rt_kprintf("[ADCBattery] setup completed\n");
    start_battery_monitor();
   
    // 注册事件回调函数
    rt_charge_set_rx_ind(charge_event_callback);
    rt_kprintf("[ADCBattery] Charge device found and callback registered\n");
    
}
float ADCBattery::get_voltage()
{
    if (!s_adc_dev) {
        rt_kprintf("[ADCBattery] get_voltage: s_adc_dev is NULL\n");
        return 0;
    }
    rt_uint32_t value = rt_adc_read((rt_adc_device_t)s_adc_dev, channel);
    float voltage = value / 10.0f; 
    return voltage;
}

int ADCBattery::get_percentage()
{
    float voltage = get_voltage();
    uint8_t percentage = battery_calculator_get_percent(&battery_calc, (uint32_t)(voltage * 10));
    return percentage;
}

bool ADCBattery::is_charging()
{
    int pin_val = rt_pin_read(CHG_STATUS);
    return pin_val == 0;
}

void ADCBattery::stop_battery_monitor()
{
    if (battery_check_timer != RT_NULL)
    {
        rt_timer_stop(battery_check_timer);
        rt_timer_delete(battery_check_timer);
        battery_check_timer = RT_NULL;
    }
}

ADCBattery::~ADCBattery()
{
    rt_charge_set_rx_ind(RT_NULL);
    
    if (battery_check_timer != RT_NULL)
    {
        rt_timer_stop(battery_check_timer);
        rt_timer_delete(battery_check_timer);
    }
}

void ADCBattery::battery_check_callback(void* parameter)
{
    ADCBattery* battery = static_cast<ADCBattery*>(parameter);
    if (battery && battery->ui_queue) 
    {
        float voltage = battery->get_voltage();
        uint8_t percentage = battery_calculator_get_percent(&battery->battery_calc, (uint32_t)(voltage * 10));
        bool is_charging = battery->is_charging();
        rt_kprintf("[ADCBattery] Battery Level %f, percent %d\n", voltage, (int)percentage);
        
        // 发送消息到UI队列
        if (percentage < 2.0f && !is_charging && battery->low_power != 1) {
            battery->low_power = 1;        
            UIAction msg = MSG_DRAW_LOW_POWER_PAGE;
            rt_mq_send(battery->ui_queue, &msg, sizeof(UIAction));
        }
        // 如果正在充电且之前处于低电量模式，则进入充电页面
        else if (is_charging && battery->low_power == 1) 
        {
            UIAction msg = MSG_DRAW_CHARGE_PAGE;
            rt_mq_send(battery->ui_queue, &msg, sizeof(UIAction));
        }
        // 如果电量充足且之前处于低电量模式，则恢复正常模式
        else if (percentage >= 2.0f && battery->low_power == 1) 
        {
            battery->low_power = 0;           
            UIAction msg = MSG_DRAW_WELCOME_PAGE;
            rt_mq_send(battery->ui_queue, &msg, sizeof(UIAction));
        }    
    }
}

// 获取low_power状态
bool ADCBattery::get_low_power_state() 
{
    return low_power;
}

// 设置low_power状态
void ADCBattery::set_low_power_state(uint8_t state) 
{
    low_power = state;
}
