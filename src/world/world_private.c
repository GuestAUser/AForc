/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

#include <limits.h>

/*
 * Cross-component world invariants and checked coordinate/index helpers.
 *
 * Index functions are intentionally unchecked hot-path operations: callers
 * must first use the validation helpers declared beside them in the private
 * interface. Keeping that split avoids repeated bounds work inside algorithms.
 */

static bool world_i64_fits_i32(int64_t value)
{
    return value >= (int64_t)INT32_MIN && value <= (int64_t)INT32_MAX;
}

AFORC_Status aforc_world_point_from_i64_internal(int64_t x,
                                                 int64_t y,
                                                 AFORC_Point *out_point)
{
    if (out_point == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!world_i64_fits_i32(x) || !world_i64_fits_i32(y))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    out_point->x = (int32_t)x;
    out_point->y = (int32_t)y;
    return AFORC_OK;
}

bool aforc_world_coordinates_contained_internal(const AFORC_TileMap *map,
                                                int64_t x,
                                                int64_t y)
{
    return map != NULL && x >= 0 && y >= 0 && x < map->size.width &&
           y < map->size.height;
}

size_t aforc_world_point_index_internal(const AFORC_TileMap *map,
                                        AFORC_Point point)
{
    return (size_t)point.y * (size_t)map->size.width + (size_t)point.x;
}

size_t aforc_world_tile_index_internal(const AFORC_TileMap *map,
                                       uint32_t layer,
                                       AFORC_Point point)
{
    return (size_t)layer * map->cell_count +
           aforc_world_point_index_internal(map, point);
}

AFORC_Status aforc_world_validate_layer_internal(const AFORC_TileMap *map,
                                                 uint32_t layer)
{
    if (map == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return layer < map->layer_count ? AFORC_OK : AFORC_ERROR_NOT_FOUND;
}

AFORC_Status aforc_world_validate_layer_point_internal(const AFORC_TileMap *map,
                                                       uint32_t layer,
                                                       AFORC_Point point)
{
    const AFORC_Status status = aforc_world_validate_layer_internal(map, layer);
    if (status != AFORC_OK)
    {
        return status;
    }
    return aforc_world_coordinates_contained_internal(map, point.x, point.y)
               ? AFORC_OK
               : AFORC_ERROR_NOT_FOUND;
}

AFORC_Status aforc_world_validate_region_internal(const AFORC_TileMap *map,
                                                  uint32_t layer,
                                                  AFORC_Rect rect)
{
    AFORC_Status status = aforc_world_validate_layer_internal(map, layer);
    int64_t right;
    int64_t bottom;

    if (status != AFORC_OK)
    {
        return status;
    }
    if (rect.width < 0 || rect.height < 0)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    right = (int64_t)rect.x + (int64_t)rect.width;
    bottom = (int64_t)rect.y + (int64_t)rect.height;
    if (rect.x < 0 || rect.y < 0 || right > (int64_t)map->size.width ||
        bottom > (int64_t)map->size.height)
    {
        return AFORC_ERROR_NOT_FOUND;
    }
    return AFORC_OK;
}

bool aforc_world_tile_matches_internal(const AFORC_TileMap *map,
                                       uint32_t layer,
                                       AFORC_Point point,
                                       AFORC_TileTestFn predicate,
                                       void *context)
{
    const size_t index = aforc_world_tile_index_internal(map, layer, point);
    return predicate != NULL &&
           predicate(map->tiles[index], layer, point, context);
}
