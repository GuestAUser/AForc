/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_test_support.h"

#include <stdint.h>

#define ASTAR_TEST_WIDTH 3
#define ASTAR_TEST_CELLS 9U
#define ASTAR_TEST_UNREACHABLE UINT64_MAX

static size_t astar_index(AFORC_Point point) {
    return (size_t)point.y * ASTAR_TEST_WIDTH + (size_t)point.x;
}

static bool astar_mask_blocked(uint16_t mask, AFORC_Point point) {
    return (mask & (uint16_t)(UINT16_C(1) << astar_index(point))) != 0U;
}

static uint64_t astar_oracle_cost(uint16_t mask, AFORC_Point start,
                                  AFORC_Point goal, uint32_t flags) {
    static const int32_t directions[8][2] = {
        {0, -1}, {-1, 0}, {1, 0}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    uint64_t distances[ASTAR_TEST_CELLS];
    bool closed[ASTAR_TEST_CELLS] = {false};
    const bool diagonal = (flags & AFORC_PATH_ALLOW_DIAGONAL) != 0U;
    const bool prevent =
        (flags & AFORC_PATH_PREVENT_CORNER_CUTTING) != 0U;
    size_t iteration;
    size_t index;

    if (astar_mask_blocked(mask, start) || astar_mask_blocked(mask, goal)) {
        return ASTAR_TEST_UNREACHABLE;
    }
    for (index = 0U; index < ASTAR_TEST_CELLS; ++index) {
        distances[index] = ASTAR_TEST_UNREACHABLE;
    }
    distances[astar_index(start)] = 0U;
    for (iteration = 0U; iteration < ASTAR_TEST_CELLS; ++iteration) {
        size_t best = SIZE_MAX;
        size_t direction;
        AFORC_Point current;

        for (index = 0U; index < ASTAR_TEST_CELLS; ++index) {
            if (!closed[index] && distances[index] != ASTAR_TEST_UNREACHABLE &&
                (best == SIZE_MAX || distances[index] < distances[best])) {
                best = index;
            }
        }
        if (best == SIZE_MAX) {
            break;
        }
        current = (AFORC_Point){(int32_t)(best % ASTAR_TEST_WIDTH),
                                (int32_t)(best / ASTAR_TEST_WIDTH)};
        if (aforc_world_point_equal(current, goal)) {
            return distances[best];
        }
        closed[best] = true;
        for (direction = 0U; direction < (diagonal ? 8U : 4U); ++direction) {
            const int32_t offset_x = directions[direction][0];
            const int32_t offset_y = directions[direction][1];
            const AFORC_Point neighbor = {current.x + offset_x,
                                          current.y + offset_y};
            size_t neighbor_index;
            uint64_t candidate;

            if (neighbor.x < 0 || neighbor.y < 0 ||
                neighbor.x >= ASTAR_TEST_WIDTH ||
                neighbor.y >= ASTAR_TEST_WIDTH ||
                astar_mask_blocked(mask, neighbor)) {
                continue;
            }
            if (offset_x != 0 && offset_y != 0 && prevent &&
                (astar_mask_blocked(mask,
                                    (AFORC_Point){neighbor.x, current.y}) ||
                 astar_mask_blocked(mask,
                                    (AFORC_Point){current.x, neighbor.y}))) {
                continue;
            }
            neighbor_index = astar_index(neighbor);
            candidate = distances[best] +
                        (offset_x != 0 && offset_y != 0 ? 14U : 10U);
            if (candidate < distances[neighbor_index]) {
                distances[neighbor_index] = candidate;
            }
        }
    }
    return ASTAR_TEST_UNREACHABLE;
}

static bool astar_path_is_valid(uint16_t mask,
                                const AFORC_PathOptions *options,
                                const AFORC_Point *points, size_t length,
                                AFORC_Point start, AFORC_Point goal,
                                uint64_t expected_cost) {
    uint64_t cost = 0U;
    size_t index;

    if (points == NULL || length == 0U ||
        !aforc_world_point_equal(points[0], start) ||
        !aforc_world_point_equal(points[length - 1U], goal)) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        if (points[index].x < 0 || points[index].y < 0 ||
            points[index].x >= ASTAR_TEST_WIDTH ||
            points[index].y >= ASTAR_TEST_WIDTH ||
            astar_mask_blocked(mask, points[index])) {
            return false;
        }
        if (index != 0U) {
            const int32_t delta_x = points[index].x - points[index - 1U].x;
            const int32_t delta_y = points[index].y - points[index - 1U].y;
            const bool diagonal = delta_x != 0 && delta_y != 0;

            if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
                (delta_x == 0 && delta_y == 0) ||
                (diagonal &&
                 (options->flags & AFORC_PATH_ALLOW_DIAGONAL) == 0U)) {
                return false;
            }
            if (diagonal &&
                (options->flags & AFORC_PATH_PREVENT_CORNER_CUTTING) != 0U &&
                (astar_mask_blocked(
                     mask, (AFORC_Point){points[index].x,
                                         points[index - 1U].y}) ||
                 astar_mask_blocked(
                     mask, (AFORC_Point){points[index - 1U].x,
                                         points[index].y}))) {
                return false;
            }
            cost += diagonal ? 14U : 10U;
        }
    }
    return cost == expected_cost;
}

