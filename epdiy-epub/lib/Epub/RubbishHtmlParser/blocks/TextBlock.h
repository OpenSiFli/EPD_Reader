#pragma once

#include "../../Renderer/Renderer.h"
#include <vector>
#include "Block.h"

typedef enum
{
  BOLD_SPAN = 1,
  ITALIC_SPAN = 2,
} SPAN_STYLE;

typedef enum
{
  JUSTIFIED = 0,
  LEFT_ALIGN = 1,
  CENTER_ALIGN = 2,
  RIGHT_ALIGN = 3,
} BLOCK_STYLE;

typedef struct
{
   const char *words_start;
   const char *words_end;
   uint16_t words_xpos;
   uint8_t words_styles;
}words_desc_type;



// represents a block of words in the html document
class TextBlock : public Block
{
private:
  // the spans of text in this block
  std::vector<const char *> spans;
  // the attributes of spans
  std::vector<uint32_t> span_attr;


  // the style of the block - left, center, right aligned
  BLOCK_STYLE style;

public:
  // where do we want to break the words into lines
  std::vector<words_desc_type> line_breaks;

  void add_span(const char *span, bool is_bold, bool is_italic);
  TextBlock(BLOCK_STYLE style) : style(style)
  {
  }
  ~TextBlock()
  {
    for (auto span : spans)
    {
      delete[] span;
    }
  }
  void set_style(BLOCK_STYLE style)
  {
    this->style = style;
  }
  BLOCK_STYLE get_style()
  {
    return style;
  }
  bool isEmpty()
  {
    return spans.empty();
  }
  // given a renderer works out where to break the words into lines
  void layout(Renderer *renderer, Epub *epub, int max_width = -1);
  void render(Renderer *renderer, int line_break_index, int x_pos, int y_pos);
  // debug helper - dumps out the contents of the block with line breaks
  void dump();
  bool is_empty()
  {
    return line_breaks.empty();
  }
  virtual BlockType getType()
  {
    return TEXT_BLOCK;
  }
};
