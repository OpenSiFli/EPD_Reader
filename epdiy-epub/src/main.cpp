#include <rtthread.h>
#include "EpubList/Epub.h"
#include "EpubList/EpubList.h"
#include "EpubList/EpubReader.h"
#include "EpubList/EpubToc.h"
#include <RubbishHtmlParser/RubbishHtmlParser.h>
#include "boards/Board.h"
#include "boards/controls/Actions.h"
#include "boards/controls/SF32_TouchControls.h"
#include "epub_screen.h"
#include "boards/SF32PaperRenderer.h"
#include "gui_app_pm.h"
#include "bf0_pm.h"
#include "epd_driver.h"
#include "type.h"
#include "reading_settings.h"

#include "UIRegionsManager.h"

#undef LOG_TAG
#undef DBG_LEVEL
#define  DBG_LEVEL            DBG_LOG //DBG_INFO  //
#define LOG_TAG                "EPUB.main"
#define TIMEOUT_SHUTDOWN_TIME 5 // 默认关机超时（小时）；0 表示不关机
#include <rtdbg.h>



extern "C"
{
  int main();
  rt_uint32_t heap_free_size(void);
  extern void set_part_disp_times(int val);
  extern const uint8_t low_power_map[];
  extern const uint8_t chargeing_map[];
  extern const uint8_t welcome_map[];
  extern const uint8_t shutdown_map[];
  // FreeType 相关
  extern const unsigned char epub_ttf_data[];
  extern const int epub_ttf_data_size;
  int epd_font_ft_preheat_is_running(void);
  void epd_font_ft_preheat_stop(void);
  int font_manager_init(const char *font_dir);
  int font_manager_get_count(void);
  const char *font_manager_get_name(int index);
}

const char *TAG = "main";



// 默认显示新主页面，而非书库页面
AppUIState ui_state = MAIN_PAGE;
// the state data for the epub list and reader
EpubListState epub_list_state;
// the state data for the epub index list
EpubTocState epub_index_state;

// 最近一次真实打开并阅读的书本索引（-1 表示无记录）
int g_last_read_index = -1;

// 阅读设置页面锚点
static int g_anchor_block = 0;
static int g_anchor_line = 0;
static bool g_has_anchor = false;
static AppUIState g_state_before_settings = MAIN_PAGE;

void handleEpub(Renderer *renderer, UIAction action);
void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw);
void back_to_main_page();

static EpubList *epub_list = nullptr;
EpubReader *reader = nullptr;
static EpubToc *contents = nullptr;
static bool charge_full = false;
Battery *battery = nullptr;
// 给open_tp_lcd和close_tp_lcd用的
Renderer *renderer = nullptr;
TouchControls *touch_controls = nullptr;

// 书库页底部按钮选择状态
bool library_bottom_mode = false; // 是否处于底部三按钮选择模式
int library_bottom_idx = 1;      // 当前底部按钮索引：0上一页,1主页面,2下一页
int book_index;//用于记录电子书触控选择
int current_page;  // 当前页面
int start_index;                   // 当前页起始索引
// 计算全局索引 = 页起始索引 + 页内偏移
int global_index;
bool toc_bottom_mode = false;
int toc_index;//用于记录目录触控选择
int toc_bottom_idx = 1; // 0:上一页,1:主页面,2:下一页
int sel;
int touch_sel;
rt_mq_t ui_queue = RT_NULL;

// 主页面选项
typedef enum {
  OPTION_OPEN_LIBRARY = 0,   // 打开书库 -> 打印 1
  OPTION_CONTINUE_READING,   // 继续阅读 -> 打印 2
  OPTION_ENTER_SETTINGS      // 进入设置 -> 打印 3
} MainOption;
void handleEpubTableContents(Renderer *renderer, UIAction action, bool needs_redraw);


