#include "SF32_TouchControls.h"
#include <Renderer/Renderer.h>
#include "Actions.h"
#include "epd_driver.h"
#include "type.h"
#include "epub_screen.h"
#include "EpubReader.h"
#include "UIRegionsManager.h"
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
extern bool toc_bottom_mode;//控制目录页面中功能选项的开关
static int last_clicked_toc_index = -1;  // -1 表示没有目录项被选中
// 添加一个变量来记录上次点击的书籍索引
static int last_clicked_book_index = -1;  // -1 表示没有书籍被选中
static bool waiting_for_confirmation = false;  // 是否正在等待确认
extern int touch_sel;
extern EpubReader *reader;

static const int SWIPE_THRESHOLD = 100;  // 最小滑动距离阈值
static bool is_touch_started = false;  // 全局或类成员变量

extern AreaRect g_area_array[];
rt_err_t SF32_TouchControls::tp_rx_indicate(rt_device_t dev, rt_size_t size)
{
    SF32_TouchControls *instance = static_cast<SF32_TouchControls*> (dev->user_data);
    struct touch_message touch_data;
    rt_uint16_t x,y;
    int i  = 0;//用于记录第一次按下的情况

    /*Read touch point data*/
    rt_device_read(dev, 0, &touch_data, 1);

    //Rotate anti-clockwise 90 degree
    x = LCD_VER_RES_MAX - touch_data.y - 1;
    y = touch_data.x;
    

    // if (TOUCH_EVENT_DOWN == touch_data.event)
    //     rt_kprintf("Touch down [%d,%d]\r\n", x, y);
    // else
    //     rt_kprintf("Touch up   [%d,%d]\r\n", x, y);
  if (TOUCH_EVENT_DOWN == touch_data.event) 
    {
        rt_kprintf("Touch down [%d,%d]\r\n", x, y);
        
        // 记录按下时的位置
        if (!is_touch_started)  // 只允许第一次触发
        {
            instance->touch_start_y = y;
            rt_kprintf("Touch start\r\n");
            is_touch_started = true;
        }
        
        instance->is_touch_down = true;
        instance->touch_current_y = 0;
        // 处理其他触控逻辑...
    }
    else
    {
        rt_kprintf("Touch up [%d,%d]\r\n", x, y);
        instance->touch_current_y = y;
        
        // 检查是否构成向上滑动手势
        if (instance->is_touch_down) 
        {
            int y_diff = instance->touch_start_y - instance->touch_current_y;  // 注意坐标转换
            rt_kprintf("Touch up diff Y: %d\r\n", y_diff);
            
            if(reader->is_overlay_active() == false)
            {
                if (y_diff > SWIPE_THRESHOLD) 
                {
                    rt_kprintf("Up swipe detected! Diff: %d\n", y_diff);
                    
                    // 发送向上滑动动作
                    UIAction action = UPGLIDE;
                    instance->last_action = action;
                    instance->on_action(action);               
                                        
                }
            }
            
             
        }
        
        // 清空坐标值，避免下次误用
        instance->touch_start_y = 0;
        // 重置触摸状态
        instance->is_touch_down = false;
        is_touch_started = false;
    }

  // 只处理按下事件，忽略释放事件
  if (TOUCH_EVENT_UP == touch_data.event) {
      return RT_EOK;
  }

  UIAction action = NONE;
  // LOG_I("TOUCH", "Received touch event %d,%d", x, y);
  // 主页面底部按键区域:左"<"、右">"、中间文本框
switch (ui_state)
{
  case MAIN_PAGE://主页面
    if (x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
    {
      rt_kprintf("Touch left < \n");
      action = UP;  // 对应 KEY3 功能
    }
    else if (x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
    {
      action = DOWN;  // 对应 KEY1 功能
      rt_kprintf("Touch right > \n");
    }
    else if (x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
    {
      action = SELECT;  // 对应 KEY2 功能
      rt_kprintf("Touch middle SELECT \n");
    }
    break;
  case SELECTING_EPUB://书库页面
      // 检查是否点击了功能控制按钮
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
        // 处理书籍选择区域
        int clicked_book_index = -1;
        
        if(x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
        {
            clicked_book_index = 0;
        }
        else if(x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
        {
            clicked_book_index = 1;
        }
        else if(x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
        {
            clicked_book_index = 2;
        }
        else if(x >= g_area_array[3].start_x && x <= g_area_array[3].end_x && y >= g_area_array[3].start_y && y <= g_area_array[3].end_y)
        {
            clicked_book_index = 3;
        }

        // 如果点击了书籍区域
        if(clicked_book_index != -1)
        {
            // 判断是第一次点击还是第二次点击
            if(waiting_for_confirmation && last_clicked_book_index == clicked_book_index)
            {
                // 第二次点击：执行打开操作
                book_index = clicked_book_index;
                library_bottom_mode = false;
                rt_kprintf("Open book%d %d\n", book_index, book_index);
                action = SELECT;
                
                // 重置状态
                waiting_for_confirmation = false;
                last_clicked_book_index = -1;
            }
            else
            {
                // 第一次点击：选择书籍并等待确认
                book_index = clicked_book_index;
                last_clicked_book_index = clicked_book_index;
                waiting_for_confirmation = true;
                action = SELECT_BOX;
                rt_kprintf("Select book%d for confirmation, waiting for second click\n", book_index);
            }
        }
    }
    break;
  case READING_EPUB: //阅读界面
    //翻页操作
    if(x >= 10 && x <= 200 && y >=10 && y <= 1010 && reader->is_overlay_active() == false)
    {
        action = UP;
    }
    else if(x >= 550 && x <= 750 && y >=10 && y <= 1010 && reader->is_overlay_active() == false)
    {
        action = DOWN;
    }
    //点击正文,推出阅读设置
    if(x >= 10 && x <= 750 && y >=10 && y <= 630 && reader->is_overlay_active())
    {
        touch_sel = 8;
        action = SELECT;
    }

    //阅读页面控制区域设置
    if(x >= g_area_array[8].start_x && x <= g_area_array[8].end_x && y >= g_area_array[8].start_y && y <= g_area_array[8].end_y && reader->is_overlay_active())//第三层
    {
        touch_sel = 8;
        action = SELECT;
    }
    else if(x >= g_area_array[9].start_x && x <= g_area_array[9].end_x && y >= g_area_array[9].start_y && y <= g_area_array[9].end_y && reader->is_overlay_active())
    {
        touch_sel = 9;
        action = SELECT;
    }
    else if(x >= g_area_array[10].start_x && x <= g_area_array[10].end_x && y >= g_area_array[10].start_y && y <= g_area_array[10].end_y && reader->is_overlay_active())
    {
        touch_sel = 10;
        action = SELECT;
    }
    else if(x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y && reader->is_overlay_active())//第一层
    {
        touch_sel= 0;
        rt_kprintf("Touch middle SELECT %d\n",touch_sel);
        action = SELECT;
    }
    else if(x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y && reader->is_overlay_active())
    {
        touch_sel= 1;
        rt_kprintf("Touch middle SELECT %d\n",touch_sel);
        action = SELECT;
    }
    else if(x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y && reader->is_overlay_active())
    {
        touch_sel= 2;
        rt_kprintf("Touch middle SELECT %d\n",touch_sel);
        action = SELECT;
    }
    else if(x >= g_area_array[3].start_x && x <= g_area_array[3].end_x && y >= g_area_array[3].start_y && y <= g_area_array[3].end_y && reader->is_overlay_active())
    {
        touch_sel = 3;//跳转-5页
        action = SELECT;
    }
    else if(x >= g_area_array[4].start_x && x <= g_area_array[4].end_x && y >= g_area_array[4].start_y && y <= g_area_array[4].end_y && reader->is_overlay_active())
    {
        touch_sel = 4;//跳转-1页
        action = SELECT;
    }
    else if(x >= g_area_array[6].start_x && x <= g_area_array[6].end_x && y >= g_area_array[6].start_y && y <= g_area_array[6].end_y && reader->is_overlay_active())
    {
        touch_sel = 6;//跳转+1页
        action = SELECT;
    }
    else if(x >= g_area_array[7].start_x && x <= g_area_array[7].end_x && y >= g_area_array[7].start_y && y <= g_area_array[7].end_y && reader->is_overlay_active())
    {
        touch_sel = 7;//跳转5页
        action = SELECT;
    }
    
    break;
  case SELECTING_TABLE_CONTENTS: //目录界面
    rt_kprintf("g_area_array[0].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[0].start_x ,g_area_array[0].end_x ,g_area_array[0].start_y ,g_area_array[0].end_y);
    rt_kprintf("g_area_array[1].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[1].start_x ,g_area_array[1].end_x ,g_area_array[1].start_y ,g_area_array[1].end_y);
    rt_kprintf("g_area_array[2].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[2].start_x ,g_area_array[2].end_x ,g_area_array[2].start_y ,g_area_array[2].end_y);
    rt_kprintf("g_area_array[3].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[3].start_x ,g_area_array[3].end_x ,g_area_array[3].start_y ,g_area_array[3].end_y);
    rt_kprintf("g_area_array[4].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[4].start_x ,g_area_array[4].end_x ,g_area_array[4].start_y ,g_area_array[4].end_y);
    rt_kprintf("g_area_array[5].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[5].start_x ,g_area_array[5].end_x ,g_area_array[5].start_y ,g_area_array[5].end_y);
    rt_kprintf("g_area_array[6].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[6].start_x ,g_area_array[6].end_x ,g_area_array[6].start_y ,g_area_array[6].end_y);
    rt_kprintf("g_area_array[7].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[7].start_x ,g_area_array[7].end_x ,g_area_array[7].start_y ,g_area_array[7].end_y);
    rt_kprintf("g_area_array[8].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[8].start_x ,g_area_array[8].end_x ,g_area_array[8].start_y ,g_area_array[8].end_y);
    rt_kprintf("g_area_array[9].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[9].start_x ,g_area_array[9].end_x ,g_area_array[9].start_y ,g_area_array[9].end_y);
    rt_kprintf("g_area_array[10].start_x %d end_x %d start_y %d end_y %d\n",g_area_array[10].start_x ,g_area_array[10].end_x ,g_area_array[10].start_y ,g_area_array[10].end_y);

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
    else// 目录项选择区域
    {
        int clicked_toc_index = -1;

        if(x >= g_area_array[0].start_x && x <= g_area_array[0].end_x && y >= g_area_array[0].start_y && y <= g_area_array[0].end_y)
        {
          clicked_toc_index = 0;
        }
        else if(x >= g_area_array[1].start_x && x <= g_area_array[1].end_x && y >= g_area_array[1].start_y && y <= g_area_array[1].end_y)
        {
          clicked_toc_index = 1;
        }
        else if(x >= g_area_array[2].start_x && x <= g_area_array[2].end_x && y >= g_area_array[2].start_y && y <= g_area_array[2].end_y)
        {
          clicked_toc_index = 2;
        }
        else if(x >= g_area_array[3].start_x && x <= g_area_array[3].end_x && y >= g_area_array[3].start_y && y <= g_area_array[3].end_y)
        {
          clicked_toc_index = 3;
        }
        else if(x >= g_area_array[4].start_x && x <= g_area_array[4].end_x && y >= g_area_array[4].start_y && y <= g_area_array[4].end_y)
        {
          clicked_toc_index = 4;
        }
        else if(x >= g_area_array[5].start_x && x <= g_area_array[5].end_x && y >= g_area_array[5].start_y && y <= g_area_array[5].end_y)
        {
          clicked_toc_index = 5;
        }
        // 如果点击了目录项区域
        if(clicked_toc_index != -1)
        {
            // 判断是第一次点击还是第二次点击
            if(waiting_for_confirmation && last_clicked_toc_index == clicked_toc_index)
            {
                // 第二次点击：执行打开操作
                toc_index = clicked_toc_index;
                library_bottom_mode = false;
                rt_kprintf("Open book%d %d\n", toc_index, toc_index);
                action = SELECT;
                
                // 重置状态
                waiting_for_confirmation = false;
                last_clicked_toc_index = -1;
            }
            else
            {
                // 第一次点击：选择目录项并等待确认
                toc_index = clicked_toc_index;
                last_clicked_toc_index = clicked_toc_index;
                waiting_for_confirmation = true;
                action = SELECT_BOX;
            }
        }
    }

    break;
  case SETTINGS_PAGE: // 设置页面
    // 设置页面每行左右箭头触控区域（与设置页布局一致）
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
    else if (x >= g_area_array[9].start_x && x <= g_area_array[9].end_x && y >= g_area_array[9].start_y && y <= g_area_array[9].end_y)
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