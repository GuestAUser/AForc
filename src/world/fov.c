/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

#include <string.h>

/*
 * Iterative recursive shadowcasting across eight octants.
 *
 * Rational slopes avoid floating-point boundary drift. An explicit growable
 * task stack replaces recursion, keeping failure propagation and scratch
 * ownership visible while preserving wall and map-edge shadow semantics.
 */

typedef struct FovSlope {
    int64_t numerator;
    uint64_t denominator;
} FovSlope;

typedef struct FovTask {
    uint32_t row;
    FovSlope start;
    FovSlope end;
    int32_t xx;
    int32_t xy;
    int32_t yx;
    int32_t yy;
} FovTask;

#define FOV_INLINE_TASK_CAPACITY 16U

typedef struct FovStack {
    const AFORC_Allocator *allocator;
    FovTask *items;
    size_t count;
    size_t capacity;
    FovTask inline_items[FOV_INLINE_TASK_CAPACITY];
} FovStack;

static int fov_slope_compare(FovSlope left, FovSlope right) {
    const bool left_negative = left.numerator < 0;
    const bool right_negative = right.numerator < 0;
    uint64_t left_magnitude;
    uint64_t right_magnitude;
    uint64_t left_product;
    uint64_t right_product;

    if (left_negative != right_negative) {
        return left_negative ? -1 : 1;
    }
    left_magnitude = left_negative ? (uint64_t)(-left.numerator)
                                   : (uint64_t)left.numerator;
    right_magnitude = right_negative ? (uint64_t)(-right.numerator)
                                     : (uint64_t)right.numerator;
    left_product = left_magnitude * right.denominator;
    right_product = right_magnitude * left.denominator;
    if (left_product == right_product) {
        return 0;
    }
    if (left_negative) {
        return left_product > right_product ? -1 : 1;
    }
    return left_product < right_product ? -1 : 1;
}

static AFORC_Status fov_stack_push(FovStack *stack, FovTask task) {
    if (stack->count == stack->capacity) {
        FovTask *replacement = NULL;
        size_t new_capacity;
        AFORC_Status status;

        if (stack->capacity > SIZE_MAX / 2U) {
            return AFORC_ERROR_OVERFLOW;
        }
        new_capacity = stack->capacity * 2U;
        if (stack->items == stack->inline_items) {
            status = aforc_alloc_array(stack->allocator, new_capacity,
                                       sizeof(*replacement),
                                       (void **)&replacement);
            if (status == AFORC_OK) {
                (void)memcpy(replacement, stack->items,
                             stack->count * sizeof(*replacement));
            }
        } else {
            status = aforc_realloc_array(stack->allocator, stack->items,
                                         new_capacity, sizeof(*replacement),
                                         (void **)&replacement);
        }
        if (status != AFORC_OK) {
            return status;
        }
        stack->items = replacement;
        stack->capacity = new_capacity;
    }
    stack->items[stack->count++] = task;
    return AFORC_OK;
}

static bool fov_within_radius(int64_t delta_x, int64_t delta_y,
                              uint32_t radius) {
    const uint64_t x = delta_x < 0 ? (uint64_t)(-delta_x)
                                   : (uint64_t)delta_x;
    const uint64_t y = delta_y < 0 ? (uint64_t)(-delta_y)
                                   : (uint64_t)delta_y;
    const uint64_t squared_radius = (uint64_t)radius * radius;
    const uint64_t squared_x = x * x;
    const uint64_t squared_y = y * y;

    return squared_x <= squared_radius &&
           squared_y <= squared_radius - squared_x;
}

