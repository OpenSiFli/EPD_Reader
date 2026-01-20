#include "EpubToc.h"

static const char *TAG = "PUBINDEX";
#define PADDING 14
#define ITEMS_PER_PAGE 6

EpubToc::~EpubToc()
{
  if (epub)
  {
    delete epub;
  }
}

void EpubToc::next()
{
  // must be loaded as we need the information from the epub
  if (!epub)
  {
    load();
  }
  state.selected_item = (state.selected_item + 1) % epub->get_toc_items_count();
}

void EpubToc::prev()
{
  // must be loaded as we need the information from the epub
  if (!epub)
  {
    load();
  }
  state.selected_item = (state.selected_item - 1 + epub->get_toc_items_count()) % epub->get_toc_items_count();
}
void EpubToc::switch_book(int target_index)
{
  if (!epub)
  {
    load();
  }
  state.selected_item = target_index % epub->get_toc_items_count();

}

bool EpubToc::load()
{
  ulog_i(TAG, "load");

  if (!epub || epub->get_path() != selected_epub.path)
  {
    renderer->show_busy();
    delete epub;

    epub = new Epub(selected_epub.path);
    if (epub->load())
    {
      ulog_i(TAG, "Epub index loaded");
      return false;
    }
  }
  return true;
}

// TODO - this is currently pretty much a copy of the epub list rendering
// we can fit a lot more on the screen by allowing variable cell heights
// and a lot of the optimisations that are used for the list aren't really
// required as we're not rendering thumbnails
void EpubToc::render()
{
  ulog_d(TAG, "Rendering EPUB index");
  // what page are we on?
  int current_page = state.selected_item / ITEMS_PER_PAGE;
  // 为底部按钮预留区域与底部间距
  const int bottom_area_height = 100;
  const int bottom_margin = 30;
  int cell_height = (renderer->get_page_height() - bottom_area_height - bottom_margin) / ITEMS_PER_PAGE;
  int start_index = current_page * ITEMS_PER_PAGE;
  int ypos = 0;
  // starting a fresh page or rendering from scratch?
  ulog_i(TAG, "Current page is %d, previous page %d, redraw=%d", current_page, state.previous_rendered_page, m_needs_redraw);
  if (current_page != state.previous_rendered_page || m_needs_redraw)
  {
    m_needs_redraw = false;
    renderer->clear_screen();
    state.previous_selected_item = -1;
    // trigger a redraw of the items
    state.previous_rendered_page = -1;
  }
  for (int i = start_index; i < start_index + ITEMS_PER_PAGE && i < epub->get_toc_items_count(); i++)
  {
    // do we need to draw a new page of items?
    if (current_page != state.previous_rendered_page)
    {
      // format the text using a text block
      TextBlock *title_block = new TextBlock(LEFT_ALIGN);
      title_block->add_span(epub->get_toc_item(i).title.c_str(), false, false);
      title_block->layout(renderer, epub, renderer->get_page_width());
      // work out the height of the title
      int text_height = cell_height - PADDING;
      int title_height = title_block->line_breaks.size() * renderer->get_line_height();
      // center the title in the cell
      int y_offset = title_height < text_height ? (text_height - title_height) / 2 : 0;
      // draw each line of the index block making sure we don't run over the cell
      int height = 0;
      for (int i = 0; i < title_block->line_breaks.size() && height < text_height; i++)
      {
        title_block->render(renderer, i, 10, ypos + height + y_offset);
        height += renderer->get_line_height();
      }
      // clean up the temporary index block
      delete title_block;
    }
    // clear the selection box around the previous selected item
    if (state.previous_selected_item == i)
    {
      for (int line = 0; line < 3; line++)
      {
        renderer->draw_rect(line, ypos + PADDING / 2 + line, renderer->get_page_width() - 2 * line, cell_height - PADDING - 2 * line, 255);
      }
    }
    // 目录页：仅在非底部按钮模式时显示列表高亮；底部模式下擦除列表高亮
    if (!m_bottom_mode)
    {
      if (state.selected_item == i)
      {
        for (int line = 0; line < 3; line++)
        {
          renderer->draw_rect(line, ypos + PADDING / 2 + line, renderer->get_page_width() - 2 * line, cell_height - PADDING - 2 * line, 0);
        }
      }
    }
    else
    {
      if (state.selected_item == i)
      {
        for (int line = 0; line < 3; line++)
        {
          renderer->draw_rect(line, ypos + PADDING / 2 + line, renderer->get_page_width() - 2 * line, cell_height - PADDING - 2 * line, 255);
        }
      }
    }
    ypos += cell_height;
  }
  state.previous_selected_item = state.selected_item;
  state.previous_rendered_page = current_page;

  // 绘制底部三按钮区域
  int page_w = renderer->get_page_width();
  int page_h = renderer->get_page_height();
  int area_y = page_h - bottom_area_height - bottom_margin;
  // 背景
  renderer->fill_rect(0, area_y, page_w, bottom_area_height, 255);
  // 三个等宽按钮
  int btn_gap = 10;
  int btn_w = (page_w - btn_gap * 4) / 3; // 左右边距各一个gap，再加中间两个gap
  int btn_h = 80;
  int btn_y = area_y + (bottom_area_height - btn_h) / 2;
  int btn_x0 = btn_gap;                    // 上一页
  int btn_x1 = btn_gap * 2 + btn_w;        // 主页面
  int btn_x2 = btn_gap * 3 + btn_w * 2;    // 下一页

  auto draw_button = [&](int x, const char* text, bool selected)
  {
    if (selected)
    {
      // 多重描边（黑色），与列表选中效果一致
      for (int i = 0; i < 5; ++i)
      {
        renderer->draw_rect(x + i, btn_y + i, btn_w - 2 * i, btn_h - 2 * i, 0);
      }
    }
    else
    {
      // 非选中用细描边（灰色）
      renderer->draw_rect(x, btn_y, btn_w, btn_h, 80);
    }
    int t_w = renderer->get_text_width(text);
    int t_h = renderer->get_line_height();
    int tx = x + (btn_w - t_w) / 2;
    int ty = btn_y + (btn_h - t_h) / 2;
    renderer->draw_text(tx, ty, text, false, true);
  };

  draw_button(btn_x0, "上一页", m_bottom_mode && m_bottom_idx == 0);
  draw_button(btn_x1, "书库", m_bottom_mode && m_bottom_idx == 1);
  draw_button(btn_x2, "下一页", m_bottom_mode && m_bottom_idx == 2);
}

uint16_t EpubToc::get_selected_toc()
{
  return epub->get_spine_index_for_toc_index(state.selected_item);
}