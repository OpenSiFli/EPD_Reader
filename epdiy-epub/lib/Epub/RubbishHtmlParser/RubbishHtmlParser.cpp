#ifndef UNIT_TEST
#include <rtthread.h>
#include <rtthread.h>
#include <rtdbg.h>
#else
#define rt_thread_delay(t)
#define LOG_E(args...)
#define LOG_I(args...)
#endif
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include <exception>
#include "../ZipFile/ZipFile.h"
#include "../Renderer/Renderer.h"
#include "htmlEntities.h"
#include "blocks/TextBlock.h"
#include "blocks/ImageBlock.h"
#include "Page.h"
#include "RubbishHtmlParser.h"
#include "../EpubList/Epub.h"

static const char *TAG = "HTML";

const char *HEADER_TAGS[] = {"h1", "h2", "h3", "h4", "h5", "h6"};
const int NUM_HEADER_TAGS = sizeof(HEADER_TAGS) / sizeof(HEADER_TAGS[0]);

const char *BLOCK_TAGS[] = {"p", "li", "div", "br"};
const int NUM_BLOCK_TAGS = sizeof(BLOCK_TAGS) / sizeof(BLOCK_TAGS[0]);

const char *BOLD_TAGS[] = {"b"};
const int NUM_BOLD_TAGS = sizeof(BOLD_TAGS) / sizeof(BOLD_TAGS[0]);

const char *ITALIC_TAGS[] = {"i"};
const int NUM_ITALIC_TAGS = sizeof(ITALIC_TAGS) / sizeof(ITALIC_TAGS[0]);

const char *IMAGE_TAGS[] = {"img"};
const int NUM_IMAGE_TAGS = sizeof(IMAGE_TAGS) / sizeof(IMAGE_TAGS[0]);

const char *SKIP_TAGS[] = {"head", "table"};
const int NUM_SKIP_TAGS = sizeof(SKIP_TAGS) / sizeof(SKIP_TAGS[0]);

bool matches(const char *tag_name, const char *possible_tags[], int possible_tag_count)
{
  for (int i = 0; i < possible_tag_count; i++)
  {
    if (strcmp(tag_name, possible_tags[i]) == 0)
    {
      return true;
    }
  }
  return false;
}

RubbishHtmlParser::RubbishHtmlParser(const char *html, int length, const std::string &base_path)
{
  m_base_path = base_path;
  m_layout_done = false;
  m_blocks_layout_done = false;
  m_layout_y = 0;
  m_layout_line_height = 0;
  m_layout_page_height = 0;
  m_layout_renderer = nullptr;
  m_layout_epub = nullptr;
  m_pages_mutex = rt_mutex_create("pg_mtx", RT_IPC_FLAG_PRIO);
  parse(html, length);
}

RubbishHtmlParser::~RubbishHtmlParser()
{
  for (auto block : blocks)
  {
    delete block;
  }

  for (auto page : pages)
  {
    delete page;
  }

  if (m_pages_mutex) {
    rt_mutex_delete(m_pages_mutex);
    m_pages_mutex = RT_NULL;
  }
}

bool RubbishHtmlParser::VisitEnter(const tinyxml2::XMLElement &element, const tinyxml2::XMLAttribute *firstAttribute)
{
  const char *tag_name = element.Name();
  if (matches(tag_name, IMAGE_TAGS, NUM_IMAGE_TAGS))
  {
    const char *src = element.Attribute("src");
    if (src)
    {
      BLOCK_STYLE style = currentTextBlock->get_style();
      if (currentTextBlock->is_empty())
      {
        blocks.pop_back();
        delete currentTextBlock;
      }
      blocks.push_back(new ImageBlock(m_base_path + src));
      currentTextBlock = new TextBlock(style);
      blocks.push_back(currentTextBlock);
    }
  }
  else if (matches(tag_name, SKIP_TAGS, NUM_SKIP_TAGS))
  {
    return false;
  }
  else if (matches(tag_name, HEADER_TAGS, NUM_HEADER_TAGS))
  {
    is_bold = true;
    currentTextBlock = new TextBlock(CENTER_ALIGN);
    blocks.push_back(currentTextBlock);
  }
  else if (matches(tag_name, BLOCK_TAGS, NUM_BLOCK_TAGS) && strcmp(tag_name, "br") != 0)
  {
    BLOCK_STYLE style;
    if (strcmp(tag_name, "li") == 0)
    {
        style = LEFT_ALIGN;
    }
    else
    {
        style = JUSTIFIED;
    }
    currentTextBlock = new TextBlock(style);
    blocks.push_back(currentTextBlock);
  }
  else if (matches(tag_name, BOLD_TAGS, NUM_BOLD_TAGS))
  {
    is_bold = true;
  }
  else if (matches(tag_name, ITALIC_TAGS, NUM_ITALIC_TAGS))
  {
    is_italic = true;
  }
  return true;
}