static AFORC_Status fov_scan_task(const AFORC_TileMap *map, uint32_t layer,
                                AFORC_Point origin, uint32_t scan_radius,
                                uint32_t visibility_radius,
                                AFORC_TileTestFn is_opaque, void *context,
                                uint8_t *visibility, FovTask task,
                                FovStack *stack) {
    FovSlope start = task.start;
    FovSlope new_start = start;
    bool blocked = false;
    uint32_t distance = task.row;

    if (fov_slope_compare(start, task.end) < 0) {
        return AFORC_OK;
    }
    while (distance <= scan_radius && !blocked) {
        const int64_t delta_y = -(int64_t)distance;
        int64_t delta_x = -(int64_t)distance;

        blocked = false;
        while (delta_x <= 0) {
            const int64_t lateral = -delta_x;
            const FovSlope left = {
                lateral * 2 + 1, (uint64_t)distance * 2U - 1U};
            const FovSlope right = {
                lateral * 2 - 1, (uint64_t)distance * 2U + 1U};
            const int64_t map_x =
                (int64_t)origin.x + delta_x * task.xx + delta_y * task.xy;
            const int64_t map_y =
                (int64_t)origin.y + delta_x * task.yx + delta_y * task.yy;
            bool opaque;

            ++delta_x;
            if (fov_slope_compare(start, right) < 0) {
                continue;
            }
            if (fov_slope_compare(task.end, left) > 0) {
                break;
            }
            if (aforc_world_coordinates_contained_internal(map, map_x, map_y)) {
                const AFORC_Point point = {(int32_t)map_x, (int32_t)map_y};
                const bool within =
                    fov_within_radius(map_x - origin.x, map_y - origin.y,
                                      visibility_radius);
                if (within) {
                    /* Boundary walls stay visible before casting their shadow. */
                    visibility[aforc_world_point_index_internal(map, point)] =
                        1U;
                    opaque = aforc_world_tile_matches_internal(
                        map, layer, point, is_opaque, context);
                } else {
                    opaque = false;
                }
            } else {
                /* Map edges close the scan so no slope leaks behind the map. */
                opaque = true;
            }
            if (blocked) {
                if (opaque) {
                    new_start = right;
                    continue;
                }
                blocked = false;
                start = new_start;
            } else if (opaque && distance < scan_radius) {
                FovTask child;
                AFORC_Status status;

                blocked = true;
                child = task;
                child.row = distance + 1U;
                child.start = start;
                child.end = left;
                status = fov_stack_push(stack, child);
                if (status != AFORC_OK) {
                    return status;
                }
                new_start = right;
            }
        }
        if (distance == scan_radius) {
            break;
        }
        ++distance;
    }
    return AFORC_OK;
}

static uint32_t fov_effective_radius(const AFORC_TileMap *map,
                                     AFORC_Point origin, uint32_t radius) {
    const uint32_t left = (uint32_t)origin.x;
    const uint32_t right = (uint32_t)(map->size.width - 1 - origin.x);
    const uint32_t top = (uint32_t)origin.y;
    const uint32_t bottom = (uint32_t)(map->size.height - 1 - origin.y);
    uint32_t farthest = left > right ? left : right;

    if (top > farthest) {
        farthest = top;
    }
    if (bottom > farthest) {
        farthest = bottom;
    }
    return radius < farthest ? radius : farthest;
}

AFORC_Status aforc_fov_compute(const AFORC_TileMap *map, uint32_t layer,
                           AFORC_Point origin, uint32_t radius,
                           AFORC_TileTestFn is_opaque, void *context,
                           uint8_t *out_visibility,
                           size_t visibility_size) {
    static const int32_t transforms[8][4] = {
        {1, 0, 0, 1},   {0, 1, 1, 0},   {0, 1, -1, 0},
        {-1, 0, 0, 1},  {-1, 0, 0, -1}, {0, -1, -1, 0},
        {0, -1, 1, 0},  {1, 0, 0, -1}};
    FovStack stack;
    uint32_t effective_radius;
    size_t octant;
    AFORC_Status status;

    if (out_visibility == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_world_validate_layer_point_internal(map, layer, origin);
    if (status != AFORC_OK) {
        return status;
    }
    if (visibility_size < map->cell_count) {
        return AFORC_ERROR_LIMIT;
    }
    (void)memset(out_visibility, 0, map->cell_count);
    out_visibility[aforc_world_point_index_internal(map, origin)] = 1U;
    effective_radius = fov_effective_radius(map, origin, radius);
    if (effective_radius == 0U) {
        return AFORC_OK;
    }
    stack.allocator = &map->allocator;
    stack.items = stack.inline_items;
    stack.count = 0U;
    stack.capacity = FOV_INLINE_TASK_CAPACITY;

    for (octant = 0U; octant < 8U; ++octant) {
        FovTask root;

        root.row = 1U;
        root.start.numerator = 1;
        root.start.denominator = 1U;
        root.end.numerator = 0;
        root.end.denominator = 1U;
        root.xx = transforms[octant][0];
        root.xy = transforms[octant][1];
        root.yx = transforms[octant][2];
        root.yy = transforms[octant][3];
        status = fov_stack_push(&stack, root);
        if (status != AFORC_OK) {
            break;
        }
        while (stack.count != 0U) {
            const FovTask task = stack.items[--stack.count];
            status = fov_scan_task(map, layer, origin, effective_radius,
                                   radius, is_opaque, context, out_visibility,
                                   task, &stack);
            if (status != AFORC_OK) {
                break;
            }
        }
        if (status != AFORC_OK) {
            break;
        }
    }
    if (stack.items != stack.inline_items) {
        aforc_free(&map->allocator, stack.items);
    }
    return status;
}
