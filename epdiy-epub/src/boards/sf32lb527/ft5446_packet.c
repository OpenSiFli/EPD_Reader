/* SPDX-License-Identifier: Apache-2.0
 * FT5x46 protocol: 3 header bytes, then five 6-byte contact records.
 * Original implementation; protocol references are recorded in BOARD_SF32LB527.md.
 */
#include "ft5446_packet.h"
#include <string.h>

int ft5446_decode(const uint8_t *data, size_t size, struct ft_frame *out)
{
    struct ft_frame frame = {0};
    uint16_t seen = 0;
    unsigned i, reported;
    if (!data || !out || size != FT5446_PACKET_SIZE) return -1;
    /* This panel's FW 0x01 leaves report RAM uninitialized until first touch.
     * Recognize only the complete captured sentinel, not arbitrary bad counts. */
    if (data[0]==0) {
        for (i=1; i<size && data[i]==0xff; ++i) {}
        if (i==size) return FT5446_DECODE_NOT_READY;
    }
    if (data[0] & 0x70) return -1; /* Factory/test mode is not a touch report. */
    reported = data[2] & 0x0f;
    if (reported > FT5446_MAX_POINTS) return -1;
    if (reported) {
        for (i=0; i<FT5446_MAX_POINTS; ++i) {
            const uint8_t *p = data + 3 + i*6;
            unsigned id = p[2] >> 4, event = p[0] >> 6;
            if (id == 0x0f) break; /* Unused record sentinel. */
            if (event == 3 || (seen & (1u << id))) return -1;
            seen |= (uint16_t)(1u << id);
            if (event == 1) continue; /* UP is absent from the active set. */
            frame.points[frame.count].id = (uint8_t)id;
            frame.points[frame.count].x = (uint16_t)(((p[0]&15u)<<8) | p[1]);
            frame.points[frame.count].y = (uint16_t)(((p[2]&15u)<<8) | p[3]);
            ++frame.count;
        }
        if (frame.count != reported) return -1;
    }
    /* Zero active contacts may leave stale point bytes in the controller. */
    *out = frame;
    return 0;
}

void ft5446_track(struct ft_tracker *state, const struct ft_frame *frame,
                  ft_emit_fn emit, void *ctx)
{
    uint16_t current = 0;
    unsigned i;
    for (i=0; i<frame->count; ++i) current |= (uint16_t)(1u << frame->points[i].id);
    for (i=0; i<FT5446_IDS; ++i)
        if ((state->active & (1u<<i)) && !(current & (1u<<i)))
            emit(FT_UP, &state->last[i], ctx);
    for (i=0; i<frame->count; ++i) {
        const struct ft_point *p = &frame->points[i];
        const struct ft_point *last = &state->last[p->id];
        if (!(state->active & (1u<<p->id))) emit(FT_DOWN, p, ctx);
        else if (p->x != last->x || p->y != last->y) emit(FT_MOVE, p, ctx);
        state->last[p->id] = *p;
    }
    state->active = current;
}

void ft5446_cancel(struct ft_tracker *state, ft_emit_fn emit, void *ctx)
{
    unsigned i;
    for (i=0; i<FT5446_IDS; ++i)
        if (state->active & (1u<<i)) emit(FT_CANCEL, &state->last[i], ctx);
    memset(state, 0, sizeof(*state));
}

int ft5446_map(const struct ft_point *p, unsigned flags, uint16_t width,
              uint16_t height, uint16_t *x, uint16_t *y)
{
    uint16_t px, py;
    if (!p || !x || !y) return -1;
    px = (flags&1) ? p->y : p->x;
    py = (flags&1) ? p->x : p->y;
    if (!width || !height || flags>7 || px>=width || py>=height) return -1;
    *x = (flags&2) ? width-1-px : px;
    *y = (flags&4) ? height-1-py : py;
    return 0;
}