bool RubbishHtmlParser::Visit(const tinyxml2::XMLText &text)
{
  addText(text.Value(), is_bold, is_italic);
  return true;
}

bool RubbishHtmlParser::VisitExit(const tinyxml2::XMLElement &element)
{
  const char *tag_name = element.Name();
  if (matches(tag_name, HEADER_TAGS, NUM_HEADER_TAGS))
  {
    is_bold = false;
  }
  else if (matches(tag_name, BLOCK_TAGS, NUM_BLOCK_TAGS))
  {
  }
  else if (matches(tag_name, BOLD_TAGS, NUM_BOLD_TAGS))
  {
    is_bold = false;
  }
  else if (matches(tag_name, ITALIC_TAGS, NUM_ITALIC_TAGS))
  {
    is_italic = false;
  }
  return true;
}

void RubbishHtmlParser::startNewTextBlock(BLOCK_STYLE style)
{
  if (currentTextBlock)
  {
    if (currentTextBlock->is_empty())
    {
      currentTextBlock->set_style(style);
      return;
    }
    else
    {
      currentTextBlock->finish();
    }
  }
  currentTextBlock = new TextBlock(style);
  blocks.push_back(currentTextBlock);
}

void RubbishHtmlParser::parse(const char *html, int length)
{
  startNewTextBlock(JUSTIFIED);
  tinyxml2::XMLDocument doc(false, tinyxml2::COLLAPSE_WHITESPACE);
  doc.Parse(html, length);
  doc.Accept(this);
}

void RubbishHtmlParser::addText(const char *text, bool is_bold, bool is_italic)
{
  std::string parsetxt = replace_html_entities((string)text);
  currentTextBlock->add_span(parsetxt.c_str(), is_bold, is_italic);
}

/*==========================================================================
 * layout_one_block
 *========================================================================*/
void RubbishHtmlParser::layout_one_block(Block *block)
{
    // 阶段 1：文字排版（不持锁，耗时操作）
    block->layout(m_layout_renderer, m_layout_epub);
    rt_thread_delay(1);

    // 阶段 2：分页（持锁，快速操作）
    rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);

    if (block->getType() == BlockType::TEXT_BLOCK)
    {
        TextBlock *textBlock = (TextBlock *)block;
        for (int i = 0; i < (int)textBlock->line_breaks.size(); i++)
        {
            // draw_text 实际渲染时 ypos = m_layout_y + line_height + margin_top
            // 所以需要确保 m_layout_y + 2*line_height <= page_height
            // 才能保证文字（含 descent）不超出屏幕底部
            if (m_layout_y + 2 * m_layout_line_height > m_layout_page_height)
            {
                pages.push_back(new Page());
                m_layout_y = 0;
            }
            pages.back()->elements.push_back(new PageLine(textBlock, i, m_layout_y));
            m_layout_y += m_layout_line_height;
        }
        m_layout_y += m_layout_line_height * 0.8;
    }
    else if (block->getType() == BlockType::IMAGE_BLOCK)
    {
        ImageBlock *imageBlock = (ImageBlock *)block;
        if (m_layout_y + imageBlock->height > m_layout_page_height)
        {
            pages.push_back(new Page());
            m_layout_y = 0;
        }
        pages.back()->elements.push_back(new PageImage(imageBlock, m_layout_y));
        m_layout_y += imageBlock->height;
    }

    rt_mutex_release(m_pages_mutex);
}

/*==========================================================================
 * layout
 *========================================================================*/
void RubbishHtmlParser::layout(Renderer *renderer, Epub *epub, int target_pages)
{
    m_layout_renderer = renderer;
    m_layout_epub = epub;
    m_layout_line_height = renderer->get_line_height();
    m_layout_page_height = renderer->get_page_height();
    m_layout_y = 0;
    m_layout_done = false;

    rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);
    pages.push_back(new Page());
    rt_mutex_release(m_pages_mutex);

    m_layout_iter = blocks.begin();

    ulog_i(TAG, "layout start (target_pages=%d)", target_pages);

    while (m_layout_iter != blocks.end())
    {
        layout_one_block(*m_layout_iter);
        ++m_layout_iter;

        if (target_pages > 0 && (int)pages.size() > target_pages)
        {
            ulog_i(TAG, "layout paused: %d pages ready, continuing later",
                   (int)pages.size());
            return;
        }
    }

    m_layout_done = true;
    ulog_i(TAG, "layout done: %d pages total", (int)pages.size());
}

/*==========================================================================
 * layout_continue
 *========================================================================*/
