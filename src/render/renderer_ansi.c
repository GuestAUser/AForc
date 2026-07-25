/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_internal.h"

#include <string.h>

/* Owns the reusable ANSI batch and front/back diff encoding. renderer.c alone
 * commits front after a successful terminal write. */

static AFORC_Status ansi_reserve(AFORC_Renderer *renderer, size_t additional)
{
    size_t required = 0u;
    size_t capacity;
    void *replacement = NULL;
    AFORC_Status status;

    /* Batch construction is transactional with respect to the displayed
     * frame: capacity may grow here, but front-buffer state is not committed
     * until renderer.c completes the terminal write. Geometric growth keeps
     * amortized append cost linear while checked arithmetic protects sizes. */
    if (!aforc_size_add(renderer->batch_size, additional, &required)) {
        return AFORC_ERROR_OVERFLOW;
    }
    if (required <= renderer->batch_capacity) {
        return AFORC_OK;
    }
    capacity = renderer->batch_capacity == 0u ? 4096u
                                               : renderer->batch_capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = required;
            break;
        }
        capacity *= 2u;
    }
    if (renderer->batch == NULL) {
        status = aforc_alloc_array(&renderer->allocator,
                                 capacity,
                                 sizeof(*renderer->batch),
                                 &replacement);
    } else {
        status = aforc_realloc_array(&renderer->allocator,
                                   renderer->batch,
                                   capacity,
                                   sizeof(*renderer->batch),
                                   &replacement);
    }
    if (status != AFORC_OK) {
        return status;
    }
    renderer->batch = (char *)replacement;
    renderer->batch_capacity = capacity;
    return AFORC_OK;
}

static AFORC_Status ansi_append(AFORC_Renderer *renderer,
                              const void *data,
                              size_t size)
{
    AFORC_Status status = ansi_reserve(renderer, size);

    if (status != AFORC_OK) {
        return status;
    }
    if (size > 0u) {
        (void)memcpy(renderer->batch + renderer->batch_size, data, size);
        renderer->batch_size += size;
    }
    return AFORC_OK;
}

static AFORC_Status ansi_literal(AFORC_Renderer *renderer, const char *literal)
{
    return ansi_append(renderer, literal, strlen(literal));
}

static AFORC_Status ansi_byte(AFORC_Renderer *renderer, unsigned char byte)
{
    return ansi_append(renderer, &byte, 1u);
}

static AFORC_Status ansi_u32(AFORC_Renderer *renderer, uint32_t value)
{
    char digits[10];
    size_t count = 0u;
    size_t index;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    for (index = 0u; index < count / 2u; ++index) {
        const char temporary = digits[index];
        digits[index] = digits[count - index - 1u];
        digits[count - index - 1u] = temporary;
    }
    return ansi_append(renderer, digits, count);
}

static AFORC_Status ansi_parameter(AFORC_Renderer *renderer, uint32_t value)
{
    const AFORC_Status status = ansi_byte(renderer, (unsigned char)';');
    return status == AFORC_OK ? ansi_u32(renderer, value) : status;
}

static AFORC_Status ansi_cursor(AFORC_Renderer *renderer,
                              uint32_t row,
                              uint32_t column)
{
    AFORC_Status status = ansi_literal(renderer, "\x1b[");

    if (status == AFORC_OK) {
        status = ansi_u32(renderer, row);
    }
    if (status == AFORC_OK) {
        status = ansi_byte(renderer, (unsigned char)';');
    }
    if (status == AFORC_OK) {
        status = ansi_u32(renderer, column);
    }
    return status == AFORC_OK ? ansi_byte(renderer, (unsigned char)'H')
                            : status;
}

