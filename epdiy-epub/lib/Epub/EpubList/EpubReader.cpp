#include <string.h>
#ifndef UNIT_TEST
#include <rtdbg.h>
#else
#define LOG_I(args...)
#define LOG_E(args...)
#define LOG_I(args...)
#define LOG_D(args...)
#endif
#include "EpubReader.h"
#include "Epub.h"
#include "../RubbishHtmlParser/RubbishHtmlParser.h"
#include "../Renderer/Renderer.h"
#include "epub_mem.h"
#include "epub_screen.h"
static const char *TAG = "EREADER";
extern "C" rt_uint32_t heap_free_size(void);

  EpubReader::~EpubReader() {
      if(epub) delete epub;
      if(parser) delete parser;
  }

bool EpubReader::load()
{
  ulog_d(TAG, "Before epub load: %d", heap_free_size());
  // do we need to load the epub?
  if (!epub || epub->get_path() != state.path)
  {
    renderer->show_busy();
    delete epub;
    delete parser;
    parser = nullptr;
    epub = new Epub(state.path);
    if (epub->load())
    {
      ulog_d(TAG, "After epub load: %d", heap_free_size());
      return false;
    }
  }
  return true;
}

void EpubReader::parse_and_layout_current_section()
{
  if (!parser)
  {
    renderer->show_busy();
    ulog_i(TAG, "Parse and render section %d", state.current_section);
    ulog_d(TAG, "Before read html: %d", heap_free_size());

    // if spine item is not found here then it will return get_spine_item(0)
    // so it does not crashes when you want to go after last page (out of vector range)
    std::string item = epub->get_spine_item(state.current_section);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    ulog_d(TAG, "After read html: %d", heap_free_size());
    parser = new RubbishHtmlParser(html, strlen(html), base_path);
    epub_mem_free(html);
    ulog_d(TAG, "After parse: %d", heap_free_size());
    // 为底部章节进度预留高度
    int reserved_bottom = renderer->get_line_height() + 10; 
    renderer->set_margin_bottom(reserved_bottom);
    parser->layout(renderer, epub);
    ulog_d(TAG, "After layout: %d", heap_free_size());
    state.pages_in_current_section = parser->get_page_count();
  }
}

void EpubReader::next()
{
  state.current_page++;
  if (state.current_page >= state.pages_in_current_section)
  {
    state.current_section++;
    state.current_page = 0;
    delete parser;
    parser = nullptr;
  }
}

void EpubReader::prev()
{
  if (state.current_page == 0)
  {
    if (state.current_section > 0)
    {
      delete parser;
      parser = nullptr;
      state.current_section--;
      ulog_d(TAG, "Going to previous section %d", state.current_section);
      parse_and_layout_current_section();
      state.current_page = state.pages_in_current_section - 1;
      return;
    }
  }
  state.current_page--;
}

void EpubReader::render()
{
  if (!parser)
  {
    parse_and_layout_current_section();
  }
  ulog_d(TAG, "rendering page %d of %d", state.current_page, parser->get_page_count());
  parser->render_page(state.current_page, renderer, epub);
  ulog_d(TAG, "rendered page %d of %d", state.current_page, parser->get_page_count());
  ulog_d(TAG, "after render: %d", heap_free_size());
  // 章节进度
  if (state.pages_in_current_section > 0) 
  {
    char buf[32];
    rt_snprintf(buf, sizeof(buf), "%d页/%d页", state.current_page + 1, state.pages_in_current_section);
    int page_w = renderer->get_page_width();
    int page_h = renderer->get_page_height();
    int text_w = renderer->get_text_width(buf);
    int text_h = renderer->get_line_height();
    int x = (page_w - text_w) / 2;
    int reserved_bottom = renderer->get_line_height() + 4; 
    const int progress_up = 6; // 上抬
    int y = page_h - text_h - 10 + reserved_bottom - progress_up;
    renderer->draw_text(x, y, buf, false, true);
  }
  // 绘制半屏覆盖操作层
  if (overlay_active)
  {
    render_overlay();
  }
}

void EpubReader::set_state_section(uint16_t current_section) {
  ulog_i(TAG, "go to section:%d", current_section);
  state.current_section = current_section;
}

