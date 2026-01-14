#include "epub_screen.h"
#include <string.h>


extern TouchControls *touch_controls;

// 主页面选项
typedef enum 
{
  OPTION_OPEN_LIBRARY = 0,
  OPTION_CONTINUE_READING,
  OPTION_ENTER_SETTINGS
} MainOption;

static MainOption main_option = OPTION_OPEN_LIBRARY; // 默认“打开书库”
static int full_refresh_period = 10; // 全刷周期次数，仅用于设置页显示

// 设置页列表项
typedef enum { SET_TOUCH = 0, SET_TIMEOUT = 1, SET_FULL_REFRESH = 2, SET_CONFIRM = 3 } SettingsItem;
static int settings_selected_idx = 0;

// 超时关机：1/3/5/7/10/不关机(0)
static const int kTimeoutOptions[] = {1, 3, 5, 7, 10, 0};
static const int kTimeoutOptionsCount = sizeof(kTimeoutOptions) / sizeof(kTimeoutOptions[0]);
static int timeout_shutdown_hours = 5; // 运行时关机超时（小时），0 表示不关机
static int timeout_idx = -1; // 指向 kTimeoutOptions 的索引

static int find_timeout_idx(int hours)
{
  for (int i = 0; i < kTimeoutOptionsCount; ++i)
  {
    if (kTimeoutOptions[i] == hours) return i;
  }
  return 2; // 默认索引：5小时
}

static void adjust_timeout(bool increase)
{
  if (timeout_idx < 0) timeout_idx = find_timeout_idx(timeout_shutdown_hours);
  if (increase)
  {
    timeout_idx = (timeout_idx + 1) % kTimeoutOptionsCount;
  }
  else
  {
    timeout_idx = (timeout_idx - 1 + kTimeoutOptionsCount) % kTimeoutOptionsCount;
  }
  timeout_shutdown_hours = kTimeoutOptions[timeout_idx];
}

void screen_init(int default_timeout_hours)
{
  timeout_shutdown_hours = default_timeout_hours;
  timeout_idx = find_timeout_idx(timeout_shutdown_hours);
}

int screen_get_timeout_shutdown_hours()
{
  if (timeout_idx < 0) timeout_idx = find_timeout_idx(timeout_shutdown_hours);
  return timeout_shutdown_hours;
}

int screen_get_main_selected_option()
{
  return (int)main_option; // 0: 打开书库, 1: 继续阅读, 2: 进入设置
}

// 主页面
static void render_main_page(Renderer *renderer)
{
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
  renderer->draw_text(left_x + (rect_w - lt_w) / 2, y + (rect_h - lt_h) / 2, lt, false, true);

  // 右 ">"
  const char *gt = ">";
  int gt_w = renderer->get_text_width(gt);
  int gt_h = renderer->get_line_height();
  renderer->draw_text(right_x + (rect_w - gt_w) / 2, y + (rect_h - gt_h) / 2, gt, false, true);

  // 中间选项文本
  int mid_x = left_x + rect_w + margin_side;
  int mid_w = right_x - margin_side - mid_x;

  const char *opt_text = NULL;
  switch (main_option)
  {
    case OPTION_OPEN_LIBRARY:     opt_text = "打开书库"; break;
    case OPTION_CONTINUE_READING: opt_text = "继续阅读"; break;
    case OPTION_ENTER_SETTINGS:   opt_text = "进入设置"; break;
  }
  int opt_w = renderer->get_text_width(opt_text);
  int opt_h = renderer->get_line_height();
  renderer->draw_text(mid_x + (mid_w - opt_w) / 2, y + (rect_h - opt_h) / 2, opt_text, false, true);
}

void handleMainPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  if (needs_redraw || action == NONE)
  {
    render_main_page(renderer);
    return;
  }
  switch (action)
  {
    case UP:   // 左切换
      if (main_option == OPTION_OPEN_LIBRARY) main_option = OPTION_ENTER_SETTINGS;
      else if (main_option == OPTION_CONTINUE_READING) main_option = OPTION_OPEN_LIBRARY;
      else main_option = OPTION_CONTINUE_READING;
      render_main_page(renderer);
      break;
    case DOWN: // 右切换
      if (main_option == OPTION_OPEN_LIBRARY) main_option = OPTION_CONTINUE_READING;
      else if (main_option == OPTION_CONTINUE_READING) main_option = OPTION_ENTER_SETTINGS;
      else main_option = OPTION_OPEN_LIBRARY;
      render_main_page(renderer);
      break;
    case SELECT:
      // 由上层 main.cpp 负责切换 页面UIState
      switch (main_option)
      {
        case OPTION_OPEN_LIBRARY:     
            rt_kprintf("1\n"); 
        break;
        case OPTION_CONTINUE_READING: 
            rt_kprintf("2\n"); 
        break;
        case OPTION_ENTER_SETTINGS:   
            rt_kprintf("3\n"); 
        break;
      }
      break;
    default:
      break;
  }
}