static bool test_exhaustive_small_maps(void) {
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    uint16_t mask;

    WORLD_TEST_CHECK(aforc_tilemap_create((AFORC_Size){3, 3}, 1U, 0U,
                                           &allocator, &map) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_path_workspace_create(&allocator, &workspace) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(aforc_path_workspace_reserve(workspace,
                                                  ASTAR_TEST_CELLS) ==
                     AFORC_OK);
    for (mask = 0U; mask < (UINT16_C(1) << ASTAR_TEST_CELLS); ++mask) {
        size_t cell;
        uint32_t mode;

        WORLD_TEST_CHECK(aforc_tilemap_fill_layer(map, 0U, 0U) == AFORC_OK);
        for (cell = 0U; cell < ASTAR_TEST_CELLS; ++cell) {
            if ((mask & (uint16_t)(UINT16_C(1) << cell)) != 0U) {
                const AFORC_Point point = {
                    (int32_t)(cell % ASTAR_TEST_WIDTH),
                    (int32_t)(cell / ASTAR_TEST_WIDTH)};
                WORLD_TEST_CHECK(aforc_tilemap_set(map, 0U, point, 1U) ==
                                 AFORC_OK);
            }
        }
        for (mode = 0U; mode < 3U; ++mode) {
            AFORC_PathOptions options = aforc_path_options_default();
            size_t start_index;

            if (mode != 0U) {
                options.flags |= AFORC_PATH_ALLOW_DIAGONAL;
            }
            if (mode == 2U) {
                options.flags |= AFORC_PATH_PREVENT_CORNER_CUTTING;
            }
            for (start_index = 0U; start_index < ASTAR_TEST_CELLS;
                 ++start_index) {
                size_t goal_index;

                for (goal_index = 0U; goal_index < ASTAR_TEST_CELLS;
                     ++goal_index) {
                    const AFORC_Point start = {
                        (int32_t)(start_index % ASTAR_TEST_WIDTH),
                        (int32_t)(start_index / ASTAR_TEST_WIDTH)};
                    const AFORC_Point goal = {
                        (int32_t)(goal_index % ASTAR_TEST_WIDTH),
                        (int32_t)(goal_index / ASTAR_TEST_WIDTH)};
                    const uint64_t expected =
                        astar_oracle_cost(mask, start, goal, options.flags);
                    const AFORC_Point *points = NULL;
                    size_t length = 0U;
                    const AFORC_Status status = aforc_pathfind_astar_workspace(
                        workspace, map, 0U, start, goal,
                        world_test_tile_nonzero, NULL, &options, &points,
                        &length);

                    if (expected == ASTAR_TEST_UNREACHABLE) {
                        WORLD_TEST_CHECK(status == AFORC_ERROR_NOT_FOUND &&
                                         points == NULL && length == 0U);
                    } else {
                        WORLD_TEST_CHECK(status == AFORC_OK);
                        WORLD_TEST_CHECK(astar_path_is_valid(
                            mask, &options, points, length, start, goal,
                            expected));
                    }
                }
            }
        }
    }
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    return true;
}

bool world_test_astar_cases(void) {
    return test_exhaustive_small_maps();
}
