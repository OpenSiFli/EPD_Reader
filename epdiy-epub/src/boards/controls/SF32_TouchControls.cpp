#include "SF32_TouchControls.h"
#include <Renderer/Renderer.h>
#include "Actions.h"
#include "epd_driver.h"
#include "type.h"
#include "epub_screen.h"
#include "EpubReader.h"

#include "UIRegionsManager.h"
#include "reading_settings.h"

#ifdef BSP_USING_TOUCHD
    #include "drv_touch.h"
#endif

volatile int g_touch_last_settings_row = -1;
volatile int g_touch_last_settings_dir = 0;
extern int settings_selected_idx;
extern AppUIState ui_state;
extern int book_index;
extern bool library_bottom_mode;
extern int library_bottom_idx;
extern int toc_index;
extern int toc_bottom_idx;
extern bool toc_bottom_mode;
static int last_clicked_toc_index = -1;
static int last_clicked_book_index = -1;
static bool waiting_for_confirmation = false;
extern int touch_sel;
extern EpubReader *reader;

static const int SWIPE_THRESHOLD = 100;
static bool is_touch_started = false;


extern AreaRect g_area_array[];

rt_err_t SF32_TouchControls::tp_rx_indicate(rt_device_t dev, rt_size_t size)
{
    SF32_TouchControls *instance = static_cast<SF32_TouchControls*> (dev->user_data);
    struct touch_message touch_data;
    rt_uint16_t x,y;
    int i  = 0;

    /*Read touch point data*/
    rt_device_read(dev, 0, &touch_data, 1);

    //Rotate anti-clockwise 90 degree
    x = LCD_VER_RES_MAX - touch_data.y - 1;
    y = touch_data.x;

  if (TOUCH_EVENT_DOWN == touch_data.event) 
    {
        rt_kprintf("Touch down [%d,%d]\r\n", x, y);
        
        if (!is_touch_started)
        {
            instance->touch_start_y = y;
            rt_kprintf("Touch start\r\n");
            is_touch_started = true;
        }
        
        instance->is_touch_down = true;
        instance->touch_current_y = 0;
    }
    else
    {
        rt_kprintf("Touch up [%d,%d]\r\n", x, y);
        instance->touch_current_y = y;
        
        if (instance->is_touch_down) 
        {
            int y_diff = instance->touch_start_y - instance->touch_current_y;
            rt_kprintf("Touch up diff Y: %d\r\n", y_diff);
            
            if(reader && reader->is_overlay_active() == false)
            {
                if (y_diff > SWIPE_THRESHOLD) 
                {
                    rt_kprintf("Up swipe detected! Diff: %d\n", y_diff);
                    UIAction action = UPGLIDE;
                    instance->last_action = action;
                    instance->on_action(action);               
                }
            }
        }
        
        instance->touch_start_y = 0;
        instance->is_touch_down = false;
        is_touch_started = false;
    }

  // 只处理按下事件，忽略释放事件
  if (TOUCH_EVENT_UP == touch_data.event) {
      return RT_EOK;
  }

  UIAction action = NONE;

switch (ui_state)
{
  case MAIN_PAGE:
    if (x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
    {
      rt_kprintf("Touch left < \n");
      action = UP;
    }
    else if (x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
    {
      action = DOWN;
      rt_kprintf("Touch right > \n");
    }
    else if (x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
    {
      action = SELECT;
      rt_kprintf("Touch middle SELECT \n");
    }
    break;
  case SELECTING_EPUB:
    if(x >= g_area_array[4].start_x && x <= g_area_array[4].end_x && y >= g_area_array[4].start_y && y <= g_area_array[4].end_y)
    {
        library_bottom_mode = true;
        library_bottom_idx = 0;
        action = SELECT;
    }
    else if(x >= g_area_array[5].start_x && x <= g_area_array[5].end_x && y >= g_area_array[5].start_y && y <= g_area_array[5].end_y)
    {
        library_bottom_mode = true;
        library_bottom_idx = 1;
        action = SELECT;
    }
    else if(x >= g_area_array[6].start_x && x <= g_area_array[6].end_x && y >= g_area_array[6].start_y && y <= g_area_array[6].end_y)
    {
        library_bottom_mode = true;
        library_bottom_idx = 2;
        action = SELECT;
    }
    else
    {
        int clicked_book_index = -1;

        if(x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
            clicked_book_index = 0;
        else if(x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
            clicked_book_index = 1;
        else if(x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
            clicked_book_index = 2;
        else if(x >= g_area_array[3].start_x && x <= g_area_array[3].end_x && y >= g_area_array[3].start_y && y <= g_area_array[3].end_y)
            clicked_book_index = 3;

        if(clicked_book_index != -1)
        {
            if(waiting_for_confirmation && last_clicked_book_index == clicked_book_index)
            {
                book_index = clicked_book_index;
                library_bottom_mode = false;
                rt_kprintf("Open book%d %d\n", book_index, book_index);
                action = SELECT;
                waiting_for_confirmation = false;
                last_clicked_book_index = -1;
            }
            else
            {
                book_index = clicked_book_index;
                last_clicked_book_index = clicked_book_index;
                waiting_for_confirmation = true;
                action = SELECT_BOX;
                rt_kprintf("Select book%d for confirmation, waiting for second click\n", book_index);
            }
        }
    }
    break;
  case READING_EPUB:
    if (reader && reader->is_overlay_active())
    {
      // overlay 激活时：检测 12 个按钮的触控区域（由 render_overlay 通过 add_area 注册）
      bool hit = false;
      for (int btn = 0; btn < 12; btn++)
      {
        if (x >= g_area_array[btn].start_x && x <= g_area_array[btn].end_x &&
            y >= g_area_array[btn].start_y && y <= g_area_array[btn].end_y)
        {
          touch_sel = btn;
          action = SELECT;
          rt_kprintf("Touch overlay button %d\n", btn);
          hit = true;
          break;
        }
      }
      // 点击 overlay 区域外的正文部分 → 关闭 overlay（相当于按确认）
      if (!hit)
      {
        int area_y = (instance->renderer->get_page_height() * 2) / 3;
        if (y < area_y)
        {
          touch_sel = 8; // 确认
          action = SELECT;
        }
      }
    }
    else
    {
      // overlay 未激活：左右翻页
      if(x >= 10 && x <= 200 && y >= 10 && y <= 1010)
      {
          action = UP;
      }
      else if(x >= 550 && x <= 750 && y >= 10 && y <= 1010)
      {
          action = DOWN;
      }
    }
    break;
  case SELECTING_TABLE_CONTENTS:
    if(x >= g_area_array[6].start_x && x <= g_area_array[6].end_x && y >= g_area_array[6].start_y && y <= g_area_array[6].end_y)
    {
        toc_bottom_mode = true;
        toc_bottom_idx = 0;
        action = SELECT;
    }
    else if(x >= g_area_array[7].start_x && x <= g_area_array[7].end_x && y >= g_area_array[7].start_y && y <= g_area_array[7].end_y)
    {
        toc_bottom_mode = true;
        toc_bottom_idx = 1;
        action = SELECT;
    }
    else if(x >= g_area_array[8].start_x && x <= g_area_array[8].end_x && y >= g_area_array[8].start_y && y <= g_area_array[8].end_y)
    {
        toc_bottom_mode = true;
        toc_bottom_idx = 2;
        action = SELECT;
    }
    else
    {
        int clicked_toc_index = -1;

        if(x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
          clicked_toc_index = 0;
        else if(x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
          clicked_toc_index = 1;
        else if(x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
          clicked_toc_index = 2;
        else if(x >= g_area_array[3].start_x && x <= g_area_array[3].end_x && y >= g_area_array[3].start_y && y <= g_area_array[3].end_y)
          clicked_toc_index = 3;
        else if(x >= g_area_array[4].start_x && x <= g_area_array[4].end_x && y >= g_area_array[4].start_y && y <= g_area_array[4].end_y)
          clicked_toc_index = 4;
        else if(x >= g_area_array[5].start_x && x <= g_area_array[5].end_x && y >= g_area_array[5].start_y && y <= g_area_array[5].end_y)
          clicked_toc_index = 5;

        if(clicked_toc_index != -1)
        {
            if(waiting_for_confirmation && last_clicked_toc_index == clicked_toc_index)
            {
                toc_index = clicked_toc_index;
                library_bottom_mode = false;
                rt_kprintf("Open book%d %d\n", toc_index, toc_index);
                action = SELECT;
                waiting_for_confirmation = false;
                last_clicked_toc_index = -1;
            }
            else
            {
                toc_index = clicked_toc_index;
                last_clicked_toc_index = clicked_toc_index;
                waiting_for_confirmation = true;
                action = SELECT_BOX;
            }
        }
    }
    break;
  case SETTINGS_PAGE:
    if (x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
    {
      settings_selected_idx = SET_TOUCH;
      action = SELECT_BOX;
      rt_kprintf("select touch switch\n");
    }
    else if (x >= g_area_array[5].start_x && x <= g_area_array[5].end_x && y >= g_area_array[5].start_y && y <= g_area_array[5].end_y)
    {
      settings_selected_idx = SET_TIMEOUT;
      action = SELECT_BOX;  
      rt_kprintf("select timeout switch\n");
    }
    else if (x >= g_area_array[8].start_x && x <= g_area_array[8].end_x && y >= g_area_array[8].start_y && y <= g_area_array[8].end_y)
    {
      settings_selected_idx = SET_FULL_REFRESH;
      action = SELECT_BOX;  
      rt_kprintf("select full refresh switch \n");
    }
    else if (x >= g_area_array[SET_READING_SETTINGS * 3 + 2].start_x && x <= g_area_array[SET_READING_SETTINGS * 3 + 2].end_x &&
             y >= g_area_array[SET_READING_SETTINGS * 3 + 2].start_y && y <= g_area_array[SET_READING_SETTINGS * 3 + 2].end_y)
    {
      settings_selected_idx = SET_READING_SETTINGS;
      action = SELECT_BOX;
      rt_kprintf("select reading settings\n");
    }
    else if (x >= g_area_array[SET_CONFIRM * 3 + 2].start_x && x <= g_area_array[SET_CONFIRM * 3 + 2].end_x &&
             y >= g_area_array[SET_CONFIRM * 3 + 2].start_y && y <= g_area_array[SET_CONFIRM * 3 + 2].end_y)
    {
      settings_selected_idx = SET_CONFIRM;
      action = SELECT;  
      rt_kprintf("select confirm button\n");
    }

    if(settings_selected_idx == SET_TOUCH && g_area_array[0].start_x<=x && x<= g_area_array[0].end_x && g_area_array[0].start_y<=y && y<=g_area_array[0].end_y)
    {
      action = SELECT;
    }
    else if(settings_selected_idx == SET_TOUCH && g_area_array[1].start_x<=x && x<= g_area_array[1].end_x && g_area_array[1].start_y<=y && y<=g_area_array[1].end_y)
    {
      action = SELECT;
    }
    else if(settings_selected_idx == SET_TIMEOUT && g_area_array[3].start_x<=x && x<= g_area_array[3].end_x && g_area_array[3].start_y<=y && y<=g_area_array[3].end_y)
    {
      action = PREV_OPTION;
      rt_kprintf("select timeout Reduce\n");
    }
    else if(settings_selected_idx == SET_TIMEOUT && g_area_array[4].start_x<=x && x<= g_area_array[4].end_x && g_area_array[4].start_y<=y && y<=g_area_array[4].end_y)
    {
      action = NEXT_OPTION;
      rt_kprintf("select timeout increase\n");
    }
    else if(settings_selected_idx == SET_FULL_REFRESH && g_area_array[6].start_x<=x && x<= g_area_array[6].end_x && g_area_array[6].start_y<=y && y<=g_area_array[6].end_y)
    {
      action = PREV_OPTION;
    }
    else if(settings_selected_idx == SET_FULL_REFRESH && g_area_array[7].start_x<=x && x<= g_area_array[7].end_x && g_area_array[7].start_y<=y && y<=g_area_array[7].end_y)
    {
      action = NEXT_OPTION;
    }
    break;
  
  case READING_SETTINGS:
    // 遍历 g_area_array[0..5]，每个对应一个设置项行
    for (int i = 0; i < 6; i++)
    {
      if (x >= g_area_array[i].start_x && x <= g_area_array[i].end_x &&
          y >= g_area_array[i].start_y && y <= g_area_array[i].end_y)
      {
        // 方案A：点到即更改 — 先移动光标到该行，再触发 SELECT
        reading_settings_set_current_item(i);
        action = SELECT;
        rt_kprintf("Touch reading setting item %d\n", i);
        break;
      }
    }
    break;

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