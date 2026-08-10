/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_ansi_internal.h"

static size_t ansi_u32_size(uint32_t value)
{
    size_t size = 1u;

    while (value >= 10u)
    {
        value /= 10u;
        ++size;
    }
    return size;
}

static size_t ansi_parameter_size(uint32_t value)
{
    return 1u + ansi_u32_size(value);
}

size_t aforc_renderer_ansi_cursor_size(uint32_t row, uint32_t column)
{
    return 4u + ansi_u32_size(row) + ansi_u32_size(column);
}

static size_t ansi_color_size(AFORC_Color color, bool foreground)
{
    size_t size;

    if (color.mode == AFORC_COLOR_DEFAULT)
    {
        return ansi_parameter_size(foreground ? 39u : 49u);
    }
    size = ansi_parameter_size(foreground ? 38u : 48u);
    if (color.mode == AFORC_COLOR_INDEXED)
    {
        return size + ansi_parameter_size(5u) + ansi_parameter_size(color.red);
    }
    return size + ansi_parameter_size(2u) + ansi_parameter_size(color.red) +
           ansi_parameter_size(color.green) + ansi_parameter_size(color.blue);
}

size_t aforc_renderer_ansi_style_size(AFORC_Cell cell)
{
    size_t size = 4u;

    for (size_t index = 0u;
         index < sizeof(ansi_style_codes) / sizeof(ansi_style_codes[0]);
         ++index)
    {
        if ((cell.style & ansi_style_codes[index].flag) != 0u)
        {
            size += ansi_parameter_size(ansi_style_codes[index].code);
        }
    }
    return size + ansi_color_size(cell.foreground, true) +
           ansi_color_size(cell.background, false);
}

size_t aforc_renderer_ansi_codepoint_size(uint32_t codepoint)
{
    if (codepoint <= UINT32_C(0x7f))
    {
        return 1u;
    }
    if (codepoint <= UINT32_C(0x7ff))
    {
        return 2u;
    }
    if (codepoint <= UINT32_C(0xffff))
    {
        return 3u;
    }
    return 4u;
}
