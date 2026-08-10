/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "astar_workspace_support.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        if (!(condition))                                                      \
        {                                                                      \
            (void)fprintf(stderr,                                              \
                          "check failed at %s:%d: %s\n",                       \
                          __FILE__,                                            \
                          __LINE__,                                            \
                          #condition);                                         \
            return false;                                                      \
        }                                                                      \
    } while (false)

static bool test_path_and_status_parity(void)
{
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    AFORC_PathOptions options = aforc_path_options_default();
    const AFORC_Point start = {0, 0};
    const AFORC_Point goal = {4, 4};

    CHECK(aforc_tilemap_create((AFORC_Size){5, 5}, 1U, 0U, &allocator, &map) ==
          AFORC_OK);
    CHECK(astar_test_create_workspace(&allocator, 25U, &workspace));
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    options.flags = AFORC_PATH_ALLOW_DIAGONAL;
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){1, 0}, 1U) == AFORC_OK);
    CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){0, 1}, 1U) == AFORC_OK);
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    options.flags |= AFORC_PATH_PREVENT_CORNER_CUTTING;
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    CHECK(aforc_tilemap_fill_layer(map, 0U, 0U) == AFORC_OK);
    options = aforc_path_options_default();
    options.max_visited = 1U;
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    options.max_visited = 0U;
    CHECK(aforc_tilemap_fill_rect(map, 0U, (AFORC_Rect){0, 2, 5, 1}, 1U) ==
          AFORC_OK);
    CHECK(astar_test_compare_query(workspace, map, start, goal, &options));
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    return true;
}

static bool test_sizing_compatibility_and_determinism(void)
{
    AstarTestTrackingAllocator tracking = {0};
    AFORC_Allocator allocator = astar_test_tracking_allocator(&tracking);
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    const AFORC_Point *reused_points = NULL;
    AFORC_Point *legacy_points = NULL;
    AFORC_PathOptions options = aforc_path_options_default();
    size_t required = 0U;
    size_t legacy_length = 0U;
    size_t reused_length = 0U;
    size_t allocations_before;
    size_t index;

    CHECK(aforc_tilemap_create((AFORC_Size){8, 6}, 1U, 0U, &allocator, &map) ==
          AFORC_OK);
    CHECK(astar_test_create_workspace(&allocator, 48U, &workspace));
    CHECK(aforc_pathfind_astar(map,
                               0U,
                               (AFORC_Point){0, 0},
                               (AFORC_Point){7, 5},
                               astar_test_tile_blocked,
                               NULL,
                               &options,
                               NULL,
                               0U,
                               &required) == AFORC_ERROR_LIMIT);
    CHECK(required > 1U);
    legacy_points = malloc(required * sizeof(*legacy_points));
    CHECK(legacy_points != NULL);
    CHECK(aforc_pathfind_astar(map,
                               0U,
                               (AFORC_Point){0, 0},
                               (AFORC_Point){7, 5},
                               astar_test_tile_blocked,
                               NULL,
                               &options,
                               legacy_points,
                               required,
                               &legacy_length) == AFORC_OK);
    allocations_before = tracking.allocations;
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){7, 5},
                                         astar_test_tile_blocked,
                                         NULL,
                                         &options,
                                         &reused_points,
                                         &reused_length) == AFORC_OK);
    CHECK(tracking.allocations == allocations_before);
    CHECK(reused_length == legacy_length);
    CHECK(reused_length > 1U);
    CHECK(aforc_world_point_equal(reused_points[1], (AFORC_Point){1, 0}));
    for (index = 0U; index < 1000U; ++index)
    {
        const AFORC_Point goal =
            (index & 1U) == 0U ? (AFORC_Point){7, 5} : (AFORC_Point){0, 5};
        CHECK(astar_test_compare_query(
            workspace, map, (AFORC_Point){0, 0}, goal, &options));
    }
    CHECK(tracking.allocations == allocations_before + 2000U);
    free(legacy_points);
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    CHECK(tracking.live == 0U);
    return true;
}

