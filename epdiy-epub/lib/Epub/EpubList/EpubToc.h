#pragma once

#include <vector>
#ifndef UNIT_TEST
#include <rtdbg.h>
#else
#define rt_thread_delay(t)
#define LOG_E(args...)
#define LOG_I(args...)
#define LOG_D(args...)
#endif
#include <sys/types.h>
extern "C" {
  #include <dirent.h>
}
#include <string.h>
#include <algorithm>

#include "Epub.h"
#include "Renderer/Renderer.h"
#include "../RubbishHtmlParser/blocks/TextBlock.h"
#include "./State.h"

class Epub;
class Renderer;

class EpubToc
{
private:
  Renderer *renderer;
  Epub *epub = nullptr;
  EpubListItem &selected_epub;
  EpubTocState &state;
  bool m_needs_redraw = false;
  // 底部按钮选择状态：是否处于底部按钮选择模式与当前索引（0:上一页,1:主页面,2:下一页）
  bool m_bottom_mode = false;
  int m_bottom_idx = 1;

public:
  EpubToc(EpubListItem &selected_epub, EpubTocState &state, Renderer *renderer) : renderer(renderer), selected_epub(selected_epub), state(state){};
  ~EpubToc();
  bool load();
  void next();
  void prev();
  void render();
  void set_needs_redraw() { m_needs_redraw = true; }
  uint16_t get_selected_toc();
  // 目录项总数
  int get_items_count() const { return epub ? epub->get_toc_items_count() : 0; }
  // 设置底部按钮选择状态
  void set_bottom_selection(bool enabled, int idx) { m_bottom_mode = enabled; m_bottom_idx = idx; }
};