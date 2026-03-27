#include "EpubList/EpubList.h"
#include "epub_screen.h"
#include <string.h>
#include "type.h"

#include "UIRegionsManager.h"


extern TouchControls *touch_controls;
extern "C" 
{
  extern void set_part_disp_times(int val);
}

// 最近一次真实打开并阅读的书本索引（由 main.cpp 维护）
extern int g_last_read_index;

// 主页面选项
typedef enum 
{
  OPTION_OPEN_LIBRARY = 0,  // 打开书库
  OPTION_CONTINUE_READING,  // 继续阅读
  OPTION_ENTER_SETTINGS     // 进入设置
} MainOption;

static MainOption main_option = OPTION_OPEN_LIBRARY; // 默认"打开书库"
// 全刷周期选项：5、10、20、每次(0)
static const int kFullRefreshOptions[] = {5, 10, 20, 0};
static const int kFullRefreshOptionsCount = sizeof(kFullRefreshOptions) / sizeof(kFullRefreshOptions[0]);
static int full_refresh_idx = 1; // 默认10次

// 获取当前全刷周期值
int screen_get_full_refresh_period() 
{
  return kFullRefreshOptions[full_refresh_idx];
}

// 切换全刷周期（循环）
void screen_cycle_full_refresh_period(bool refresh) 
{
  if(refresh)
  {
    full_refresh_idx = (full_refresh_idx + 1) % kFullRefreshOptionsCount;
  }
  else
  {
    full_refresh_idx = (full_refresh_idx - 1 + kFullRefreshOptionsCount) % kFullRefreshOptionsCount;
  }
}

// 设置全刷周期索引
void screen_set_full_refresh_idx(int idx) 
{
  if (idx >= 0 && idx < kFullRefreshOptionsCount) full_refresh_idx = idx;
}

// 获取当前全刷周期索引
int screen_get_full_refresh_idx() 
{
  return full_refresh_idx;
}


int settings_selected_idx = 0;

// 超时关机：5/10/30分钟、1小时、不关机(0)
static const int kTimeoutOptions[] = {5, 10, 30, 60, 0}; // 单位：分钟，0为不关机
static const int kTimeoutOptionsCount = sizeof(kTimeoutOptions) / sizeof(kTimeoutOptions[0]);
static int timeout_shutdown_minutes = 30; // 默认30分钟
static int timeout_idx = -1; // 

static int find_timeout_idx(int minutes)
{
  for (int i = 0; i < kTimeoutOptionsCount; ++i)
  {
    if (kTimeoutOptions[i] == minutes) return i;
  }
  return 2; // 默认索引：30分钟
}

static void adjust_timeout(bool increase)
{
  if (timeout_idx < 0) timeout_idx = find_timeout_idx(timeout_shutdown_minutes);
  if (increase)
  {
    timeout_idx = (timeout_idx + 1) % kTimeoutOptionsCount;
  }
  else
  {
    timeout_idx = (timeout_idx - 1 + kTimeoutOptionsCount) % kTimeoutOptionsCount;
  }
  timeout_shutdown_minutes = kTimeoutOptions[timeout_idx];
}

void screen_init(int default_timeout_minutes)
{
  timeout_shutdown_minutes = default_timeout_minutes;
  timeout_idx = find_timeout_idx(timeout_shutdown_minutes);
}

int screen_get_timeout_shutdown_minutes()
{
  if (timeout_idx < 0) timeout_idx = find_timeout_idx(timeout_shutdown_minutes);
  return timeout_shutdown_minutes;
}

int screen_get_main_selected_option()
{
  return (int)main_option; // 0: 打开书库, 1: 继续阅读, 2: 进入设置
}

