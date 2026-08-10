/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/ui.h"

#include "ui_internal.h"

#include <limits.h>

/*
 * Integer-only layout primitives.
 *
 * Sequential layouts advance a saturating cursor, while indexed splits are
 * order-independent and cover the available axis exactly. All coordinate
 * arithmetic is widened before narrowing back to the public int32 geometry.
 */

static bool axis_valid(AFORC_UILayoutAxis axis)
{
    return axis == AFORC_UI_LAYOUT_ROW || axis == AFORC_UI_LAYOUT_COLUMN;
}

static bool anchor_valid(AFORC_UIAnchor anchor)
{
    return anchor == AFORC_UI_ANCHOR_TOP_LEFT ||
           anchor == AFORC_UI_ANCHOR_TOP_CENTER ||
           anchor == AFORC_UI_ANCHOR_TOP_RIGHT ||
           anchor == AFORC_UI_ANCHOR_CENTER_LEFT ||
           anchor == AFORC_UI_ANCHOR_CENTER ||
           anchor == AFORC_UI_ANCHOR_CENTER_RIGHT ||
           anchor == AFORC_UI_ANCHOR_BOTTOM_LEFT ||
           anchor == AFORC_UI_ANCHOR_BOTTOM_CENTER ||
           anchor == AFORC_UI_ANCHOR_BOTTOM_RIGHT;
}

AFORC_Status aforc_ui_layout_init(AFORC_UILayout *layout,
                                  AFORC_Rect bounds,
                                  AFORC_UILayoutAxis axis,
                                  int32_t gap)
{
    if (layout == NULL || !aforc_ui_rect_valid(bounds) || !axis_valid(axis) ||
        gap < 0)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    layout->bounds = bounds;
    layout->axis = axis;
    layout->gap = gap;
    layout->cursor = 0;
    return AFORC_OK;
}

AFORC_Status aforc_ui_layout_next(AFORC_UILayout *layout,
                                  int32_t extent,
                                  AFORC_Rect *out_rect)
{
    int64_t limit;
    int64_t position;
    int64_t end;

    if (layout == NULL || out_rect == NULL || extent < 0 ||
        !aforc_ui_rect_valid(layout->bounds) || !axis_valid(layout->axis) ||
        layout->gap < 0 || layout->cursor < 0)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    limit = layout->axis == AFORC_UI_LAYOUT_ROW ? layout->bounds.width
                                                : layout->bounds.height;
    if (layout->cursor > limit || extent > limit - layout->cursor)
    {
        return AFORC_ERROR_LIMIT;
    }
    position = (layout->axis == AFORC_UI_LAYOUT_ROW ? layout->bounds.x
                                                    : layout->bounds.y) +
               layout->cursor;
    if (position > INT32_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_rect = layout->bounds;
    if (layout->axis == AFORC_UI_LAYOUT_ROW)
    {
        out_rect->x = (int32_t)position;
        out_rect->width = extent;
    }
    else
    {
        out_rect->y = (int32_t)position;
        out_rect->height = extent;
    }

    end = layout->cursor + extent;
    if (end == limit || layout->gap > limit - end)
    {
        layout->cursor = limit;
    }
    else
    {
        layout->cursor = end + layout->gap;
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_layout_split(AFORC_Rect bounds,
                                   AFORC_UILayoutAxis axis,
                                   size_t count,
                                   int32_t gap,
                                   size_t index,
                                   AFORC_Rect *out_rect)
{
    uint64_t count64;
    uint64_t index64;
    uint64_t limit;
    uint64_t gap_total;
    uint64_t available;
    uint64_t base;
    uint64_t remainder;
    uint64_t offset;
    uint64_t extent;
    int64_t position;

    if (out_rect == NULL || !aforc_ui_rect_valid(bounds) || !axis_valid(axis) ||
        count == 0U || gap < 0)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (index >= count)
    {
        return AFORC_ERROR_LIMIT;
    }
    count64 = (uint64_t)count;
    index64 = (uint64_t)index;
    limit =
        (uint64_t)(axis == AFORC_UI_LAYOUT_ROW ? bounds.width : bounds.height);
    if (gap > 0 && count64 - 1U > limit / (uint32_t)gap)
    {
        return AFORC_ERROR_LIMIT;
    }
    gap_total = (count64 - 1U) * (uint32_t)gap;
    available = limit - gap_total;
    base = available / count64;
    remainder = available % count64;
    /* Leading panes receive one remainder cell each, keeping the split
       deterministic while exactly covering the available extent. */
    offset = index64 * base + (index64 < remainder ? index64 : remainder) +
             index64 * (uint32_t)gap;
    extent = base + (index64 < remainder ? 1U : 0U);
    position =
        (axis == AFORC_UI_LAYOUT_ROW ? bounds.x : bounds.y) + (int64_t)offset;
    if (position > INT32_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_rect = bounds;
    if (axis == AFORC_UI_LAYOUT_ROW)
    {
        out_rect->x = (int32_t)position;
        out_rect->width = (int32_t)extent;
    }
    else
    {
        out_rect->y = (int32_t)position;
        out_rect->height = (int32_t)extent;
    }
    return AFORC_OK;
}

AFORC_Status aforc_ui_layout_anchor(AFORC_Rect bounds,
                                    AFORC_Size size,
                                    AFORC_UIAnchor anchor,
                                    AFORC_Rect *out_rect)
{
    int32_t horizontal_offset;
    int32_t vertical_offset;
    unsigned int column;
    unsigned int row;
    int64_t x;
    int64_t y;

    if (out_rect == NULL || !aforc_ui_rect_valid(bounds) ||
        !anchor_valid(anchor) || size.width < 0 || size.height < 0)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (size.width > bounds.width || size.height > bounds.height)
    {
        return AFORC_ERROR_LIMIT;
    }
    column = (unsigned int)anchor % 3U;
    row = (unsigned int)anchor / 3U;
    horizontal_offset = column == 0U   ? 0
                        : column == 1U ? (bounds.width - size.width) / 2
                                       : bounds.width - size.width;
    vertical_offset = row == 0U   ? 0
                      : row == 1U ? (bounds.height - size.height) / 2
                                  : bounds.height - size.height;
    x = (int64_t)bounds.x + horizontal_offset;
    y = (int64_t)bounds.y + vertical_offset;
    if (x > INT32_MAX || y > INT32_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_rect = (AFORC_Rect){(int32_t)x, (int32_t)y, size.width, size.height};
    return AFORC_OK;
}
