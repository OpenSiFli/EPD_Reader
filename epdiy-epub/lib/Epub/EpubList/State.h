#pragma once

#include <stdint.h>

const int MAX_EPUB_LIST_SIZE = 20;
const int MAX_PATH_SIZE = 256;
const int MAX_TITLE_SIZE = 100;

// nice and simple state that can be persisted easily
typedef struct
{
  char path[MAX_PATH_SIZE];//存储 EPUB 文件的路径
  char title[MAX_TITLE_SIZE];//存储 EPUB 文件的标题
  uint16_t current_section;//记录当前阅读的章节编号（从0开始）
  uint16_t current_page;//记录当前章节内的页码（从0开始）
  uint16_t pages_in_current_section;//记录当前章节总共有多少页
} EpubListItem;

// this is held in the RTC memory
typedef struct
{
  int previous_rendered_page;
  int previous_selected_item;
  int selected_item;
  int num_epubs;
  bool is_loaded;
  EpubListItem epub_list[MAX_EPUB_LIST_SIZE];
} EpubListState;

// this is held in the RTC memory
typedef struct
{
  int previous_rendered_page;//记录当前选中的目录项索引
  int previous_selected_item;//记录上一次选中的目录项索引
  int selected_item;//记录上次渲染的页面索引
} EpubTocState;