static AFORC_Status ansi_color(AFORC_Renderer *renderer,
                             AFORC_Color color,
                             bool foreground)
{
    AFORC_Status status = AFORC_OK;

    if (color.mode == AFORC_COLOR_DEFAULT) {
        return ansi_parameter(renderer, foreground ? 39u : 49u);
    }
    status = ansi_parameter(renderer, foreground ? 38u : 48u);
    if (status != AFORC_OK) {
        return status;
    }
    if (color.mode == AFORC_COLOR_INDEXED) {
        status = ansi_parameter(renderer, 5u);
        return status == AFORC_OK ? ansi_parameter(renderer, color.red)
                                : status;
    }
    status = ansi_parameter(renderer, 2u);
    if (status == AFORC_OK) {
        status = ansi_parameter(renderer, color.red);
    }
    if (status == AFORC_OK) {
        status = ansi_parameter(renderer, color.green);
    }
    return status == AFORC_OK ? ansi_parameter(renderer, color.blue) : status;
}

static AFORC_Status ansi_style_flag(AFORC_Renderer *renderer,
                                  AFORC_CellStyle style,
                                  AFORC_CellStyle flag,
                                  uint32_t code)
{
    return (style & flag) == 0u ? AFORC_OK : ansi_parameter(renderer, code);
}

static AFORC_Status ansi_style(AFORC_Renderer *renderer, AFORC_Cell cell)
{
    AFORC_Status status = ansi_literal(renderer, "\x1b[0");

    /* Begin with SGR reset rather than trying to emit inverse deltas between
     * arbitrary style combinations. This costs a few bytes per changed run,
     * but prevents attributes from leaking across skipped unchanged cells. */
#define AFORC_RENDERER_STYLE(flag, code)                                         \
    do {                                                                         \
        if (status == AFORC_OK) {                                                \
            status = ansi_style_flag(renderer, cell.style, flag, code);          \
        }                                                                        \
    } while (false)
    AFORC_RENDERER_STYLE(AFORC_STYLE_BOLD, 1u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_DIM, 2u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_ITALIC, 3u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_UNDERLINE, 4u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_BLINK, 5u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_REVERSE, 7u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_HIDDEN, 8u);
    AFORC_RENDERER_STYLE(AFORC_STYLE_STRIKETHROUGH, 9u);
#undef AFORC_RENDERER_STYLE
    if (status == AFORC_OK) {
        status = ansi_color(renderer, cell.foreground, true);
    }
    if (status == AFORC_OK) {
        status = ansi_color(renderer, cell.background, false);
    }
    return status == AFORC_OK ? ansi_byte(renderer, (unsigned char)'m')
                            : status;
}

static AFORC_Status ansi_codepoint(AFORC_Renderer *renderer, uint32_t codepoint)
{
    unsigned char bytes[4];
    size_t count;

    if (codepoint <= UINT32_C(0x7f)) {
        bytes[0] = (unsigned char)codepoint;
        count = 1u;
    } else if (codepoint <= UINT32_C(0x7ff)) {
        bytes[0] = (unsigned char)(0xc0u | (codepoint >> 6u));
        bytes[1] = (unsigned char)(0x80u | (codepoint & 0x3fu));
        count = 2u;
    } else if (codepoint <= UINT32_C(0xffff)) {
        bytes[0] = (unsigned char)(0xe0u | (codepoint >> 12u));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        bytes[2] = (unsigned char)(0x80u | (codepoint & 0x3fu));
        count = 3u;
    } else {
        bytes[0] = (unsigned char)(0xf0u | (codepoint >> 18u));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 12u) & 0x3fu));
        bytes[2] = (unsigned char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        bytes[3] = (unsigned char)(0x80u | (codepoint & 0x3fu));
        count = 4u;
    }
    return ansi_append(renderer, bytes, count);
}

typedef struct AnsiEncodingState {
    AFORC_Cell active_style;
    bool style_known;
    bool ascii_width;
} AnsiEncodingState;

static size_t ansi_u32_size(uint32_t value)
{
    size_t size = 1u;

    while (value >= 10u) {
        value /= 10u;
        ++size;
    }
    return size;
}

static size_t ansi_parameter_size(uint32_t value)
{
    return 1u + ansi_u32_size(value);
}

