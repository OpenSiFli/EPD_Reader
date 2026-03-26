/**
 * @file font_ft.c
 * @brief Vector font engine for epdiy-epub reader using FreeType.
 *
 * 线程安全：
 *   rasterize_glyph 通过互斥锁保护所有操作（包括缓存查找），
 *   允许后台预热线程和主线程 layout 安全并发。
 *   互斥锁开销约 1-2us，相比 TF 卡 I/O 的 2-10ms 可忽略。
 */
#include "epd_driver.h"
#include "font_ft.h"
#include "epub_mem.h"
#include "rtdbg.h"
#include <rtthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H

#undef LOG_TAG
#define LOG_TAG "font_ft"

extern rt_uint32_t heap_free_size(void);

/*==========================================================================
 * UTF-8 decoder
 *========================================================================*/
typedef struct { uint8_t mask; uint8_t lead; uint32_t beg; uint32_t end; int bits_stored; } utf_t;
static const utf_t utf_table[] = {
    {0b00111111, 0b10000000, 0,       0,        6},
    {0b01111111, 0b00000000, 0x0000,  0x007F,   7},
    {0b00011111, 0b11000000, 0x0080,  0x07FF,   5},
    {0b00001111, 0b11100000, 0x0800,  0xFFFF,   4},
    {0b00000111, 0b11110000, 0x10000, 0x10FFFF, 3},
};
static int utf8_len(uint8_t ch) {
    int len = 0;
    for (int i = 0; i < 5; i++) { if ((ch & ~utf_table[i].mask) == utf_table[i].lead) break; len++; }
    return len;
}
static uint32_t next_cp(const uint8_t **string) {
    if (**string == 0) return 0;
    int bytes = utf8_len(**string);
    const uint8_t *chr = *string;
    *string += bytes;
    int shift = utf_table[0].bits_stored * (bytes - 1);
    uint32_t codep = (*chr++ & utf_table[bytes].mask) << shift;
    for (int i = 1; i < bytes; i++, chr++) { shift -= utf_table[0].bits_stored; codep |= (*chr & utf_table[0].mask) << shift; }
    return codep;
}

/*==========================================================================
 * Glyph cache
 *========================================================================*/
typedef struct CachedGlyph {
    uint32_t codepoint; uint8_t width; uint8_t height; uint8_t advance_x;
    int16_t left; int16_t top; uint8_t *bitmap_4bpp;
    struct CachedGlyph *hash_next, *lru_prev, *lru_next;
} CachedGlyph;

#define CACHE_HASH_SIZE  256
#define CACHE_HASH_MASK  (CACHE_HASH_SIZE - 1)
#define CACHE_MAX_MEMORY (3 * 1024 * 1024)

static CachedGlyph *g_hash_table[CACHE_HASH_SIZE];
static CachedGlyph  g_lru_sentinel;
static int g_cache_count = 0, g_cache_memory = 0;

