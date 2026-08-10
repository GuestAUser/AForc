/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_diff_model.h"

#include "../src/render/renderer_internal.h"

enum
{
    RENDERER_DIFF_PARAMETER_CAPACITY = 24
};

typedef struct TerminalModel
{
    AFORC_Cell *cells;
    AFORC_Size size;
    int32_t row, column;
    AFORC_Color foreground, background;
    AFORC_CellStyle style;
} TerminalModel;

static void terminal_reset(TerminalModel *terminal)
{
    const AFORC_Cell cell = aforc_cell_default();

    terminal->foreground = cell.foreground;
    terminal->background = cell.background;
    terminal->style = cell.style;
}

static void terminal_clear(TerminalModel *terminal)
{
    const AFORC_Cell cell = aforc_cell_default();
    const size_t count =
        (size_t)terminal->size.width * (size_t)terminal->size.height;

    aforc_renderer_fill_cells(terminal->cells, count, cell);
}

static AFORC_CellStyle terminal_style(uint32_t parameter)
{
    static const AFORC_CellStyle styles[] = {AFORC_STYLE_NONE,
                                             AFORC_STYLE_BOLD,
                                             AFORC_STYLE_DIM,
                                             AFORC_STYLE_ITALIC,
                                             AFORC_STYLE_UNDERLINE,
                                             AFORC_STYLE_BLINK,
                                             AFORC_STYLE_NONE,
                                             AFORC_STYLE_REVERSE,
                                             AFORC_STYLE_HIDDEN,
                                             AFORC_STYLE_STRIKETHROUGH};
    return parameter < sizeof(styles) / sizeof(styles[0]) ? styles[parameter]
                                                          : AFORC_STYLE_NONE;
}

static bool terminal_extended_color(const uint32_t *parameters,
                                    size_t parameter_count,
                                    size_t *index,
                                    AFORC_Color *output)
{
    if (*index >= parameter_count)
    {
        return false;
    }
    if (parameters[*index] == 5U)
    {
        if (*index + 1U >= parameter_count ||
            parameters[*index + 1U] > UINT8_MAX)
        {
            return false;
        }
        *output = aforc_color_indexed((uint8_t)parameters[*index + 1U]);
        *index += 2U;
        return true;
    }
    if (parameters[*index] != 2U || *index + 3U >= parameter_count ||
        parameters[*index + 1U] > UINT8_MAX ||
        parameters[*index + 2U] > UINT8_MAX ||
        parameters[*index + 3U] > UINT8_MAX)
    {
        return false;
    }
    *output = aforc_color_rgb((uint8_t)parameters[*index + 1U],
                              (uint8_t)parameters[*index + 2U],
                              (uint8_t)parameters[*index + 3U]);
    *index += 4U;
    return true;
}

