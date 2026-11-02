/* SPDX-License-Identifier: Apache-2.0 */
#ifndef FT5446_PACKET_H
#define FT5446_PACKET_H
#include <stdint.h>
#include <stddef.h>
#define FT5446_MAX_POINTS 5
#define FT5446_PACKET_SIZE (3 + 6 * FT5446_MAX_POINTS)
#define FT5446_IDS 15
#define FT5446_NATIVE_WIDTH 684
#define FT5446_NATIVE_HEIGHT 1216
#define FT5446_DECODE_NOT_READY 1
enum ft_event { FT_DOWN, FT_MOVE, FT_UP, FT_CANCEL };
struct ft_point { uint16_t x, y; uint8_t id; };
struct ft_frame { uint8_t count; struct ft_point points[FT5446_MAX_POINTS]; };
struct ft_tracker { uint16_t active; struct ft_point last[FT5446_IDS]; };
typedef void (*ft_emit_fn)(enum ft_event, const struct ft_point *, void *);
/* Decode a complete report starting at register 0. Negative = malformed;
 * NOT_READY = the exact observed startup sentinel (00 followed by 32 FF).
 * Only a zero return changes out. */
int ft5446_decode(const uint8_t *data, size_t size, struct ft_frame *out);
void ft5446_track(struct ft_tracker *, const struct ft_frame *, ft_emit_fn, void *);
void ft5446_cancel(struct ft_tracker *, ft_emit_fn, void *);
/* flags: bit 0 swap XY, bit 1 invert X, bit 2 invert Y; no silent clamping. */
int ft5446_map(const struct ft_point *, unsigned flags, uint16_t width,
              uint16_t height, uint16_t *x, uint16_t *y);
#endif
