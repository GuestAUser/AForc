/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/ui.h"

/*
 * Stateless ASCII widget composition over AFORC_UICanvas.
 *
 * Widgets borrow all styles, labels, and menu items for the duration of a
 * synchronous draw. They clip through child canvases and stop immediately on
 * the first plot error, so callers can use transactional render backends.
 */

static bool align_valid(AFORC_UIAlign align) {
    return align == AFORC_UI_ALIGN_START || align == AFORC_UI_ALIGN_CENTER ||
           align == AFORC_UI_ALIGN_END;
}

static bool text_ascii(const char *text, size_t length) {
    if (text == NULL) {
        return length == 0U;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char byte = (unsigned char)text[index];

        if (byte < 0x20U || byte > 0x7eU) {
            return false;
        }
    }
    return true;
}

static uint32_t multiply_divide(uint64_t value,
                                uint32_t multiplier,
                                uint64_t divisor) {
    uint64_t quotient = 0U;
    uint64_t remainder = 0U;
    const uint64_t value_quotient = value / divisor;
    const uint64_t value_remainder = value % divisor;

    /*
     * Compute floor(value * multiplier / divisor) without materializing the
     * potentially overflowing product. The loop performs binary long
     * multiplication while carrying a remainder that always stays < divisor.
     */
    for (unsigned int bit = 32U; bit > 0U; --bit) {
        quotient *= 2U;
        if (remainder >= divisor - remainder) {
            remainder -= divisor - remainder;
            ++quotient;
        } else {
            remainder *= 2U;
        }
        if ((multiplier & (UINT32_C(1) << (bit - 1U))) != 0U) {
            quotient += value_quotient;
            if (value_remainder >= divisor - remainder) {
                remainder -= divisor - value_remainder;
                ++quotient;
            } else {
                remainder += value_remainder;
            }
        }
    }
    return (uint32_t)quotient;
}

