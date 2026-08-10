/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_WORLD_PRIVATE_H
#define AFORC_WORLD_PRIVATE_H

#include "../../include/aforc/world.h"

#if defined(__GNUC__) || defined(__clang__)
#define AFORC_WORLD_PRIVATE __attribute__((visibility("hidden")))
#else
#define AFORC_WORLD_PRIVATE
#endif

struct AFORC_TileMap
{
    AFORC_Allocator allocator;
    AFORC_Size size;
    uint32_t layer_count;
    size_t cell_count;
    AFORC_Tile *tiles;
};

AFORC_WORLD_PRIVATE AFORC_Status aforc_world_point_from_i64_internal(
    int64_t x, int64_t y, AFORC_Point *out_point);
AFORC_WORLD_PRIVATE bool aforc_world_coordinates_contained_internal(
    const AFORC_TileMap *map, int64_t x, int64_t y);

/* Every module uses these row-major indices after explicit bounds checks. */
AFORC_WORLD_PRIVATE size_t
aforc_world_point_index_internal(const AFORC_TileMap *map, AFORC_Point point);
AFORC_WORLD_PRIVATE size_t aforc_world_tile_index_internal(
    const AFORC_TileMap *map, uint32_t layer, AFORC_Point point);

AFORC_WORLD_PRIVATE AFORC_Status
aforc_world_validate_layer_internal(const AFORC_TileMap *map, uint32_t layer);
AFORC_WORLD_PRIVATE AFORC_Status aforc_world_validate_layer_point_internal(
    const AFORC_TileMap *map, uint32_t layer, AFORC_Point point);
AFORC_WORLD_PRIVATE AFORC_Status aforc_world_validate_region_internal(
    const AFORC_TileMap *map, uint32_t layer, AFORC_Rect rect);

/* NULL predicates invoke no callback and make every tile passable/transparent.
 */
AFORC_WORLD_PRIVATE bool
aforc_world_tile_matches_internal(const AFORC_TileMap *map,
                                  uint32_t layer,
                                  AFORC_Point point,
                                  AFORC_TileTestFn predicate,
                                  void *context);

#endif
