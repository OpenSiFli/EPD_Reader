#include "EpubList/EpubList.h"
#include "epub_screen.h"
#include <string.h>
#include "type.h"

#include "UIRegionsManager.h"


extern TouchControls *touch_controls;
extern "C" 
{
  extern void set_part_disp_times(int val);
  #include "pan.h"
  #include "wheather.h"
}

// 最近一次真实打开并阅读的书本索引（由 main.cpp 维护）
extern int g_last_read_index;

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
bool bluetooth_ui_enabled = false;
bool bluetooth_pending_toggle = false;
volatile int g_bluetooth_connected = 0;

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

MainOption screen_get_main_selected_option()
{
  return main_option;
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
    case OPTION_ENTER_SETTINGS:   opt_text = "进入设置"; break;
    case OPTION_WEATHER:          opt_text = "查看天气"; break;
    case OPTION_CONTINUE_READING:
      opt_text = has_continue_reading ? "继续阅读" : "无阅读记录";
      break;
    case OPTION_BLANK_PAGE:
      opt_text = "空白页面";
      break;
    default:
      opt_text = "打开书库";
      break;
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
      main_option = (MainOption)((main_option + OPTION_COUNT - 1) % OPTION_COUNT);
      render_main_page(renderer);
      break;
    case DOWN:
      main_option = (MainOption)((main_option + 1) % OPTION_COUNT);
      render_main_page(renderer);
      break;
    case SELECT:
      switch (main_option)
      {
        case OPTION_OPEN_LIBRARY:     rt_kprintf("1\n"); break;
        case OPTION_ENTER_SETTINGS:   rt_kprintf("2\n"); break;
        case OPTION_WEATHER:          rt_kprintf("3\n"); break;
        case OPTION_CONTINUE_READING: rt_kprintf("4\n"); break;
        default:
          break;
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

  // 4) 蓝牙开关（绑定真实蓝牙栈；打开后再进入 PAN 连接流程）
  {
    char buf[48];
    rt_snprintf(buf, sizeof(buf), "蓝牙：%s", bluetooth_ui_enabled ? "开" : "关");
    draw_setting_row(SET_BLUETOOTH, buf, y);
    y += item_h + gap;
  }

  // 5) 阅读设置（进入子页面）
  {
    draw_setting_row(SET_READING_SETTINGS, "阅读设置", y);
    y += item_h + gap;
  }

  
  // 6) 确认按钮
  {
    int confirm_h = 120;
    int confirm_w = item_w;
    int confirm_x = (page_w - confirm_w) / 2;
    int confirm_y = page_h - confirm_h - 60;
    int confirm_touch_h = confirm_h + 40;
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
    static_add_area(confirm_x, confirm_y, confirm_w, confirm_touch_h, SET_CONFIRM * 3 + 2);
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
      if (settings_selected_idx == SET_TIMEOUT && touch_row == SET_TIMEOUT && touch_dir == -1)
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
      if (settings_selected_idx == SET_TIMEOUT && touch_row == SET_TIMEOUT && touch_dir == +1)
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
      if (settings_selected_idx == SET_BLUETOOTH)
      {
        bluetooth_pending_toggle = true;

        render_settings_page(renderer);
      }
      else if (settings_selected_idx == SET_TIMEOUT)
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
      if (settings_selected_idx == SET_BLUETOOTH)
      {
        bluetooth_pending_toggle = true;
        render_settings_page(renderer);
      }
      else if (settings_selected_idx == SET_TIMEOUT)
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
      if (settings_selected_idx == SET_BLUETOOTH)
      {
        bluetooth_pending_toggle = true;
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

// Weather page implementation (merged here)
extern volatile int g_weather_last_button;

typedef struct
{
  char left_top_label[16];
  char left_top_value[32];
  char left_bottom_label[16];
  char left_bottom_value[32];
  char right_top_label[16];
  char right_top_value[32];
  char right_bottom_label[16];
  char right_bottom_value[32];
} WeatherCardData;

static WeatherCardData g_weather_cards[3] = {
  {"地点", "--", "温度", "--", "更新时间", "--:--:--", "天气", "--"},
  {"国家", "--", "路径", "--", "时区", "--", "偏移", "--"},
  {"状态", "未更新", "代码", "--", "网络", "未连接", "来源", "心知天气"},
};

typedef struct
{
  const char *label;
  const char *value;
} WeatherCityOption;

static const WeatherCityOption kWeatherCityOptions[] = {
  {"北京", "beijing"},
  {"上海", "shanghai"},
  {"南京", "nanjing"},
  {"深圳", "shenzhen"},
};
static const int kWeatherCityOptionCount = sizeof(kWeatherCityOptions) / sizeof(kWeatherCityOptions[0]);
static int g_weather_selected_button = 1;
static int g_weather_city_selected_index = 0;
static int g_weather_city_pending_index = 0;
extern volatile int g_weather_city_last_hit;

static int weather_find_city_option_index(const char *city)
{
  if (city == RT_NULL || city[0] == '\0')
    return 2;

  for (int i = 0; i < kWeatherCityOptionCount; ++i)
  {
    if (strcmp(city, kWeatherCityOptions[i].value) == 0)
      return i;
  }

  return 2;
}

static void weather_city_page_sync_selection(void)
{
  g_weather_city_pending_index = weather_find_city_option_index(weather_get_city());
  g_weather_city_selected_index = g_weather_city_pending_index;
}

static void weather_extract_time(const char *last_update, char *time_text, rt_size_t size)
{
  const char *time_start;

  if (size == 0)
    return;

  if (last_update == RT_NULL || last_update[0] == '\0')
  {
    rt_snprintf(time_text, size, "--:--:--");
    return;
  }

  time_start = strchr(last_update, 'T');
  if (time_start != RT_NULL && strlen(time_start + 1) >= 8)
  {
    rt_snprintf(time_text, size, "%.*s", 8, time_start + 1);
    return;
  }

  rt_snprintf(time_text, size, "%s", last_update);
}

static void weather_extract_last_region(const char *path, char *region_text, rt_size_t size)
{
  const char *segment_start;
  const char *cursor;

  if (size == 0)
    return;

  if (path == RT_NULL || path[0] == '\0')
  {
    rt_snprintf(region_text, size, "--");
    return;
  }

  segment_start = path;
  for (cursor = path; *cursor != '\0'; ++cursor)
  {
    if (*cursor == ',' || *cursor == '/' || *cursor == '|' || *cursor == '>')
      segment_start = cursor + 1;
  }

  while (*segment_start == ' ')
    segment_start++;

  if (*segment_start == '\0')
    segment_start = path;

  rt_snprintf(region_text, size, "%s", segment_start);
}

static void weather_sync_cards_from_snapshot(void)
{
  const weather_snapshot_t *snapshot = weather_get_snapshot();
  char update_time[16];
  char temperature[16];
  char region_name[32];

  weather_extract_time(snapshot->last_update, update_time, sizeof(update_time));
  weather_extract_last_region(snapshot->path, region_name, sizeof(region_name));
  if (snapshot->temperature[0] != '\0')
    rt_snprintf(temperature, sizeof(temperature), "%s度", snapshot->temperature);
  else
    rt_snprintf(temperature, sizeof(temperature), "--");

  rt_snprintf(g_weather_cards[0].left_top_value, sizeof(g_weather_cards[0].left_top_value), "%s",
              snapshot->location[0] ? snapshot->location : "--");
  rt_snprintf(g_weather_cards[0].left_bottom_value, sizeof(g_weather_cards[0].left_bottom_value), "%s", temperature);
  rt_snprintf(g_weather_cards[0].right_top_value, sizeof(g_weather_cards[0].right_top_value), "%s", update_time);
  rt_snprintf(g_weather_cards[0].right_bottom_value, sizeof(g_weather_cards[0].right_bottom_value), "%s",
              snapshot->weather_text[0] ? snapshot->weather_text : "--");

  rt_snprintf(g_weather_cards[1].left_top_value, sizeof(g_weather_cards[1].left_top_value), "%s",
              snapshot->country[0] ? snapshot->country : "--");
  rt_snprintf(g_weather_cards[1].left_bottom_value, sizeof(g_weather_cards[1].left_bottom_value), "%s",
              region_name);
  rt_snprintf(g_weather_cards[1].right_top_value, sizeof(g_weather_cards[1].right_top_value), "%s",
              snapshot->timezone[0] ? snapshot->timezone : "--");
  rt_snprintf(g_weather_cards[1].right_bottom_value, sizeof(g_weather_cards[1].right_bottom_value), "%s",
              snapshot->timezone_offset[0] ? snapshot->timezone_offset : "--");

  rt_snprintf(g_weather_cards[2].left_top_value, sizeof(g_weather_cards[2].left_top_value), "%s",
              snapshot->status[0] ? snapshot->status : "未更新");
  rt_snprintf(g_weather_cards[2].left_bottom_value, sizeof(g_weather_cards[2].left_bottom_value), "%s",
              snapshot->weather_code[0] ? snapshot->weather_code : "--");
  rt_snprintf(g_weather_cards[2].right_top_value, sizeof(g_weather_cards[2].right_top_value), "%s",
              pan_service_is_pan_connected() ? "已连接" : "未连接");
  rt_snprintf(g_weather_cards[2].right_bottom_value, sizeof(g_weather_cards[2].right_bottom_value), "%s",
              snapshot->valid ? "心知天气" : "等待更新");
}

static void draw_weather_card(Renderer *renderer, int x, int y, int width, int height, const WeatherCardData *card)
{
  int line_height = renderer->get_line_height();
  int left_x = x + 24;
  int right_x = x + width / 2 - 20;
  int row1_y = y + 24;
  int row2_y = row1_y + line_height + 14;

  char left_line_1[64];
  char left_line_2[64];
  char right_line_1[64];
  char right_line_2[64];

  rt_snprintf(left_line_1, sizeof(left_line_1), "%s：%s", card->left_top_label, card->left_top_value);
  rt_snprintf(left_line_2, sizeof(left_line_2), "%s：%s", card->left_bottom_label, card->left_bottom_value);
  rt_snprintf(right_line_1, sizeof(right_line_1), "%s：%s", card->right_top_label, card->right_top_value);
  rt_snprintf(right_line_2, sizeof(right_line_2), "%s：%s", card->right_bottom_label, card->right_bottom_value);

  renderer->draw_rect(x, y, width, height, 0);
  renderer->draw_text(left_x, row1_y, left_line_1, false, true);
  renderer->draw_text(left_x, row2_y, left_line_2, false, true);
  renderer->draw_text(right_x, row1_y, right_line_1, false, true);
  renderer->draw_text(right_x, row2_y, right_line_2, false, true);
}

int handleWeatherPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  // consume any touch-mark set by touch handler
  int pressed = g_weather_last_button;
  g_weather_last_button = -1;
  bool should_redraw = needs_redraw || pressed >= 0;
  bool should_try_refresh = false;

  if (pressed >= 0)
  {
    g_weather_selected_button = pressed;
    should_redraw = true;
  }

  if (action == UP || action == DOWN)
  {
    g_weather_selected_button = (g_weather_selected_button + 1) % 3;
    should_redraw = true;
  }

  if (action == SELECT)
  {
    int selected_button = pressed >= 0 ? pressed : g_weather_selected_button;

    if (selected_button == 0)
    {
      return 2;
    }
    else if (selected_button == 1)
    {
      return 1; // go back to main page
    }
    else if (selected_button == 2)
    {
      should_try_refresh = true;
      should_redraw = true;
    }
  }

  if (needs_redraw && (!weather_get_snapshot()->valid || !pan_service_is_pan_connected()))
    should_try_refresh = true;

  if (should_try_refresh)
  {
    if (pan_service_is_pan_connected() || pan_service_is_bt_connected())
      renderer->show_busy();
    (void)weather_refresh();
  }

  if (should_redraw)
  {
    weather_sync_cards_from_snapshot();
    clear_areas();
    int w = renderer->get_page_width();
    int h = renderer->get_page_height();
    renderer->fill_rect(0, 0, w, h, 255);

    int card_x = 32;
    int card_w = w - card_x * 2;
    int card_h = 180;
    int card_gap = 8;
    int card_top = 54;
    for (int i = 0; i < 3; ++i)
    {
      int card_y = card_top + i * (card_h + card_gap);
      draw_weather_card(renderer, card_x, card_y, card_w, card_h, &g_weather_cards[i]);
    }

    int lh = renderer->get_line_height();
    int btn_h = 62;
    int top_btn_h = 62;
    int btn_gap = 24;
    int btn_margin = 24;
    int btn_w = (w - btn_margin * 2 - btn_gap) / 2;
    int btn_y = h - btn_h - 40;
    int top_btn_y = btn_y - top_btn_h - 18;
    int left_x = btn_margin;
    int right_x = left_x + btn_w + btn_gap;
    int top_x = btn_margin;
    int top_w = w - btn_margin * 2;
    int btn_touch_shift = 28;

    int btn_text_x_offset = 0;
    int btn_text_y_offset = -10;

    add_area(top_x, top_btn_y + btn_touch_shift, top_w, top_btn_h); // index 0
    add_area(left_x, btn_y + btn_touch_shift, btn_w, btn_h);        // index 1
    add_area(right_x, btn_y + btn_touch_shift, btn_w, btn_h);       // index 2

    if (g_weather_selected_button == 0)
    {
      for (int i = 0; i < 4; ++i)
        renderer->draw_rect(top_x + i, top_btn_y + i, top_w - 2 * i, top_btn_h - 2 * i, 0);
    }
    else
    {
      renderer->draw_rect(top_x, top_btn_y, top_w, top_btn_h, 0);
    }
    const char *top_label = "城市选择";
    int tw = renderer->get_text_width(top_label);
    renderer->draw_text(top_x + (top_w - tw) / 2 + btn_text_x_offset, top_btn_y + (top_btn_h - lh) / 2 + btn_text_y_offset, top_label, false, true);

    if (g_weather_selected_button == 1)
    {
      for (int i = 0; i < 4; ++i)
        renderer->draw_rect(left_x + i, btn_y + i, btn_w - 2 * i, btn_h - 2 * i, 0);
    }
    else
    {
      renderer->draw_rect(left_x, btn_y, btn_w, btn_h, 0);
    }
    const char *left_label = "返回";
    int lw = renderer->get_text_width(left_label);
    renderer->draw_text(left_x + (btn_w - lw) / 2 + btn_text_x_offset, btn_y + (btn_h - lh) / 2 + btn_text_y_offset, left_label, false, true);

    if (g_weather_selected_button == 2)
    {
      for (int i = 0; i < 4; ++i)
        renderer->draw_rect(right_x + i, btn_y + i, btn_w - 2 * i, btn_h - 2 * i, 0);
    }
    else
    {
      renderer->draw_rect(right_x, btn_y, btn_w, btn_h, 0);
    }
    const char *right_label = "更新";
    int rw = renderer->get_text_width(right_label);
    renderer->draw_text(right_x + (btn_w - rw) / 2 + btn_text_x_offset, btn_y + (btn_h - lh) / 2 + btn_text_y_offset, right_label, false, true);

    return 0;
  }

  return 0;
}

int handleWeatherCityPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  int pressed = g_weather_city_last_hit;
  g_weather_city_last_hit = -1;
  bool should_redraw = needs_redraw || pressed >= 0;

  if (needs_redraw)
    weather_city_page_sync_selection();

  if (pressed >= 0)
  {
    g_weather_city_selected_index = pressed;
    if (pressed >= 0 && pressed < kWeatherCityOptionCount)
      g_weather_city_pending_index = pressed;
    should_redraw = true;
  }

  if (action == UP)
  {
    g_weather_city_selected_index = (g_weather_city_selected_index - 1 + kWeatherCityOptionCount + 2) % (kWeatherCityOptionCount + 2);
    should_redraw = true;
  }
  else if (action == DOWN)
  {
    g_weather_city_selected_index = (g_weather_city_selected_index + 1) % (kWeatherCityOptionCount + 2);
    should_redraw = true;
  }

  if (action == SELECT)
  {
    int selected_item = pressed >= 0 ? pressed : g_weather_city_selected_index;
    if (selected_item >= 0 && selected_item < kWeatherCityOptionCount)
    {
      g_weather_city_pending_index = selected_item;
      should_redraw = true;
    }
    else if (selected_item == kWeatherCityOptionCount)
    {
      return 1;
    }
    else if (selected_item == kWeatherCityOptionCount + 1)
    {
      if (weather_set_city(kWeatherCityOptions[g_weather_city_pending_index].value) != RT_EOK)
      {
        weather_set_status("城市设置失败");
        should_redraw = true;
      }
      else
      {
        if (pan_service_is_pan_connected() || pan_service_is_bt_connected())
          renderer->show_busy();
        (void)weather_refresh();
        return 2;
      }
    }
  }

  if (!should_redraw)
    return 0;

  clear_areas();
  int w = renderer->get_page_width();
  int h = renderer->get_page_height();
  int lh = renderer->get_line_height();
  renderer->fill_rect(0, 0, w, h, 255);

  const char *title = "城市选择";
  int title_w = renderer->get_text_width(title);
  renderer->draw_text((w - title_w) / 2, 44, title, false, true);

  char current_city_text[48];
  rt_snprintf(current_city_text, sizeof(current_city_text), "当前：%s", kWeatherCityOptions[g_weather_city_pending_index].label);
  int current_city_w = renderer->get_text_width(current_city_text);
  renderer->draw_text((w - current_city_w) / 2, 44 + lh + 8, current_city_text, false, true);

  int item_x = 24;
  int item_w = w - item_x * 2;
  int item_h = 72;
  int item_gap = 12;
  int item_top = 44 + (lh + 8) * 2;
  for (int i = 0; i < kWeatherCityOptionCount; ++i)
  {
    int item_y = item_top + i * (item_h + item_gap);
    add_area(item_x, item_y, item_w, item_h);

    if (g_weather_city_selected_index == i)
    {
      for (int border = 0; border < 4; ++border)
        renderer->draw_rect(item_x + border, item_y + border, item_w - border * 2, item_h - border * 2, 0);
    }
    else
    {
      renderer->draw_rect(item_x, item_y, item_w, item_h, 0);
    }

    const char *city_label = kWeatherCityOptions[i].label;
    renderer->draw_text(item_x + 24, item_y + (item_h - lh) / 2 - 6, city_label, false, true);

    if (g_weather_city_pending_index == i)
    {
      const char *selected_label = "已选";
      int selected_w = renderer->get_text_width(selected_label);
      renderer->draw_text(item_x + item_w - selected_w - 24, item_y + (item_h - lh) / 2 - 6, selected_label, false, true);
    }
  }

  int btn_h = 62;
  int btn_gap = 24;
  int btn_margin = 24;
  int btn_w = (w - btn_margin * 2 - btn_gap) / 2;
  int btn_y = h - btn_h - 40;
  int left_x = btn_margin;
  int right_x = left_x + btn_w + btn_gap;
  int btn_touch_shift = 28;
  int btn_text_y = btn_y + (btn_h - lh) / 2 - 10;

  add_area(left_x, btn_y + btn_touch_shift, btn_w, btn_h);
  add_area(right_x, btn_y + btn_touch_shift, btn_w, btn_h);

  if (g_weather_city_selected_index == kWeatherCityOptionCount)
  {
    for (int border = 0; border < 4; ++border)
      renderer->draw_rect(left_x + border, btn_y + border, btn_w - border * 2, btn_h - border * 2, 0);
  }
  else
  {
    renderer->draw_rect(left_x, btn_y, btn_w, btn_h, 0);
  }
  const char *back_label = "返回";
  int back_w = renderer->get_text_width(back_label);
  renderer->draw_text(left_x + (btn_w - back_w) / 2, btn_text_y, back_label, false, true);

  if (g_weather_city_selected_index == kWeatherCityOptionCount + 1)
  {
    for (int border = 0; border < 4; ++border)
      renderer->draw_rect(right_x + border, btn_y + border, btn_w - border * 2, btn_h - border * 2, 0);
  }
  else
  {
    renderer->draw_rect(right_x, btn_y, btn_w, btn_h, 0);
  }
  const char *confirm_label = "确认";
  int confirm_w = renderer->get_text_width(confirm_label);
  renderer->draw_text(right_x + (btn_w - confirm_w) / 2, btn_text_y, confirm_label, false, true);

  return 0;
}

// ============================================================
// 空白页面（测试用）：绘制45°斜线
// ============================================================
static int blank_line_width = 1;     // 斜线宽度（像素）
static int blank_line_offset = 8;    // 第二条线偏移量（像素）
static int blank_vline_spacing = 0;  // 垂直线间距：0=不画，>0=每隔N像素画一条垂直线

static void render_blank_page(Renderer *renderer)
{
  clear_areas();

  int w = renderer->get_page_width();
  int h = renderer->get_page_height();

  // 白色背景
  renderer->fill_rect(0, 0, w, h, 255);

  // 绘制两条45°平行斜线
  {
    int len = (w < h) ? w : h;
    int offsets[] = {0, blank_line_offset};
    for (int line = 0; line < 2; line++)
    {
      int y_off = offsets[line];
      for (int i = 0; i <= len; i++)
      {
        for (int t = 0; t < blank_line_width; t++)
        {
          int px = i;
          int py = i + y_off + t;
          if (px >= 0 && px < w && py >= 0 && py < h)
            renderer->draw_pixel(px, py, 0);
        }
      }
    }
  }

  // 绘制垂直线（与斜线共存）
  if (blank_vline_spacing > 0)
  {
    for (int x = 0; x < w; x += blank_vline_spacing)
    {
      for (int y = 0; y < h; y++)
        renderer->draw_pixel(x, y, 0);
    }
  }

  // 标题
  const char *title = "空白页面";
  int title_w = renderer->get_text_width(title);
  int title_h = renderer->get_line_height();
  renderer->draw_text((w - title_w) / 2, 44, title, true, true);

  // 显示当前参数
  char info[64];
  if (blank_vline_spacing > 0)
    rt_snprintf(info, sizeof(info), "线宽：%d 偏移：%d 垂直：%d", blank_line_width, blank_line_offset, blank_vline_spacing);
  else
    rt_snprintf(info, sizeof(info), "线宽：%d 偏移：%d", blank_line_width, blank_line_offset);
  int info_w = renderer->get_text_width(info);
  renderer->draw_text((w - info_w) / 2, 44 + title_h + 12, info, false, true);

  // 底部按钮：返回
  int btn_h = 62;
  int btn_w = 200;
  int btn_x = (w - btn_w) / 2;
  int btn_y = h - btn_h - 40;
  renderer->draw_rect(btn_x, btn_y, btn_w, btn_h, 0);
  const char *btn_label = "返回";
  int bw = renderer->get_text_width(btn_label);
  int bh = renderer->get_line_height();
  add_area(btn_x, btn_y, btn_w, btn_h);
  renderer->draw_text(btn_x + (btn_w - bw) / 2, btn_y + (btn_h - bh) / 2, btn_label, false, true);
}

int handleBlankPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (needs_redraw || action == NONE)
  {
    render_blank_page(renderer);
    return 0;
  }

  switch (action)
  {
    case UP:
      // 增大线宽
      if (blank_line_width < 20)
        blank_line_width++;
      render_blank_page(renderer);
      break;
    case DOWN:
      // 减小线宽
      if (blank_line_width > 1)
        blank_line_width--;
      render_blank_page(renderer);
      //RT_ASSERT(0);
      break;
    case SELECT:
      return 1; // 返回主页面
    default:
      break;
  }
  return 0;
}

// 串口命令：控制空白页面斜线偏移
// 用法：blank_offset [值]  — 不带参数显示当前值，带参数设置新值
static void blank_offset(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("当前偏移量：%d px\n", blank_line_offset);
        rt_kprintf("用法：blank_offset <像素值>  (0~200)\n");
        return;
    }

    int val = atoi(argv[1]);
    if (val < 0) val = 0;
    if (val > 200) val = 200;
    blank_line_offset = val;
    rt_kprintf("偏移量已设为：%d px\n", blank_line_offset);
}