// 绘制主页面
static void render_main_page(Renderer *renderer)
{

  clear_areas(); // 清除之前的区域记录

  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);

  const char *title = "S I F L I";
  int title_w = renderer->get_text_width(title);
  int title_h = renderer->get_line_height();
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  renderer->draw_text(center_x - title_w / 2, center_y - title_h / 2, title, true, true);

  int margin_side = 10;
  int margin_bottom = 60; // 与底部距离
  int rect_w = 80;
  int rect_h = 40;
  int y = renderer->get_page_height() - rect_h - margin_bottom;
  int left_x = margin_side;
  int right_x = renderer->get_page_width() - rect_w - margin_side;

  // 左 "<"
  const char *lt = "<";
  int lt_w = renderer->get_text_width(lt);
  int lt_h = renderer->get_line_height();

  int left_arrow_x = margin_side;
  int left_arrow_y = y + margin_bottom;
  add_area(left_arrow_x, left_arrow_y, rect_w, rect_h);

  renderer->draw_text(left_x + (rect_w - lt_w) / 2, y + (rect_h - lt_h) / 2, lt, false, true);

  // 右 ">"
  const char *gt = ">";
  int gt_w = renderer->get_text_width(gt);
  int gt_h = renderer->get_line_height();

  int right_arrow_x = right_x;
  int right_arrow_y = y + margin_bottom;
  add_area(right_arrow_x, right_arrow_y, rect_w, rect_h);

  renderer->draw_text(right_x + (rect_w - gt_w) / 2, y + (rect_h - gt_h) / 2, gt, false, true);

  // 中间选项文本
  int mid_x = left_x + rect_w + margin_side;
  int mid_w = right_x - margin_side - mid_x;

  const char *opt_text = NULL;
  extern EpubListState epub_list_state;
  bool has_continue_reading = (epub_list_state.num_epubs > 0 && g_last_read_index >= 0 && g_last_read_index < epub_list_state.num_epubs);
  switch (main_option)
  {
    case OPTION_OPEN_LIBRARY:     opt_text = "打开书库"; break;
    case OPTION_CONTINUE_READING:
      opt_text = has_continue_reading ? "继续阅读" : "无阅读记录";
      break;
    case OPTION_ENTER_SETTINGS:   opt_text = "进入设置"; break;
  }
  int opt_w = renderer->get_text_width(opt_text);
  int opt_h = renderer->get_line_height();

  int option_x = mid_x + (mid_w - opt_w) / 2 ;
  int option_y = y + margin_bottom;
    
  add_area(option_x, option_y, opt_w, opt_h);

  renderer->draw_text(mid_x + (mid_w - opt_w) / 2, y + (rect_h - opt_h) / 2, opt_text, false, true);
}
//主界面处理
void handleMainPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (needs_redraw || action == NONE)
  {
    render_main_page(renderer);
    return;
  }
  switch (action)
  {
    case UP:
      if (main_option == OPTION_OPEN_LIBRARY) main_option = OPTION_ENTER_SETTINGS;
      else if (main_option == OPTION_CONTINUE_READING) main_option = OPTION_OPEN_LIBRARY;
      else main_option = OPTION_CONTINUE_READING;
      render_main_page(renderer);
      break;
    case DOWN:
      if (main_option == OPTION_OPEN_LIBRARY) main_option = OPTION_CONTINUE_READING;
      else if (main_option == OPTION_CONTINUE_READING) main_option = OPTION_ENTER_SETTINGS;
      else main_option = OPTION_OPEN_LIBRARY;
      render_main_page(renderer);
      break;
    case SELECT:
      switch (main_option)
      {
        case OPTION_OPEN_LIBRARY:     rt_kprintf("1\n"); break;
        case OPTION_CONTINUE_READING: rt_kprintf("2\n"); break;
        case OPTION_ENTER_SETTINGS:   rt_kprintf("3\n"); break;
      }
      break;
    default:
      break;
  }
}

