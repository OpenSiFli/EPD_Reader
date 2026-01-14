#pragma once

#include <vector>
#include <sys/types.h>
extern "C" {
  #include <dirent.h>
}
#include <string.h>
#include <algorithm>
#include "Epub.h"
#include "Renderer/Renderer.h"
#include "../RubbishHtmlParser/blocks/TextBlock.h"
#include "../RubbishHtmlParser/htmlEntities.h"
#include "./State.h"

#ifndef UNIT_TEST
  #include <rtthread.h>
  #include <rtthread.h>
#endif
#include <warning.h>
#include "boards/controls/TouchControls.h"
class Epub;
class Renderer;



class EpubList
{
private:
  Renderer *renderer;
  EpubListState &state;
  bool m_needs_redraw = false;
   TouchControls* touch_controls = nullptr;  
  // 底部按钮选择状态：是否处于底部按钮选择模式与当前索引（0:上一页,1:主页面,2:下一页）
  bool m_bottom_mode = false;
  int m_bottom_idx = 1;
public:
  EpubList(Renderer *renderer, EpubListState &state) : renderer(renderer), state(state){};
  void setTouchControls(TouchControls* controls) { touch_controls = controls; }
  // 设置底部按钮选择状态
  void set_bottom_selection(bool enabled, int idx) { m_bottom_mode = enabled; m_bottom_idx = idx; }
  bool is_bottom_mode() const { return m_bottom_mode; }
  int bottom_index() const { return m_bottom_idx; }
  ~EpubList() {}
  bool load(const char *path);
  void set_needs_redraw() { m_needs_redraw = true; }
  void next();
  void prev();
  void render();
};