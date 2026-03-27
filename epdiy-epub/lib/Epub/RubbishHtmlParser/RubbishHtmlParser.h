#pragma once

#include <string>
#include <list>
#include <vector>
#include <tinyxml2.h>
#include "blocks/TextBlock.h"
#include <rtthread.h>

using namespace std;

class Page;
class Renderer;
class Epub;

// 页面锚点：记录某一页第一个元素在 blocks 列表中的位置
typedef struct {
  int block_index;     // 第几个 block（从 0 开始）
  int line_index;      // 该 block 中的第几行（TextBlock）或 -1（ImageBlock）
} PageAnchor;

class RubbishHtmlParser : public tinyxml2::XMLVisitor
{
private:
  bool is_bold = false;
  bool is_italic = false;

  std::list<Block *> blocks;
  TextBlock *currentTextBlock = nullptr;
  std::vector<Page *> pages;

  std::string m_base_path;

  void startNewTextBlock(BLOCK_STYLE style);

  // --- 分段 layout 状态 ---
  std::list<Block *>::iterator m_layout_iter;
  int  m_layout_y;
  int  m_layout_line_height;
  int  m_layout_page_height;
  bool m_layout_done;
  bool m_blocks_layout_done;
  Renderer *m_layout_renderer;
  Epub     *m_layout_epub;

  // pages 互斥锁
  rt_mutex_t m_pages_mutex;

  void layout_one_block(Block *block);

public:
  RubbishHtmlParser(const char *html, int length, const std::string &base_path);
  ~RubbishHtmlParser();

  bool VisitEnter(const tinyxml2::XMLElement &element, const tinyxml2::XMLAttribute *firstAttribute);
  bool Visit(const tinyxml2::XMLText &text);
  bool VisitExit(const tinyxml2::XMLElement &element);

  void parse(const char *html, int length);
  void addText(const char *text, bool is_bold, bool is_italic);

  void layout(Renderer *renderer, Epub *epub, int target_pages = 0);
  void layout_continue(int target_pages = 0);
  bool is_layout_done() { return m_layout_done; }

  int get_page_count()
  {
    rt_mutex_take(m_pages_mutex, RT_WAITING_FOREVER);
    int count = pages.size();
    rt_mutex_release(m_pages_mutex);
    return count;
  }
  const std::list<Block *> &get_blocks()
  {
    return blocks;
  }
  void render_page(int page_index, Renderer *renderer, Epub *epub);

  // 获取指定页面第一个元素的锚点（block 序号 + 行号）
  PageAnchor get_page_anchor(int page_index);

  // 根据锚点查找新排版中对应的页码，返回 -1 表示未找到
  int find_page_by_anchor(const PageAnchor &anchor);
};