AFORC_UIPanelStyle aforc_ui_panel_style_ascii(AFORC_Cell border,
                                           AFORC_Cell fill,
                                           bool fill_interior) {
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
                             const AFORC_UIPanelStyle *style) {
    AFORC_UICanvas visible;
    AFORC_Status status;

    if (style == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK) {
        return status;
    }
    for (int64_t y = visible.clip.y;
         y < (int64_t)visible.clip.y + visible.clip.height;
         ++y) {
        const int64_t row = y - rect.y;

        for (int64_t x = visible.clip.x;
             x < (int64_t)visible.clip.x + visible.clip.width;
             ++x) {
            const int64_t column = x - rect.x;
            AFORC_Cell cell;
            bool draw = true;

            if (row == 0) {
                cell = column == 0
                           ? style->top_left
                           : column + 1 == rect.width ? style->top_right
                                                       : style->horizontal;
            } else if (row + 1 == rect.height) {
                cell = column == 0
                           ? style->bottom_left
                           : column + 1 == rect.width ? style->bottom_right
                                                       : style->horizontal;
            } else if (column == 0 || column + 1 == rect.width) {
                cell = style->vertical;
            } else {
                cell = style->fill;
                draw = style->fill_interior;
            }
            if (draw) {
                status = visible.plot(
                    visible.context,
                    (AFORC_Point){(int32_t)x, (int32_t)y},
                    cell);
                if (status != AFORC_OK) {
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
                             AFORC_Cell text_cell) {
    AFORC_UICanvas visible;
    size_t drawn_length;
    int64_t start;
    int64_t end;
    int64_t first;
    int64_t last;
    AFORC_Status status;

    if (!align_valid(align) || !text_ascii(text, text_length)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK) {
        return status;
    }
    if (rect.width == 0 || rect.height == 0 || text_length == 0U ||
        rect.y < visible.clip.y ||
        rect.y >= (int64_t)visible.clip.y + visible.clip.height) {
        return AFORC_OK;
    }
    drawn_length = text_length < (size_t)rect.width
                       ? text_length
                       : (size_t)rect.width;
    start = rect.x;
    if (align == AFORC_UI_ALIGN_CENTER) {
        start += (rect.width - (int64_t)drawn_length) / 2;
    } else if (align == AFORC_UI_ALIGN_END) {
        start += rect.width - (int64_t)drawn_length;
    }
    end = start + (int64_t)drawn_length;
    first = start > visible.clip.x ? start : visible.clip.x;
    last = end < (int64_t)visible.clip.x + visible.clip.width
               ? end
               : (int64_t)visible.clip.x + visible.clip.width;
    for (int64_t x = first; x < last; ++x) {
        text_cell.codepoint = (unsigned char)text[(size_t)(x - start)];
        status = visible.plot(visible.context,
                              (AFORC_Point){(int32_t)x, rect.y},
                              text_cell);
        if (status != AFORC_OK) {
            return status;
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_draw_progress(const AFORC_UICanvas *canvas,
                                AFORC_Rect rect,
                                uint64_t value,
                                uint64_t maximum,
                                const AFORC_UIProgressStyle *style) {
    AFORC_UICanvas visible;
    uint32_t filled;
    AFORC_Status status;

    if (style == NULL || maximum == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK) {
        return status;
    }
    if (value > maximum) {
        value = maximum;
    }
    filled = multiply_divide(value, (uint32_t)rect.width, maximum);
    for (int64_t y = visible.clip.y;
         y < (int64_t)visible.clip.y + visible.clip.height;
         ++y) {
        for (int64_t x = visible.clip.x;
             x < (int64_t)visible.clip.x + visible.clip.width;
             ++x) {
            const uint32_t column = (uint32_t)(x - rect.x);
            const AFORC_Cell cell = column < filled ? style->filled
                                                  : style->empty;

            status = visible.plot(
                visible.context,
                (AFORC_Point){(int32_t)x, (int32_t)y},
                cell);
            if (status != AFORC_OK) {
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
                              const AFORC_UIButtonStyle *style) {
    const AFORC_UIPanelStyle *panel;
    AFORC_Cell text_cell;
    AFORC_Rect label_rect;
    AFORC_Status status;

    if (style == NULL || !text_ascii(label, label_length)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!enabled) {
        panel = &style->disabled_panel;
        text_cell = style->disabled_text;
    } else if (focused) {
        panel = &style->focused_panel;
        text_cell = style->focused_text;
    } else {
        panel = &style->normal_panel;
        text_cell = style->normal_text;
    }
    status = aforc_ui_draw_panel(canvas, rect, panel);
    if (status != AFORC_OK || rect.width <= 2 || rect.height <= 2) {
        return status;
    }
    label_rect = (AFORC_Rect){rect.x + 1,
                            rect.y + (rect.height - 1) / 2,
                            rect.width - 2,
                            1};
    return aforc_ui_draw_label(canvas, label_rect, label, label_length,
                             AFORC_UI_ALIGN_CENTER, text_cell);
}

AFORC_Status aforc_ui_draw_menu(const AFORC_UICanvas *canvas,
                            AFORC_Rect rect,
                            const AFORC_UIMenuItem *items,
                            size_t item_count,
                            const AFORC_UIMenuState *state,
                            const AFORC_UIMenuStyle *style) {
    AFORC_UICanvas visible;
    size_t visible_count;
    AFORC_Status status;

    if (state == NULL || style == NULL ||
        (items == NULL && item_count != 0U)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if ((state->selected != AFORC_UI_NO_INDEX &&
         state->selected >= item_count) ||
        state->scroll > item_count) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ui_canvas_child(canvas, rect, &visible);
    if (status != AFORC_OK) {
        return status;
    }
    visible_count = item_count - state->scroll;
    if (visible_count > (size_t)rect.height) {
        visible_count = (size_t)rect.height;
    }
    /* Validate every visible label before plotting to avoid partial widgets. */
    for (size_t row = 0U; row < visible_count; ++row) {
        const AFORC_UIMenuItem *item = &items[state->scroll + row];

        if (!text_ascii(item->label, item->label_length)) {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
    }
    for (size_t row = 0U; row < visible_count; ++row) {
        const size_t index = state->scroll + row;
        const AFORC_UIMenuItem *item = &items[index];
        const bool selected = index == state->selected;
        const AFORC_Cell row_cell = !item->enabled
                                      ? style->disabled
                                      : selected ? style->selected
                                                 : style->normal;
        const AFORC_Rect row_rect = {rect.x, rect.y + (int32_t)row,
                                   rect.width, 1};

        status = aforc_ui_canvas_fill(&visible, row_rect, row_cell);
        if (status != AFORC_OK) {
            return status;
        }
        if (selected && rect.width > 0) {
            status = aforc_ui_canvas_plot(
                &visible,
                (AFORC_Point){rect.x, row_rect.y},
                style->cursor);
            if (status != AFORC_OK) {
                return status;
            }
        }
        if (rect.width > 2) {
            const AFORC_Rect label_rect = {rect.x + 2, row_rect.y,
                                         rect.width - 2, 1};

            status = aforc_ui_draw_label(&visible, label_rect, item->label,
                                       item->label_length, AFORC_UI_ALIGN_START,
                                       row_cell);
            if (status != AFORC_OK) {
                return status;
            }
        }
    }
    return AFORC_OK;
}