void RubbishHtmlParser::layout_continue(int target_pages)
{
    if (m_layout_done) return;

    int pages_before = (int)pages.size();

    ulog_i(TAG, "layout_continue (have %d pages, target +%d)",
           pages_before, target_pages);

    while (m_layout_iter != blocks.end())
    {
        layout_one_block(*m_layout_iter);
        ++m_layout_iter;

        if (target_pages > 0 && (int)pages.size() > pages_before + target_pages)
        {
            ulog_i(TAG, "layout_continue paused: %d pages now",
                   (int)pages.size());
            return;
        }
    }

    m_layout_done = true;
    ulog_i(TAG, "layout_continue done: %d pages total", (int)pages.size());
}

/*==========================================================================
 * render_page — 持锁渲染
 *========================================================================*/
void RubbishHtmlParser::render_page(int page_index, Renderer *renderer, Epub *epub)
{
  renderer->clear_screen();
  if (renderer->has_gray()) {
    renderer->flush_display();
  }

  rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);

  if(page_index < (int)pages.size())
  {
    pages.at(page_index)->render(renderer, epub);
  }
  else
  {
      rt_mutex_release(m_pages_mutex);
      ulog_i(TAG, "render_page out of range");
      uint16_t y = renderer->get_page_height()/2-20;
      renderer->draw_rect(1, y, renderer->get_page_width(), 105, 125);
      renderer->draw_text_box("Reached the limit of the book\nUse the SELECT button",
                              10, y, renderer->get_page_width(), 80, false, false);
      return;
  }

  rt_mutex_release(m_pages_mutex);
}

/*==========================================================================
 * get_page_anchor — 获取指定页第一个元素的锚点
 *
 * 锚点 = {block 在 blocks 列表中的序号, 该 block 中的行号}
 * 这个信息与字体/字号无关，重新排版后可用于定位同一段文字。
 *========================================================================*/
PageAnchor RubbishHtmlParser::get_page_anchor(int page_index)
{
  PageAnchor anchor = {0, 0};

  rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);

  if (page_index < 0 || page_index >= (int)pages.size() ||
      pages[page_index]->elements.empty())
  {
    rt_mutex_release(m_pages_mutex);
    return anchor;
  }

  PageElement *first_elem = pages[page_index]->elements[0];

  // 判断是 PageLine 还是 PageImage，获取对应的 block 指针
  Block *target_block = nullptr;
  int target_line = -1;

  // 尝试当作 PageLine
  // PageLine 的 block 字段是 TextBlock*，PageImage 的 block 字段是 ImageBlock*
  // 通过遍历 blocks 列表匹配指针来确定序号
  // 先检查是否能匹配 TextBlock
  PageLine *pl = dynamic_cast<PageLine *>(first_elem);
  if (pl) {
    target_block = pl->block;
    target_line = pl->line_break_index;
  } else {
    PageImage *pi = dynamic_cast<PageImage *>(first_elem);
    if (pi) {
      target_block = pi->block;
      target_line = -1;
    }
  }

  rt_mutex_release(m_pages_mutex);

  if (!target_block) return anchor;

  // 在 blocks 列表中找到这个 block 的序号
  int idx = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it, ++idx) {
    if (*it == target_block) {
      anchor.block_index = idx;
      anchor.line_index = target_line;
      return anchor;
    }
  }

  return anchor;
}

/*==========================================================================
 * find_page_by_anchor — 根据锚点查找新排版中对应的页码
 *
 * 扫描所有已排好的页面，找到第一个包含指定 block（或之后的 block）的页。
 * 对于 TextBlock，尽量找到包含 anchor.line_index 对应文本的页
 * （但由于字体变化，行号可能不同，所以匹配到同一个 block 就算成功）。
 *========================================================================*/
int RubbishHtmlParser::find_page_by_anchor(const PageAnchor &anchor)
{
  // 先根据 block_index 找到目标 block 指针
  Block *target_block = nullptr;
  int idx = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it, ++idx) {
    if (idx == anchor.block_index) {
      target_block = *it;
      break;
    }
  }
  if (!target_block) return 0;

  rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);

  // 扫描所有页，找到第一个包含 target_block 的页
  for (int p = 0; p < (int)pages.size(); p++) {
    for (auto elem : pages[p]->elements) {
      PageLine *pl = dynamic_cast<PageLine *>(elem);
      if (pl && pl->block == target_block) {
        rt_mutex_release(m_pages_mutex);
        return p;
      }
      PageImage *pi = dynamic_cast<PageImage *>(elem);
      if (pi && pi->block == target_block) {
        rt_mutex_release(m_pages_mutex);
        return p;
      }
    }
  }

  rt_mutex_release(m_pages_mutex);
  return 0;  // 没找到，回到第 0 页
}