static inline uint32_t hash_cp(uint32_t cp) { return (cp ^ (cp >> 8)) & CACHE_HASH_MASK; }
static void lru_remove(CachedGlyph *g) { g->lru_prev->lru_next = g->lru_next; g->lru_next->lru_prev = g->lru_prev; }
static void lru_push_front(CachedGlyph *g) {
    g->lru_next = g_lru_sentinel.lru_next; g->lru_prev = &g_lru_sentinel;
    g_lru_sentinel.lru_next->lru_prev = g; g_lru_sentinel.lru_next = g;
}
static CachedGlyph *cache_lookup(uint32_t cp) {
    uint32_t h = hash_cp(cp); CachedGlyph *g = g_hash_table[h];
    while (g) { if (g->codepoint == cp) { lru_remove(g); lru_push_front(g); return g; } g = g->hash_next; }
    return NULL;
}
static void cache_evict_one(void) {
    CachedGlyph *victim = g_lru_sentinel.lru_prev;
    if (victim == &g_lru_sentinel) return;
    lru_remove(victim);
    uint32_t h = hash_cp(victim->codepoint);
    CachedGlyph **pp = &g_hash_table[h];
    while (*pp) { if (*pp == victim) { *pp = victim->hash_next; break; } pp = &(*pp)->hash_next; }
    int entry_size = sizeof(CachedGlyph);
    if (victim->bitmap_4bpp) { entry_size += ((victim->width + 1) / 2) * victim->height; epub_mem_free(victim->bitmap_4bpp); }
    epub_mem_free(victim); g_cache_count--; g_cache_memory -= entry_size;
}
static void cache_insert(CachedGlyph *g) {
    int bw = (g->width + 1) / 2, es = sizeof(CachedGlyph) + bw * g->height;
    while (g_cache_memory + es > CACHE_MAX_MEMORY && g_cache_count > 0) cache_evict_one();
    uint32_t h = hash_cp(g->codepoint); g->hash_next = g_hash_table[h]; g_hash_table[h] = g;
    lru_push_front(g); g_cache_count++; g_cache_memory += es;
}
static void cache_clear(void) {
    while (g_cache_count > 0) cache_evict_one();
    memset(g_hash_table, 0, sizeof(g_hash_table));
    g_lru_sentinel.lru_next = &g_lru_sentinel; g_lru_sentinel.lru_prev = &g_lru_sentinel;
}

/*==========================================================================
 * FreeType font state + mutex
 *========================================================================*/
static FT_Library g_ft_library = NULL;
static FT_Face    g_ft_face = NULL;
static int        g_font_valid = 0;
static int        g_pixel_size = 32;
static float      g_line_spacing = 1.2f;
static int        g_bold_level = 0;
static int g_ascent = 0, g_descent = 0, g_line_gap = 0;

static rt_mutex_t g_ft_mutex = RT_NULL;

static int  g_has_weight_axis = 0;
static int  g_weight_axis_index = -1;
static long g_weight_min = 100;
static long g_weight_max = 900;
static long g_weight_default = 400;
static int  g_num_axes = 0;
static const long BOLD_WEIGHT_MAP[] = {400, 600, 700};

/*==========================================================================
 * CMap cache
 *========================================================================*/
#define CMAP_CACHE_SIZE  4096
#define CMAP_CACHE_MASK  (CMAP_CACHE_SIZE - 1)
#define CMAP_CACHE_EMPTY 0xFFFFFFFF

typedef struct { uint32_t codepoint; FT_UInt glyph_index; } CMapCacheEntry;
static CMapCacheEntry g_cmap_cache[CMAP_CACHE_SIZE];

static void cmap_cache_clear(void) {
    for (int i = 0; i < CMAP_CACHE_SIZE; i++) g_cmap_cache[i].codepoint = CMAP_CACHE_EMPTY;
}
static FT_UInt cmap_cache_lookup_locked(uint32_t codepoint) {
    uint32_t idx = (codepoint ^ (codepoint >> 12)) & CMAP_CACHE_MASK;
    CMapCacheEntry *e = &g_cmap_cache[idx];
    if (e->codepoint == codepoint) return e->glyph_index;
    FT_UInt gi = FT_Get_Char_Index(g_ft_face, codepoint);
    e->codepoint = codepoint; e->glyph_index = gi;
    return gi;
}

/*==========================================================================
 * Variable font / metrics / init helpers
 *========================================================================*/
