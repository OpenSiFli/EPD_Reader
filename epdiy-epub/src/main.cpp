#include <rtthread.h>
#include "EpubList/Epub.h"
#include "EpubList/EpubList.h"
#include "EpubList/EpubReader.h"
#include "EpubList/EpubToc.h"
#include <RubbishHtmlParser/RubbishHtmlParser.h>
#include "boards/Board.h"
#include "boards/controls/SF32_TouchControls.h"
#include "boards/SF32PaperRenderer.h"
#include "gui_app_pm.h"
#include "epd_tps.h"
#include "epd_pin_defs.h"
#include "drv_gpio.h"
#include "bf0_pm.h"
#include "epd_driver.h"
#undef LOG_TAG
#undef DBG_LEVEL
#define  DBG_LEVEL            DBG_LOG //DBG_INFO  //
#define LOG_TAG                "EPUB.main"

#include <rtdbg.h>



extern "C"
{
  int main();
  rt_uint32_t heap_free_size(void);
}


const char *TAG = "main";

typedef enum
{
  SELECTING_EPUB,
  SELECTING_TABLE_CONTENTS,
  READING_EPUB
} UIState;

// default to showing the list of epubs to the user
UIState ui_state = SELECTING_EPUB;
// the state data for the epub list and reader
EpubListState epub_list_state;
// the state data for the epub index list
EpubTocState epub_index_state;

void handleEpub(Renderer *renderer, UIAction action);
void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw);

static EpubList *epub_list = nullptr;
static EpubReader *reader = nullptr;
static EpubToc *contents = nullptr;


uint8_t touch_enable = 0;
// 声明全局变量，以便open_tp_lcd和close_tp_lcd函数可以访问
Renderer *renderer = nullptr;
TouchControls *touch_controls = nullptr;

//extern rt_err_t tps_enter_sleep(void);
static uint8_t open_state = 1;

void close_tp_lcd()
{ 
  
  if(open_state)
  {
    SF32PaperRenderer *sf32_renderer = static_cast<SF32PaperRenderer*>(renderer);
    sf32_renderer->powerOffLcd();
    open_state = 0;
  }
  if(!touch_enable)
  {
    // 需要进行类型转换，因为touch_controls是基类指针
    SF32_TouchControls *sf32_touch_controls = static_cast<SF32_TouchControls*>(touch_controls);
    sf32_touch_controls->powerOffTouch();
    
  }
  rt_pm_release(PM_SLEEP_MODE_IDLE); 
}

void open_tp_lcd()
{ 
  rt_pm_request(PM_SLEEP_MODE_IDLE);
  if(!open_state)
  {
    SF32PaperRenderer *sf32_renderer = static_cast<SF32PaperRenderer*>(renderer);
    sf32_renderer->powerOnLcd();
    open_state = 1;
  }

  if(touch_enable)
  {
    // 需要进行类型转换，因为touch_controls是基类指针
    SF32_TouchControls *sf32_touch_controls = static_cast<SF32_TouchControls*>(touch_controls);
    sf32_touch_controls->powerOnTouch();
  }
  rt_pm_release(PM_SLEEP_MODE_IDLE); 
}
void handleEpub(Renderer *renderer, UIAction action)
{
  if (!reader)
  {
    reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
    reader->load();
  }
  switch (action)
  {
  case UP:
    reader->prev();
    break;
  case DOWN:
    reader->next();
    break;
  case SELECT:
    // switch back to main screen
    ui_state = SELECTING_EPUB;
    renderer->clear_screen();
    // clear the epub reader away
    delete reader;
    reader = nullptr;
    // force a redraw
    if (!epub_list)
    {
      epub_list = new EpubList(renderer, epub_list_state);
    }
    handleEpubList(renderer, NONE, true);
    return;
  case NONE:
  default:
    break;
  }
  reader->render();
}

