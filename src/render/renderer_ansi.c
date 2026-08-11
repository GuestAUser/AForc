/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_internal.h"

typedef struct AnsiEncodingState
{
    AFORC_Cell active_style;
    bool style_known;
    bool ascii_width;
} AnsiEncodingState;

static bool ansi_styles_equal(AFORC_Cell left, AFORC_Cell right)
{
    return left.style == right.style &&
           aforc_renderer_colors_equal(left.foreground, right.foreground) &&
           aforc_renderer_colors_equal(left.background, right.background);
}

static bool ansi_cell_cost(AFORC_Cell cell,
                           AnsiEncodingState *state,
                           bool include_codepoint,
                           size_t *cost)
{
    if (!state->style_known || !ansi_styles_equal(cell, state->active_style))
    {
        if (!aforc_size_add(*cost, aforc_renderer_ansi_style_size(cell), cost))
        {
            return false;
        }
        state->active_style = cell;
        state->style_known = true;
    }
    return !include_codepoint ||
           aforc_size_add(
               *cost, aforc_renderer_ansi_codepoint_size(cell.codepoint), cost);
}

static bool ansi_cell_needs_output(const AFORC_Renderer *renderer,
                                   size_t index,
                                   AFORC_Cell cleared)
{
    const AFORC_Cell previous =
        renderer->invalidated ? cleared : renderer->front[index];

    return !aforc_renderer_cells_equal(previous, renderer->back[index]);
}

static int32_t ansi_next_changed(const AFORC_Renderer *renderer,
                                 int32_t row,
                                 int32_t column,
                                 AFORC_Cell cleared)
{
    for (; column < renderer->size.width; ++column)
    {
        const size_t index =
            (size_t)row * (size_t)renderer->size.width + (size_t)column;

        if (ansi_cell_needs_output(renderer, index, cleared))
        {
            return column;
        }
    }
    return renderer->size.width;
}

static size_t ansi_ascii_run_length(const AFORC_Renderer *renderer,
                                    int32_t row,
                                    int32_t column,
                                    AFORC_Cell cleared,
                                    AFORC_Cell active_style)
{
    size_t count = 0U;

    while (column < renderer->size.width)
    {
        const size_t index =
            (size_t)row * (size_t)renderer->size.width + (size_t)column;
        const AFORC_Cell cell = renderer->back[index];

        if (!ansi_cell_needs_output(renderer, index, cleared) ||
            !aforc_renderer_cell_is_valid(cell) ||
            cell.codepoint > (uint32_t)'~' ||
            !ansi_styles_equal(cell, active_style))
        {
            break;
        }
        ++count;
        ++column;
    }
    return count;
}

static bool ansi_gap_is_cheaper(const AFORC_Renderer *renderer,
                                int32_t row,
                                int32_t gap_start,
                                int32_t next_changed,
                                AnsiEncodingState state)
{
    const size_t next_index =
        (size_t)row * (size_t)renderer->size.width + (size_t)next_changed;
    const AFORC_Cell next_cell = renderer->back[next_index];
    size_t bridge_cost = 0u;
    size_t restart_cost = aforc_renderer_ansi_cursor_size(
        (uint32_t)row + 1u, (uint32_t)next_changed + 1u);

    if (!state.ascii_width || !aforc_renderer_cell_is_valid(next_cell) ||
        !aforc_size_add(restart_cost,
                        aforc_renderer_ansi_style_size(next_cell),
                        &restart_cost))
    {
        return false;
    }
    for (int32_t column = gap_start; column < next_changed; ++column)
    {
        const size_t index =
            (size_t)row * (size_t)renderer->size.width + (size_t)column;
        const AFORC_Cell cell = renderer->back[index];

        if (!aforc_renderer_cell_is_valid(cell) ||
            cell.codepoint > (uint32_t)'~' ||
            !ansi_cell_cost(cell, &state, true, &bridge_cost) ||
            bridge_cost >= restart_cost)
        {
            return false;
        }
    }
    return ansi_cell_cost(next_cell, &state, false, &bridge_cost) &&
           bridge_cost < restart_cost;
}

AFORC_Status aforc_renderer_build_ansi(AFORC_Renderer *renderer)
{
    const AFORC_Cell cleared = aforc_cell_default();
    bool emitted = false;
    AnsiEncodingState encoding = {cleared, false, true};
    AFORC_Status status = AFORC_OK;

    renderer->batch_size = 0u;
    if (renderer->invalidated)
    {
        status = aforc_renderer_ansi_literal(renderer, "\x1b[0m\x1b[2J\x1b[H");
    }
    for (int32_t row = 0; status == AFORC_OK && row < renderer->size.height;
         ++row)
    {
        int32_t column = 0;

        while (status == AFORC_OK && column < renderer->size.width)
        {
            size_t index =
                (size_t)row * (size_t)renderer->size.width + (size_t)column;

            if (!ansi_cell_needs_output(renderer, index, cleared))
            {
                ++column;
                continue;
            }
            status = aforc_renderer_ansi_cursor(
                renderer, (uint32_t)row + 1u, (uint32_t)column + 1u);
            encoding.style_known = false;
            encoding.ascii_width = true;
            {
                int32_t bridge_until = column;

                while (status == AFORC_OK && column < renderer->size.width)
                {
                    index = (size_t)row * (size_t)renderer->size.width +
                            (size_t)column;
                    if (!ansi_cell_needs_output(renderer, index, cleared) &&
                        column >= bridge_until)
                    {
                        const int32_t next_changed = ansi_next_changed(
                            renderer, row, column + 1, cleared);

                        if (next_changed == renderer->size.width ||
                            !ansi_gap_is_cheaper(
                                renderer, row, column, next_changed, encoding))
                        {
                            break;
                        }
                        bridge_until = next_changed;
                    }
                    if (!aforc_renderer_cell_is_valid(renderer->back[index]))
                    {
                        return AFORC_ERROR_FORMAT;
                    }
                    if (!encoding.style_known ||
                        !ansi_styles_equal(renderer->back[index],
                                           encoding.active_style))
                    {
                        status = aforc_renderer_ansi_style(
                            renderer, renderer->back[index]);
                        encoding.active_style = renderer->back[index];
                        encoding.style_known = true;
                    }
                    if (status == AFORC_OK &&
                        ansi_cell_needs_output(renderer, index, cleared))
                    {
                        const size_t run_length =
                            ansi_ascii_run_length(renderer,
                                                  row,
                                                  column,
                                                  cleared,
                                                  encoding.active_style);

                        if (run_length > 1U)
                        {
                            status = aforc_renderer_ansi_ascii_cells(
                                renderer, &renderer->back[index], run_length);
                            emitted = true;
                            column += (int32_t)run_length;
                            continue;
                        }
                    }
                    if (status == AFORC_OK)
                    {
                        status = aforc_renderer_ansi_codepoint(
                            renderer, renderer->back[index].codepoint);
                    }
                    if (renderer->back[index].codepoint > (uint32_t)'~')
                    {
                        encoding.ascii_width = false;
                    }
                    emitted = true;
                    ++column;
                    if (!encoding.ascii_width)
                    {
                        break;
                    }
                }
            }
        }
    }
    if (status == AFORC_OK && emitted)
    {
        status = aforc_renderer_ansi_literal(renderer, "\x1b[0m");
    }
    return status;
}