// 设置页面
void render_settings_page(Renderer *renderer)
{

  clear_areas(); // 清除之前的区域记录

  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);

  // 标题
  const char *title = "设置";
  int title_w = renderer->get_text_width(title);
  int title_h = renderer->get_line_height();
  int page_w = renderer->get_page_width();
  int page_h = renderer->get_page_height();
  renderer->draw_text((page_w - title_w) / 2, 40, title, true, true);

  // 列表项布局参数
  int margin_lr = 6;
  int item_h = 100;
  int gap = 40;        // 缩小间距以容纳更多选项
  int arrow_col_w = 40;
  int y = 40 + title_h + 20;

  int item_w = page_w - margin_lr * 2 - arrow_col_w * 2;
  int item_x = margin_lr + arrow_col_w;
  int lh = renderer->get_line_height();

  // ---- 辅助 lambda：绘制一个设置项行 ----
  auto draw_setting_row = [&](int idx, const char *text, int row_y) {
    // 左右箭头（仅选中时显示）
    if (settings_selected_idx == idx)
    {
      const char *lt = "<"; int lt_w = renderer->get_text_width(lt);
      static_add_area(margin_lr, row_y, arrow_col_w, item_h, idx * 3);
      renderer->draw_text(margin_lr + (arrow_col_w - lt_w) / 2, row_y + (item_h - lh) / 2, lt, false, true);

      const char *gt = ">"; int gt_w = renderer->get_text_width(gt);
      static_add_area(page_w - arrow_col_w + margin_lr, row_y, arrow_col_w, item_h, idx * 3 + 1);
      renderer->draw_text(page_w - margin_lr - arrow_col_w + (arrow_col_w - gt_w) / 2, row_y + (item_h - lh) / 2, gt, false, true);

      for (int k = 0; k < 5; ++k) renderer->draw_rect(item_x + k, row_y + k, item_w - 2 * k, item_h - 2 * k, 0);
    }
    else
    {
      renderer->draw_rect(item_x, row_y, item_w, item_h, 0);
    }

    int t_w = renderer->get_text_width(text);
    int tx = item_x + (item_w - t_w) / 2;
    if (tx < item_x + 4) tx = item_x + 4;
    if (tx + t_w > item_x + item_w - 4) tx = item_x + item_w - t_w - 4;
    static_add_area(item_x, row_y, item_w, item_h, idx * 3 + 2);
    renderer->draw_text(tx, row_y + (item_h - lh) / 2, text, false, true);
  };

  // 1) 触控开关
  {
    bool touch_on = touch_controls ? touch_controls->isTouchEnabled() : false;
    char buf[48];
    rt_snprintf(buf, sizeof(buf), "触控开关：%s", touch_on ? "开" : "关");
    draw_setting_row(SET_TOUCH, buf, y);
    y += item_h + gap;
  }

  // 2) 超时关机
  {
    char buf[64];
    if (timeout_shutdown_minutes == 0)
      rt_snprintf(buf, sizeof(buf), "超时关机：不关机");
    else if (timeout_shutdown_minutes < 60)
      rt_snprintf(buf, sizeof(buf), "超时关机：%d分钟", timeout_shutdown_minutes);
    else
      rt_snprintf(buf, sizeof(buf), "超时关机：%d小时", timeout_shutdown_minutes / 60);
    draw_setting_row(SET_TIMEOUT, buf, y);
    y += item_h + gap;
  }

  // 3) 全刷周期
  {
    char buf[64];
    int fr_val = screen_get_full_refresh_period();
    if (fr_val == 0)
      rt_snprintf(buf, sizeof(buf), "全刷周期：每次");
    else
      rt_snprintf(buf, sizeof(buf), "全刷周期：%d 次", fr_val);
    draw_setting_row(SET_FULL_REFRESH, buf, y);
    y += item_h + gap;
  }

  // 4) 阅读设置（进入子页面）
  {
    draw_setting_row(SET_READING_SETTINGS, "阅读设置", y);
    y += item_h + gap;
  }

  // 5) 确认按钮
  {
    int confirm_h = 120;
    int confirm_w = item_w;
    int confirm_x = (page_w - confirm_w) / 2;
    int confirm_y = page_h - confirm_h - 60;
    if (settings_selected_idx == SET_CONFIRM)
    {
      for (int i = 0; i < 5; ++i) renderer->draw_rect(confirm_x + i, confirm_y + i, confirm_w - 2 * i, confirm_h - 2 * i, 0);
    }
    else
    {
      renderer->draw_rect(confirm_x, confirm_y, confirm_w, confirm_h, 0);
    }
    const char *confirm = "确认";
    int c_w = renderer->get_text_width(confirm);
    int c_h = renderer->get_line_height();
    static_add_area(confirm_x, confirm_y, confirm_w, confirm_h, SET_CONFIRM * 3 + 2);
    renderer->draw_text(confirm_x + (confirm_w - c_w) / 2, confirm_y + (confirm_h - c_h) / 2, confirm, false, true);
  }
}

