/**
 * @file font_ft.h
 * @brief Vector font engine for epdiy-epub reader using FreeType.
 */
#ifndef FONT_FT_H
#define FONT_FT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*==========================================================================
 * TTF font management API
 *========================================================================*/

/**
 * Initialize from in-memory TTF data (e.g. Flash XIP built-in font).
 * The data must remain valid for the lifetime of the engine.
 */
int epd_font_ft_init(const uint8_t *ttf_data, int ttf_size, int pixel_size);

/**
 * Initialize from file path (FreeType native streaming I/O).
 * Large fonts are read directly from TF card without loading into PSRAM.
 */
int epd_font_ft_init_from_file(const char *path, int pixel_size);

/**
 * Initialize using the built-in compiled TTF font (epub_ttf_data in Flash).
 */
int epd_font_ft_init_builtin(int pixel_size);

/**
 * Shut down the font engine and free all cached glyphs.
 */
void epd_font_ft_deinit(void);

/**
 * Change font size at runtime. Invalidates glyph cache.
 */
void epd_font_ft_set_size(int pixel_size);
int  epd_font_ft_get_size(void);

/**
 * Set line spacing multiplier (default 1.2).
 */
void epd_font_ft_set_line_spacing(float spacing);

/**
 * Set bold simulation level: 0=Normal, 1=Medium, 2=Bold.
 * Invalidates glyph cache.
 */
void epd_font_ft_set_bold(int level);
int  epd_font_ft_get_bold(void);

/**
 * Font metrics for current size.
 */
int epd_font_ft_get_ascent(void);
int epd_font_ft_get_descent(void);
int epd_font_ft_get_line_height(void);

/**
 * Preheat glyph cache: rasterize all characters in the given text (blocking).
 */
void epd_font_ft_preheat(const char *text);

/**
 * Async preheat: start a background thread to rasterize all characters
 * in the given text. The text is copied internally, caller can free it
 * after this call returns.
 * Calling this while a previous async preheat is running will stop
 * the previous one first.
 */
void epd_font_ft_preheat_async(const char *text);

/**
 * Stop the background preheat thread if running.
 * Blocks until the thread exits.
 */
void epd_font_ft_preheat_stop(void);

/**
 * Check if background preheat is still running.
 * Returns 1 if running, 0 if idle/done.
 */
int epd_font_ft_preheat_is_running(void);

/*==========================================================================
 * Glyph cache statistics
 *========================================================================*/

int  epd_font_ft_cache_count(void);
int  epd_font_ft_cache_memory(void);
void epd_font_ft_cache_clear(void);

/**
 * Query whether the current font is a variable font with a weight axis.
 * Returns 1 if variable weight is available, 0 otherwise.
 */
int epd_font_ft_is_variable_weight(void);

#ifdef __cplusplus
}
#endif

#endif /* FONT_FT_H */