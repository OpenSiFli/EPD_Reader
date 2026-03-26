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
extern "C" void epd_font_ft_preheat(const char *text);
extern "C" void epd_font_ft_preheat_async(const char *text);
extern "C" void epd_font_ft_preheat_stop(void);
static const char *TAG = "EREADER";
extern "C" rt_uint32_t heap_free_size(void);

  EpubReader::~EpubReader() {
      stop_layout_thread();
      epd_font_ft_preheat_stop();
      if(epub) delete epub;
      if(parser) delete parser;
  }

void EpubReader::update_page_count()
{
    if (!parser) return;
    if (parser->is_layout_done()) {
        state.pages_in_current_section = parser->get_page_count();
    } else {
        int count = parser->get_page_count() - 1;
        state.pages_in_current_section = (count < 1) ? 1 : count;
    }
}

/*==========================================================================
 * 后台排版线程
 *========================================================================*/

void EpubReader::layout_thread_entry(void *param)
{
    EpubReader *self = (EpubReader *)param;
    self->layout_thread_func();
}

void EpubReader::layout_thread_func()
{
    m_layout_running = 1;
    ulog_i(TAG, "Layout thread started");

    while (!m_layout_stop && parser && !parser->is_layout_done())
    {
        parser->layout_continue(1);
        update_page_count();

        ulog_d(TAG, "bg layout: %d pages, done=%d",
               state.pages_in_current_section,
               parser->is_layout_done() ? 1 : 0);

        rt_thread_delay(1);
    }

    ulog_i(TAG, "Layout thread exiting: stop=%d, done=%d, pages=%d",
           m_layout_stop,
           (parser && parser->is_layout_done()) ? 1 : 0,
           parser ? parser->get_page_count() : 0);
    m_layout_running = 0;
}

void EpubReader::start_layout_thread()
{
    if (m_layout_running || !parser || parser->is_layout_done()) return;

    m_layout_stop = 0;
    m_layout_thread = rt_thread_create("ly_bg",
                                        layout_thread_entry,
                                        this,
                                        32768,
                                        24,
                                        10);
    if (m_layout_thread) {
        rt_thread_startup(m_layout_thread);
        ulog_i(TAG, "Layout thread created");
    } else {
        ulog_e(TAG, "Failed to create layout thread");
    }
}

void EpubReader::stop_layout_thread()
{
    if (!m_layout_running && m_layout_thread == RT_NULL) return;

    m_layout_stop = 1;

    for (int i = 0; i < 500 && m_layout_running; i++) {
        rt_thread_delay(rt_tick_from_millisecond(10));
    }

    if (m_layout_running) {
        ulog_e(TAG, "Layout thread did not exit in time!");
    }

    m_layout_thread = RT_NULL;
    m_layout_stop = 0;
}

/*==========================================================================
 * 核心逻辑
 *========================================================================*/

bool EpubReader::load()
{
  ulog_d(TAG, "Before epub load: %d", heap_free_size());
  if (!epub || epub->get_path() != state.path)
  {
    stop_layout_thread();
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
    stop_layout_thread();

    renderer->show_busy();
    ulog_i(TAG, "Parse and render section %d", state.current_section);
    ulog_d(TAG, "Before read html: %d", heap_free_size());

    std::string item = epub->get_spine_item(state.current_section);
    std::string base_path = item.substr(0, item.find_last_of('/') + 1);
    char *html = reinterpret_cast<char *>(epub->get_item_contents(item));
    ulog_d(TAG, "After read html: %d", heap_free_size());
    parser = new RubbishHtmlParser(html, strlen(html), base_path);

    epd_font_ft_preheat_async(html);

    epub_mem_free(html);
    ulog_d(TAG, "After parse: %d", heap_free_size());

    m_layout_batch_size = 1;
    parser->layout(renderer, epub, m_layout_batch_size);
    update_page_count();

    ulog_d(TAG, "After initial layout: %d pages ready, done=%d, heap=%d",
           state.pages_in_current_section,
           parser->is_layout_done() ? 1 : 0,
           heap_free_size());

    // 注意：不在这里启动后台线程！
    // 由 render() 在渲染完成后统一启动，
    // 避免 prev() 等需要同步排完全部的场景下出现两个线程同时 layout_continue 的竞态。
  }
}

