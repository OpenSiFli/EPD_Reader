#pragma once

class Epub;
class Renderer;
class RubbishHtmlParser;

#include "./State.h"

class EpubReader
{
private:
  EpubListItem &state;
  Epub *epub = nullptr;
  Renderer *renderer = nullptr;
  RubbishHtmlParser *parser = nullptr;
  // 阅读页半屏覆盖操作层状态
  bool overlay_active = false;
  int overlay_selected = 0; // 0..10，共11个
  int overlay_jump_acc = 0; // 覆盖层累积跳页值（可为负）
  // 覆盖层中心属性模式：触控开关 或 全刷周期
  enum OverlayCenterMode { CENTER_TOUCH = 0, CENTER_FULL_REFRESH = 1 };
  OverlayCenterMode overlay_center_mode = CENTER_TOUCH;
  // 触控开关当前状态（由上层同步）
  bool overlay_touch_enabled = false;
  // 全刷周期索引：0->5, 1->10, 2->20, 3->不刷新(0)
  int overlay_fr_idx = 0;

  void parse_and_layout_current_section();
  void render_overlay();

public:
  EpubReader(EpubListItem &state, Renderer *renderer) : state(state), renderer(renderer){};
  ~EpubReader();
  bool load();
  void next();
  void prev();
  void jump_pages(int delta);
  void render();
  void set_state_section(uint16_t current_section);
  // 覆盖层控制
  void start_overlay() { overlay_active = true; overlay_selected = 0; overlay_jump_acc = 0; }
  void stop_overlay() { overlay_active = false; }
  bool is_overlay_active() const { return overlay_active; }
  void overlay_move_left();
  void overlay_move_right();
  int get_overlay_selected() const { return overlay_selected; }
  // 覆盖层跳页累积控制
  void overlay_add_jump(int d) { overlay_jump_acc += d; }
  void overlay_reset_jump() { overlay_jump_acc = 0; }
  int overlay_get_jump() const { return overlay_jump_acc; }
  // 覆盖层中心属性控制
  void overlay_set_center_mode_touch() { overlay_center_mode = CENTER_TOUCH; }
  void overlay_set_center_mode_full_refresh() { overlay_center_mode = CENTER_FULL_REFRESH; }
  bool overlay_is_center_touch() const { return overlay_center_mode == CENTER_TOUCH; }
  void overlay_set_touch_enabled(bool en) { overlay_touch_enabled = en; }
  bool overlay_get_touch_enabled() const { return overlay_touch_enabled; }
  void overlay_cycle_full_refresh();
  int overlay_get_full_refresh_value() const;
};