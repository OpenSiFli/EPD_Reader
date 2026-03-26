#include "epd_internals.h"
static const uint8_t  _dummy_bm[] = {0};
static const EpdGlyph _dummy_gl[] = {{0}};
static const EpdUnicodeInterval _dummy_iv[] = {{0x20, 0x7E, 0}};
const EpdFont regular_font = {
    _dummy_bm, _dummy_gl, _dummy_iv,
    1, 0, 20, 16, -4,
};
const EpdFont bold_font = regular_font;
const EpdFont italic_font = regular_font;
const EpdFont bold_italic_font = regular_font;