static void detect_variable_font(void) {
    g_has_weight_axis = 0; g_weight_axis_index = -1; g_num_axes = 0;
    if (!g_ft_face) return;
    if (!FT_HAS_MULTIPLE_MASTERS(g_ft_face)) return;
    FT_MM_Var *mm_var = NULL;
    if (FT_Get_MM_Var(g_ft_face, &mm_var) != 0 || !mm_var) return;
    g_num_axes = (int)mm_var->num_axis;
    for (unsigned int i = 0; i < mm_var->num_axis; i++) {
        if (mm_var->axis[i].tag == FT_MAKE_TAG('w','g','h','t')) {
            g_has_weight_axis = 1; g_weight_axis_index = (int)i;
            g_weight_min = mm_var->axis[i].minimum >> 16;
            g_weight_max = mm_var->axis[i].maximum >> 16;
            g_weight_default = mm_var->axis[i].def >> 16;
            ulog_i(LOG_TAG, "Variable font: wght axis[%d] min=%ld default=%ld max=%ld",
                   i, g_weight_min, g_weight_default, g_weight_max);
            break;
        }
    }
    FT_Done_MM_Var(g_ft_library, mm_var);
}
static void apply_variable_weight(long wv) {
    if (!g_has_weight_axis || !g_ft_face || g_weight_axis_index < 0) return;
    if (wv < g_weight_min) wv = g_weight_min; if (wv > g_weight_max) wv = g_weight_max;
    FT_Fixed coords[8]; if (g_num_axes > 8) return;
    FT_Get_Var_Design_Coordinates(g_ft_face, g_num_axes, coords);
    coords[g_weight_axis_index] = wv << 16;
    if (FT_Set_Var_Design_Coordinates(g_ft_face, g_num_axes, coords))
        ulog_e(LOG_TAG, "FT_Set_Var_Design_Coordinates failed");
    else ulog_i(LOG_TAG, "Variable weight set to %ld", wv);
}
static void recalc_metrics(void) {
    if (!g_font_valid || !g_ft_face) return;
    FT_Set_Pixel_Sizes(g_ft_face, 0, g_pixel_size);
    g_ascent = (int)(g_ft_face->size->metrics.ascender >> 6);
    g_descent = (int)(g_ft_face->size->metrics.descender >> 6);
    g_line_gap = (int)((g_ft_face->size->metrics.height >> 6) - g_ascent + g_descent);
}
static void ensure_library(void) {
    if (!g_ft_library)
        if (FT_Init_FreeType(&g_ft_library)) { ulog_e(LOG_TAG, "FT_Init failed"); g_ft_library = NULL; }
}
static void ensure_mutex(void) {
    if (g_ft_mutex == RT_NULL) g_ft_mutex = rt_mutex_create("ft_mtx", RT_IPC_FLAG_PRIO);
}
static void post_init(void) {
    detect_variable_font();
    if (g_has_weight_axis && g_bold_level > 0) apply_variable_weight(BOLD_WEIGHT_MAP[g_bold_level]);
    recalc_metrics();
}

/*==========================================================================
 * Public init / deinit / setters
 *========================================================================*/
