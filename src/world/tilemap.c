/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

#include <string.h>

/*
 * Dense layered tile storage and its ownership boundary.
 *
 * A map copies its allocator and owns one layer-major tile allocation until
 * destruction. Rows remain contiguous, enabling direct indexed access and
 * cache-friendly rectangular fills after a single region validation.
 */

static bool tilemap_allocator_is_valid(const AFORC_Allocator *allocator)
{
    return allocator != NULL && allocator->allocate != NULL &&
           allocator->reallocate != NULL && allocator->deallocate != NULL;
}

AFORC_Status aforc_tilemap_create(AFORC_Size size,
                                  uint32_t layer_count,
                                  AFORC_Tile initial_tile,
                                  const AFORC_Allocator *allocator,
                                  AFORC_TileMap **out_map)
{
    AFORC_TileMap *map = NULL;
    size_t cell_count = 0U;
    size_t tile_count = 0U;
    AFORC_Status status;
    size_t index;

    if (out_map == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_map = NULL;
    if (!tilemap_allocator_is_valid(allocator) || size.width <= 0 ||
        size.height <= 0 || layer_count == 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
#if SIZE_MAX < UINT32_MAX
    if ((uint32_t)size.width > SIZE_MAX || (uint32_t)size.height > SIZE_MAX ||
        layer_count > SIZE_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
#endif
    if (!aforc_size_multiply(
            (size_t)size.width, (size_t)size.height, &cell_count) ||
        !aforc_size_multiply(cell_count, (size_t)layer_count, &tile_count))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    status = aforc_alloc_array(allocator, 1U, sizeof(*map), (void **)&map);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(map, 0, sizeof(*map));
    map->allocator = *allocator;
    map->size = size;
    map->layer_count = layer_count;
    map->cell_count = cell_count;
    status = aforc_alloc_array(
        allocator, tile_count, sizeof(*map->tiles), (void **)&map->tiles);
    if (status != AFORC_OK)
    {
        aforc_free(allocator, map);
        return status;
    }
    for (index = 0U; index < tile_count; ++index)
    {
        map->tiles[index] = initial_tile;
    }
    *out_map = map;
    return AFORC_OK;
}

void aforc_tilemap_destroy(AFORC_TileMap *map)
{
    AFORC_Allocator allocator;

    if (map == NULL)
    {
        return;
    }
    allocator = map->allocator;
    aforc_free(&allocator, map->tiles);
    aforc_free(&allocator, map);
}

AFORC_Status aforc_tilemap_get_dimensions(const AFORC_TileMap *map,
                                          AFORC_Size *out_size,
                                          uint32_t *out_layer_count)
{
    if (map == NULL || out_size == NULL || out_layer_count == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_size = map->size;
    *out_layer_count = map->layer_count;
    return AFORC_OK;
}

AFORC_Status aforc_tilemap_cell_count(const AFORC_TileMap *map,
                                      size_t *out_count)
{
    if (map == NULL || out_count == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_count = map->cell_count;
    return AFORC_OK;
}

bool aforc_tilemap_contains(const AFORC_TileMap *map, AFORC_Point point)
{
    return aforc_world_coordinates_contained_internal(map, point.x, point.y);
}

AFORC_Status aforc_tilemap_get(const AFORC_TileMap *map,
                               uint32_t layer,
                               AFORC_Point point,
                               AFORC_Tile *out_tile)
{
    AFORC_Status status;

    if (out_tile == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, point);
    if (status != AFORC_OK)
    {
        return status;
    }
    *out_tile = map->tiles[aforc_world_tile_index_internal(map, layer, point)];
    return AFORC_OK;
}

AFORC_Status aforc_tilemap_set(AFORC_TileMap *map,
                               uint32_t layer,
                               AFORC_Point point,
                               AFORC_Tile tile)
{
    const AFORC_Status status =
        aforc_world_validate_layer_point_internal(map, layer, point);
    if (status != AFORC_OK)
    {
        return status;
    }
    map->tiles[aforc_world_tile_index_internal(map, layer, point)] = tile;
    return AFORC_OK;
}

AFORC_Status
aforc_tilemap_fill_layer(AFORC_TileMap *map, uint32_t layer, AFORC_Tile tile)
{
    AFORC_Status status = aforc_world_validate_layer_internal(map, layer);
    size_t index;
    size_t offset;

    if (status != AFORC_OK)
    {
        return status;
    }
    offset = (size_t)layer * map->cell_count;
    for (index = 0U; index < map->cell_count; ++index)
    {
        map->tiles[offset + index] = tile;
    }
    return AFORC_OK;
}

AFORC_Status aforc_tilemap_fill_rect(AFORC_TileMap *map,
                                     uint32_t layer,
                                     AFORC_Rect rect,
                                     AFORC_Tile tile)
{
    AFORC_Status status =
        aforc_world_validate_region_internal(map, layer, rect);
    size_t row;
    size_t column;

    if (status != AFORC_OK)
    {
        return status;
    }
    for (row = 0U; row < (size_t)rect.height; ++row)
    {
        const AFORC_Point row_start = {
            rect.x, (int32_t)((int64_t)rect.y + (int64_t)row)};
        const size_t offset =
            aforc_world_tile_index_internal(map, layer, row_start);
        for (column = 0U; column < (size_t)rect.width; ++column)
        {
            map->tiles[offset + column] = tile;
        }
    }
    return AFORC_OK;
}
