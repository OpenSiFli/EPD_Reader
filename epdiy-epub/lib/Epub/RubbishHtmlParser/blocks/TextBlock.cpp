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

typedef struct {
  uint8_t mask;    /* char data will be bitwise AND with this */
  uint8_t lead;    /* start bytes of current char in utf-8 encoded character */
  uint32_t beg;    /* beginning of codepoint range */
  uint32_t end;    /* end of codepoint range */
  int bits_stored; /* the number of bits from the codepoint that fits in char */
} utf_t;
/*
 * UTF-8 decode inspired from rosetta code
 * https://rosettacode.org/wiki/UTF-8_encode_and_decode#C
 */
static const utf_t utf[] = {
    /*             mask        lead        beg      end       bits */
    {0b00111111, 0b10000000, 0, 0, 6},
    {0b01111111, 0b00000000, 0000, 0177, 7},
    {0b00011111, 0b11000000, 0200, 03777, 5},
    {0b00001111, 0b11100000, 04000, 0177777, 4},
    {0b00000111, 0b11110000, 0200000, 04177777, 3},
};

static int utf8_len(const char ch)
{


	int len = 0;
	for(int i = 0; i < sizeof(utf); ++i) {
		if((ch & ~utf[i].mask) == utf[i].lead) {
			break;
		}
		++len;
	}
	if(len > 4) { /* Malformed leading byte */
		assert("invalid unicode.");
	}
	return len;
}

// TODO - is there any more whitespace we should consider?
static bool is_whitespace(char c)
{
  return (c == ' ' || c == '\r' || c == '\n');
}