int epd_font_ft_init(const uint8_t *ttf_data, int ttf_size, int pixel_size) {
    if (!ttf_data || ttf_size <= 0) return -1;
    epd_font_ft_preheat_stop(); ensure_library(); if (!g_ft_library) return -1; ensure_mutex();
    if (g_ft_face) { FT_Done_Face(g_ft_face); g_ft_face = NULL; }
    if (g_font_valid) { cache_clear(); cmap_cache_clear(); g_font_valid = 0; }
    if (FT_New_Memory_Face(g_ft_library, ttf_data, ttf_size, 0, &g_ft_face)) { ulog_e(LOG_TAG, "FT_New_Memory_Face failed"); return -1; }
    g_font_valid = 1; g_pixel_size = pixel_size;
    memset(g_hash_table, 0, sizeof(g_hash_table));
    g_lru_sentinel.lru_next = &g_lru_sentinel; g_lru_sentinel.lru_prev = &g_lru_sentinel;
    g_cache_count = 0; g_cache_memory = 0; cmap_cache_clear(); post_init();
    ulog_i(LOG_TAG, "Font ready (memory): size=%d ascent=%d descent=%d var_weight=%d", g_pixel_size, g_ascent, g_descent, g_has_weight_axis);
    return 0;
}
int epd_font_ft_init_from_file(const char *path, int pixel_size) {
    if (!path) return -1;
    epd_font_ft_preheat_stop(); ensure_library(); if (!g_ft_library) return -1; ensure_mutex();
    if (g_ft_face) { FT_Done_Face(g_ft_face); g_ft_face = NULL; }
    if (g_font_valid) { cache_clear(); cmap_cache_clear(); g_font_valid = 0; }
    if (FT_New_Face(g_ft_library, path, 0, &g_ft_face)) { ulog_e(LOG_TAG, "FT_New_Face failed: %s", path); return -1; }
    g_font_valid = 1; g_pixel_size = pixel_size;
    memset(g_hash_table, 0, sizeof(g_hash_table));
    g_lru_sentinel.lru_next = &g_lru_sentinel; g_lru_sentinel.lru_prev = &g_lru_sentinel;
    g_cache_count = 0; g_cache_memory = 0; cmap_cache_clear(); post_init();
    ulog_i(LOG_TAG, "Font ready (file): size=%d ascent=%d descent=%d var_weight=%d", g_pixel_size, g_ascent, g_descent, g_has_weight_axis);
    return 0;
}
void epd_font_ft_deinit(void) {
    epd_font_ft_preheat_stop(); cache_clear(); cmap_cache_clear();
    if (g_ft_face) { FT_Done_Face(g_ft_face); g_ft_face = NULL; }
    g_font_valid = 0; g_has_weight_axis = 0; g_weight_axis_index = -1; g_num_axes = 0;
}
void epd_font_ft_set_size(int pixel_size) {
    if (pixel_size == g_pixel_size) return;
    epd_font_ft_preheat_stop();
    g_pixel_size = pixel_size; cache_clear(); cmap_cache_clear(); recalc_metrics();
}
int  epd_font_ft_get_size(void) { return g_pixel_size; }
void epd_font_ft_set_line_spacing(float s) { g_line_spacing = s; }
void epd_font_ft_set_bold(int level) {
    if (level < 0) level = 0; if (level > 2) level = 2;
    if (level == g_bold_level) return;
    epd_font_ft_preheat_stop(); g_bold_level = level; cache_clear(); cmap_cache_clear();
    if (g_has_weight_axis) { apply_variable_weight(BOLD_WEIGHT_MAP[level]); recalc_metrics(); }
}
int  epd_font_ft_get_bold(void)           { return g_bold_level; }
int  epd_font_ft_is_variable_weight(void) { return g_has_weight_axis; }
int  epd_font_ft_get_ascent(void)         { return g_ascent; }
int  epd_font_ft_get_descent(void)        { return g_descent; }
int  epd_font_ft_get_line_height(void)    { return (int)((g_ascent - g_descent + g_line_gap) * g_line_spacing); }
int  epd_font_ft_cache_count(void)        { return g_cache_count; }
int  epd_font_ft_cache_memory(void)       { return g_cache_memory; }
void epd_font_ft_cache_clear(void)        { epd_font_ft_preheat_stop(); cache_clear(); cmap_cache_clear(); }

/*==========================================================================
 * rasterize_glyph — 线程安全版本
 *
 * 所有路径都加锁：cache_lookup 修改 LRU 链表指针，不是线程安全的。
 * 互斥锁开销约 1-2us（RT-Thread priority inheritance mutex），
 * 相比 TF 卡 I/O 的 2-10ms 和缓存命中的 memcpy 可忽略不计。
 *========================================================================*/
