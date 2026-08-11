/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TESTS_ASTAR_WORKSPACE_SUPPORT_H
#define AFORC_TESTS_ASTAR_WORKSPACE_SUPPORT_H

#include "aforc/world.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct AstarTestTrackingAllocator
{
    size_t attempts;
    size_t allocations;
    size_t frees;
    size_t live;
    size_t fail_at;
} AstarTestTrackingAllocator;

AFORC_Allocator
astar_test_tracking_allocator(AstarTestTrackingAllocator *tracking);
bool astar_test_tile_blocked(AFORC_Tile tile,
                             uint32_t layer,
                             AFORC_Point position,
                             void *context);
bool astar_test_compare_query(AFORC_PathWorkspace *workspace,
                              const AFORC_TileMap *map,
                              AFORC_Point start,
                              AFORC_Point goal,
                              const AFORC_PathOptions *options);
bool astar_test_create_workspace(const AFORC_Allocator *allocator,
                                 size_t cell_capacity,
                                 AFORC_PathWorkspace **out_workspace);
bool astar_test_workspace_reuse(int32_t width,
                                int32_t height,
                                size_t iterations);

#endif