void EpubReader::next()
{
  state.current_page++;

  if (parser && !parser->is_layout_done() &&
      state.current_page >= state.pages_in_current_section)
  {
    renderer->show_busy();
    while (parser && !parser->is_layout_done() &&
           state.current_page >= state.pages_in_current_section)
    {
      rt_thread_delay(rt_tick_from_millisecond(50));
      update_page_count();
    }

    ulog_i(TAG, "waited for layout: now %d pages, done=%d",
           state.pages_in_current_section,
           parser->is_layout_done() ? 1 : 0);
  }

  if (state.current_page >= state.pages_in_current_section &&
      (!parser || parser->is_layout_done()))
  {
    stop_layout_thread();
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
      stop_layout_thread();
      delete parser;
      parser = nullptr;
      state.current_section--;
      ulog_d(TAG, "Going to previous section %d", state.current_section);

      // 解析并排第 1 页（不启动后台线程）
      parse_and_layout_current_section();

      // 同步排完全部（要知道最后一页是哪页）
      if (!parser->is_layout_done())
      {
        parser->layout_continue(0);
      }
      update_page_count();

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

  // 确保要渲染的页在已排好的范围内
  if (!parser->is_layout_done() &&
      state.current_page >= parser->get_page_count())
  {
    renderer->show_busy();
    while (!parser->is_layout_done() &&
           state.current_page >= parser->get_page_count())
    {
      rt_thread_delay(rt_tick_from_millisecond(50));
    }
    update_page_count();
  }

  ulog_d(TAG, "rendering page %d of %d", state.current_page, parser->get_page_count());
  parser->render_page(state.current_page, renderer, epub);
  ulog_d(TAG, "rendered page %d of %d", state.current_page, parser->get_page_count());
  ulog_d(TAG, "after render: %d", heap_free_size());

  // 渲染完成后启动后台线程排剩余页
  // 这是唯一启动后台线程的地方，确保不会有两个线程同时 layout_continue
  start_layout_thread();
}

void EpubReader::set_state_section(uint16_t current_section) {
  ulog_i(TAG, "go to section:%d", current_section);
  stop_layout_thread();
  state.current_section = current_section;
}

bool EpubReader::continue_layout()
{
    if (!parser || parser->is_layout_done()) return false;
    update_page_count();
    return !parser->is_layout_done();
}

bool EpubReader::has_pending_layout()
{
    return (parser && !parser->is_layout_done()) || m_layout_running;
}

/*==========================================================================
 * 锚点：保存/恢复阅读位置
 *========================================================================*/

void EpubReader::save_anchor(int &out_block_index, int &out_line_index)
{
  out_block_index = 0;
  out_line_index = 0;

  if (!parser) return;

  PageAnchor anchor = parser->get_page_anchor(state.current_page);
  out_block_index = anchor.block_index;
  out_line_index = anchor.line_index;

  ulog_i(TAG, "Anchor saved: page=%d -> block=%d, line=%d",
         state.current_page, out_block_index, out_line_index);
}

void EpubReader::restore_by_anchor(int block_index, int line_index)
{
  if (!parser) return;

  stop_layout_thread();
  if (!parser->is_layout_done()) {
    renderer->show_busy();
    parser->layout_continue(0);
  }
  update_page_count();

  PageAnchor anchor = {block_index, line_index};
  int new_page = parser->find_page_by_anchor(anchor);

  ulog_i(TAG, "Anchor restored: block=%d, line=%d -> new page=%d (total %d)",
         block_index, line_index, new_page, state.pages_in_current_section);

  state.current_page = new_page;

  epd_font_ft_preheat_stop();
  // 不在这里启动后台线程，render() 会启动
}

void EpubReader::preheat(int num_sections)
{
    if (!epub) return;

    int total = epub->get_spine_items_count();
    int start, end;

    if (num_sections < 0) {
        start = 0;
        end = total;
    } else {
        int half = num_sections / 2;
        start = state.current_section - half;
        if (start < 0) start = 0;
        end = start + num_sections;
        if (end > total) end = total;
    }

    ulog_i(TAG, "Preheating font cache: sections %d-%d of %d",
           start, end - 1, total);

    for (int i = start; i < end; i++) {
        std::string item = epub->get_spine_item(i);
        size_t size = 0;
        char *html = reinterpret_cast<char *>(epub->get_item_contents(item, &size));
        if (html && size > 0) {
            epd_font_ft_preheat(html);
            epub_mem_free(html);
        }
    }

    ulog_i(TAG, "Preheat done");
}