static bool terminal_sgr(TerminalModel *terminal,
                         const uint32_t *parameters,
                         size_t parameter_count)
{
    if (parameter_count == 0U)
    {
        terminal_reset(terminal);
        return true;
    }
    for (size_t index = 0U; index < parameter_count;)
    {
        const uint32_t parameter = parameters[index++];
        const AFORC_CellStyle style = terminal_style(parameter);

        if (style != AFORC_STYLE_NONE)
        {
            terminal->style |= style;
            continue;
        }
        switch (parameter)
        {
            case 0U:
                terminal_reset(terminal);
                break;
            case 39U:
                terminal->foreground = aforc_color_default();
                break;
            case 49U:
                terminal->background = aforc_color_default();
                break;
            case 38U:
            case 48U:
            {
                AFORC_Color color;

                if (!terminal_extended_color(
                        parameters, parameter_count, &index, &color))
                {
                    return false;
                }
                if (parameter == 38U)
                {
                    terminal->foreground = color;
                }
                else
                {
                    terminal->background = color;
                }
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

static bool terminal_escape(TerminalModel *terminal,
                            const unsigned char *bytes,
                            size_t size,
                            size_t *consumed)
{
    uint32_t parameters[RENDERER_DIFF_PARAMETER_CAPACITY];
    size_t parameter_count = 0U;
    size_t index = 2U;
    bool has_digits = false;
    uint32_t value = 0U;

    if (size < 3U || bytes[0] != 0x1bU || bytes[1] != (unsigned char)'[')
    {
        return false;
    }
    while (index < size)
    {
        const unsigned char byte = bytes[index++];

        if (byte >= (unsigned char)'0' && byte <= (unsigned char)'9')
        {
            const uint32_t digit = (uint32_t)(byte - (unsigned char)'0');

            if (value > (UINT32_MAX - digit) / UINT32_C(10))
            {
                return false;
            }
            value = value * UINT32_C(10) + digit;
            has_digits = true;
            continue;
        }
        if (byte == (unsigned char)';')
        {
            if (!has_digits ||
                parameter_count == RENDERER_DIFF_PARAMETER_CAPACITY)
            {
                return false;
            }
            parameters[parameter_count++] = value;
            value = 0U;
            has_digits = false;
            continue;
        }
        if (has_digits)
        {
            if (parameter_count == RENDERER_DIFF_PARAMETER_CAPACITY)
            {
                return false;
            }
            parameters[parameter_count++] = value;
        }
        *consumed = index;
        if (byte == (unsigned char)'H')
        {
            if (parameter_count == 0U)
            {
                terminal->row = 0;
                terminal->column = 0;
                return true;
            }
            if (parameter_count != 2U || parameters[0] == 0U ||
                parameters[1] == 0U ||
                parameters[0] > (uint32_t)terminal->size.height ||
                parameters[1] > (uint32_t)terminal->size.width)
            {
                return false;
            }
            terminal->row = (int32_t)(parameters[0] - 1U);
            terminal->column = (int32_t)(parameters[1] - 1U);
            return true;
        }
        if (byte == (unsigned char)'J')
        {
            if (parameter_count != 1U || parameters[0] != 2U)
            {
                return false;
            }
            terminal_clear(terminal);
            return true;
        }
        return byte == (unsigned char)'m' &&
               terminal_sgr(terminal, parameters, parameter_count);
    }
    return false;
}

static bool terminal_codepoint(const unsigned char *bytes,
                               size_t size,
                               uint32_t *codepoint,
                               size_t *consumed)
{
    size_t count;
    uint32_t value;

    if (size == 0U)
    {
        return false;
    }
    if (bytes[0] <= 0x7fU)
    {
        count = 1U;
        value = bytes[0];
    }
    else if ((bytes[0] & 0xe0U) == 0xc0U)
    {
        count = 2U;
        value = bytes[0] & 0x1fU;
    }
    else if ((bytes[0] & 0xf0U) == 0xe0U)
    {
        count = 3U;
        value = bytes[0] & 0x0fU;
    }
    else if ((bytes[0] & 0xf8U) == 0xf0U)
    {
        count = 4U;
        value = bytes[0] & 0x07U;
    }
    else
    {
        return false;
    }
    if (count > size)
    {
        return false;
    }
    for (size_t index = 1U; index < count; ++index)
    {
        if ((bytes[index] & 0xc0U) != 0x80U)
        {
            return false;
        }
        value = (value << 6U) | (bytes[index] & 0x3fU);
    }
    *codepoint = value;
    *consumed = count;
    return true;
}

static bool
terminal_apply(TerminalModel *terminal, const char *batch, size_t batch_size)
{
    const unsigned char *bytes = (const unsigned char *)batch;
    size_t offset = 0U;

    while (offset < batch_size)
    {
        size_t consumed = 0U;

        if (bytes[offset] == 0x1bU)
        {
            if (!terminal_escape(
                    terminal, bytes + offset, batch_size - offset, &consumed))
            {
                return false;
            }
        }
        else
        {
            uint32_t codepoint = 0U;
            size_t cell_index;

            if (!terminal_codepoint(bytes + offset,
                                    batch_size - offset,
                                    &codepoint,
                                    &consumed) ||
                terminal->row < 0 || terminal->column < 0 ||
                terminal->row >= terminal->size.height ||
                terminal->column >= terminal->size.width)
            {
                return false;
            }
            cell_index = (size_t)terminal->row * (size_t)terminal->size.width +
                         (size_t)terminal->column;
            terminal->cells[cell_index].codepoint = codepoint;
            terminal->cells[cell_index].foreground = terminal->foreground;
            terminal->cells[cell_index].background = terminal->background;
            terminal->cells[cell_index].style = terminal->style;
            ++terminal->column;
        }
        if (consumed == 0U)
        {
            return false;
        }
        offset += consumed;
    }
    return true;
}

bool renderer_diff_apply_ansi(AFORC_Cell *cells,
                              AFORC_Size size,
                              const char *batch,
                              size_t batch_size)
{
    TerminalModel terminal;

    if (cells == NULL || batch == NULL || size.width <= 0 || size.height <= 0)
    {
        return false;
    }
    terminal.cells = cells;
    terminal.size = size;
    terminal.row = 0;
    terminal.column = 0;
    terminal_reset(&terminal);
    return terminal_apply(&terminal, batch, batch_size);
}