void handleEpub(Renderer *renderer, UIAction action)
{
    if (!reader)
    {
        reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
        reader->load();
        // 记录最近一次进入阅读的书籍索引
        g_last_read_index = epub_list_state.selected_item;
    }
    
    switch (action)
    {
    case UP:
        if (reader->is_overlay_active())
        {
            reader->overlay_move_left();
        }
        else
        {
            reader->prev();
        }
        break;
    case DOWN:
        if (reader->is_overlay_active())
        {
            reader->overlay_move_right();
        }
        else
        {
            reader->next();
        }
        break;
    case SELECT:
        if (reader->is_overlay_active())
        {
            int sel = reader->get_overlay_selected();
            // 1/3：改变中心属性；2：执行当前属性（触控取反 / 全刷周期循环）
            if(touch_sel >=0 && touch_sel <=11)
            {
              sel = -1;
            }
            if (sel == 0 || touch_sel == 0)
            {
                if(reader->overlay_is_center_touch())
                {
                    reader->overlay_set_center_mode_full_refresh();
                }
                else
                {
                    reader->overlay_set_center_mode_touch();
                }
            }
            else if (sel == 2 || touch_sel == 2)
            {
                if(reader->overlay_is_center_touch())
                {
                    reader->overlay_set_center_mode_full_refresh();
                }
                else
                {
                    reader->overlay_set_center_mode_touch();
                }
            }
            else if (sel == 1 || touch_sel == 1)
            {
                // 中心矩形：根据当前属性执行
                if (reader->overlay_is_center_touch())
                {
                    bool cur = touch_controls ? touch_controls->isTouchEnabled() : false;
                    if (touch_controls)
                    {
                        touch_controls->setTouchEnable(!cur);
                        if (!cur) touch_controls->powerOnTouch(); else touch_controls->powerOffTouch();
                    }
                    reader->overlay_set_touch_enabled(!cur);
                }
                else
                {
                    reader->overlay_cycle_full_refresh();  //设置全刷周期，在 5/10/20/每次(0) 之间循环
                    set_part_disp_times(reader->overlay_get_full_refresh_value());
                }
            }
            if (sel == 9 || touch_sel == 9) //目录
            {
                ui_state = SELECTING_TABLE_CONTENTS;
                renderer->set_margin_bottom(0);
                reader->stop_overlay();
                delete reader;
                reader = nullptr;
                contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item], epub_index_state, renderer);
                contents->load();
                contents->set_needs_redraw();
                handleEpubTableContents(renderer, NONE, true);
                touch_sel = -1;
                return;
            }
            else if (sel == 8 || touch_sel == 8) //确认：1.按第六格累积值跳页
            {
                // 跳转到第六格显示的目标页
                int target = reader->overlay_get_target_page();
                if (target < 1) target = 1;
                extern EpubListState epub_list_state;
                epub_list_state.epub_list[epub_list_state.selected_item].current_page = (uint16_t)(target - 1);
                reader->overlay_reset_jump();
                reader->stop_overlay();
            }
            else if (sel == 10 || touch_sel == 10) //书库
            {
                ui_state = SELECTING_EPUB;
                renderer->set_margin_bottom(0);
                reader->stop_overlay();
                renderer->clear_screen();
                delete reader;
                reader = nullptr;
                if (!epub_list)
                {
                    epub_list = new EpubList(renderer, epub_list_state);
                }
                handleEpubList(renderer, NONE, true);
                touch_sel = -1;
                return;
            }
            else if (sel == 11 || touch_sel == 11) //设置（阅读设置）
            {
                // 保存锚点
                reader->save_anchor(g_anchor_block, g_anchor_line);
                g_has_anchor = true;
                g_state_before_settings = READING_EPUB;
                renderer->set_margin_bottom(0);
                reader->stop_overlay();
                ui_state = READING_SETTINGS;
                reading_settings_draw(renderer);
                touch_sel = -1;
                return;
            }
            else if (sel == 3 || touch_sel == 3)
            {
                reader->overlay_adjust_target_page(-5);
            }
            else if (sel == 4 || touch_sel == 4) 
            {
                reader->overlay_adjust_target_page(-1);
            }
            else if (sel == 6 || touch_sel == 6)
            {
                reader->overlay_adjust_target_page(1);
            }
            else if (sel == 7 || touch_sel == 7) 
            {
                reader->overlay_adjust_target_page(5);
            }
            touch_sel = -1;
        }
        else
        {
            // switch back to main screen
            renderer->clear_screen();
            // clear the epub reader away
            delete reader;
            reader = nullptr;
            // force a redraw
            if (!epub_list)
            {
                epub_list = new EpubList(renderer, epub_list_state);
            }
            renderer->set_margin_bottom(0);
            back_to_main_page();

            return;
        }
        break;
    case UPGLIDE:
        // 激活阅读页下半屏覆盖操作层
        // 防止重复激活
        if (!reader->is_overlay_active()) {
            reader->start_overlay();
            // 默认中心属性为触控开关，初始同步当前触控状态
            reader->overlay_set_center_mode_touch();
            if (touch_controls)
                reader->overlay_set_touch_enabled(touch_controls->isTouchEnabled());
        }
        break;
    case NONE:
    default:
        break;
    }
    reader->render();
}
//目录页的处理
void handleEpubTableContents(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (!contents)
  {
    contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item], epub_index_state, renderer);
    contents->set_needs_redraw();
    contents->load();
  }
  
  if (needs_redraw)
  {
    toc_bottom_mode = false;
    toc_bottom_idx = 1;
  }
  switch (action)
  {
  case UP:
    if (toc_bottom_mode)
    {
      if (toc_bottom_idx > 0)
      {
        toc_bottom_idx--;
      }
      else
      {
        int per_page = 6;
        int start_idx = (epub_index_state.selected_item / per_page) * per_page;
        int end_idx = start_idx + per_page - 1;
        int count = contents->get_items_count();
        if (end_idx >= count) end_idx = count - 1;
        toc_bottom_mode = false;
        epub_index_state.selected_item = end_idx;
      }
    }
    else
    {
      int per_page = 6; 
      int start_idx = (epub_index_state.selected_item / per_page) * per_page;
      if (contents->get_items_count() > 0 && epub_index_state.selected_item == start_idx)
      {
        toc_bottom_mode = true;
        toc_bottom_idx = 2; // 下一页
      }
      else
      {
        contents->prev();
      }
    }
    break;
  case DOWN:
    if (toc_bottom_mode)
    {
      if (toc_bottom_idx < 2)
      {
        toc_bottom_idx++;
      }
      else
      {
        int per_page = 6;
        int start_idx = (epub_index_state.selected_item / per_page) * per_page;
        toc_bottom_mode = false;
        epub_index_state.selected_item = start_idx;
      }
    }
    else
    {
      int per_page = 6;
      int start_idx = (epub_index_state.selected_item / per_page) * per_page;
      int end_idx = start_idx + per_page - 1;
      int count = contents->get_items_count();
      if (end_idx >= count) end_idx = count - 1;
      if (count > 0 && epub_index_state.selected_item == end_idx)
      {
        toc_bottom_mode = true;
        toc_bottom_idx = 0;
      }
      else
      {
        contents->next();
      }
    }
    break;
  case SELECT_BOX:
    if (toc_bottom_mode) 
    {  
        toc_bottom_mode = false;
    }
    current_page = epub_index_state.selected_item / 6;
    start_index = current_page * 6;
    global_index = start_index + toc_index;
    if (global_index < contents->get_items_count() && contents->get_items_count() > 0) 
    {
        epub_index_state.selected_item = global_index;
        switch(toc_index)
        {
            case 0: contents->switch_book(global_index); break;
            case 1: contents->switch_book(global_index); break;
            case 2: contents->switch_book(global_index); break;
            case 3: contents->switch_book(global_index); break;
            case 4: contents->switch_book(global_index); break;
            case 5: contents->switch_book(global_index); break;
            default: break;
        }
    }
    break;
  case SELECT:
    if (toc_bottom_mode)
    {
      int per_page = 6;
      int count = contents->get_items_count();
      int current_page = (count > 0) ? (epub_index_state.selected_item / per_page) : 0;
      int max_page = (count == 0) ? 0 : ((count - 1) / per_page);
      if (toc_bottom_idx == 1)
      {
        rt_kprintf("从目录页返回书库页\n");
        ui_state = SELECTING_EPUB;
        if (contents)
        {
          delete contents;
          contents = nullptr;
        }
        handleEpubList(renderer, NONE, true);
        return;
      }
      else if (toc_bottom_idx == 0)
      {
        if (current_page > 0)
        {
          int new_page = current_page - 1;
          epub_index_state.selected_item = new_page * per_page;
          if (epub_index_state.selected_item < 0) 
              epub_index_state.selected_item = 0;
          toc_bottom_mode = false;
          contents->set_needs_redraw();
        }
      }
      else if (toc_bottom_idx == 2)
      {
        if (current_page < max_page)
        {
          int new_page = current_page + 1;
          epub_index_state.selected_item = new_page * per_page;
          if (epub_index_state.selected_item >= count)
              epub_index_state.selected_item = ((count - 1) / per_page) * per_page;
          toc_bottom_mode = false;
          contents->set_needs_redraw();
        }
      }
    }
    else
    {
      ui_state = READING_EPUB;
      reader = new EpubReader(epub_list_state.epub_list[epub_list_state.selected_item], renderer);
      reader->set_state_section(contents->get_selected_toc());
      reader->load();
      g_last_read_index = epub_list_state.selected_item;
      delete contents;
      handleEpub(renderer, NONE);
      return;
    }
    break;
  case NONE:
  default:
    break;
  }
  contents->set_bottom_selection(toc_bottom_mode, toc_bottom_idx);
  contents->render();
}

