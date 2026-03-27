#pragma once

class Epub;
class Renderer;
class RubbishHtmlParser;

#include "./State.h"
#include <rtthread.h>
#include <RubbishHtmlParser/RubbishHtmlParser.h>

class EpubReader
{
private:
  EpubListItem &state;
  Epub *epub = nullptr;
  Renderer *renderer = nullptr;
  RubbishHtmlParser *parser = nullptr;
  int m_layout_batch_size = 1;

  // --- 后台排版线程 ---
  rt_thread_t  m_layout_thread = RT_NULL;
  volatile int m_layout_stop = 0;
  volatile int m_layout_running = 0;

  static void layout_thread_entry(void *param);
  void        layout_thread_func();
  void        stop_layout_thread();
  void        start_layout_thread();

  void parse_and_layout_current_section();
  void update_page_count();

public:
  EpubReader(EpubListItem &state, Renderer *renderer) : state(state), renderer(renderer){};
  ~EpubReader();
  bool load();
  void next();
  void prev();
  void render();
  void set_state_section(uint16_t current_section);
  void preheat(int num_sections = 1);

  bool continue_layout();
  bool has_pending_layout();

  // 保存当前阅读位置的锚点（进入设置前调用）
  void save_anchor(int &out_block_index, int &out_line_index);

  // 字体/排版变更后，根据锚点重新定位页码（排完全部后调用）
  void restore_by_anchor(int block_index, int line_index);
};