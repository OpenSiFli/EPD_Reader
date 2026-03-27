#include "bf0_hal.h"
#include "EpubList/Epub.h"
#include "EpubList/EpubList.h"
#include "EpubList/EpubReader.h"
#include "EpubList/EpubToc.h"
#include "bf0_pm.h"
#include "boards/Board.h"
#include "boards/SF32PaperRenderer.h"
#include "boards/controls/SF32_TouchControls.h"
#include "epd_driver.h"
#include "gui_app_pm.h"
#include "reading_settings.h"
#include <RubbishHtmlParser/RubbishHtmlParser.h>
#include <rtthread.h>

#undef LOG_TAG
#undef DBG_LEVEL
#define DBG_LEVEL DBG_LOG // DBG_INFO  //
#define LOG_TAG "EPUB.main"

#include <rtdbg.h>

extern "C" {
int main();
rt_uint32_t heap_free_size(void);
extern const uint8_t low_power_map[];
extern const uint8_t chargeing_map[];
extern const uint8_t welcome_map[];
extern const uint8_t shutdown_map[];
extern const unsigned char epub_ttf_data[];
extern const int epub_ttf_data_size;
int epd_font_ft_preheat_is_running(void);
void epd_font_ft_preheat_stop(void);
int font_manager_init(const char *font_dir);
int font_manager_get_count(void);
const char *font_manager_get_name(int index);
}

const char *TAG = "main";

typedef enum {
  SELECTING_EPUB,
  SELECTING_TABLE_CONTENTS,
  READING_EPUB,
  READING_SETTINGS
} UIState;
typedef enum {
  MAIN_MENU,
  WELCOME_PAGE,
  LOW_POWER_PAGE,
  CHARGING_PAGE
} UIState2;

// default to showing the list of epubs to the user
UIState ui_state = SELECTING_EPUB;
static UIState g_state_before_settings = SELECTING_EPUB;
UIState2 lowpower_ui_state = MAIN_MENU;
// the state data for the epub list and reader
EpubListState epub_list_state;
// the state data for the epub index list
EpubTocState epub_index_state;

void handleEpub(Renderer *renderer, UIAction action);
void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw);

static EpubList *epub_list = nullptr;
static EpubReader *reader = nullptr;
static EpubToc *contents = nullptr;
static bool charge_full = false;
// 设置页面锚点：记录进入设置前的阅读位置
static int g_anchor_block = 0;
static int g_anchor_line = 0;
static bool g_has_anchor = false;
Battery *battery = nullptr;
Renderer *renderer = nullptr;
TouchControls *touch_controls = nullptr;

rt_mq_t ui_queue = RT_NULL;