//书库页的处理
void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (!epub_list)
  {
    ulog_i("main", "Creating epub list");
    epub_list = new EpubList(renderer, epub_list_state);
    epub_list->setTouchControls(touch_controls);
    if (epub_list->load("/"))
    {
      ulog_i("main", "Epub files loaded");
    }
  }
  if (needs_redraw)
  {
    epub_list->set_needs_redraw();
    library_bottom_mode = false;
    library_bottom_idx = 1;
  }
  switch (action)
  {
  case UP:
    if (library_bottom_mode)
    {
      if (library_bottom_idx > 0)
      {
        library_bottom_idx--;
      }
      else
      {
        int per_page = 4;
        int start_idx = (epub_list_state.selected_item / per_page) * per_page;
        int end_idx = start_idx + per_page - 1;
        if (end_idx >= epub_list_state.num_epubs) end_idx = epub_list_state.num_epubs - 1;
        library_bottom_mode = false;
        epub_list_state.selected_item = end_idx;
      }
    }
    else
    {
      int per_page = 4; 
      int start_idx = (epub_list_state.selected_item / per_page) * per_page;
      if (epub_list_state.num_epubs > 0 && epub_list_state.selected_item == start_idx)
      {
        library_bottom_mode = true;
        library_bottom_idx = 2;
      }
      else
      {
        epub_list->prev();
      }
    }
    break;
  case DOWN:
    if (library_bottom_mode)
    {
      if (library_bottom_idx < 2)
      {
        library_bottom_idx++;
      }
      else
      {
        int per_page = 4; 
        int start_idx = (epub_list_state.selected_item / per_page) * per_page;
        int end_idx = start_idx + per_page - 1;
        if (end_idx >= epub_list_state.num_epubs) end_idx = epub_list_state.num_epubs - 1;
        library_bottom_mode = false;
        epub_list_state.selected_item = start_idx;
      }
    }
    else
    {
      int per_page = 4; 
      int start_idx = (epub_list_state.selected_item / per_page) * per_page;
      int end_idx = start_idx + per_page - 1;
      if (end_idx >= epub_list_state.num_epubs) end_idx = epub_list_state.num_epubs - 1;
      if (epub_list_state.num_epubs > 0 && epub_list_state.selected_item == end_idx)
      {
        library_bottom_mode = true;
        library_bottom_idx = 0;
      }
      else
      {
        epub_list->next();
      }
    }
    break;
  case SELECT_BOX:
    if (library_bottom_mode) {
        library_bottom_mode = false;
    }
    current_page = epub_list_state.selected_item / 4;
    start_index = current_page * 4;
    global_index = start_index + book_index;
    if (global_index < epub_list_state.num_epubs) 
    {
        if(book_index == 0) epub_list->switch_book(global_index);
        else if(book_index == 1) epub_list->switch_book(global_index);
        else if(book_index == 2) epub_list->switch_book(global_index);
        else if(book_index == 3) epub_list->switch_book(global_index);
    }
    break;
  case SELECT:
    if (library_bottom_mode)
    {
        int per_page = 4;
        int current_page = epub_list_state.selected_item / per_page;
        int max_page = (epub_list_state.num_epubs == 0) ? 0 : ( (epub_list_state.num_epubs - 1) / per_page );
        if (library_bottom_idx == 1)
        {
            rt_kprintf("从书库页返回主页面\n");
            back_to_main_page();
            return;
        }
        else if (library_bottom_idx == 0)
        {
            if (current_page > 0)
            {
                int new_page = current_page - 1;
                epub_list_state.selected_item = new_page * per_page;
                if (epub_list_state.selected_item < 0) 
                    epub_list_state.selected_item = 0;
                library_bottom_mode = false;
                epub_list->set_needs_redraw();
            }
        }
        else if (library_bottom_idx == 2)
        {
            if (current_page < max_page)
            {
                int new_page = current_page + 1;
                epub_list_state.selected_item = new_page * per_page;
                if (epub_list_state.selected_item >= epub_list_state.num_epubs)
                    epub_list_state.selected_item = ((epub_list_state.num_epubs - 1) / per_page) * per_page;
                library_bottom_mode = false;
                epub_list->set_needs_redraw();
            }
        }
    }
    else
    {
        ui_state = SELECTING_TABLE_CONTENTS;
        contents = new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item], epub_index_state, renderer);
        contents->load();
        contents->set_needs_redraw();
        handleEpubTableContents(renderer, NONE, true);
        return;
    }
    break;
  case NONE:
  default:
    break;
  }
  epub_list->set_bottom_selection(library_bottom_mode, library_bottom_idx);
  epub_list->render();
}

