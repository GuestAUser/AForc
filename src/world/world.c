/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

/*
 * Allocation-free geometry primitives shared by world consumers.
 *
 * Arithmetic widens before combining public int32 coordinates; operations
 * that must narrow report overflow rather than saturating or wrapping.
 */

bool aforc_world_point_equal(AFORC_Point left, AFORC_Point right)
{
    return left.x == right.x && left.y == right.y;
}

AFORC_Status
aforc_world_point_add(AFORC_Point left, AFORC_Point right, AFORC_Point *out_sum)
{
    return aforc_world_point_from_i64_internal(
        (int64_t)left.x + (int64_t)right.x,
        (int64_t)left.y + (int64_t)right.y,
        out_sum);
}

uint64_t aforc_world_point_manhattan(AFORC_Point left, AFORC_Point right)
{
    const uint64_t delta_x = left.x >= right.x
                                 ? (uint64_t)((int64_t)left.x - right.x)
                                 : (uint64_t)((int64_t)right.x - left.x);
    const uint64_t delta_y = left.y >= right.y
                                 ? (uint64_t)((int64_t)left.y - right.y)
                                 : (uint64_t)((int64_t)right.y - left.y);
    return delta_x + delta_y;
}

bool aforc_world_rect_is_empty(AFORC_Rect rect)
{
    return rect.width <= 0 || rect.height <= 0;
}

bool aforc_world_rect_intersects(AFORC_Rect left, AFORC_Rect right)
{
    const int64_t left_right = (int64_t)left.x + left.width;
    const int64_t right_right = (int64_t)right.x + right.width;
    const int64_t left_bottom = (int64_t)left.y + left.height;
    const int64_t right_bottom = (int64_t)right.y + right.height;

    if (aforc_world_rect_is_empty(left) || aforc_world_rect_is_empty(right))
    {
        return false;
    }
    return (int64_t)left.x < right_right && (int64_t)right.x < left_right &&
           (int64_t)left.y < right_bottom && (int64_t)right.y < left_bottom;
}

AFORC_Status aforc_world_rect_translate(AFORC_Rect rect,
                                        AFORC_Point delta,
                                        AFORC_Rect *out_rect)
{
    AFORC_Point translated;
    AFORC_Status status;

    if (out_rect == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_world_point_from_i64_internal(
        (int64_t)rect.x + delta.x, (int64_t)rect.y + delta.y, &translated);
    if (status != AFORC_OK)
    {
        return status;
    }
    out_rect->x = translated.x;
    out_rect->y = translated.y;
    out_rect->width = rect.width;
    out_rect->height = rect.height;
    return AFORC_OK;
}
