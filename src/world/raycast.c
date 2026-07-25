/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

/*
 * Deterministic integer supercover traversal.
 *
 * Every grid cell touched by the segment is tested, including both cells at
 * exact corner crossings. No floating-point rounding or temporary storage can
 * change callback order or the reported first hit.
 */

AFORC_Status aforc_grid_raycast(const AFORC_TileMap *map, uint32_t layer,
                            AFORC_Point start, AFORC_Point end,
                            AFORC_TileTestFn is_blocked, void *context,
                            bool *out_blocked, AFORC_Point *out_hit) {
    AFORC_Status status;
    int64_t x;
    int64_t y;
    int64_t delta_x;
    int64_t delta_y;
    int64_t step_x;
    int64_t step_y;
    int64_t steps_x;
    int64_t steps_y;

    if (out_blocked == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_blocked = false;
    if (out_hit != NULL) {
        *out_hit = end;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, start);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, end);
    if (status != AFORC_OK) {
        return status;
    }
    x = start.x;
    y = start.y;
    delta_x = end.x >= start.x ? (int64_t)end.x - start.x
                               : (int64_t)start.x - end.x;
    delta_y = end.y >= start.y ? (int64_t)end.y - start.y
                               : (int64_t)start.y - end.y;
    step_x = start.x < end.x ? 1 : -1;
    step_y = start.y < end.y ? 1 : -1;
    steps_x = 0;
    steps_y = 0;

    for (;;) {
        const AFORC_Point point = {(int32_t)x, (int32_t)y};
        if (aforc_world_tile_matches_internal(map, layer, point, is_blocked,
                                            context)) {
            *out_blocked = true;
            if (out_hit != NULL) {
                *out_hit = point;
            }
            return AFORC_OK;
        }
        if (x == end.x && y == end.y) {
            return AFORC_OK;
        }
        if (((steps_x * 2) + 1) * delta_y ==
            ((steps_y * 2) + 1) * delta_x) {
            const AFORC_Point horizontal = {(int32_t)(x + step_x), (int32_t)y};
            const AFORC_Point vertical = {(int32_t)x, (int32_t)(y + step_y)};

            /*
             * An exact corner crossing touches both side cells before the
             * diagonal cell. Horizontal precedes vertical for stable hits.
             */
            if (aforc_world_tile_matches_internal(map, layer, horizontal,
                                                is_blocked, context)) {
                *out_blocked = true;
                if (out_hit != NULL) {
                    *out_hit = horizontal;
                }
                return AFORC_OK;
            }
            if (aforc_world_tile_matches_internal(map, layer, vertical,
                                                is_blocked, context)) {
                *out_blocked = true;
                if (out_hit != NULL) {
                    *out_hit = vertical;
                }
                return AFORC_OK;
            }
            x += step_x;
            y += step_y;
            ++steps_x;
            ++steps_y;
        } else if (((steps_x * 2) + 1) * delta_y <
                   ((steps_y * 2) + 1) * delta_x) {
            x += step_x;
            ++steps_x;
        } else {
            y += step_y;
            ++steps_y;
        }
    }
}