FINSH_FUNCTION_EXPORT_ALIAS(blank_offset, __cmd_blank_offset, Set blank page line offset);

// 串口命令：控制空白页面线宽
// 用法：blank_width [值]  — 不带参数显示当前值，带参数设置新值
static void blank_width(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("当前线宽：%d px\n", blank_line_width);
        rt_kprintf("用法：blank_width <像素值>  (1~20)\n");
        return;
    }

    int val = atoi(argv[1]);
    if (val < 1) val = 1;
    if (val > 20) val = 20;
    blank_line_width = val;
    rt_kprintf("线宽已设为：%d px\n", blank_line_width);
}

FINSH_FUNCTION_EXPORT_ALIAS(blank_width, __cmd_blank_width, Set blank page line width);

// 串口命令：控制空白页面垂直线间距
// 用法：blank_vlines [间距]  — 不带参数显示当前值，带参数设置间距（0=不画垂直线）
extern Renderer *renderer;
static void blank_vline_cmd(int argc, char **argv)
{
    if (argc < 2)
    {
        rt_kprintf("当前垂直线间距：%d px (%s)\n", blank_vline_spacing,
                   blank_vline_spacing > 0 ? "开启" : "关闭");
        rt_kprintf("用法：blank_vlines <间距>  (0=不画, 1~200=间距像素数)\n");
        return;
    }

    int val = atoi(argv[1]);
    if (val < 0) val = 0;
    if (val > 200) val = 200;
    blank_vline_spacing = val;
    rt_kprintf("垂直线间距已设为：%d px\n", blank_vline_spacing);

    // 如果当前在空白页面，立即刷新显示
    extern AppUIState ui_state;
    if (ui_state == BLANK_PAGE && renderer)
    {
        render_blank_page(renderer);
        renderer->flush_display();
    }
}

FINSH_FUNCTION_EXPORT_ALIAS(blank_vline_cmd, __cmd_blank_vlines, Set blank page vertical lines spacing);
