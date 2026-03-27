
#include "SF32_TouchControls.h"
#include <Renderer/Renderer.h>
#include "epd_driver.h"
#ifdef BSP_USING_TOUCHD
    #include "drv_touch.h"
#endif

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
  if (x >= 10 && x <= 10 + instance->ui_button_width && y < 200)
  {
    action = DOWN;
    instance->renderPressedState(instance->renderer, UP, false);
  }
  else if (x >= 150 && x <= 150 + instance->ui_button_width && y < 200)
  {
    action = UP;
    instance->renderPressedState(instance->renderer, DOWN, false);
  }
  else if (x >= 300 && x <= 300 + instance->ui_button_width && y < 200)
  {
    action = SELECT;
  }
  else
  {

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
  renderer->set_margin_top(0);
  uint16_t x_offset = 10;
  uint16_t x_triangle = x_offset + 70;
  // DOWN
  renderer->draw_rect(x_offset, 1, ui_button_width, ui_button_height, 0);
  renderer->draw_triangle(x_triangle, 20, x_triangle - 5, 6, x_triangle + 5, 6, 0);
  // UP
  x_offset = ui_button_width + 30;
  x_triangle = x_offset + 70;
  renderer->draw_rect(x_offset, 1, ui_button_width, ui_button_height, 0);
  renderer->draw_triangle(x_triangle, 6, x_triangle - 5, 20, x_triangle + 5, 20, 0);
  // SELECT
  x_offset = ui_button_width * 2 + 60;
  renderer->draw_rect(x_offset, 1, ui_button_width, ui_button_height, 0);
  renderer->draw_circle(x_offset + (ui_button_width / 2) + 9, 15, 5, 0);
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
  renderer->set_margin_top(0);
  switch (action)
  {
  case DOWN:
  {
    if (state)
    {
      renderer->fill_triangle(80, 20, 75, 6, 85, 6, 0);
    }
    else
    {
      renderer->fill_triangle(81, 19, 76, 7, 86, 7, 255);
    }
    //renderer->flush_area(76, 6, 10, 15);
    break;
  }
  case UP:
  {
    if (state)
    {
      renderer->fill_triangle(220, 6, 220 - 5, 20, 220 + 5, 20, 0);
    }
    else
    {
      renderer->fill_triangle(221, 7, 221 - 5, 19, 221 + 5, 19, 255);
    }
    //renderer->flush_area(195, 225, 10, 15);
  }
  break;
  case SELECT:
  {
    uint16_t x_circle = (ui_button_width * 2 + 60) + (ui_button_width / 2) + 9;
    renderer->fill_circle(x_circle, 15, 5, 0);
    //renderer->flush_area(x_circle - 3, 12, 6, 6);
    // TODO - this causes a stack overflow when select is picked
    // renderPressedState(renderer, last_action, false);
  }
  break;
  case LAST_INTERACTION:
  case NONE:
    break;
  }
  renderer->set_margin_top(35);
}