void draw_battery_level(Renderer *renderer, float voltage, float percentage)
{
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
  renderer->set_margin_top(35);
}

void clear_charge_icon(Renderer *renderer)
{
    const int icon_size = 30;
    int battery_width = 40;
    int margin_right = 0;
    int margin_top = 0;
    int xpos = renderer->get_page_width() - battery_width - margin_right - icon_size - 4;
    int ypos = margin_top;
    renderer->fill_rect(xpos, ypos - 30, icon_size, icon_size, 255);
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
    
    if (battery->is_charging()) 
    {
      draw_lightning(renderer, xpos + icon_size/2, ypos + icon_size/2, icon_size);
    } 
    else 
    {
      clear_charge_icon(renderer);
    }
}

void handleUserInteraction(Renderer *renderer, UIAction ui_action, bool needs_redraw)
{
    if (battery && battery->get_low_power_state() == 1) 
    {
        rt_kprintf("low power state\n");
        return;
    }
    
    uint32_t start_tick = rt_tick_get();
    switch (ui_state)
    {
    case MAIN_PAGE:
      handleMainPage(renderer, ui_action, needs_redraw);
      if (ui_action == SELECT && screen_get_main_selected_option() == 2)
      {
        ui_state = SETTINGS_PAGE;
        int r = handleSettingsPage(renderer, NONE, true);
        if (r == 2) {
          g_state_before_settings = SETTINGS_PAGE;
          ui_state = READING_SETTINGS;
          reading_settings_draw(renderer);
        }
      }
      else if (ui_action == SELECT && screen_get_main_selected_option() == 1)
      {
        if (!(g_last_read_index >= 0 && g_last_read_index < epub_list_state.num_epubs)) {
          return;
        }
        if (reader) { delete reader; reader = nullptr; }
        int last_idx = g_last_read_index;
        EpubListItem &last_item = epub_list_state.epub_list[last_idx];
        reader = new EpubReader(last_item, renderer);
        reader->set_state_section(last_item.current_section);
        reader->load();
        ui_state = READING_EPUB;
        handleEpub(renderer, NONE);
      }
      else if (ui_action == SELECT && screen_get_main_selected_option() == 0)
      {
        ui_state = SELECTING_EPUB;
        handleEpubList(renderer, NONE, true);      
      }
      break;
    case READING_EPUB:
        handleEpub(renderer, ui_action);
        break;
    case READING_SETTINGS:
    {
        bool still_in_settings = reading_settings_handle_action(renderer, ui_action);
        if (!still_in_settings) {
            if (g_state_before_settings == READING_EPUB) {
                // 从阅读页的覆盖层进入的 → 回到阅读页
                ui_state = READING_EPUB;
                delete reader;
                reader = nullptr;
                handleEpub(renderer, NONE);
                // 用锚点恢复阅读位置
                if (g_has_anchor && reader) {
                    reader->restore_by_anchor(g_anchor_block, g_anchor_line);
                    reader->render();
                    g_has_anchor = false;
                }
            } else {
                // 从主界面设置页进入的 → 回到设置页
                ui_state = SETTINGS_PAGE;
                (void)handleSettingsPage(renderer, NONE, true);
            }
        }
        return;
    }
    case SELECTING_TABLE_CONTENTS:
        handleEpubTableContents(renderer, ui_action, needs_redraw);
        break;
    case SETTINGS_PAGE:
    {
      int settings_result = handleSettingsPage(renderer, ui_action, needs_redraw);
      if (settings_result == 1)
      {
        // 回主页
        ui_state = MAIN_PAGE;
        handleMainPage(renderer, NONE, true);
      }
      else if (settings_result == 2)
      {
        // 进入阅读设置页面
        g_state_before_settings = SETTINGS_PAGE;
        ui_state = READING_SETTINGS;
        reading_settings_draw(renderer);
      }
      break;
    }
    case SELECTING_EPUB:
    default:
        handleEpubList(renderer, ui_action, needs_redraw);
        break;
    }
    rt_kprintf("Renderer time=%d \r\n", rt_tick_get() - start_tick);
}

