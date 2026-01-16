
#include "SF32_TouchControls.h"
#include <Renderer/Renderer.h>
#include "epd_driver.h"
#ifdef BSP_USING_TOUCHD
    #include "drv_touch.h"
#endif

volatile int g_touch_last_settings_row = -1;
volatile int g_touch_last_settings_dir = 0;

rt_err_t SF32_TouchControls::tp_rx_indicate(rt_device_t dev, rt_size_t size)
{
    SF32_TouchControls *instance = static_cast<SF32_TouchControls*> (dev->user_data);
    struct touch_message touch_data;
    rt_uint16_t x,y;

    /*Read touch point data*/
    rt_device_read(dev, 0, &touch_data, 1);

    //Rotate anti-clockwise 90 degree
    x = LCD_VER_RES_MAX - touch_data.y - 1;
    y = touch_data.x;
    

    if (TOUCH_EVENT_DOWN == touch_data.event)
        rt_kprintf("Touch down [%d,%d]\r\n", x, y);
    else
        rt_kprintf("Touch up   [%d,%d]\r\n", x, y);



  UIAction action = NONE;
  // LOG_I("TOUCH", "Received touch event %d,%d", x, y);
  // 主页面底部按键区域:左"<"、右">"、中间文本框
    int page_w = instance->renderer->get_page_width();
    int page_h = instance->renderer->get_page_height();
    int margin_side = 10;
    int margin_bottom = 60; 
    int rect_w = 80;
    int rect_h = 40;
    int y_bottom = page_h - rect_h - margin_bottom;
    int left_x = margin_side;
    int right_x = page_w - rect_w - margin_side;
    int mid_x = left_x + rect_w + margin_side;
    int mid_w = right_x - margin_side - mid_x;

    if (x >= left_x && x <= left_x + rect_w && y >= y_bottom && y <= y_bottom + rect_h)
    {
      rt_kprintf("Touch left < \n");
      action = UP;
    }

    else if (x >= right_x && x <= right_x + rect_w && y >= y_bottom && y <= y_bottom + rect_h)
    {
      action = DOWN;
      rt_kprintf("Touch right > \n");
    }
  
  // 设置页面每行左右箭头触控区域（与设置页布局一致）
  if (action == NONE)
  {
    int page_w2 = instance->renderer->get_page_width();
    int margin_lr2 = 6;
    int item_h2 = 100;
    int gap2 = 54;
    int arrow_col_w2 = 40;
    int lh2 = instance->renderer->get_line_height();
    int y_start2 = 40 + lh2 + 20;
    g_touch_last_settings_row = -1;
    g_touch_last_settings_dir = 0;
    for (int row = 0; row < 3; ++row)
    {
      int ry = y_start2 + row * (item_h2 + gap2);
      int left_ax = margin_lr2;
      int right_ax = page_w2 - margin_lr2 - arrow_col_w2;
      if (x >= left_ax && x <= left_ax + arrow_col_w2 && y >= ry && y <= ry + item_h2)
      {
        action = UP;
        g_touch_last_settings_row = row;
        g_touch_last_settings_dir = -1; // 左=减
        break;
      }
      if (x >= right_ax && x <= right_ax + arrow_col_w2 && y >= ry && y <= ry + item_h2)
      {
        action = DOWN;
        g_touch_last_settings_row = row;
        g_touch_last_settings_dir = +1; // 右=加
        break;
      }
    }
  }

  instance->last_action = action;
  if (action != NONE)
  {
    instance->on_action(action);
  }
    
    return RT_EOK;
}
extern uint8_t touch_enable;
SF32_TouchControls::SF32_TouchControls(Renderer *renderer, ActionCallback_t on_action)
  : on_action(on_action), renderer(renderer)
{
    tp_device = rt_device_find("touch");

      if (RT_EOK == rt_device_open(tp_device, RT_DEVICE_FLAG_RDONLY))
      {
          /*Setup rx indicate callback*/
          tp_device->user_data = (void *)this;
          rt_device_set_rx_indicate(tp_device, tp_rx_indicate);
      }
      if(!touch_enable)
      {
        rt_device_control(tp_device, RTGRAPHIC_CTRL_POWEROFF, NULL);
      }
}


void SF32_TouchControls::render(Renderer *renderer)
{
  renderer->set_margin_top(35);
}

void SF32_TouchControls::powerOffTouch()
{
   if (tp_device) 
    {
      rt_device_control(tp_device, RTGRAPHIC_CTRL_POWEROFF, NULL);
      rt_kprintf("touch close\n");
    } else {
        rt_kprintf("no touch device found\n");
    }
}

void SF32_TouchControls::powerOnTouch()
{
   if (tp_device) {
      rt_device_control(tp_device, RTGRAPHIC_CTRL_POWERON, NULL);
      rt_kprintf("touch open\n");
    } else {
        rt_kprintf("no touch device found\n");
    }
}
void SF32_TouchControls::renderPressedState(Renderer *renderer, UIAction action, bool state)
{
  renderer->set_margin_top(35);
}