static bool test_allocator_fail_points(void)
{
    size_t failure;

    for (failure = 0U; failure <= 3U; ++failure)
    {
        AstarTestTrackingAllocator tracking = {0};
        AFORC_Allocator allocator = astar_test_tracking_allocator(&tracking);
        AFORC_PathWorkspace *workspace = NULL;

        tracking.fail_at = failure == 0U ? 1U : 0U;
        if (failure == 0U)
        {
            CHECK(aforc_path_workspace_create(&allocator, &workspace) ==
                  AFORC_ERROR_OUT_OF_MEMORY);
        }
        else
        {
            CHECK(aforc_path_workspace_create(&allocator, &workspace) ==
                  AFORC_OK);
            tracking.fail_at = tracking.attempts + failure;
            CHECK(aforc_path_workspace_reserve(workspace, 64U) ==
                  AFORC_ERROR_OUT_OF_MEMORY);
        }
        aforc_path_workspace_destroy(workspace);
        CHECK(tracking.live == 0U);
    }
    for (failure = 1U; failure <= 3U; ++failure)
    {
        AstarTestTrackingAllocator tracking = {0};
        AFORC_Allocator allocator = astar_test_tracking_allocator(&tracking);
        AFORC_TileMap *map = NULL;
        AFORC_PathWorkspace *workspace = NULL;
        const AFORC_Point *points = NULL;
        size_t length = 0U;

        CHECK(aforc_tilemap_create(
                  (AFORC_Size){3, 3}, 1U, 0U, &allocator, &map) == AFORC_OK);
        CHECK(astar_test_create_workspace(&allocator, 9U, &workspace));
        tracking.fail_at = tracking.attempts + failure;
        CHECK(aforc_path_workspace_reserve(workspace, 64U) ==
              AFORC_ERROR_OUT_OF_MEMORY);
        CHECK(tracking.live == 6U);
        CHECK(aforc_pathfind_astar_workspace(workspace,
                                             map,
                                             0U,
                                             (AFORC_Point){0, 0},
                                             (AFORC_Point){2, 2},
                                             astar_test_tile_blocked,
                                             NULL,
                                             NULL,
                                             &points,
                                             &length) == AFORC_OK);
        CHECK(length == 5U);
        aforc_path_workspace_destroy(workspace);
        aforc_tilemap_destroy(map);
        CHECK(tracking.live == 0U);
    }
    return true;
}

static bool test_epoch_wrap_reset(void)
{
    AstarTestTrackingAllocator tracking = {0};
    AFORC_Allocator allocator = astar_test_tracking_allocator(&tracking);
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    const AFORC_Point *points = NULL;
    size_t length = 0U;
    size_t allocations_before;
    uint32_t index;

    CHECK(aforc_tilemap_create((AFORC_Size){3, 3}, 1U, 0U, &allocator, &map) ==
          AFORC_OK);
    CHECK(astar_test_create_workspace(&allocator, 9U, &workspace));
    allocations_before = tracking.allocations;
    for (index = 0U; index < UINT16_MAX; ++index)
    {
        CHECK(aforc_pathfind_astar_workspace(workspace,
                                             map,
                                             0U,
                                             (AFORC_Point){0, 0},
                                             (AFORC_Point){2, 2},
                                             astar_test_tile_blocked,
                                             NULL,
                                             NULL,
                                             &points,
                                             &length) == AFORC_OK);
        CHECK(length == 5U);
    }
    CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){1, 0}, 1U) == AFORC_OK);
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){2, 0},
                                         astar_test_tile_blocked,
                                         NULL,
                                         NULL,
                                         &points,
                                         &length) == AFORC_OK);
    CHECK(length == 5U);
    CHECK(aforc_world_point_equal(points[1], (AFORC_Point){0, 1}));
    CHECK(tracking.allocations == allocations_before);
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    CHECK(tracking.live == 0U);
    return true;
}

static bool test_option_boundaries(void)
{
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    AFORC_PathOptions options = aforc_path_options_default();
    const AFORC_Point *points = NULL;
    size_t length = 0U;

    CHECK(aforc_tilemap_create((AFORC_Size){3, 3}, 1U, 0U, &allocator, &map) ==
          AFORC_OK);
    CHECK(astar_test_create_workspace(&allocator, 9U, &workspace));
    options.flags = AFORC_PATH_PREVENT_CORNER_CUTTING;
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){2, 2},
                                         astar_test_tile_blocked,
                                         NULL,
                                         &options,
                                         &points,
                                         &length) == AFORC_OK);
    CHECK(length == 5U);
    options.flags = UINT32_C(1) << 31;
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){2, 0},
                                         astar_test_tile_blocked,
                                         NULL,
                                         &options,
                                         &points,
                                         &length) ==
          AFORC_ERROR_INVALID_ARGUMENT);
    options = aforc_path_options_default();
    options.max_visited = 2U;
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){2, 0},
                                         astar_test_tile_blocked,
                                         NULL,
                                         &options,
                                         &points,
                                         &length) == AFORC_ERROR_LIMIT);
    options.max_visited = 3U;
    CHECK(aforc_pathfind_astar_workspace(workspace,
                                         map,
                                         0U,
                                         (AFORC_Point){0, 0},
                                         (AFORC_Point){2, 0},
                                         astar_test_tile_blocked,
                                         NULL,
                                         &options,
                                         &points,
                                         &length) == AFORC_OK);
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    return true;
}

int main(void)
{
    if (!test_path_and_status_parity())
        return 1;
    if (!test_sizing_compatibility_and_determinism())
        return 2;
    if (!test_allocator_fail_points())
        return 3;
    if (!test_epoch_wrap_reset())
        return 4;
    if (!test_option_boundaries())
        return 5;
    if (!astar_test_benchmark_size(72, 36, 1000U))
        return 6;
    if (!astar_test_benchmark_size(120, 60, 1000U))
        return 7;
    (void)puts("astar workspace: ok");
    return 0;
}