static CachedGlyph *rasterize_glyph(uint32_t codepoint) {
    if (!g_font_valid || !g_ft_face) return NULL;

    rt_mutex_take(g_ft_mutex, RT_WAITING_FOREVER);

    /* 缓存命中 — 加锁后查找，保护 LRU 链表操作 */
    CachedGlyph *cached = cache_lookup(codepoint);
    if (cached) {
        rt_mutex_release(g_ft_mutex);
        return cached;
    }

    /* 缓存未命中 — FreeType 加载 + 渲染 + 存入缓存 */
    FT_UInt gi = cmap_cache_lookup_locked(codepoint);
    if (gi == 0 && codepoint != '?') gi = cmap_cache_lookup_locked('?');
    if (FT_Load_Glyph(g_ft_face, gi, FT_LOAD_DEFAULT) != 0) {
        rt_mutex_release(g_ft_mutex); return NULL;
    }

    if (g_bold_level > 0 && !g_has_weight_axis &&
        g_ft_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        static const int EMBOLDEN_STRENGTH[] = {0, 1, 2};
        FT_Pos strength = (FT_Pos)(g_ft_face->size->metrics.y_ppem * EMBOLDEN_STRENGTH[g_bold_level]);
        FT_Outline_Embolden(&g_ft_face->glyph->outline, strength);
    }

    if (FT_Render_Glyph(g_ft_face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        rt_mutex_release(g_ft_mutex); return NULL;
    }

    FT_GlyphSlot slot = g_ft_face->glyph;
    int w = slot->bitmap.width, h = slot->bitmap.rows;

    CachedGlyph *g = (CachedGlyph *)epub_mem_malloc(sizeof(CachedGlyph));
    if (!g) {
        rt_kprintf("[FONT] malloc CachedGlyph FAILED cp=0x%04X heap=%d\n", codepoint, heap_free_size());
        rt_mutex_release(g_ft_mutex); return NULL;
    }
    memset(g, 0, sizeof(*g));
    g->codepoint = codepoint; g->width = (w > 255) ? 255 : w; g->height = (h > 255) ? 255 : h;
    g->advance_x = (uint8_t)(slot->advance.x >> 6); g->left = slot->bitmap_left; g->top = slot->bitmap_top;

    if (w > 0 && h > 0) {
        int bw = (w + 1) / 2;
        g->bitmap_4bpp = (uint8_t *)epub_mem_malloc(bw * h);
        if (!g->bitmap_4bpp) {
            rt_kprintf("[FONT] malloc bitmap FAILED cp=0x%04X size=%d heap=%d\n", codepoint, bw*h, heap_free_size());
            epub_mem_free(g); rt_mutex_release(g_ft_mutex); return NULL;
        }
        memset(g->bitmap_4bpp, 0, bw * h);
        for (int y = 0; y < h; y++) {
            const uint8_t *src_row = slot->bitmap.buffer + y * slot->bitmap.pitch;
            for (int x = 0; x < w; x++) {
                uint8_t v4 = src_row[x] >> 4;
                if (x & 1) g->bitmap_4bpp[y*bw + x/2] |= (v4 << 4);
                else       g->bitmap_4bpp[y*bw + x/2]  = v4;
            }
        }
    }
    cache_insert(g);
    rt_mutex_release(g_ft_mutex);
    return g;
}

/*==========================================================================
 * EpdFont compatibility + draw_char + get_char_bounds
 *========================================================================*/
static EpdGlyph g_tmp_glyph;

const EpdGlyph *epd_get_glyph(const EpdFont *font, uint32_t code_point) {
    (void)font;
    CachedGlyph *cg = rasterize_glyph(code_point);
    if (!cg) return NULL;
    g_tmp_glyph.width = cg->width; g_tmp_glyph.height = cg->height;
    g_tmp_glyph.advance_x = cg->advance_x; g_tmp_glyph.left = cg->left; g_tmp_glyph.top = cg->top;
    g_tmp_glyph.compressed_size = 0; g_tmp_glyph.data_offset = 0;
    return &g_tmp_glyph;
}

EpdFontProperties epd_font_properties_default(void) {
    EpdFontProperties props = { .fg_color = 0, .bg_color = 15, .fallback_glyph = 0, .flags = EPD_DRAW_ALIGN_LEFT };
    return props;
}

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }

static enum EpdDrawError draw_char(uint8_t *buffer, int *cursor_x, int cursor_y, uint32_t cp, const EpdFontProperties *props) {
    CachedGlyph *cg = rasterize_glyph(cp);
    if (!cg && props->fallback_glyph) cg = rasterize_glyph(props->fallback_glyph);
    if (!cg) return EPD_DRAW_GLYPH_FALLBACK_FAILED;
    uint8_t width = cg->width, height = cg->height;
    int left = cg->left, byte_width = (width + 1) / 2;
    const uint8_t *bitmap = cg->bitmap_4bpp;
    uint8_t color_lut[16];
    for (int c = 0; c < 16; c++) { int diff = (int)props->fg_color - (int)props->bg_color; color_lut[c] = imax(0, imin(15, props->bg_color + c * diff / 15)); }
    bool bg_needed = props->flags & EPD_DRAW_BACKGROUND;
    if (bitmap) {
        for (int y = 0; y < height; y++) {
            int yy = cursor_y - cg->top + y, start_pos = *cursor_x + left, x = imax(0, -start_pos);
            for (int xx = start_pos + x; xx < start_pos + width; xx++) {
                uint8_t bm = bitmap[y * byte_width + x / 2]; bm = (x & 1) ? (bm >> 4) : (bm & 0xF);
                if (bg_needed || bm) epd_draw_pixel(xx, yy, color_lut[bm] << 4, buffer);
                x++;
            }
        }
    }
    *cursor_x += cg->advance_x;
    return EPD_DRAW_SUCCESS;
}

static void get_char_bounds(uint32_t cp, int *x, int *y, int *minx, int *miny, int *maxx, int *maxy, const EpdFontProperties *props) {
    CachedGlyph *cg = rasterize_glyph(cp);
    if (!cg && props->fallback_glyph) cg = rasterize_glyph(props->fallback_glyph);
    if (!cg) return;
    int x1 = *x + cg->left, y1 = *y + (-(int)cg->top), x2 = x1 + cg->width, y2 = y1 + cg->height;
    if (props->flags & EPD_DRAW_BACKGROUND) {
        *minx = imin(*x, imin(*minx, x1)); *maxx = imax(imax(*x + cg->advance_x, x2), *maxx);
        *miny = imin(*y - g_ascent, imin(*miny, y1)); *maxy = imax(*y - g_descent, imax(*maxy, y2));
    } else {
        if (x1 < *minx) *minx = x1; if (y1 < *miny) *miny = y1;
        if (x2 > *maxx) *maxx = x2; if (y2 > *maxy) *maxy = y2;
    }
    *x += cg->advance_x;
}

/*==========================================================================
 * Public string API
 *========================================================================*/
void epd_get_text_bounds(const EpdFont *font, const char *string, const int *x, const int *y,
                         int *x1, int *y1, int *w, int *h, const EpdFontProperties *properties) {
    (void)font; assert(properties != NULL);
    if (*string == '\0') { *w = 0; *h = 0; *y1 = *y; *x1 = *x; return; }
    int minx = 100000, miny = 100000, maxx = -1, maxy = -1, temp_x = *x, temp_y = *y;
    uint32_t c; while ((c = next_cp((const uint8_t **)&string))) get_char_bounds(c, &temp_x, &temp_y, &minx, &miny, &maxx, &maxy, properties);
    *x1 = imin(*x, minx); *w = maxx - *x1; *y1 = miny; *h = maxy - miny;
}

static bool is_delimiter(uint32_t c) {
    bool sp = (c==' '||c=='\r'||c=='\n')||(c>=0x4e00&&c<=0x9fa5);
    bool pu = (c==0xFF0C)||(c==0xFF1F)||(c==0xFF1A)||(c==0xFF01)||(c==0x3001)||(c==0xFF0E)||(c==0xFF1B)||(c==0x201C)||(c==0x201D)||(c==0x300A)||(c==0x300B)||(c==0x2026)||(c==0x2014);
    return sp||pu;
}

int epd_get_fixed_width_words(const EpdFont *font, const char *string, const char **end_text, unsigned int fixed_width, const EpdFontProperties *properties) {
    (void)font; assert(properties != NULL);
    if (*string == '\0' || fixed_width == 0) { *end_text = string; return 0; }
    char *volatile temp_string = (char *)string; const char *last_word_end = temp_string; int last_word_width = 0;
    int minx = 100000, miny = 100000, maxx = -1, maxy = -1, temp_x = 0, temp_y = 0;
    uint32_t c;
    while ((c = next_cp((const uint8_t **)&temp_string))) {
        get_char_bounds(c, &temp_x, &temp_y, &minx, &miny, &maxx, &maxy, properties);
        if ((maxx - minx) > (int)fixed_width) break;
        if (is_delimiter(c)) { last_word_end = temp_string; last_word_width = maxx - minx; }
    }
    if ((maxx - minx) > (int)fixed_width) {
        if (last_word_width != 0) { *end_text = last_word_end; return last_word_width; }
        else { *end_text = temp_string; return fixed_width; }
    } else { *end_text = temp_string; return maxx - minx; }
}