void handleEpubTableContents(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (!contents)
  {
    contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item], epub_index_state, renderer);
    contents->set_needs_redraw();
    contents->load();
  }
  switch (action)
  {
  case UP:
    contents->prev();
    break;
  case DOWN:
    contents->next();
    break;
  case SELECT:
    // setup the reader state
    ui_state = READING_EPUB;
    // create the reader and load the book
    reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
    reader->set_state_section(contents->get_selected_toc());
    reader->load();
    //switch to reading the epub
    delete contents;
    handleEpub(renderer, NONE);
    return;
  case NONE:
  default:
    break;
  }
  contents->render();
}

void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw)
{
  // load up the epub list from the filesystem
  if (!epub_list)
  {
    ulog_i("main", "Creating epub list");
    epub_list = new EpubList(renderer, epub_list_state);
    if (epub_list->load("/"))
    {
      ulog_i("main", "Epub files loaded");
    }
  }
  if (needs_redraw)
  {
    epub_list->set_needs_redraw();
  }
  // work out what the user wants us to do
  switch (action)
  {
  case UP:
    epub_list->prev();
    break;
  case DOWN:
    epub_list->next();
    break;
  case SELECT:
    // 检查是否选中了底部特殊区域
    if (epub_list_state.selected_item == -1) {
      // 打印"1"
      rt_kprintf("1\n");
      touch_enable = !touch_enable;  

      // 刷新屏幕以更新底部区域的文本显示
      epub_list->render();

      return;
    } 
    else 
    {
      // switch to reading the epub
      // setup the reader state
      ui_state = SELECTING_TABLE_CONTENTS;
      // create the reader and load the book
      contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item], epub_index_state, renderer);
      contents->load();
      contents->set_needs_redraw();
      handleEpubTableContents(renderer, NONE, true);
      return;
    }
  case NONE:
  default:
    // nothing to do
    break;
  }
  epub_list->render();
}
// TODO - add the battery level
void draw_battery_level(Renderer *renderer, float voltage, float percentage)
{
  // clear the margin so we can draw the battery in the right place
  renderer->set_margin_top(0);
  int width = 40;
  int height = 20;
  int margin_right = 5;
  int margin_top = 10;
  int xpos = renderer->get_page_width() - width - margin_right;
  int ypos = margin_top;
  int percent_width = width * percentage / 100;
  renderer->fill_rect(xpos, ypos, width, height, 255);
  renderer->fill_rect(xpos + width - percent_width, ypos, percent_width, height, 0);
  renderer->draw_rect(xpos, ypos, width, height, 0);
  renderer->fill_rect(xpos - 4, ypos + height / 4, 4, height / 2, 0);
  // put the margin back
  renderer->set_margin_top(35);
}
void draw_lightning(Renderer *renderer, int x, int y, int size) {
    const float tilt_factor = 0.3f;
    int tri1_A_x = x + 1;
    int tri1_A_y = y + 1;
    int tri1_B_x = tri1_A_x - size/4;
    int tri1_B_y = tri1_A_y + (int)(size/4 * tilt_factor);
    int tri1_C_x = tri1_A_x + (int)(size/2 * tilt_factor); 
    int tri1_C_y = tri1_A_y - size/2;
    renderer->fill_triangle(tri1_A_x, tri1_A_y, tri1_B_x, tri1_B_y, tri1_C_x, tri1_C_y, 0);

    int tri2_D_x = x;
    int tri2_D_y = y;
    int tri2_E_x = tri2_D_x + size/4;
    int tri2_E_y = tri2_D_y - (int)(size/4 * tilt_factor);
    int tri2_F_x = tri2_D_x - (int)(size/2 * tilt_factor);
    int tri2_F_y = tri2_D_y + size/2;
    renderer->fill_triangle(tri2_D_x, tri2_D_y, tri2_E_x, tri2_E_y, tri2_F_x, tri2_F_y, 0);
}