// move past anything that should be considered part of a word
static const char *skip_word(const char *start, const char *end)
{
  const char *cursor = start;
  while (cursor < end && !is_whitespace(*cursor))
  {
    int len = utf8_len(*cursor);

    if(len > 1) //Anything longer than 1 byte is considered as a word
    {
      cursor += len; 
      break;
    }
    else
    {
      cursor++;
    }
  }

  //Skip white space
  while (cursor < end && is_whitespace(*cursor))
  {
    cursor++;
  }
  return cursor;
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

static bool has_chinese_char(const std::vector<const char*>& spans)
{
    if (spans.empty()) return false;

    for (const char* span : spans) {
        if (!span) continue;

        const char* p = span;
        while (*p != '\0') {
            int char_len = utf8_len(*p);
            if (char_len == 3) {
                if (*p >= 0xE4 && *p <= 0xE9) { 
                    return true;
                }
            }
            // Move to the next character
            p += (char_len > 0) ? char_len : 1;
        }
    }
    return false; // No Chinese, return false
}

static bool has_leading_indent(const std::vector<const char*>& spans) {
    if (spans.empty()) return false;

    // Find the first non-empty span
    for (const char* span : spans) {
        if (!span || *span == '\0') continue;

        const char* p = span;
        int space_count = 0;  // Half-width space count
        int full_space_count = 0;  // Full-width space count

        // Full-width space UTF-8 encoding: 0xE3 0x80 0x80
        while (*p != '\0') {
            if (*p == ' ') {
                space_count++;
                p++;
            } else if ((unsigned char)*p == 0xE3 && 
                       (unsigned char)*(p+1) == 0x80 && 
                       (unsigned char)*(p+2) == 0x80) {
                full_space_count++;
                p += 3;
            } else {
                break;  // Stop counting when a non-space character is encountered
            }
        }

        if (full_space_count >= 2 || space_count >= 4) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}
// given a renderer works out where to break the words into lines
void TextBlock::layout(Renderer *renderer, Epub *epub, int max_width)
{
    int page_width = max_width != -1 ? max_width : renderer->get_page_width();
    bool is_first_line = true; // first line of a paragraph
    int indent_width = 0;
    bool has_chinese = has_chinese_char(spans);
    bool has_leading = has_leading_indent(spans);
    bool is_chinese_justified = (style == JUSTIFIED) && has_chinese && !has_leading; 

    const int extra_spacing = 16;  
    if (is_chinese_justified)
    {
        const char* chinese_char = "字";
        const char* temp_end;
        int single_char_width = renderer->get_fixed_width_words(
            chinese_char, &temp_end, page_width, false, false
        );
        if (temp_end == chinese_char + strlen(chinese_char) && single_char_width > 0)
        {
            indent_width = single_char_width * 2 + extra_spacing;
        }
    }
    else {
        indent_width = 0;
    }

    for (int i = 0; i < spans.size(); i++)
    {
        uint32_t span_length = span_attr[i] >> 8;
        if (span_length == 0) continue;

        uint8_t span_style = (uint8_t)span_attr[i] & 0xFF;
        const char *p_start = spans[i];
        const char *p_span_end = spans[i] + span_length;
        const char *p_end = nullptr;

        do {
            line_desc_type desc;
            int current_max_width = page_width;

            if (is_first_line && is_chinese_justified )
            {
                current_max_width = page_width - indent_width;
                current_max_width = current_max_width > 0 ? current_max_width : 1;
            }

            int width = renderer->get_fixed_width_words(p_start, &p_end, current_max_width, 
                                                        span_style & BOLD_SPAN, span_style & ITALIC_SPAN);

            if (p_start == p_end || width < 0)
            {
                ulog_e(TAG, "TextBlock layout error: invalid width");
                break;
            }

            if (style == RIGHT_ALIGN)
            {
                desc.words_xpos = page_width - width;
            }
            else if (style == CENTER_ALIGN)
            {
                desc.words_xpos = (page_width - width) / 2;
            }
            else
            {
                desc.words_xpos = (is_first_line && is_chinese_justified) ? indent_width : 0;
            }

            desc.words_start = p_start;
            desc.words_end = p_end;
            desc.words_styles = span_style;

            bool need_spare = (style == JUSTIFIED && p_end != p_span_end);
            desc.spare_width = need_spare ? (current_max_width - width) : 0;

            line_breaks.push_back(desc);

            if (is_first_line)
            {
                is_first_line = false;
            }

            p_start = p_end;
        } while (p_end != p_span_end);
    }

    spans.shrink_to_fit();
    line_breaks.shrink_to_fit();
}
void TextBlock::render(Renderer *renderer, int line_break_index, int x_pos, int y_pos)
{
    line_desc_type *p_desc = &line_breaks[line_break_index];
    char backup_char;
    // get the style
    uint8_t style = p_desc->words_styles;
    
    if(p_desc->spare_width > 0)
    {
        //Calculate the words space_width
        int words_count = 0;
        const char *p = p_desc->words_start;
        while (p < p_desc->words_end)
        {
          p = skip_word(p, p_desc->words_end);
          words_count++;
        }
        float space_width = (float)p_desc->spare_width / (float)(words_count - 1);

        // render the words with justified spacing
        x_pos += p_desc->words_xpos;
        p = p_desc->words_start;
        while (p < p_desc->words_end)
        {
          int offset = 0;
          char *p_end = (char *)skip_word(p, p_desc->words_end);
          backup_char = *p_end;
          *p_end = '\0'; //Change the end character to '\0'
          offset = renderer->draw_text2(x_pos, y_pos, p, style & BOLD_SPAN, style & ITALIC_SPAN);
          *p_end = backup_char; //Restore
          p = p_end;
          x_pos += offset + space_width; //Add the space width to the next word
        }
    }
    else
    {
      backup_char = *p_desc->words_end;
      *((char *)p_desc->words_end) = '\0'; //Change the end character to '\0'
      // render the word
      renderer->draw_text(x_pos + p_desc->words_xpos, y_pos, p_desc->words_start, style & BOLD_SPAN, style & ITALIC_SPAN);
      *((char *)p_desc->words_end) = backup_char; //Restore the end character
    }
}
// debug helper - dumps out the contents of the block with line breaks
void TextBlock::dump()
{
  // for (int i = 0; i < words.size(); i++)
  // {
  //   printf("##%d#%s## ", word_widths[i], words[i]);
  // }
}