// 设置页面交互处理
// 返回值：0=继续，1=回主页，2=进阅读设置
int handleSettingsPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  // 读取并清除一次性的触控箭头标记
  int touch_row = g_touch_last_settings_row;
  int touch_dir = g_touch_last_settings_dir;
  g_touch_last_settings_row = -1;
  g_touch_last_settings_dir = 0;

  if (needs_redraw || action == NONE)
  {
    render_settings_page(renderer);
    return 0;
  }

  switch (action)
  {
    case UP:
      if (settings_selected_idx == SET_TIMEOUT && touch_row == 1 && touch_dir == -1)
      {
        adjust_timeout(false);
        render_settings_page(renderer);
      }
      else
      {
        if (settings_selected_idx > 0) settings_selected_idx--; else settings_selected_idx = SET_CONFIRM;
        render_settings_page(renderer);
      }
      break;
    case DOWN:
      if (settings_selected_idx == SET_TIMEOUT && touch_row == 1 && touch_dir == +1)
      {
        adjust_timeout(true);
        render_settings_page(renderer);
      }
      else
      {
        if (settings_selected_idx < SET_CONFIRM) settings_selected_idx++; else settings_selected_idx = SET_TOUCH;
        render_settings_page(renderer);
      }
      break;
    case SELECT_BOX:
      if(settings_selected_idx == SET_READING_SETTINGS)
      {
        return 2; // 进入阅读设置
      }
      else if(settings_selected_idx == SET_CONFIRM)
      {
        render_settings_page(renderer);
        return 1; // 回主页
      }
      render_settings_page(renderer);
      break;
    case PREV_OPTION:
      if (settings_selected_idx == SET_TIMEOUT)
      {
        adjust_timeout(false);
        render_settings_page(renderer);
      }
      else if(settings_selected_idx == SET_FULL_REFRESH)
      {
        screen_cycle_full_refresh_period(false);
        set_part_disp_times(screen_get_full_refresh_period());
        render_settings_page(renderer);
      }
      break;
    case NEXT_OPTION:
      if (settings_selected_idx == SET_TIMEOUT)
      {
        adjust_timeout(true);
        render_settings_page(renderer);
      }
      else if(settings_selected_idx == SET_FULL_REFRESH)
      {
        screen_cycle_full_refresh_period(true);
        set_part_disp_times(screen_get_full_refresh_period());
        render_settings_page(renderer);
      }  
      break;
    case SELECT:
      if (settings_selected_idx == SET_TOUCH)
      {
        bool current_state = touch_controls ? touch_controls->isTouchEnabled() : false;
        if (touch_controls)
        {
          touch_controls->setTouchEnable(!current_state);
          if (!current_state) touch_controls->powerOnTouch();
          else touch_controls->powerOffTouch();
        }
        render_settings_page(renderer);
        break;
      }
      if (settings_selected_idx == SET_TIMEOUT)
      {
        adjust_timeout(true);
        render_settings_page(renderer);
        break;
      }
      if (settings_selected_idx == SET_FULL_REFRESH)
      {
        screen_cycle_full_refresh_period(true);
        set_part_disp_times(screen_get_full_refresh_period());
        render_settings_page(renderer);
        break;
      }
      if (settings_selected_idx == SET_READING_SETTINGS)
      {
        return 2; // 进入阅读设置页面
      }
      if (settings_selected_idx == SET_CONFIRM)
      {
        return 1; // 回主页
      }
      break;
    default:
      break;
  }
  return 0;
}