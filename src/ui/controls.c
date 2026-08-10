/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ui_internal.h"

static uint32_t
multiply_divide(uint64_t value, uint32_t multiplier, uint64_t divisor)
{
    uint64_t quotient = 0U;
    uint64_t remainder = 0U;
    const uint64_t value_quotient = value / divisor;
    const uint64_t value_remainder = value % divisor;

    for (unsigned int bit = 32U; bit > 0U; --bit)
    {
        quotient *= 2U;
        if (remainder >= divisor - remainder)
        {
            remainder -= divisor - remainder;
            ++quotient;
        }
        else
        {
            remainder *= 2U;
        }
        if ((multiplier & (UINT32_C(1) << (bit - 1U))) != 0U)
        {
            quotient += value_quotient;
            if (value_remainder >= divisor - remainder)
            {
                remainder -= divisor - value_remainder;
                ++quotient;
            }
            else
            {
                remainder += value_remainder;
            }
        }
    }
    return (uint32_t)quotient;
}

AFORC_Status aforc_ui_draw_progress(const AFORC_UICanvas *canvas,
                                    AFORC_Rect rect,
                                    uint64_t value,
                                    uint64_t maximum,
                                    const AFORC_UIProgressStyle *style)
{
    AFORC_UICanvas visible;
    uint32_t filled;
    AFORC_Status status;

    if (style == NULL || maximum == 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (value > maximum)
    {
        value = maximum;
    }
    filled = multiply_divide(value, (uint32_t)rect.width, maximum);
    for (int64_t y = visible.clip.y;
         y < (int64_t)visible.clip.y + visible.clip.height;
         ++y)
    {
        for (int64_t x = visible.clip.x;
             x < (int64_t)visible.clip.x + visible.clip.width;
             ++x)
        {
            const uint32_t column = (uint32_t)(x - rect.x);
            const AFORC_Cell cell =
                column < filled ? style->filled : style->empty;

            status = visible.plot(
                visible.context, (AFORC_Point){(int32_t)x, (int32_t)y}, cell);
            if (status != AFORC_OK)
            {
                return status;
            }
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_draw_button(const AFORC_UICanvas *canvas,
                                  AFORC_Rect rect,
                                  const char *label,
                                  size_t label_length,
                                  bool focused,
                                  bool enabled,
                                  const AFORC_UIButtonStyle *style)
{
    const AFORC_UIPanelStyle *panel;
    AFORC_Cell text_cell;
    AFORC_Rect label_rect;
    AFORC_Status status;

    if (style == NULL || !aforc_ui_text_ascii(label, label_length))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!enabled)
    {
        panel = &style->disabled_panel;
        text_cell = style->disabled_text;
    }
    else if (focused)
    {
        panel = &style->focused_panel;
        text_cell = style->focused_text;
    }
    else
    {
        panel = &style->normal_panel;
        text_cell = style->normal_text;
    }
    status = aforc_ui_draw_panel(canvas, rect, panel);
    if (status != AFORC_OK || rect.width <= 2 || rect.height <= 2)
    {
        return status;
    }
    label_rect = (AFORC_Rect){
        rect.x + 1, rect.y + (rect.height - 1) / 2, rect.width - 2, 1};
    return aforc_ui_draw_label(canvas,
                               label_rect,
                               label,
                               label_length,
                               AFORC_UI_ALIGN_CENTER,
                               text_cell);
}

AFORC_Status aforc_ui_draw_menu(const AFORC_UICanvas *canvas,
                                AFORC_Rect rect,
                                const AFORC_UIMenuItem *items,
                                size_t item_count,
                                const AFORC_UIMenuState *state,
                                const AFORC_UIMenuStyle *style)
{
    AFORC_UICanvas visible;
    size_t visible_count;
    AFORC_Status status;

    if (state == NULL || style == NULL || (items == NULL && item_count != 0U))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if ((state->selected != AFORC_UI_NO_INDEX &&
         state->selected >= item_count) ||
        state->scroll > item_count)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK)
    {
        return status;
    }
    visible_count = item_count - state->scroll;
    if (visible_count > (size_t)rect.height)
    {
        visible_count = (size_t)rect.height;
    }
    for (size_t row = 0U; row < visible_count; ++row)
    {
        const AFORC_UIMenuItem *item = &items[state->scroll + row];

        if (!aforc_ui_text_ascii(item->label, item->label_length))
        {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
    }
    for (size_t row = 0U; row < visible_count; ++row)
    {
        const size_t index = state->scroll + row;
        const AFORC_UIMenuItem *item = &items[index];
        const bool selected = index == state->selected;
        AFORC_Cell row_cell;
        const AFORC_Rect row_rect = {
            rect.x, rect.y + (int32_t)row, rect.width, 1};

        if (!item->enabled)
        {
            row_cell = style->disabled;
        }
        else if (selected)
        {
            row_cell = style->selected;
        }
        else
        {
            row_cell = style->normal;
        }
        status = aforc_ui_canvas_fill(&visible, row_rect, row_cell);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (selected && rect.width > 0)
        {
            status = aforc_ui_canvas_plot(
                &visible, (AFORC_Point){rect.x, row_rect.y}, style->cursor);
            if (status != AFORC_OK)
            {
                return status;
            }
        }
        if (rect.width > 2)
        {
            const AFORC_Rect label_rect = {
                rect.x + 2, row_rect.y, rect.width - 2, 1};

            status = aforc_ui_draw_label(&visible,
                                         label_rect,
                                         item->label,
                                         item->label_length,
                                         AFORC_UI_ALIGN_START,
                                         row_cell);
            if (status != AFORC_OK)
            {
                return status;
            }
        }
    }
    return AFORC_OK;
}
