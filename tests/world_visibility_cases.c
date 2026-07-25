/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_test_support.h"

#include <stdint.h>
#include <string.h>

#define FOV_SPILL_EXTENT 513
#define FOV_SPILL_CELLS ((size_t)FOV_SPILL_EXTENT * FOV_SPILL_EXTENT)

static bool test_raycast(void) {
    static const AFORC_Point expected[] = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 2}, {2, 2},
        {3, 2}, {2, 3}, {3, 3}, {4, 3}, {3, 4}, {4, 4}};
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    WorldTestTrace trace = {0};
    AFORC_Point hit = {-1, -1};
    bool blocked = false;
    size_t index;

    WORLD_TEST_CHECK(aforc_tilemap_create((AFORC_Size){5, 5}, 1U, 0U,
                                           &allocator, &map) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_grid_raycast(
                         map, 0U, (AFORC_Point){0, 0}, (AFORC_Point){4, 4},
                         world_test_trace_nonzero, &trace, &blocked, &hit) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(!blocked &&
                     aforc_world_point_equal(hit, (AFORC_Point){4, 4}) &&
                     !trace.overflowed &&
                     trace.count == sizeof(expected) / sizeof(expected[0]));
    for (index = 0U; index < trace.count; ++index) {
        WORLD_TEST_CHECK(aforc_world_point_equal(trace.points[index],
                                                 expected[index]));
    }
    WORLD_TEST_CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){1, 0}, 1U) ==
                     AFORC_OK);
    (void)memset(&trace, 0, sizeof(trace));
    WORLD_TEST_CHECK(aforc_grid_raycast(
                         map, 0U, (AFORC_Point){0, 0}, (AFORC_Point){4, 4},
                         world_test_trace_nonzero, &trace, &blocked, &hit) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(blocked && trace.count == 2U &&
                     aforc_world_point_equal(hit, (AFORC_Point){1, 0}));
    WORLD_TEST_CHECK(aforc_grid_raycast(
                         map, 0U, (AFORC_Point){-1, 0}, (AFORC_Point){4, 4},
                         world_test_trace_nonzero, &trace, &blocked, &hit) ==
                     AFORC_ERROR_NOT_FOUND);
    aforc_tilemap_destroy(map);
    return true;
}

static bool test_fov_geometry(void) {
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    uint8_t visibility[49];
    int32_t y;
    int32_t x;

    WORLD_TEST_CHECK(aforc_tilemap_create((AFORC_Size){7, 7}, 1U, 0U,
                                           &allocator, &map) == AFORC_OK);
    (void)memset(visibility, 0xA5, sizeof(visibility));
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){3, 3}, 2U, NULL, NULL,
                         visibility, sizeof(visibility) - 1U) ==
                     AFORC_ERROR_LIMIT);
    WORLD_TEST_CHECK(visibility[0] == UINT8_C(0xA5));
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){3, 3}, 2U, NULL, NULL,
                         visibility, sizeof(visibility)) == AFORC_OK);
    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 7; ++x) {
            const int32_t delta_x = x - 3;
            const int32_t delta_y = y - 3;
            const bool expected =
                delta_x * delta_x + delta_y * delta_y <= 4;
            WORLD_TEST_CHECK((visibility[(size_t)y * 7U + (size_t)x] != 0U) ==
                             expected);
        }
    }
    WORLD_TEST_CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){4, 3}, 1U) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){3, 3}, 3U,
                         world_test_tile_nonzero, NULL, visibility,
                         sizeof(visibility)) == AFORC_OK);
    WORLD_TEST_CHECK(visibility[3U * 7U + 4U] != 0U);
    WORLD_TEST_CHECK(visibility[3U * 7U + 5U] == 0U);
    WORLD_TEST_CHECK(visibility[3U * 7U + 6U] == 0U);
    WORLD_TEST_CHECK(aforc_tilemap_fill_layer(map, 0U, 0U) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){0, 0}, UINT32_MAX, NULL, NULL,
                         visibility, sizeof(visibility)) == AFORC_OK);
    for (x = 0; x < 49; ++x) {
        WORLD_TEST_CHECK(visibility[x] != 0U);
    }
    aforc_tilemap_destroy(map);
    return true;
}

static bool test_fov_common_case_is_allocation_free(void) {
    WorldTestAllocatorState state = {0};
    AFORC_Allocator allocator = world_test_tracking_allocator(&state);
    AFORC_TileMap *map = NULL;
    uint8_t visibility[81];
    size_t attempts_before;

    WORLD_TEST_CHECK(aforc_tilemap_create((AFORC_Size){9, 9}, 1U, 0U,
                                           &allocator, &map) == AFORC_OK);
    attempts_before = state.attempts;
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){4, 4}, 4U, NULL, NULL,
                         visibility, sizeof(visibility)) == AFORC_OK);
    WORLD_TEST_CHECK(state.attempts == attempts_before);
    aforc_tilemap_destroy(map);
    WORLD_TEST_CHECK(state.live == 0U);
    return true;
}

static uint32_t fov_spill_hash(uint32_t x, uint32_t y) {
    uint32_t value = x * UINT32_C(0x9e3779b1) ^
                     y * UINT32_C(0x85ebca6b) ^ UINT32_C(148);

    value ^= value >> 16U;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15U;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16U;
    return value;
}

static bool test_fov_spill_cleanup(void) {
    WorldTestAllocatorState state = {0};
    AFORC_Allocator allocator = world_test_tracking_allocator(&state);
    AFORC_TileMap *map = NULL;
    uint8_t visibility[FOV_SPILL_CELLS];
    size_t attempts_before;
    int32_t y;
    int32_t x;

    WORLD_TEST_CHECK(aforc_tilemap_create(
                         (AFORC_Size){FOV_SPILL_EXTENT, FOV_SPILL_EXTENT}, 1U,
                         0U, &allocator, &map) == AFORC_OK);
    for (y = 0; y < FOV_SPILL_EXTENT; ++y) {
        for (x = 0; x < FOV_SPILL_EXTENT; ++x) {
            if (fov_spill_hash((uint32_t)x, (uint32_t)y) % 1000U < 20U) {
                WORLD_TEST_CHECK(aforc_tilemap_set(
                                     map, 0U, (AFORC_Point){x, y}, 1U) ==
                                 AFORC_OK);
            }
        }
    }
    attempts_before = state.attempts;
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){256, 256}, 256U,
                         world_test_tile_nonzero, NULL, visibility,
                         sizeof(visibility)) == AFORC_OK);
    WORLD_TEST_CHECK(state.attempts > attempts_before && state.live == 2U);
    state.fail_at = state.attempts + 1U;
    WORLD_TEST_CHECK(aforc_fov_compute(
                         map, 0U, (AFORC_Point){256, 256}, 256U,
                         world_test_tile_nonzero, NULL, visibility,
                         sizeof(visibility)) == AFORC_ERROR_OUT_OF_MEMORY);
    WORLD_TEST_CHECK(state.live == 2U);
    aforc_tilemap_destroy(map);
    WORLD_TEST_CHECK(state.live == 0U);
    return true;
}

bool world_test_visibility_cases(void) {
    return test_raycast() && test_fov_geometry() &&
           test_fov_common_case_is_allocation_free() &&
           test_fov_spill_cleanup();
}
