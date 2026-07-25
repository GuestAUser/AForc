/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_RENDER_RENDERER_INTERNAL_H
#define AFORC_RENDER_RENDERER_INTERNAL_H

#include "../core/common_internal.h"
#include "aforc/renderer.h"

struct AFORC_Renderer {
    AFORC_Allocator allocator;
    AFORC_Size size;
    AFORC_Cell *front;
    AFORC_Cell *back;
    char *batch;
    size_t batch_size;
    size_t batch_capacity;
    bool invalidated;
};

static inline bool aforc_renderer_color_is_valid(AFORC_Color color)
{
    return color.mode >= AFORC_COLOR_DEFAULT && color.mode <= AFORC_COLOR_RGB;
}

static inline bool aforc_renderer_cell_is_valid(AFORC_Cell cell)
{
    const AFORC_CellStyle known_styles =
        AFORC_STYLE_BOLD | AFORC_STYLE_DIM | AFORC_STYLE_ITALIC |
        AFORC_STYLE_UNDERLINE | AFORC_STYLE_BLINK | AFORC_STYLE_REVERSE |
        AFORC_STYLE_HIDDEN | AFORC_STYLE_STRIKETHROUGH;
    const bool scalar = cell.codepoint <= UINT32_C(0x10ffff) &&
                        !(cell.codepoint >= UINT32_C(0xd800) &&
                          cell.codepoint <= UINT32_C(0xdfff));
    const bool printable = cell.codepoint >= UINT32_C(0x20) &&
                           !(cell.codepoint >= UINT32_C(0x7f) &&
                             cell.codepoint < UINT32_C(0xa0));

    return scalar && printable &&
           aforc_renderer_color_is_valid(cell.foreground) &&
           aforc_renderer_color_is_valid(cell.background) &&
           (cell.style & (AFORC_CellStyle)~known_styles) == 0u;
}

static inline AFORC_Status aforc_renderer_cell_count(AFORC_Size size,
                                                  size_t *out_count)
{
    size_t count = 0u;

    if (out_count == NULL || size.width <= 0 || size.height <= 0) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_size_multiply((size_t)size.width,
                           (size_t)size.height,
                           &count) ||
        count > SIZE_MAX / sizeof(AFORC_Cell)) {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_count = count;
    return AFORC_OK;
}

static inline void aforc_renderer_fill_cells(AFORC_Cell *cells,
                                            size_t count,
                                            AFORC_Cell cell)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        cells[index] = cell;
    }
}

static inline bool aforc_renderer_colors_equal(AFORC_Color left,
                                              AFORC_Color right)
{
    if (left.mode != right.mode) {
        return false;
    }
    if (left.mode == AFORC_COLOR_DEFAULT) {
        return true;
    }
    if (left.mode == AFORC_COLOR_INDEXED) {
        return left.red == right.red;
    }
    return left.red == right.red && left.green == right.green &&
           left.blue == right.blue;
}

static inline bool aforc_renderer_cells_equal(AFORC_Cell left, AFORC_Cell right)
{
    return left.codepoint == right.codepoint && left.style == right.style &&
           aforc_renderer_colors_equal(left.foreground, right.foreground) &&
           aforc_renderer_colors_equal(left.background, right.background);
}

AFORC_INTERNAL AFORC_Status aforc_renderer_build_ansi(AFORC_Renderer *renderer);
AFORC_INTERNAL AFORC_Status aforc_renderer_ansi_literal(
    AFORC_Renderer *renderer,
    const char *literal);
AFORC_INTERNAL AFORC_Status aforc_renderer_ansi_cursor(
    AFORC_Renderer *renderer,
    uint32_t row,
    uint32_t column);
AFORC_INTERNAL AFORC_Status aforc_renderer_ansi_style(
    AFORC_Renderer *renderer,
    AFORC_Cell cell);
AFORC_INTERNAL AFORC_Status aforc_renderer_ansi_codepoint(
    AFORC_Renderer *renderer,
    uint32_t codepoint);
AFORC_INTERNAL size_t aforc_renderer_ansi_cursor_size(uint32_t row,
                                                      uint32_t column);
AFORC_INTERNAL size_t aforc_renderer_ansi_style_size(AFORC_Cell cell);
AFORC_INTERNAL size_t aforc_renderer_ansi_codepoint_size(uint32_t codepoint);

#endif
