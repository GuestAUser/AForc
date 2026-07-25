/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "astar_workspace_support.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct QueryResult {
    AFORC_Status status;
    AFORC_Point *points;
    size_t length;
} QueryResult;

static void *tracking_allocate(void *context, size_t size) {
    AstarTestTrackingAllocator *tracking = context;
    void *memory;

    ++tracking->attempts;
    if (tracking->fail_at != 0U && tracking->attempts == tracking->fail_at) {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL) {
        ++tracking->allocations;
        ++tracking->live;
    }
    return memory;
}

static void *tracking_reallocate(void *context, void *memory, size_t size) {
    AstarTestTrackingAllocator *tracking = context;
    const bool new_allocation = memory == NULL;
    void *replacement;

    ++tracking->attempts;
    if (tracking->fail_at != 0U && tracking->attempts == tracking->fail_at) {
        return NULL;
    }
    replacement = realloc(memory, size);
    if (replacement != NULL && new_allocation) {
        ++tracking->allocations;
        ++tracking->live;
    }
    return replacement;
}

static void tracking_free(void *context, void *memory) {
    AstarTestTrackingAllocator *tracking = context;

    if (memory != NULL) {
        free(memory);
        ++tracking->frees;
        --tracking->live;
    }
}

AFORC_Allocator astar_test_tracking_allocator(
    AstarTestTrackingAllocator *tracking) {
    const AFORC_Allocator allocator = {
        tracking, tracking_allocate, tracking_reallocate, tracking_free};
    return allocator;
}

bool astar_test_tile_blocked(AFORC_Tile tile, uint32_t layer,
                             AFORC_Point position, void *context) {
    (void)layer;
    (void)position;
    (void)context;
    return tile != 0U;
}

bool astar_test_point_equal(AFORC_Point left, AFORC_Point right) {
    return left.x == right.x && left.y == right.y;
}

static bool path_equal(const AFORC_Point *left, const AFORC_Point *right,
                       size_t length) {
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (!astar_test_point_equal(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

static QueryResult legacy_query(const AFORC_TileMap *map, AFORC_Point start,
                                AFORC_Point goal,
                                const AFORC_PathOptions *options) {
    QueryResult result = {AFORC_ERROR_STATE, NULL, 0U};
    size_t cell_count = 0U;

    if (aforc_tilemap_cell_count(map, &cell_count) != AFORC_OK) {
        return result;
    }
    result.points = malloc(cell_count * sizeof(*result.points));
    if (result.points == NULL) {
        result.status = AFORC_ERROR_OUT_OF_MEMORY;
        return result;
    }
    result.status = aforc_pathfind_astar(
        map, 0U, start, goal, astar_test_tile_blocked, NULL, options,
        result.points, cell_count, &result.length);
    return result;
}

bool astar_test_compare_query(AFORC_PathWorkspace *workspace,
                              const AFORC_TileMap *map, AFORC_Point start,
                              AFORC_Point goal,
                              const AFORC_PathOptions *options) {
    QueryResult legacy = legacy_query(map, start, goal, options);
    const AFORC_Point *reused_points = NULL;
    size_t reused_length = 0U;
    const AFORC_Status reused_status = aforc_pathfind_astar_workspace(
        workspace, map, 0U, start, goal, astar_test_tile_blocked, NULL, options,
        &reused_points, &reused_length);
    const bool passed =
        legacy.status == reused_status && legacy.length == reused_length &&
        (legacy.status != AFORC_OK ||
         (reused_points != NULL &&
          path_equal(legacy.points, reused_points, legacy.length)));

    free(legacy.points);
    return passed;
}

bool astar_test_create_workspace(const AFORC_Allocator *allocator,
                                 size_t cell_capacity,
                                 AFORC_PathWorkspace **out_workspace) {
    AFORC_Status status =
        aforc_path_workspace_create(allocator, out_workspace);

    if (status == AFORC_OK) {
        status =
            aforc_path_workspace_reserve(*out_workspace, cell_capacity);
    }
    if (status != AFORC_OK) {
        aforc_path_workspace_destroy(*out_workspace);
        *out_workspace = NULL;
    }
    return status == AFORC_OK;
}

static uint64_t timestamp_ns(void) {
    struct timespec value;

    if (timespec_get(&value, TIME_UTC) != TIME_UTC || value.tv_sec < 0) {
        return 0U;
    }
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) +
           (uint64_t)value.tv_nsec;
}

bool astar_test_benchmark_size(int32_t width, int32_t height,
                               size_t iterations) {
    AstarTestTrackingAllocator tracking = {0};
    AFORC_Allocator allocator = astar_test_tracking_allocator(&tracking);
    AFORC_TileMap *map = NULL;
    AFORC_PathWorkspace *workspace = NULL;
    AFORC_Point *legacy_points = NULL;
    const AFORC_Point *reused_points = NULL;
    AFORC_PathOptions options = aforc_path_options_default();
    const AFORC_Point start = {0, 0};
    const AFORC_Point goal = {width - 1, height - 1};
    const size_t cells =
        (size_t)(uint32_t)width * (size_t)(uint32_t)height;
    size_t length = 0U;
    size_t index;
    size_t allocations_before;
    size_t legacy_allocations;
    size_t workspace_allocations;
    uint64_t start_ns;
    uint64_t legacy_ns;
    uint64_t workspace_ns;
    AFORC_Status legacy_status = AFORC_ERROR_STATE;
    AFORC_Status workspace_status = AFORC_ERROR_STATE;
    bool passed = false;

    if (aforc_tilemap_create((AFORC_Size){width, height}, 1U, 0U,
                             &allocator, &map) != AFORC_OK ||
        !astar_test_create_workspace(&allocator, cells, &workspace)) {
        goto cleanup;
    }
    legacy_points = malloc(cells * sizeof(*legacy_points));
    if (legacy_points == NULL) {
        goto cleanup;
    }
    options.max_visited = 128U;
    allocations_before = tracking.allocations;
    start_ns = timestamp_ns();
    for (index = 0U; start_ns != 0U && index < iterations; ++index) {
        legacy_status = aforc_pathfind_astar(
            map, 0U, start, goal, astar_test_tile_blocked, NULL, &options,
            legacy_points, cells, &length);
    }
    legacy_ns = timestamp_ns() - start_ns;
    legacy_allocations = tracking.allocations - allocations_before;
    allocations_before = tracking.allocations;
    start_ns = timestamp_ns();
    for (index = 0U; start_ns != 0U && index < iterations; ++index) {
        workspace_status = aforc_pathfind_astar_workspace(
            workspace, map, 0U, start, goal, astar_test_tile_blocked, NULL,
            &options, &reused_points, &length);
    }
    workspace_ns = timestamp_ns() - start_ns;
    workspace_allocations = tracking.allocations - allocations_before;
    passed = start_ns != 0U && legacy_status == workspace_status &&
             workspace_allocations == 0U;
    (void)printf(
        "astar benchmark %dx%d max_visited=128 iterations=%zu "
        "legacy_ns=%" PRIu64 " workspace_ns=%" PRIu64
        " legacy_allocations=%zu workspace_query_allocations=%zu\n",
        width, height, iterations, legacy_ns, workspace_ns,
        legacy_allocations, workspace_allocations);

cleanup:
    free(legacy_points);
    aforc_path_workspace_destroy(workspace);
    aforc_tilemap_destroy(map);
    return passed && tracking.live == 0U;
}