void EpubReader::render_overlay()
{
  int page_w = renderer->get_page_width();
  int page_h = renderer->get_page_height();
  int area_y = (page_h * 2) / 3;    // 覆盖下方 1/3 屏幕
  int area_h = page_h - area_y;
  
  renderer->fill_rect(0, area_y, page_w, area_h, 240);

  // 三行布局：3,5,3
  const int rows = 3;
  const int cols[rows] = {3, 5, 3};
  const int gap_h = 20; // 行间距
  const int gap_w = 10;
  const int row_h = 80; // 每行高度
  // 纵向居中放置三行
  int content_h = rows * row_h + (rows + 1) * gap_h;
  int y0 = area_y + (area_h - content_h) / 2;
  if (y0 < area_y + 4) y0 = area_y + 4;

  int index = 0;
  auto fill_label = [&](int idx, char *label, size_t cap) {
    switch (idx)
    {
      case 0: rt_snprintf(label, cap, "<"); break;
      case 1:
      {
        if (overlay_center_mode == CENTER_TOUCH)
        {
          rt_snprintf(label, cap, "触摸开关：%s", overlay_touch_enabled ? "开" : "关");
        }
        else
        {
          int v = overlay_get_full_refresh_value();
          if (v == 0)
            rt_snprintf(label, cap, "全刷周期：每次");
          else
            rt_snprintf(label, cap, "全刷周期：%d次", v);
        }
        break;
      }
      case 2: rt_snprintf(label, cap, ">"); break;
      case 3: rt_snprintf(label, cap, "-5"); break;
      case 4: rt_snprintf(label, cap, "-1"); break;
      case 5: rt_snprintf(label, cap, "%d", overlay_jump_acc); break;
      case 6: rt_snprintf(label, cap, "+1"); break;
      case 7: rt_snprintf(label, cap, "+5"); break;
      case 8: rt_snprintf(label, cap, "确认"); break;
      case 9: rt_snprintf(label, cap, "目录"); break;
      case 10: rt_snprintf(label, cap, "书库"); break;
      default: label[0] = '\0'; break;
    }
  };
  for (int r = 0; r < rows; ++r)
  {
    int c = cols[r];
    int y = y0 + gap_h + r * (row_h + gap_h);
    // 顶部第1行(3列)采用不等宽布局：1/3半宽，2双宽
    if (r == 0)
    {
      int usable_w = page_w - (c + 1) * gap_w;
      // 宽度权重为 1:3:1（约 左20% / 中60% / 右20%）
      int w0 = (usable_w * 1) / 5;
      int w1 = (usable_w * 3) / 5;
      int w2 = usable_w - w0 - w1; 
      int widths[3] = { w0, w1, w2 };
      int cur_x = gap_w;
      for (int i = 0; i < c; ++i)
      {
        int w = widths[i];
        int x = cur_x;
        bool selected = (index == overlay_selected);
        if (selected)
        {
          for (int k = 0; k < 5; ++k)
          {
            renderer->draw_rect(x + k, y + k, w - 2 * k, row_h - 2 * k, 0);
          }
        }
        else
        {
          renderer->draw_rect(x, y, w, row_h, 80);
        }
        char label[32];
        fill_label(index, label, sizeof(label));
        int t_w = renderer->get_text_width(label);
        int t_h = renderer->get_line_height();
        int tx = x + (w - t_w) / 2;
        int ty = y + (row_h - t_h) / 2;
        renderer->draw_text(tx, ty, label, false, true);
        index++;
        cur_x = x + w + gap_w;
      }
    }
    else
    {
      int usable_w = page_w - (c + 1) * gap_w;
      int btn_w = usable_w / c;
      for (int i = 0; i < c; ++i)
      {
        int x = gap_w + i * (btn_w + gap_w);
      bool selected = (index == overlay_selected);
      if (selected)
      {
        for (int k = 0; k < 5; ++k)
        {
          renderer->draw_rect(x + k, y + k, btn_w - 2 * k, row_h - 2 * k, 0);
        }
      }
      else
      {
        renderer->draw_rect(x, y, btn_w, row_h, 80);
      }
        char label[32];
        fill_label(index, label, sizeof(label));
      int t_w = renderer->get_text_width(label);
      int t_h = renderer->get_line_height();
      int tx = x + (btn_w - t_w) / 2;
      int ty = y + (row_h - t_h) / 2;
      renderer->draw_text(tx, ty, label, false, true);
      index++;
      }
    }
  }
}

void EpubReader::overlay_move_left()
{
  if (!overlay_active) return;
  overlay_selected = (overlay_selected - 1 + 11) % 11;
}

void EpubReader::overlay_move_right()
{
  if (!overlay_active) return;
  overlay_selected = (overlay_selected + 1) % 11;
}

void EpubReader::jump_pages(int delta)
{
  if (delta == 0) return;
  if (!parser) //没解析的情况下 则解析当前节
  {
    parse_and_layout_current_section();
  }
  int spine_count = epub ? epub->get_spine_items_count() : 0; //获取章节总数
  if (spine_count <= 0) return;

  //检查是不是第一页
  auto at_book_start = [&]() -> bool 
  {
    return state.current_section == 0 && state.current_page == 0;
  };
  //检查是不是最后一页
  auto at_book_end = [&]() -> bool 
  {
    if (!parser) return false;
    return (state.current_section == spine_count - 1) && (state.current_page >= state.pages_in_current_section - 1);
  };
  // 开始实现页面跳转
  if (delta > 0)
  {
    for (int i = 0; i < delta; ++i)
    {
      if (at_book_end()) break;
      next();
      // 如果跨节，parser 在 next() 时会置空；后续渲染时会自动 parse
      if (!parser)
      {
        parse_and_layout_current_section();
      }
    }
  }
  else // delta < 0
  {
    for (int i = 0; i < -delta; ++i)
    {
      if (at_book_start()) break;
      prev();
      if (!parser)
      {
        //空就解析
        parse_and_layout_current_section();
      }
    }
  }
}

void EpubReader::overlay_cycle_full_refresh()
{
  screen_cycle_full_refresh_period();
}

int EpubReader::overlay_get_full_refresh_value() const
{
  return screen_get_full_refresh_period();
}