const char* getCurrentPageName() {
  switch (ui_state) 
  {
    case MAIN_PAGE:     return "MAIN_PAGE";
    case SELECTING_EPUB: return "SELECTING_EPUB";
    case SELECTING_TABLE_CONTENTS: return "SELECTING_TABLE_CONTENTS";
    case READING_EPUB:  return "READING_EPUB";
    case READING_SETTINGS: return "READING_SETTINGS";
    case SETTINGS_PAGE: return "SETTINGS_PAGE";
    case WELCOME_PAGE:  return "WELCOME_PAGE";
    case LOW_POWER_PAGE: return "LOW_POWER_PAGE";
    case CHARGING_PAGE: return "CHARGING_PAGE";
    case SHUTDOWN_PAGE: return "SHUTDOWN_PAGE";
    default:            return "UNKNOWN_PAGE";
  }
}

void back_to_main_page()
{
  if (ui_state == MAIN_PAGE) 
  {
    rt_kprintf("已经在主页面，无需返回\n");
    return;
  }
  if (ui_state == SELECTING_TABLE_CONTENTS) 
  {
    if (contents) 
    {
      delete contents;
      contents = nullptr;
    }
  }
  bool hydrate_success = renderer->hydrate();
  renderer->reset();
  renderer->set_margin_top(35);
  renderer->set_margin_left(10);
  renderer->set_margin_right(10);
  ui_state = MAIN_PAGE;
  handleUserInteraction(renderer, NONE, true);

  if (battery)
  {
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  touch_controls->render(renderer);
  renderer->flush_display();
}

void draw_welcome_page(Battery *battery)
{
  if (ui_state == WELCOME_PAGE) return;
  ui_state = WELCOME_PAGE;
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 649, img_height = 150;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer* fb_renderer = static_cast<EpdiyFrameBufferRenderer*>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, welcome_map);
  renderer->flush_display();
}

