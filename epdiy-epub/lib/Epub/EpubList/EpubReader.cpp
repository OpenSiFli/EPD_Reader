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
  // 半透明效果不可用，使用浅灰底区分
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
  for (int r = 0; r < rows; ++r)
  {
    int c = cols[r];
    int usable_w = page_w - (c + 1) * gap_w;
    int btn_w = usable_w / c;
    int y = y0 + gap_h + r * (row_h + gap_h);
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
      // 文本：第9个显示"确认"，其余显示编号
      char label[16];
      if (index == 8)
      {
        rt_snprintf(label, sizeof(label), "确认");
      }
      else
      {
        rt_snprintf(label, sizeof(label), "%d", index + 1);
      }
      int t_w = renderer->get_text_width(label);
      int t_h = renderer->get_line_height();
      int tx = x + (btn_w - t_w) / 2;
      int ty = y + (row_h - t_h) / 2;
      renderer->draw_text(tx, ty, label, false, true);
      index++;
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