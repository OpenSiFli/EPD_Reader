#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "TextBlock.h"
#ifndef UNIT_TEST
#include <rtdbg.h>
#else
#define LOG_I(args...)
#define LOG_E(args...)
#define LOG_D(args...)
#define LOG_W(args...)
#endif

static const char *TAG = "TextBlock";

// TODO - is there any more whitespace we should consider?
static bool is_whitespace(char c)
{
  return (c == ' ' || c == '\r' || c == '\n');
}

// move past anything that should be considered part of a work
static int skip_word(const char *text, int index, int length)
{
  while (index < length && !is_whitespace(text[index]))
  {
    index++;
  }
  return index;
}
 
// skip past any white space characters
static int skip_whitespace(const char *html, int index, int length)
{
  while (index < length && is_whitespace(html[index]))
  {
    index++;
  }
  return index;
}

void TextBlock::add_span(const char *span, bool is_bold, bool is_italic)
{
  // adding a span to text block
  // make a copy of the text as we'll modify it
  uint32_t length = strlen(span);
  char *text = new char[length + 1];
  strcpy(text, span);
  spans.push_back(text);
  span_attr.push_back(length << 8 | (is_bold ? BOLD_SPAN : 0) | (is_italic ? ITALIC_SPAN : 0));
}

// given a renderer works out where to break the words into lines
void TextBlock::layout(Renderer *renderer, Epub *epub, int max_width)
{

  int page_width = max_width != -1 ? max_width : renderer->get_page_width();
  int cur_xpos = 0;

  ulog_i(TAG, "TextBlock::layout page_width=%d, style=%d", page_width,style);

  // measure each word
  for (int i = 0; i < spans.size(); i++)
  {
    uint32_t span_length = span_attr[i] >> 8;
    if(0 == span_length) continue;

    uint8_t span_style  = (uint8_t) span_attr[i] & 0xFF;
    const char *p_span_end =  spans[i] + span_length;
    const char *p_start =  spans[i];
    const char *p_end = NULL;

    



    do{

      words_desc_type desc;
      //Get the words fits to current line

      int width = renderer->get_fixed_width_words(p_start, &p_end, page_width - cur_xpos, span_style & BOLD_SPAN, span_style & ITALIC_SPAN);

      if((p_start == p_end) || (width <= 0))
      {
        ulog_e(TAG, "TextBlock p_start=%x, p_end=%x, span_end=%x, xpos=%d,w=%d", p_start, p_end, p_span_end, cur_xpos, width);
        assert(0);
      }

      if (style == RIGHT_ALIGN)
      {
        cur_xpos = page_width - width;
      }
      if (style == CENTER_ALIGN)
      {
        cur_xpos = (page_width - width) / 2;
      }

      desc.words_start = p_start;
      desc.words_end = p_end;
      desc.words_styles = span_style;
      desc.words_xpos = cur_xpos;
      line_breaks.push_back(desc);

      // move the cursor to the end of the word
      p_start = p_end;
      cur_xpos = 0;
    }while(p_end != p_span_end);
    
  }

  spans.shrink_to_fit();
  line_breaks.shrink_to_fit();
}
void TextBlock::render(Renderer *renderer, int line_break_index, int x_pos, int y_pos)
{
    words_desc_type *p_desc = &line_breaks[line_break_index];
    char backup_char = *p_desc->words_end;
    
    *((char *)p_desc->words_end) = '\0'; //Change the end character to '\0'

       // get the style
    uint8_t style = p_desc->words_styles;
    // render the word
    renderer->draw_text(x_pos + p_desc->words_xpos, y_pos, p_desc->words_start, style & BOLD_SPAN, style & ITALIC_SPAN);

    *((char *)p_desc->words_end) = backup_char; //Restore the end character
}
// debug helper - dumps out the contents of the block with line breaks
void TextBlock::dump()
{
  // for (int i = 0; i < words.size(); i++)
  // {
  //   printf("##%d#%s## ", word_widths[i], words[i]);
  // }
}