static char *my_strsep(char **stringp, const char *delim) {
    char *s = *stringp, *p = NULL;
    if (s && *s && (p = strpbrk(s, delim))) *p++ = 0;
    *stringp = p; return s;
}

static enum EpdDrawError epd_write_line(const char *string, int *cursor_x, int *cursor_y, uint8_t *framebuffer, const EpdFontProperties *properties) {
    if (*string == '\0') return EPD_DRAW_SUCCESS;
    assert(properties != NULL);
    EpdFontProperties props = *properties;
    enum EpdFontFlags alignment_mask = EPD_DRAW_ALIGN_LEFT | EPD_DRAW_ALIGN_RIGHT | EPD_DRAW_ALIGN_CENTER;
    enum EpdFontFlags alignment = props.flags & alignment_mask;
    if ((alignment & (alignment - 1)) != 0) return EPD_DRAW_INVALID_FONT_FLAGS;
    int x1=0,y1=0,w=0,h=0,tmp_x=*cursor_x,tmp_y=*cursor_y;
    epd_get_text_bounds(NULL, string, &tmp_x, &tmp_y, &x1, &y1, &w, &h, &props);
    if (w < 0 || h < 0) return EPD_DRAW_NO_DRAWABLE_CHARACTERS;
    int local_x=*cursor_x, local_y=*cursor_y, x_init=local_x, y_init=local_y;
    switch (alignment) { case EPD_DRAW_ALIGN_CENTER: local_x -= w/2; break; case EPD_DRAW_ALIGN_RIGHT: local_x -= w; break; default: break; }
    if (props.flags & EPD_DRAW_BACKGROUND) { uint8_t bg = props.bg_color; for (int l = local_y - g_ascent; l < local_y - g_descent; l++) epd_draw_hline(local_x, l, w, bg << 4, framebuffer); }
    enum EpdDrawError err = EPD_DRAW_SUCCESS; uint32_t c;
    while ((c = next_cp((const uint8_t **)&string))) err |= draw_char(framebuffer, &local_x, local_y, c, &props);
    *cursor_x += local_x - x_init; *cursor_y += local_y - y_init;
    return err;
}

enum EpdDrawError epd_write_default(const EpdFont *font, const char *string, int *cursor_x, int *cursor_y, uint8_t *framebuffer) {
    const EpdFontProperties props = epd_font_properties_default();
    return epd_write_string(font, string, cursor_x, cursor_y, framebuffer, &props);
}

enum EpdDrawError epd_write_string(const EpdFont *font, const char *string, int *cursor_x, int *cursor_y, uint8_t *framebuffer, const EpdFontProperties *properties) {
    (void)font;
    if (string == NULL) { ulog_e(LOG_TAG, "NULL string!"); return EPD_DRAW_STRING_INVALID; }
    char *tofree = rt_strdup(string);
    if (!tofree) { ulog_e(LOG_TAG, "strdup failed!"); return EPD_DRAW_FAILED_ALLOC; }
    char *newstring = tofree; char *token; enum EpdDrawError err = EPD_DRAW_SUCCESS;
    int line_start = *cursor_x, line_h = epd_font_ft_get_line_height();
    while ((token = my_strsep(&newstring, "\n")) != NULL) { *cursor_x = line_start; err |= epd_write_line(token, cursor_x, cursor_y, framebuffer, properties); *cursor_y += line_h; }
    rt_free(tofree);
    return err;
}