static size_t ansi_cursor_size(uint32_t row, uint32_t column)
{
    return 4u + ansi_u32_size(row) + ansi_u32_size(column);
}

static size_t ansi_color_size(AFORC_Color color, bool foreground)
{
    size_t size;

    if (color.mode == AFORC_COLOR_DEFAULT) {
        return ansi_parameter_size(foreground ? 39u : 49u);
    }
    size = ansi_parameter_size(foreground ? 38u : 48u);
    if (color.mode == AFORC_COLOR_INDEXED) {
        return size + ansi_parameter_size(5u) +
               ansi_parameter_size(color.red);
    }
    return size + ansi_parameter_size(2u) +
           ansi_parameter_size(color.red) +
           ansi_parameter_size(color.green) +
           ansi_parameter_size(color.blue);
}

static size_t ansi_style_size(AFORC_Cell cell)
{
    size_t size = 4u;

    if ((cell.style & AFORC_STYLE_BOLD) != 0u) {
        size += ansi_parameter_size(1u);
    }
    if ((cell.style & AFORC_STYLE_DIM) != 0u) {
        size += ansi_parameter_size(2u);
    }
    if ((cell.style & AFORC_STYLE_ITALIC) != 0u) {
        size += ansi_parameter_size(3u);
    }
    if ((cell.style & AFORC_STYLE_UNDERLINE) != 0u) {
        size += ansi_parameter_size(4u);
    }
    if ((cell.style & AFORC_STYLE_BLINK) != 0u) {
        size += ansi_parameter_size(5u);
    }
    if ((cell.style & AFORC_STYLE_REVERSE) != 0u) {
        size += ansi_parameter_size(7u);
    }
    if ((cell.style & AFORC_STYLE_HIDDEN) != 0u) {
        size += ansi_parameter_size(8u);
    }
    if ((cell.style & AFORC_STYLE_STRIKETHROUGH) != 0u) {
        size += ansi_parameter_size(9u);
    }
    return size + ansi_color_size(cell.foreground, true) +
           ansi_color_size(cell.background, false);
}

static size_t ansi_codepoint_size(uint32_t codepoint)
{
    if (codepoint <= UINT32_C(0x7f)) {
        return 1u;
    }
    if (codepoint <= UINT32_C(0x7ff)) {
        return 2u;
    }
    if (codepoint <= UINT32_C(0xffff)) {
        return 3u;
    }
    return 4u;
}

static bool ansi_styles_equal(AFORC_Cell left, AFORC_Cell right)
{
    return left.style == right.style &&
           aforc_renderer_colors_equal(left.foreground, right.foreground) &&
           aforc_renderer_colors_equal(left.background, right.background);
}

static bool ansi_cost_add(size_t *cost, size_t additional)
{
    if (additional > SIZE_MAX - *cost) {
        return false;
    }
    *cost += additional;
    return true;
}

static bool ansi_cell_cost(AFORC_Cell cell,
                           AnsiEncodingState *state,
                           bool include_codepoint,
                           size_t *cost)
{
    if (!state->style_known ||
        !ansi_styles_equal(cell, state->active_style)) {
        if (!ansi_cost_add(cost, ansi_style_size(cell))) {
            return false;
        }
        state->active_style = cell;
        state->style_known = true;
    }
    return !include_codepoint ||
           ansi_cost_add(cost, ansi_codepoint_size(cell.codepoint));
}

static int32_t ansi_next_changed(const AFORC_Renderer *renderer,
                                 int32_t row,
                                 int32_t column)
{
    for (; column < renderer->size.width; ++column) {
        const size_t index = (size_t)row * (size_t)renderer->size.width +
                             (size_t)column;

        if (!aforc_renderer_cells_equal(renderer->front[index],
                                        renderer->back[index])) {
            return column;
        }
    }
    return renderer->size.width;
}