void draw_low_power_page(Battery *battery)
{
  if (ui_state == LOW_POWER_PAGE) return;
  ui_state = LOW_POWER_PAGE;
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 200, img_height = 200;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer* fb_renderer = static_cast<EpdiyFrameBufferRenderer*>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, low_power_map);
  renderer->flush_display();
}

void draw_charge_page(Battery *battery)
{
  if (ui_state == CHARGING_PAGE) return;
  ui_state = CHARGING_PAGE;
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 200, img_height = 200;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer* fb_renderer = static_cast<EpdiyFrameBufferRenderer*>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, chargeing_map);
  renderer->flush_display();
}

void draw_shutdown_page()
{
    renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);
    if (battery) 
    {
        renderer->set_margin_top(35);
        draw_charge_status(renderer, battery);
        draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
    }
    const int img_width = 200, img_height = 200;
    int center_x = renderer->get_page_width() / 2;
    int center_y = 35 + (renderer->get_page_height() - 35) / 2;
    int x_pos = center_x - img_width / 2;
    int y_pos = center_y - img_height / 2;
    EpdiyFrameBufferRenderer* fb_renderer = static_cast<EpdiyFrameBufferRenderer*>(renderer);
    fb_renderer->show_img(x_pos, y_pos, img_width, img_height, shutdown_map);
    const char *shutdown_text = "请长按 Key1 开机";
    int text_width = renderer->get_text_width(shutdown_text);
    int text_x = center_x - text_width / 2;
    int text_y = y_pos + img_height + 10;
    renderer->draw_text(text_x, text_y, shutdown_text, false, true);
    renderer->flush_display();
}

