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

static const char *TAG = "EREADER";
extern rt_uint32_t heap_free_size(void);


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
    free(html);
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
}

void EpubReader::set_state_section(uint16_t current_section) {
  ulog_i(TAG, "go to section:%d", current_section);
  state.current_section = current_section;
}