void handleEpub(Renderer *renderer, UIAction action) {
  if (!reader) {
    reader = new EpubReader(
        epub_list_state.epub_list[epub_list_state.selected_item], renderer);
    reader->load();
  }
  switch (action) {
  case UP:
    reader->prev();
    break;
  case DOWN:
    reader->next();
    break;
  case SELECT:
    ui_state = SELECTING_EPUB;
    renderer->clear_screen();
    delete reader;
    reader = nullptr;
    if (!epub_list) {
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

void handleEpubTableContents(Renderer *renderer, UIAction action,
                             bool needs_redraw) {
  if (!contents) {
    contents =
        new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item],
                    epub_index_state, renderer);
    contents->set_needs_redraw();
    contents->load();
  }
  switch (action) {
  case UP:
    contents->prev();
    break;
  case DOWN:
    contents->next();
    break;
  case SELECT:
    ui_state = READING_EPUB;
    reader = new EpubReader(
        epub_list_state.epub_list[epub_list_state.selected_item], renderer);
    reader->set_state_section(contents->get_selected_toc());
    reader->load();
    delete contents;
    handleEpub(renderer, NONE);
    return;
  case NONE:
  default:
    break;
  }
  contents->render();
}

void handleEpubList(Renderer *renderer, UIAction action, bool needs_redraw) {
  if (!epub_list) {
    ulog_i("main", "Creating epub list");
    epub_list = new EpubList(renderer, epub_list_state);
    epub_list->setTouchControls(touch_controls);
    if (epub_list->load("/")) {
      ulog_i("main", "Epub files loaded");
    }
  }
  if (needs_redraw) {
    epub_list->set_needs_redraw();
  }
  switch (action) {
  case UP:
    epub_list->prev();
    break;
  case DOWN:
    epub_list->next();
    break;
  case SELECT:
    if (epub_list_state.selected_item == -1) {
      rt_kprintf("touch open or off\n");
      bool current_state = touch_controls->isTouchEnabled();
      touch_controls->setTouchEnable(!current_state);
      if (!current_state) {
        touch_controls->powerOnTouch();
      } else {
        touch_controls->powerOffTouch();
      }
      epub_list->render();
      return;
    } else {
      ui_state = SELECTING_TABLE_CONTENTS;
      contents =
          new EpubToc(epub_list_state.epub_list[epub_list_state.selected_item],
                      epub_index_state, renderer);
      contents->load();
      contents->set_needs_redraw();
      handleEpubTableContents(renderer, NONE, true);
      return;
    }
  case NONE:
  default:
    break;
  }
  epub_list->render();
}

void draw_battery_level(Renderer *renderer, float voltage, float percentage) {
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

void clear_charge_icon(Renderer *renderer) {
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
  int tri1_B_x = tri1_A_x - size / 4;
  int tri1_B_y = tri1_A_y + (int)(size / 4 * tilt_factor);
  int tri1_C_x = tri1_A_x + (int)(size / 2 * tilt_factor);
  int tri1_C_y = tri1_A_y - size / 2;
  renderer->fill_triangle(tri1_A_x, tri1_A_y, tri1_B_x, tri1_B_y, tri1_C_x, tri1_C_y, 0);

  int tri2_D_x = x;
  int tri2_D_y = y;
  int tri2_E_x = tri2_D_x + size / 4;
  int tri2_E_y = tri2_D_y - (int)(size / 4 * tilt_factor);
  int tri2_F_x = tri2_D_x - (int)(size / 2 * tilt_factor);
  int tri2_F_y = tri2_D_y + size / 2;
  renderer->fill_triangle(tri2_D_x, tri2_D_y, tri2_E_x, tri2_E_y, tri2_F_x, tri2_F_y, 0);
}

void draw_charge_status(Renderer *renderer, Battery *battery) {
  const int icon_size = 30;
  int battery_width = 40;
  int margin_right = 0;
  int margin_top = 3;
  int xpos = renderer->get_page_width() - battery_width - margin_right - icon_size - 4;
  int ypos = margin_top;
  if (battery->is_charging()) {
    draw_lightning(renderer, xpos + icon_size / 2, ypos + icon_size / 2, icon_size);
  } else {
    clear_charge_icon(renderer);
  }
}

void handleUserInteraction(Renderer *renderer, UIAction ui_action, bool needs_redraw) {
  if (battery && battery->get_low_power_state() == 1) {
    rt_kprintf("low power state\n");
    return;
  }
  uint32_t start_tick = rt_tick_get();

  if (ui_action == LONG_SELECT && ui_state != READING_SETTINGS) {
    g_state_before_settings = ui_state;
    // 进入设置前保存阅读锚点
    if (ui_state == READING_EPUB && reader) {
      reader->save_anchor(g_anchor_block, g_anchor_line);
      g_has_anchor = true;
    } else {
      g_has_anchor = false;
    }
    ui_state = READING_SETTINGS;
    reading_settings_draw(renderer);
    return;
  }

  switch (ui_state) {
  case READING_EPUB:
    handleEpub(renderer, ui_action);
    break;
  case READING_SETTINGS:
  {
    bool still_in_settings = reading_settings_handle_action(renderer, ui_action);
    if (!still_in_settings) {
      ui_state = g_state_before_settings;
      if (ui_state == READING_EPUB) {
        delete reader;
        reader = nullptr;
        handleEpub(renderer, NONE);
        // 用锚点恢复阅读位置（handleEpub 已经创建了新 reader 并排了第 1 页）
        if (g_has_anchor && reader) {
          reader->restore_by_anchor(g_anchor_block, g_anchor_line);
          reader->render();
          g_has_anchor = false;
        }
      } else if (ui_state == SELECTING_TABLE_CONTENTS) {
        delete contents;
        contents = nullptr;
        handleEpubTableContents(renderer, NONE, true);
      } else {
        handleEpubList(renderer, NONE, true);
      }
    }
    return;
  }
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

const char *getCurrentPageName() {
  switch (lowpower_ui_state) {
  case MAIN_MENU:     return "MAIN_MENU";
  case WELCOME_PAGE:  return "WELCOME_PAGE";
  case LOW_POWER_PAGE: return "LOW_POWER_PAGE";
  case CHARGING_PAGE: return "CHARGING_PAGE";
  default:            return "UNKNOWN_PAGE";
  }
}

void back_to_main_page() {
  if (strcmp(getCurrentPageName(), "MAIN_MENU") == 0) return;
  lowpower_ui_state = MAIN_MENU;
  if (ui_state == SELECTING_TABLE_CONTENTS) {
    if (contents) { delete contents; contents = nullptr; }
  }
  bool hydrate_success = renderer->hydrate();
  renderer->reset();
  renderer->set_margin_top(35);
  renderer->set_margin_left(10);
  renderer->set_margin_right(10);
  if (!epub_list) {
    epub_list = new EpubList(renderer, epub_list_state);
    if (epub_list->load("/")) { ulog_i("main", "Epub files loaded"); }
  }
  handleUserInteraction(renderer, NONE, true);
  if (battery) {
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  touch_controls->render(renderer);
  renderer->flush_display();
}

void draw_welcome_page(Battery *battery) {
  if (strcmp(getCurrentPageName(), "WELCOME_PAGE") == 0) return;
  lowpower_ui_state = WELCOME_PAGE;
  touch_controls->powerOffTouch();
  touch_controls->setTouchEnable(false);
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 649, img_height = 150;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer *fb_renderer = static_cast<EpdiyFrameBufferRenderer *>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, welcome_map);
  renderer->flush_display();
}

void draw_low_power_page(Battery *battery) {
  if (strcmp(getCurrentPageName(), "LOW_POWER_PAGE") == 0) return;
  lowpower_ui_state = LOW_POWER_PAGE;
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 200, img_height = 200;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer *fb_renderer = static_cast<EpdiyFrameBufferRenderer *>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, low_power_map);
  renderer->flush_display();
}

void draw_charge_page(Battery *battery) {
  if (strcmp(getCurrentPageName(), "CHARGING_PAGE") == 0) return;
  lowpower_ui_state = CHARGING_PAGE;
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 0);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 200, img_height = 200;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  EpdiyFrameBufferRenderer *fb_renderer = static_cast<EpdiyFrameBufferRenderer *>(renderer);
  fb_renderer->show_img(center_x - img_width / 2, center_y - img_height / 2, img_width, img_height, chargeing_map);
  renderer->flush_display();
}

void draw_shutdown_page() {
  renderer->fill_rect(0, 0, renderer->get_page_width(), renderer->get_page_height(), 255);
  if (battery) {
    renderer->set_margin_top(35);
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  const int img_width = 200, img_height = 200;
  int center_x = renderer->get_page_width() / 2;
  int center_y = 35 + (renderer->get_page_height() - 35) / 2;
  int x_pos = center_x - img_width / 2;
  int y_pos = center_y - img_height / 2;
  EpdiyFrameBufferRenderer *fb_renderer = static_cast<EpdiyFrameBufferRenderer *>(renderer);
  fb_renderer->show_img(x_pos, y_pos, img_width, img_height, shutdown_map);
  const char *shutdown_text = "请长按 Key1 开机";
  int text_width = renderer->get_text_width(shutdown_text);
  int text_x = center_x - text_width / 2;
  int text_y = y_pos + img_height + 10;
  renderer->draw_text(text_x, text_y, shutdown_text, false, true);
  renderer->flush_display();
}

void main_task(void *param) {
  ulog_i("main", "Powering up the board");
  Board *board = Board::factory();
  board->power_up();

  rt_kprintf("TTF data at %p, size=%d\n", epub_ttf_data, epub_ttf_data_size);
  rt_kprintf("PSRAM free before font init: %d\n", heap_free_size());
  rt_kprintf("PSRAM free after font init: %d\n", heap_free_size());

  ulog_i("main", "Creating renderer");
  ::renderer = board->get_renderer();
  ulog_i("main", "Starting file system");
  board->start_filesystem();

  int font_count = font_manager_init("/fonts");
  for (int i = 0; i < font_count; i++) {
    rt_kprintf("Font [%d]: %s\n", i, font_manager_get_name(i));
  }
  reading_settings_load(renderer);

  ui_queue = rt_mq_create("ui_act", sizeof(UIAction), 10, 0);

  ulog_i("main", "Starting battery monitor");
  battery = board->get_battery(ui_queue);
  if (battery) { battery->setup(); }

  renderer->set_margin_top(35);
  renderer->set_margin_left(10);
  renderer->set_margin_right(10);

  ulog_i("main", "Setting up controls");
  ButtonControls *button_controls = board->get_button_controls(ui_queue);
  ::touch_controls = board->get_touch_controls(renderer, ui_queue);
  ulog_i("main", "Controls configured");

  if (button_controls->did_wake_from_deep_sleep()) {
    bool hydrate_success = renderer->hydrate();
    UIAction ui_action = button_controls->get_deep_sleep_action();
    handleUserInteraction(renderer, ui_action, !hydrate_success);
  } else {
    renderer->reset();
    handleUserInteraction(renderer, NONE, true);
  }

  if (battery) {
    draw_charge_status(renderer, battery);
    draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
  }
  touch_controls->render(renderer);
  renderer->flush_display();
  if (!touch_controls->isTouchEnabled()) {
    touch_controls->powerOffTouch();
  }
  board->sleep_filesystem();
  rt_tick_t last_user_interaction = rt_tick_get_millisecond();
  int last_battery_percent = battery ? battery->get_percentage() : -1;
  bool last_battery_charging = battery ? battery->is_charging() : false;

  while (rt_tick_get_millisecond() - last_user_interaction < 60 * 1000 * 60 * 5)
  {
    if (rt_tick_get_millisecond() - last_user_interaction >= 60 * 1000 * 5 &&
        battery && battery->get_low_power_state() != 1 &&
        strcmp(getCurrentPageName(), "WELCOME_PAGE") != 0 &&
        strcmp(getCurrentPageName(), "CHARGING_PAGE") != 0 &&
        strcmp(getCurrentPageName(), "LOW_POWER_PAGE") != 0) {
      draw_welcome_page(battery);
    }

    uint32_t msg_data;
    if (rt_mq_recv(ui_queue, &msg_data, sizeof(uint32_t),
                   rt_tick_from_millisecond(500)) == RT_EOK)
    {
      UIAction ui_action = (UIAction)msg_data;

      // 充电状态更新
      if (ui_action == MSG_UPDATE_CHARGE_STATUS) {
        if (battery) {
          int percentage = battery->get_percentage();
          if (percentage >= 98 && charge_full == false) {
            clear_charge_icon(renderer);
            renderer->flush_display();
            charge_full = true;
            rt_kprintf("Battery level is full, skip sending charge status update message\n");
          } else if (percentage < 98) {
            rt_kprintf("Charge status changed\n");
            charge_full = false;
            draw_charge_status(renderer, battery);
            draw_battery_level(renderer, battery->get_voltage(), battery->get_percentage());
            renderer->flush_display();
          }
        }
        continue;
      }

      // 电池 UI 消息
      if (ui_action == MSG_DRAW_LOW_POWER_PAGE ||
          ui_action == MSG_DRAW_CHARGE_PAGE ||
          ui_action == MSG_DRAW_WELCOME_PAGE) {
        rt_kprintf("battery msg: %d\n", ui_action);
        switch (ui_action) {
        case MSG_DRAW_LOW_POWER_PAGE:
          rt_kprintf("low_power\n");
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
      } else {
        // ============================================================
        // 普通 UIAction 消息（翻页、选择等）
        // ============================================================
        rt_kprintf("no battery msg: %d\n", msg_data);
        board->wakeup_filesystem();
        if (ui_action != NONE) {
          if (strcmp(getCurrentPageName(), "WELCOME_PAGE") == 0) {
            back_to_main_page();
            last_user_interaction = rt_tick_get_millisecond();
            epd_font_ft_preheat_stop();
            board->sleep_filesystem();
            continue;
          }
          last_user_interaction = rt_tick_get_millisecond();
          touch_controls->renderPressedState(renderer, ui_action);
          handleUserInteraction(renderer, ui_action, false);

          // 用户操作后停止预热线程（可能切换了章节/字体）
          epd_font_ft_preheat_stop();
          // 只有后台排版线程不在跑时才 sleep SD 卡
          // 后台排版线程需要读 SD 卡上的字体文件
          if (!(reader != nullptr && reader->has_pending_layout())) {
            board->sleep_filesystem();
          }
        }
      }
      touch_controls->render(renderer);
      // 用户操作或电池UI消息后，需要立即刷屏
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
        // 后台排版线程在跑，需要读 SD 卡字体文件，保持唤醒
        board->wakeup_filesystem();
      } else if (epd_font_ft_preheat_is_running()) {
        // 字体预热线程在跑，保持唤醒
      } else {
        // 都空闲，sleep SD 卡
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
  draw_shutdown_page();
  board->prepare_to_sleep();
  ulog_i("main", "Entering deep sleep");
  rt_thread_delay(rt_tick_from_millisecond(500));
}

extern "C" {
int main() {
  ulog_i("main", "epub list state num_epubs=%d", epub_list_state.num_epubs);
  ulog_i("main", "epub list state is_loaded=%d", epub_list_state.is_loaded);
  ulog_i("main", "epub list state selected_item=%d", epub_list_state.selected_item);
  ulog_i("main", "Memory before main task start %d", heap_free_size());
  main_task(NULL);
  while (1) {
    rt_thread_delay(1000);
    ulog_i("main", "__main_lopp__");
  }
  return 0;
}
}