static bool ansi_gap_is_cheaper(const AFORC_Renderer *renderer,
                                int32_t row,
                                int32_t gap_start,
                                int32_t next_changed,
                                AnsiEncodingState state)
{
    size_t bridge_cost = 0u;
    size_t restart_cost;

    if (!state.ascii_width) {
        return false;
    }
    for (int32_t column = gap_start; column < next_changed; ++column) {
        const size_t index = (size_t)row * (size_t)renderer->size.width +
                             (size_t)column;
        const AFORC_Cell cell = renderer->back[index];

        if (!aforc_renderer_cell_is_valid(cell) ||
            cell.codepoint > (uint32_t)'~' ||
            !ansi_cell_cost(cell, &state, true, &bridge_cost)) {
            return false;
        }
    }
    {
        const size_t index = (size_t)row * (size_t)renderer->size.width +
                             (size_t)next_changed;
        const AFORC_Cell cell = renderer->back[index];

        if (!aforc_renderer_cell_is_valid(cell) ||
            !ansi_cell_cost(cell, &state, false, &bridge_cost)) {
            return false;
        }
        restart_cost = ansi_cursor_size((uint32_t)row + 1u,
                                        (uint32_t)next_changed + 1u);
        if (!ansi_cost_add(&restart_cost, ansi_style_size(cell))) {
            return false;
        }
    }
    return bridge_cost < restart_cost;
}

AFORC_Status aforc_renderer_build_ansi(AFORC_Renderer *renderer)
{
    int32_t row;
    int32_t column;
    bool emitted = false;
    AnsiEncodingState encoding = {aforc_cell_default(), false, true};
    AFORC_Status status = AFORC_OK;

    /* Independent runs remain self-contained. Short unchanged ASCII gaps join
     * a run only when their exact encoded cells cost less than restarting with
     * an absolute cursor and full style. */
    renderer->batch_size = 0u;
    if (renderer->invalidated) {
        status = ansi_literal(renderer, "\x1b[0m\x1b[2J\x1b[H");
    }
    for (row = 0; status == AFORC_OK && row < renderer->size.height; ++row) {
        column = 0;
        while (status == AFORC_OK && column < renderer->size.width) {
            size_t index = (size_t)row * (size_t)renderer->size.width +
                           (size_t)column;

            if (!renderer->invalidated &&
                aforc_renderer_cells_equal(renderer->front[index],
                                          renderer->back[index])) {
                ++column;
                continue;
            }
            status = ansi_cursor(renderer,
                                  (uint32_t)row + 1u,
                                  (uint32_t)column + 1u);
            encoding.style_known = false;
            encoding.ascii_width = true;
            {
                int32_t bridge_until = column;

                while (status == AFORC_OK &&
                       column < renderer->size.width) {
                    index = (size_t)row * (size_t)renderer->size.width +
                            (size_t)column;
                    if (!renderer->invalidated &&
                        aforc_renderer_cells_equal(renderer->front[index],
                                                   renderer->back[index])) {
                        if (column >= bridge_until) {
                            const int32_t next_changed = ansi_next_changed(
                                renderer, row, column + 1);

                            if (next_changed == renderer->size.width ||
                                !ansi_gap_is_cheaper(renderer, row, column,
                                                     next_changed, encoding)) {
                                break;
                            }
                            bridge_until = next_changed;
                        }
                    }
                    if (!aforc_renderer_cell_is_valid(
                            renderer->back[index])) {
                        return AFORC_ERROR_FORMAT;
                    }
                    if (!encoding.style_known ||
                        !ansi_styles_equal(renderer->back[index],
                                           encoding.active_style)) {
                        status = ansi_style(renderer, renderer->back[index]);
                        encoding.active_style = renderer->back[index];
                        encoding.style_known = true;
                    }
                    if (status == AFORC_OK) {
                        status = ansi_codepoint(
                            renderer, renderer->back[index].codepoint);
                    }
                    if (renderer->back[index].codepoint > (uint32_t)'~') {
                        encoding.ascii_width = false;
                    }
                    emitted = true;
                    ++column;
                }
            }
        }
    }
    if (status == AFORC_OK && emitted) {
        status = ansi_literal(renderer, "\x1b[0m");
    }
    return status;
}