/*==========================================================================
 * Built-in font + blocking preheat
 *========================================================================*/
extern const uint8_t epub_ttf_data[];
extern const int epub_ttf_data_size;

int epd_font_ft_init_builtin(int pixel_size) {
    ulog_i(LOG_TAG, "Loading built-in TTF font (%d bytes)", epub_ttf_data_size);
    return epd_font_ft_init(epub_ttf_data, epub_ttf_data_size, pixel_size);
}

void epd_font_ft_preheat(const char *text) {
    if (!text || !g_font_valid) return;
    const uint8_t *p = (const uint8_t *)text;
    uint32_t cp; int total = 0, old_count = g_cache_count;
    while ((cp = next_cp(&p)) != 0) { rasterize_glyph(cp); total++; }
    ulog_i(LOG_TAG, "Preheat: %d chars, %d new glyphs, total=%d (%d KB)",
           total, g_cache_count - old_count, g_cache_count, g_cache_memory / 1024);
}

/*==========================================================================
 * Async preheat — 后台线程扫字填缓存
 *========================================================================*/
static rt_thread_t g_preheat_thread = RT_NULL;
static volatile int g_preheat_stop = 0;
static volatile int g_preheat_running = 0;
static char *g_preheat_text = NULL;

static void preheat_thread_entry(void *param)
{
    (void)param;
    g_preheat_running = 1;
    const uint8_t *p = (const uint8_t *)g_preheat_text;
    uint32_t cp;
    int total = 0, old_count = g_cache_count;

    ulog_i(LOG_TAG, "Async preheat started");

    while ((cp = next_cp(&p)) != 0) {
        if (g_preheat_stop) {
            ulog_i(LOG_TAG, "Async preheat stopped early at %d chars", total);
            break;
        }
        rasterize_glyph(cp);
        total++;

        /* 每 50 个字符让出 CPU，让主线程 layout 优先
         * rt_thread_delay 在锁外（rasterize_glyph 返回后锁已释放） */
        if (total % 50 == 0)
            rt_thread_delay(1);
    }

    ulog_i(LOG_TAG, "Async preheat done: %d chars, %d new glyphs, total=%d (%d KB)",
           total, g_cache_count - old_count, g_cache_count, g_cache_memory / 1024);

    if (g_preheat_text) { rt_free(g_preheat_text); g_preheat_text = NULL; }
    g_preheat_running = 0;
}

void epd_font_ft_preheat_async(const char *text)
{
    if (!text || !g_font_valid) return;
    epd_font_ft_preheat_stop();

    int len = strlen(text);
    g_preheat_text = (char *)rt_malloc(len + 1);
    if (!g_preheat_text) { ulog_e(LOG_TAG, "Async preheat: malloc failed (%d)", len+1); return; }
    memcpy(g_preheat_text, text, len + 1);
    g_preheat_stop = 0;

    g_preheat_thread = rt_thread_create("ft_pre", preheat_thread_entry, RT_NULL,
                                         16384, 25, 10);
    if (g_preheat_thread) {
        rt_thread_startup(g_preheat_thread);
        ulog_i(LOG_TAG, "Async preheat thread started (%d bytes)", len);
    } else {
        ulog_e(LOG_TAG, "Failed to create preheat thread");
        rt_free(g_preheat_text); g_preheat_text = NULL;
    }
}

void epd_font_ft_preheat_stop(void)
{
    if (!g_preheat_running && g_preheat_thread == RT_NULL) return;
    g_preheat_stop = 1;
    if (g_preheat_thread != RT_NULL) {
        for (int i = 0; i < 500 && g_preheat_running; i++)
            rt_thread_delay(rt_tick_from_millisecond(10));
        if (g_preheat_running) ulog_e(LOG_TAG, "Preheat thread did not exit!");
        g_preheat_thread = RT_NULL;
    }
    if (g_preheat_text) { rt_free(g_preheat_text); g_preheat_text = NULL; }
    g_preheat_stop = 0;
}

int epd_font_ft_preheat_is_running(void) { return g_preheat_running; }