void draw_charge_status(Renderer *renderer, Battery *battery)
{
    const int icon_size = 30;
    int battery_width = 40;
    int margin_right = 0;
    int margin_top = 3;
    int xpos = renderer->get_page_width() - battery_width - margin_right - icon_size - 4;
    int ypos = margin_top;
    
    if (battery->is_charging()) {
        draw_lightning(renderer, xpos + icon_size/2, ypos + icon_size/2, icon_size);
    } 
}

void draw_welcome_page(Battery *battery)
{
  open_tp_lcd();
  
  // 清除渲染器内部缓冲区
  renderer->clear_screen();
  
  // 设置白色背景
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }

  const char *welcome_text = "欢                 迎";
  int text_width = renderer->get_text_width(welcome_text);
  int text_height = renderer->get_line_height();
  // 居中
  int center_x = renderer->get_page_width() / 2;

  int usable_height = renderer->get_page_height() - 35; 
  int center_y = 35 + usable_height / 2; 
  int x_pos = center_x - text_width / 2;
  int y_pos = center_y - text_height / 2;
  
  renderer->draw_text(x_pos, y_pos, welcome_text, true);
  // 显示
  renderer->flush_display();
  
  rt_thread_delay(500);
  close_tp_lcd();
}

void handleUserInteraction(Renderer *renderer, UIAction ui_action, bool needs_redraw)
{
  uint32_t start_tick = rt_tick_get();
  switch (ui_state)
  {
  case READING_EPUB:
    handleEpub(renderer, ui_action);
    break;
  case SELECTING_TABLE_CONTENTS:
    handleEpubTableContents(renderer, ui_action, needs_redraw);
    break;
  case SELECTING_EPUB:
  default:
    handleEpubList(renderer, ui_action, needs_redraw);
    break;
  }
  rt_kprintf("Renderer time=%d \r\n", rt_tick_get() - start_tick);
}



void HAL_LPAON_Sleep(void)
{
    hwp_lpsys_aon->WER |= LPSYS_AON_WER_HP2LP_REQ;
    // HAL_HPAON_CANCEL_LP_ACTIVE_REQUEST();
    HAL_LPAON_DISABLE_PAD();
    HAL_LPAON_DISABLE_AON_PAD();
#ifndef SF32LB55X
    HAL_LPAON_DISABLE_VLP();
#endif
    /* force lpsys to enter sleep */
    hwp_lpsys_aon->PMR = (3UL << LPSYS_AON_PMR_MODE_Pos) | (1 << LPSYS_AON_PMR_CPUWAIT_Pos) | (1 << LPSYS_AON_PMR_FORCE_SLEEP_Pos);
}


