/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

/*
 * Callback-driven collision queries over validated tile regions.
 *
 * Rectangle scans are deterministic row-major traversals and return the first
 * blocked point in that order. A NULL predicate is handled by the shared
 * world policy as no blocked tiles and is never invoked.
 */

AFORC_Status aforc_grid_point_blocked(const AFORC_TileMap *map,
                                      uint32_t layer,
                                      AFORC_Point point,
                                      AFORC_TileTestFn is_blocked,
                                      void *context,
                                      bool *out_blocked)
{
    AFORC_Status status;

    if (out_blocked == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, point);
    if (status != AFORC_OK)
    {
        return status;
    }
    *out_blocked = aforc_world_tile_matches_internal(
        map, layer, point, is_blocked, context);
    return AFORC_OK;
}

AFORC_Status aforc_grid_rect_blocked(const AFORC_TileMap *map,
                                     uint32_t layer,
                                     AFORC_Rect rect,
                                     AFORC_TileTestFn is_blocked,
                                     void *context,
                                     bool *out_blocked,
                                     AFORC_Point *out_first_blocked)
{
    AFORC_Status status;
    size_t row;
    size_t column;

    if (out_blocked == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_blocked = false;
    if (out_first_blocked != NULL)
    {
        out_first_blocked->x = 0;
        out_first_blocked->y = 0;
    }
    status = aforc_world_validate_region_internal(map, layer, rect);
    if (status != AFORC_OK)
    {
        return status;
    }
    for (row = 0U; row < (size_t)rect.height; ++row)
    {
        for (column = 0U; column < (size_t)rect.width; ++column)
        {
            const AFORC_Point point = {
                (int32_t)((int64_t)rect.x + (int64_t)column),
                (int32_t)((int64_t)rect.y + (int64_t)row)};
            if (aforc_world_tile_matches_internal(
                    map, layer, point, is_blocked, context))
            {
                *out_blocked = true;
                if (out_first_blocked != NULL)
                {
                    *out_first_blocked = point;
                }
                return AFORC_OK;
            }
        }
    }
    return AFORC_OK;
}