void main_task(void *param)
{
  ulog_i("main", "Powering up the board");
  Board *board = Board::factory();
  board->power_up();

  rt_kprintf("TTF data at %p, size=%d\n", epub_ttf_data, epub_ttf_data_size);
  rt_kprintf("PSRAM free before font init: %d\n", heap_free_size());

  ulog_i("main", "Creating renderer");
  ::renderer = board->get_renderer();
  ulog_i("main", "Starting file system");
  board->start_filesystem();

  // FreeType 字体管理器初始化
  int font_count = font_manager_init("/fonts");
  for (int i = 0; i < font_count; i++) {
    rt_kprintf("Font [%d]: %s\n", i, font_manager_get_name(i));
  }
  reading_settings_load(renderer);

  rt_kprintf("PSRAM free after font init: %d\n", heap_free_size());

  ui_queue = rt_mq_create("ui_act", sizeof(UIAction), 10, 0);
  ulog_i("main", "Starting battery monitor");
  battery = board->get_battery(ui_queue);
  if (battery)
  {
    battery->setup();
  }
  renderer->set_margin_top(35);
  renderer->set_margin_left(10);
  renderer->set_margin_right(10);

  ulog_i("main", "Setting up controls");
  ButtonControls *button_controls = board->get_button_controls(ui_queue);
  ::touch_controls = board->get_touch_controls(renderer, ui_queue);

  ulog_i("main", "Controls configured");
  if (button_controls->did_wake_from_deep_sleep())
  {
    bool hydrate_success = renderer->hydrate();
    UIAction ui_action = button_controls->get_deep_sleep_action();
    handleUserInteraction(renderer, ui_action, !hydrate_success);
  }
  else
  {
    renderer->reset();
    ui_state = MAIN_PAGE;
    handleUserInteraction(renderer, NONE, true);
  }

  if (battery)
  {
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  touch_controls->render(renderer);
  renderer->flush_display();
  if(!touch_controls->isTouchEnabled())
  {
    touch_controls->powerOffTouch();
  }
  board->sleep_filesystem();
  rt_tick_t last_user_interaction = rt_tick_get_millisecond();
  int last_battery_percent = battery ? battery->get_percentage() : -1;
  bool last_battery_charging = battery ? battery->is_charging() : false;

  screen_init(TIMEOUT_SHUTDOWN_TIME);

  while ((rt_tick_get_millisecond() - last_user_interaction < 60 * 1000 * 60 * TIMEOUT_SHUTDOWN_TIME))
  {
    if (rt_tick_get_millisecond() - last_user_interaction >= 60 * 1000 * screen_get_timeout_shutdown_minutes() && 
      battery && battery->get_low_power_state() != 1 && 
      ui_state != WELCOME_PAGE && 
      ui_state != CHARGING_PAGE && 
      ui_state != LOW_POWER_PAGE &&
      screen_get_timeout_shutdown_minutes())
    {
      renderer->set_margin_bottom(0);
      draw_welcome_page(battery);      
    }
    uint32_t msg_data;
    if (rt_mq_recv(ui_queue, &msg_data, sizeof(uint32_t), rt_tick_from_millisecond(500)) == RT_EOK)
    {
      UIAction ui_action = (UIAction)msg_data;
    
      if (ui_action == MSG_UPDATE_CHARGE_STATUS)
      {       
        if (battery)
        {
            int percentage = battery->get_percentage();
            if (percentage >= 98 && charge_full == false) 
            {
                clear_charge_icon(renderer);
                renderer->flush_display();
                charge_full = true;
                rt_kprintf("Battery level is full, skip sending charge status update message\n");
            }
            else if(percentage < 98)
            {
                rt_kprintf("Charge status changed\n");
                charge_full = false;
                draw_charge_status(renderer, battery);
                draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
                renderer->flush_display();
            }        
        }
        continue;
      }
      if (ui_action == MSG_DRAW_LOW_POWER_PAGE || ui_action == MSG_DRAW_CHARGE_PAGE || ui_action == MSG_DRAW_WELCOME_PAGE)
      {
        rt_kprintf("battery msg: %d\n", ui_action);
        switch (ui_action)
        {
            case MSG_DRAW_LOW_POWER_PAGE:
                rt_kprintf("low_power\n");
                renderer->set_margin_bottom(0);
                draw_low_power_page(battery);
                break;
            case MSG_DRAW_CHARGE_PAGE:
                rt_kprintf("charge_power\n");
                draw_charge_page(battery);
                break;
            case MSG_DRAW_WELCOME_PAGE:
                rt_kprintf("power ok , welcome\n");
                draw_welcome_page(battery);
                break;
            default:
                break;
        }
      }
      else
      {
        rt_kprintf("no battery msg: %d\n", msg_data);    
        board->wakeup_filesystem();
        if (ui_action != NONE)
        {
            if(ui_state == WELCOME_PAGE)
            {
              back_to_main_page();
              last_user_interaction = rt_tick_get_millisecond();
              epd_font_ft_preheat_stop();
              board->sleep_filesystem();
              continue;
            }
            last_user_interaction = rt_tick_get_millisecond();
            touch_controls->renderPressedState(renderer, ui_action);
            AppUIState state_before = ui_state;
            handleUserInteraction(renderer, ui_action, false);

            // 用户操作后停止预热线程（可能切换了章节/字体）
            epd_font_ft_preheat_stop();
            // SD 卡电源保护：
            // 如果操作前后都停留在不需要 SD 卡的页面，跳过 sleep 以避免下次 wakeup 卡顿
            bool stayed_in_no_fs_page = 
                (state_before == MAIN_PAGE || state_before == SETTINGS_PAGE || state_before == READING_SETTINGS) &&
                (ui_state == MAIN_PAGE || ui_state == SETTINGS_PAGE || ui_state == READING_SETTINGS);
            if (!stayed_in_no_fs_page &&
                !epd_font_ft_preheat_is_running() &&
                !(ui_state == READING_EPUB && reader != nullptr && reader->has_pending_layout())) {
              board->sleep_filesystem();
            }
        }
      }         
      touch_controls->render(renderer);
      // 用户操作或电池UI消息后，立即刷屏
      if (battery) {
        draw_charge_status(renderer, battery);
        draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
        last_battery_percent = battery->get_percentage();
        last_battery_charging = battery->is_charging();
      }
      renderer->flush_display();

    } else {
      // ============================================================
      // rt_mq_recv 超时（500ms 无按键）— SD 卡电源管理
      // ============================================================
      if (ui_state == READING_EPUB && reader != nullptr
          && reader->has_pending_layout())
      {
        // 后台排版线程还在跑，SD 卡需要保持唤醒
        board->wakeup_filesystem();
      } else if (epd_font_ft_preheat_is_running()) {
        // 字体预热线程还在跑，SD 卡保持唤醒
      } else {
        // 都空闲，可以 sleep SD 卡
        board->sleep_filesystem();
      }
    }

    // 电池状态 + 刷屏（仅在电量百分比或充电状态变化时才刷新）
    if (battery) {
      int cur_percent = battery->get_percentage();
      bool cur_charging = battery->is_charging();
      if (cur_percent != last_battery_percent || cur_charging != last_battery_charging) {
        ulog_i("main", "Battery changed: %d%%->%d%%, charging=%d->%d",
               last_battery_percent, cur_percent,
               last_battery_charging, cur_charging);
        draw_charge_status(renderer, battery);
        draw_battery_level(renderer, battery->get_voltage(), cur_percent);
        renderer->flush_display();
        last_battery_percent = cur_percent;
        last_battery_charging = cur_charging;
      }
    }
  }

  ulog_i("main", "Saving state");
  renderer->dehydrate();
  board->stop_filesystem();
  renderer->set_margin_bottom(0);
  draw_shutdown_page();
  board->prepare_to_sleep();
  ulog_i("main", "Entering deep sleep");
  rt_thread_delay(rt_tick_from_millisecond(500));
}

extern "C"
{
  int main()
  {
    rt_pm_request(PM_SLEEP_MODE_IDLE); 
    ulog_i("main", "epub list state num_epubs=%d", epub_list_state.num_epubs);
    ulog_i("main", "epub list state is_loaded=%d", epub_list_state.is_loaded);
    ulog_i("main", "epub list state selected_item=%d", epub_list_state.selected_item);
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