void main_task(void *param)
{
  // start the board up
  ulog_i("main", "Powering up the board");
  Board *board = Board::factory();
  board->power_up();
  // create the renderer for the board
  ulog_i("main", "Creating renderer");
  ::renderer = board->get_renderer();
  // bring the file system up - SPIFFS or SDCard depending on the defines in platformio.ini
  ulog_i("main", "Starting file system");
  board->start_filesystem();

  // battery details
  ulog_i("main", "Starting battery monitor");
  Battery *battery = board->get_battery();
  if (battery)
  {
    battery->setup();
  }

  // make space for the battery display
  renderer->set_margin_top(35);
  // page margins
  renderer->set_margin_left(10);
  renderer->set_margin_right(10);

  // create a message queue for UI events
  rt_mq_t ui_queue = rt_mq_create("ui_act", sizeof(UIAction), 10, 0);

  // set the controls up
  ulog_i("main", "Setting up controls");
  ButtonControls *button_controls = board->get_button_controls(ui_queue);
  ::touch_controls = board->get_touch_controls(renderer, ui_queue);

  ulog_i("main", "Controls configured");
  // work out if we were woken from deep sleep
  if (button_controls->did_wake_from_deep_sleep())
  {
    // restore the renderer state - it should have been saved when we went to sleep...
    bool hydrate_success = renderer->hydrate();
    UIAction ui_action = button_controls->get_deep_sleep_action();
    handleUserInteraction(renderer, ui_action, !hydrate_success);
  }
  else
  {
    // reset the screen
    renderer->reset();
    // make sure the UI is in the right state
    handleUserInteraction(renderer, NONE, true);
  }

  // draw the battery level before flushing the screen
  if (battery)
  {
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  touch_controls->render(renderer);
  renderer->flush_display();
  close_tp_lcd();
  // keep track of when the user last interacted and go to sleep after N seconds
  rt_tick_t last_user_interaction = rt_tick_get_millisecond();

  while (rt_tick_get_millisecond() - last_user_interaction < 60 * 1000 *100)
  {
    // 检查是否超过60秒无操作
    if (rt_tick_get_millisecond() - last_user_interaction >= 120 * 1000)
    {
      draw_welcome_page(battery);
      // 等待用户交互以退出屏保
      while (true)
      {
        UIAction ui_action = NONE;
        if (rt_mq_recv(ui_queue, &ui_action, sizeof(UIAction), RT_WAITING_FOREVER) == RT_EOK)
        {
          if (ui_action != NONE)
          {
            // 有用户交互，退出屏保，重新进入主界面
            last_user_interaction = rt_tick_get_millisecond();
            
            open_tp_lcd();
            
            bool hydrate_success = renderer->hydrate();
            if (!hydrate_success) {
              renderer->reset();
              renderer->set_margin_top(35);
              renderer->set_margin_left(10);
              renderer->set_margin_right(10);
            }
            
            handleUserInteraction(renderer, ui_action, !hydrate_success);
            
            if (battery)
            {
              draw_charge_status(renderer, battery);
              draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
            }
            touch_controls->render(renderer);
            
            //显示
            renderer->flush_display();
            close_tp_lcd();
            break;
          }
        }
      }
    }

    UIAction ui_action = NONE;
    // wait for something to happen for 60 seconds
    if (rt_mq_recv(ui_queue, &ui_action, sizeof(UIAction), rt_tick_from_millisecond(60000)) == RT_EOK)
    {
      open_tp_lcd();
      if (ui_action != NONE)
      {
        //rt_kprintf("ui_action = %d\n", ui_action);
        // something happened!
        last_user_interaction = rt_tick_get_millisecond();
        // show feedback on the touch controls
         touch_controls->renderPressedState(renderer, ui_action);
         handleUserInteraction(renderer, ui_action, false);

        // // make sure to clear the feedback on the touch controls
         touch_controls->render(renderer);
      }
    }
    // update the battery level - do this even if there is no interaction so we
    // show the battery level even if the user is idle
    if (battery)
    {
      ulog_i("main", "Battery Level %f, percent %d", battery->get_voltage(), battery->get_percentage());
      draw_charge_status(renderer, battery);
      draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
    }
    open_tp_lcd();
    renderer->flush_display();
    close_tp_lcd();
  }

  ulog_i("main", "Saving state");
  // save the state of the renderer
  renderer->dehydrate();


  // turn off the filesystem
  board->stop_filesystem();
  // get ready to go to sleep
  board->prepare_to_sleep();
  //ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
  ulog_i("main", "Entering deep sleep");
  // configure deep sleep options
  // button_controls->setup_deep_sleep();
  rt_thread_delay(rt_tick_from_millisecond(500));
  // go to sleep
  //esp_deep_sleep_start();
}

extern "C"
{
  int main()
  {
    rt_pm_request(PM_SLEEP_MODE_IDLE);
    HAL_LPAON_Sleep();
    // dump out the epub list state
    ulog_i("main", "epub list state num_epubs=%d", epub_list_state.num_epubs);
    ulog_i("main", "epub list state is_loaded=%d", epub_list_state.is_loaded);
    ulog_i("main", "epub list state selected_item=%d", epub_list_state.selected_item);

    rt_thread_delay(1000);

    ulog_i("main", "Memory before main task start %d", heap_free_size());
    main_task(NULL);
    while(1)
    {
      rt_thread_delay(1000);
      ulog_i("main","__main_lopp__");
    }
    return 0;
  }
}