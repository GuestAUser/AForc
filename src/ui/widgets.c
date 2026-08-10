/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ui_internal.h"

static bool align_valid(AFORC_UIAlign align)
{
    return align == AFORC_UI_ALIGN_START || align == AFORC_UI_ALIGN_CENTER ||
           align == AFORC_UI_ALIGN_END;
}

AFORC_UIPanelStyle aforc_ui_panel_style_ascii(AFORC_Cell border,
                                              AFORC_Cell fill,
                                              bool fill_interior)
{
    AFORC_UIPanelStyle style;

    style.top_left = border;
    style.top_left.codepoint = '+';
    style.top_right = border;
    style.top_right.codepoint = '+';
    style.bottom_left = border;
    style.bottom_left.codepoint = '+';
    style.bottom_right = border;
    style.bottom_right.codepoint = '+';
    style.horizontal = border;
    style.horizontal.codepoint = '-';
    style.vertical = border;
    style.vertical.codepoint = '|';
    style.fill = fill;
    style.fill.codepoint = ' ';
    style.fill_interior = fill_interior;
    return style;
}

AFORC_Status aforc_ui_draw_panel(const AFORC_UICanvas *canvas,
                                 AFORC_Rect rect,
                                 const AFORC_UIPanelStyle *style)
{
    AFORC_UICanvas visible;
    AFORC_Status status;

    if (style == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK)
    {
        return status;
    }
    for (int64_t y = visible.clip.y;
         y < (int64_t)visible.clip.y + visible.clip.height;
         ++y)
    {
        const int64_t row = y - rect.y;

        for (int64_t x = visible.clip.x;
             x < (int64_t)visible.clip.x + visible.clip.width;
             ++x)
        {
            const int64_t column = x - rect.x;
            AFORC_Cell cell;
            bool draw = true;

            if (row == 0)
            {
                if (column == 0)
                {
                    cell = style->top_left;
                }
                else if (column + 1 == rect.width)
                {
                    cell = style->top_right;
                }
                else
                {
                    cell = style->horizontal;
                }
            }
            else if (row + 1 == rect.height)
            {
                if (column == 0)
                {
                    cell = style->bottom_left;
                }
                else if (column + 1 == rect.width)
                {
                    cell = style->bottom_right;
                }
                else
                {
                    cell = style->horizontal;
                }
            }
            else if (column == 0 || column + 1 == rect.width)
            {
                cell = style->vertical;
            }
            else
            {
                cell = style->fill;
                draw = style->fill_interior;
            }
            if (draw)
            {
                status = visible.plot(visible.context,
                                      (AFORC_Point){(int32_t)x, (int32_t)y},
                                      cell);
                if (status != AFORC_OK)
                {
                    return status;
                }
            }
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_draw_label(const AFORC_UICanvas *canvas,
                                 AFORC_Rect rect,
                                 const char *text,
                                 size_t text_length,
                                 AFORC_UIAlign align,
                                 AFORC_Cell text_cell)
{
    AFORC_UICanvas visible;
    size_t drawn_length;
    int64_t start;
    int64_t end;
    int64_t first;
    int64_t last;
    AFORC_Status status;

    if (!align_valid(align) || !aforc_ui_text_ascii(text, text_length))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (rect.width == 0 || rect.height == 0 || text_length == 0U ||
        rect.y < visible.clip.y ||
        rect.y >= (int64_t)visible.clip.y + visible.clip.height)
    {
        return AFORC_OK;
    }
    drawn_length =
        text_length < (size_t)rect.width ? text_length : (size_t)rect.width;
    start = rect.x;
    if (align == AFORC_UI_ALIGN_CENTER)
    {
        start += (rect.width - (int64_t)drawn_length) / 2;
    }
    else if (align == AFORC_UI_ALIGN_END)
    {
        start += rect.width - (int64_t)drawn_length;
    }
    end = start + (int64_t)drawn_length;
    first = start > visible.clip.x ? start : visible.clip.x;
    last = end < (int64_t)visible.clip.x + visible.clip.width
               ? end
               : (int64_t)visible.clip.x + visible.clip.width;
    for (int64_t x = first; x < last; ++x)
    {
        text_cell.codepoint = (unsigned char)text[(size_t)(x - start)];
        status = visible.plot(
            visible.context, (AFORC_Point){(int32_t)x, rect.y}, text_cell);
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}
