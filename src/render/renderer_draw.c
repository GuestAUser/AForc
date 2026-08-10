/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_internal.h"

#include <limits.h>

/* Owns allocation-free, clipped back-buffer mutation. It cannot publish a
 * frame or alter the front buffer used by diff generation. */

AFORC_Status aforc_renderer_clear(AFORC_Renderer *renderer, AFORC_Cell cell)
{
    size_t count = 0u;
    AFORC_Status status;

    if (renderer == NULL || !aforc_renderer_cell_is_valid(cell))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_renderer_cell_count(renderer->size, &count);
    if (status != AFORC_OK)
    {
        return status;
    }
    aforc_renderer_fill_cells(renderer->back, count, cell);
    return AFORC_OK;
}

AFORC_Status aforc_renderer_put(AFORC_Renderer *renderer,
                                AFORC_Point position,
                                AFORC_Cell cell)
{
    size_t index;

    if (renderer == NULL || !aforc_renderer_cell_is_valid(cell))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (position.x < 0 || position.y < 0 ||
        position.x >= renderer->size.width ||
        position.y >= renderer->size.height)
    {
        return AFORC_OK;
    }
    index =
        (size_t)position.y * (size_t)renderer->size.width + (size_t)position.x;
    renderer->back[index] = cell;
    return AFORC_OK;
}

AFORC_Status aforc_renderer_get(const AFORC_Renderer *renderer,
                                AFORC_Point position,
                                AFORC_Cell *out_cell)
{
    size_t index;

    if (renderer == NULL || out_cell == NULL || position.x < 0 ||
        position.y < 0 || position.x >= renderer->size.width ||
        position.y >= renderer->size.height)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    index =
        (size_t)position.y * (size_t)renderer->size.width + (size_t)position.x;
    *out_cell = renderer->back[index];
    return AFORC_OK;
}

AFORC_Status aforc_renderer_fill_rect(AFORC_Renderer *renderer,
                                      AFORC_Rect rect,
                                      AFORC_Cell cell)
{
    int64_t left = (int64_t)rect.x;
    int64_t top = (int64_t)rect.y;
    int64_t right = left + (int64_t)rect.width;
    int64_t bottom = top + (int64_t)rect.height;
    int64_t row;
    int64_t column;

    if (renderer == NULL || !aforc_renderer_cell_is_valid(cell))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (rect.width <= 0 || rect.height <= 0 || right <= 0 || bottom <= 0 ||
        left >= (int64_t)renderer->size.width ||
        top >= (int64_t)renderer->size.height)
    {
        return AFORC_OK;
    }
    if (left < 0)
    {
        left = 0;
    }
    if (top < 0)
    {
        top = 0;
    }
    if (right > (int64_t)renderer->size.width)
    {
        right = (int64_t)renderer->size.width;
    }
    if (bottom > (int64_t)renderer->size.height)
    {
        bottom = (int64_t)renderer->size.height;
    }
    for (row = top; row < bottom; ++row)
    {
        for (column = left; column < right; ++column)
        {
            const size_t index =
                (size_t)row * (size_t)renderer->size.width + (size_t)column;
            renderer->back[index] = cell;
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_renderer_draw_ascii(AFORC_Renderer *renderer,
                                       AFORC_Point position,
                                       const char *text,
                                       size_t length,
                                       AFORC_Cell cell)
{
    int64_t cursor_x = (int64_t)position.x;
    int64_t cursor_y = (int64_t)position.y;
    const int64_t origin_x = (int64_t)position.x;
    size_t index;

    if (renderer == NULL || (text == NULL && length != 0u) ||
        !aforc_renderer_cell_is_valid(cell))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < length; ++index)
    {
        if ((unsigned char)text[index] > 0x7fu)
        {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0u; index < length; ++index)
    {
        const unsigned char byte = (unsigned char)text[index];

        if (byte == (unsigned char)'\n')
        {
            cursor_x = origin_x;
            if (cursor_y < INT64_MAX)
            {
                ++cursor_y;
            }
            continue;
        }
        if (byte == (unsigned char)'\r')
        {
            cursor_x = origin_x;
            continue;
        }
        if (cursor_x >= 0 && cursor_y >= 0 &&
            cursor_x < (int64_t)renderer->size.width &&
            cursor_y < (int64_t)renderer->size.height)
        {
            const size_t cell_index =
                (size_t)cursor_y * (size_t)renderer->size.width +
                (size_t)cursor_x;
            AFORC_Cell output = cell;

            output.codepoint =
                byte < 0x20u || byte == 0x7fu ? (uint32_t)' ' : (uint32_t)byte;
            renderer->back[cell_index] = output;
        }
        if (cursor_x < INT64_MAX)
        {
            ++cursor_x;
        }
    }
    return AFORC_OK;
}