// 设置页面
static void render_settings_page(Renderer *renderer)
{
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);

  // 标题
  const char *title = "设置";
  int title_w = renderer->get_text_width(title);
  int title_h = renderer->get_line_height();
  int page_w = renderer->get_page_width();
  int page_h = renderer->get_page_height();
  renderer->draw_text((page_w - title_w) / 2, 40, title, true, true);

  // 列表项布局参数
  int margin_lr = 6; // 左右边距，给左右触控箭头
  int item_h = 100;   // 矩形高度
  int gap = 54;      // 列表项之间的间距
  int arrow_col_w = 40; // 左右触控箭头列宽度
  int y = 40 + title_h + 20; // 第一项起始Y

  // 1) 触控开关
  int item_w = page_w - margin_lr * 2 - arrow_col_w * 2; // 为左右箭头列留边
  int item_x = margin_lr + arrow_col_w;
  {
    const char *lt = "<"; int lt_w = renderer->get_text_width(lt);
    renderer->draw_text(margin_lr + (arrow_col_w - lt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, lt, false, true);
    const char *gt = ">"; int gt_w = renderer->get_text_width(gt);
    renderer->draw_text(page_w - margin_lr - arrow_col_w + (arrow_col_w - gt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, gt, false, true);
  }
  if (settings_selected_idx == SET_TOUCH)
  {
    // 选中强化：多重描边，提高可见度
    for (int i = 0; i < 5; ++i) renderer->draw_rect(item_x + i, y + i, item_w - 2 * i, item_h - 2 * i, 0);
  }
  else
  {
    renderer->draw_rect(item_x, y, item_w, item_h, 0); //画框线
  }
  bool touch_on = touch_controls ? touch_controls->isTouchEnabled() : false;
  char buf1[48];
  rt_snprintf(buf1, sizeof(buf1), "触控开关：%s", touch_on ? "开" : "关");
  int t1_w = renderer->get_text_width(buf1);
  int lh = renderer->get_line_height();
  {
    int tx = item_x + (item_w - t1_w) / 2;
    if (tx < item_x + 4) tx = item_x + 4;
    if (tx + t1_w > item_x + item_w - 4) tx = item_x + item_w - t1_w - 4;
    renderer->draw_text(tx, y + (item_h - lh) / 2, buf1, false, true);
  }
  y += item_h + gap;

  // 2) 超时关机
  {
    const char *lt = "<"; int lt_w = renderer->get_text_width(lt);
    renderer->draw_text(margin_lr + (arrow_col_w - lt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, lt, false, true);
    const char *gt = ">"; int gt_w = renderer->get_text_width(gt);
    renderer->draw_text(page_w - margin_lr - arrow_col_w + (arrow_col_w - gt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, gt, false, true);
  }
  if (settings_selected_idx == SET_TIMEOUT)
  {
    for (int i = 0; i < 5; ++i) renderer->draw_rect(item_x + i, y + i, item_w - 2 * i, item_h - 2 * i, 0);
  }
  else
  {
    renderer->draw_rect(item_x, y, item_w, item_h, 0);
  }
  char buf2[64];
  if (timeout_shutdown_hours == 0)
  {
    rt_snprintf(buf2, sizeof(buf2), "超时关机：不关机");
  }
  else
  {
    rt_snprintf(buf2, sizeof(buf2), "超时关机：%d 小时", timeout_shutdown_hours);
  }
  {
    int t2_w = renderer->get_text_width(buf2);
    int tx = item_x + (item_w - t2_w) / 2;
    if (tx < item_x + 4) tx = item_x + 4;
    if (tx + t2_w > item_x + item_w - 4) tx = item_x + item_w - t2_w - 4;
    renderer->draw_text(tx, y + (item_h - lh) / 2, buf2, false, true);
  }
  y += item_h + gap;

  // 3) 全刷周期
  {
    const char *lt = "<"; int lt_w = renderer->get_text_width(lt);
    renderer->draw_text(margin_lr + (arrow_col_w - lt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, lt, false, true);
    const char *gt = ">"; int gt_w = renderer->get_text_width(gt);
    renderer->draw_text(page_w - margin_lr - arrow_col_w + (arrow_col_w - gt_w) / 2, y + (item_h - renderer->get_line_height()) / 2, gt, false, true);
  }
  if (settings_selected_idx == SET_FULL_REFRESH)
  {
    for (int i = 0; i < 5; ++i) renderer->draw_rect(item_x + i, y + i, item_w - 2 * i, item_h - 2 * i, 0);
  }
  else
  {
    renderer->draw_rect(item_x, y, item_w, item_h, 0);
  }
  char buf3[64];
  rt_snprintf(buf3, sizeof(buf3), "全刷周期：%d 次", full_refresh_period);
  {
    int t3_w = renderer->get_text_width(buf3);
    int tx = item_x + (item_w - t3_w) / 2;
    if (tx < item_x + 4) tx = item_x + 4;
    if (tx + t3_w > item_x + item_w - 4) tx = item_x + item_w - t3_w - 4;
    renderer->draw_text(tx, y + (item_h - lh) / 2, buf3, false, true);
  }
  y += item_h + gap;

  // 底部 确认 按钮
  int confirm_h = 120; // 矩形框高度
  int confirm_w = item_w; // 宽度
  int confirm_x = (page_w - confirm_w) / 2; // 居中
  int confirm_y = page_h - confirm_h - 60; // 距离底部位置
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
  renderer->draw_text(confirm_x + (confirm_w - c_w) / 2, confirm_y + (confirm_h - c_h) / 2, confirm, false, true);
}

bool handleSettingsPage(Renderer *renderer, UIAction action, bool needs_redraw)
{
  // 读取并清除一次性的触控箭头标记，避免后续硬件按键误用
  int touch_row = g_touch_last_settings_row;
  int touch_dir = g_touch_last_settings_dir;
  g_touch_last_settings_row = -1;
  g_touch_last_settings_dir = 0;

  if (needs_redraw || action == NONE)
  {
    render_settings_page(renderer);
    return false;
  }

  switch (action)
  {
    case UP:
      // 触控箭头若命中“超时关机”行且为左箭头（减），执行减；否则执行上下选择
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
      // 触控箭头若命中“超时关机”行且为右箭头（加），执行加；否则执行上下选择
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
        // SELECT 在超时关机项上为加操作（循环）
        adjust_timeout(true);
        render_settings_page(renderer);
        break;
      }
      if (settings_selected_idx == SET_CONFIRM)
      {
        // 由上层切回主页面
        return true;
      }
      // 其他项当前不处理
      break;
    default:
      break;
  }
  return false;
}
