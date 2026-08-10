/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_test_support.h"

#include <stdint.h>

static bool test_geometry(void)
{
    AFORC_Point point = {17, 23};
    AFORC_Rect rect;

    WORLD_TEST_CHECK(
        aforc_world_point_equal((AFORC_Point){1, 2}, (AFORC_Point){1, 2}));
    WORLD_TEST_CHECK(
        !aforc_world_point_equal((AFORC_Point){1, 2}, (AFORC_Point){2, 1}));
    WORLD_TEST_CHECK(aforc_world_point_add((AFORC_Point){7, -9},
                                           (AFORC_Point){-4, 12},
                                           &point) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_world_point_equal(point, (AFORC_Point){3, 3}));
    WORLD_TEST_CHECK(aforc_world_point_add((AFORC_Point){INT32_MAX, 0},
                                           (AFORC_Point){1, 0},
                                           &point) == AFORC_ERROR_OVERFLOW);
    WORLD_TEST_CHECK(
        aforc_world_point_manhattan((AFORC_Point){INT32_MIN, INT32_MIN},
                                    (AFORC_Point){INT32_MAX, INT32_MAX}) ==
        UINT64_C(8589934590));
    WORLD_TEST_CHECK(aforc_world_rect_is_empty((AFORC_Rect){0, 0, 0, 4}));
    WORLD_TEST_CHECK(aforc_world_rect_is_empty((AFORC_Rect){0, 0, -1, 4}));
    WORLD_TEST_CHECK(aforc_world_rect_intersects((AFORC_Rect){0, 0, 3, 3},
                                                 (AFORC_Rect){2, 2, 3, 3}));
    WORLD_TEST_CHECK(!aforc_world_rect_intersects((AFORC_Rect){0, 0, 3, 3},
                                                  (AFORC_Rect){3, 0, 2, 2}));
    WORLD_TEST_CHECK(aforc_world_rect_translate((AFORC_Rect){1, 2, 3, 4},
                                                (AFORC_Point){-3, 5},
                                                &rect) == AFORC_OK);
    WORLD_TEST_CHECK(rect.x == -2 && rect.y == 7 && rect.width == 3 &&
                     rect.height == 4);
    WORLD_TEST_CHECK(
        aforc_world_rect_translate((AFORC_Rect){INT32_MAX, 0, 1, 1},
                                   (AFORC_Point){1, 0},
                                   &rect) == AFORC_ERROR_OVERFLOW);
    return true;
}

static bool test_tilemap_and_collision(void)
{
    WorldTestAllocatorState state = {0};
    AFORC_Allocator allocator = world_test_tracking_allocator(&state);
    AFORC_TileMap *map = NULL;
    AFORC_Size size = {0, 0};
    uint32_t layers = 0U;
    size_t cells = 0U;
    AFORC_Tile tile = 0U;
    AFORC_Point first = {-1, -1};
    bool blocked = false;
    const size_t attempts_before_overflow = state.attempts;

    WORLD_TEST_CHECK(aforc_tilemap_create((AFORC_Size){INT32_MAX, INT32_MAX},
                                          UINT32_MAX,
                                          0U,
                                          &allocator,
                                          &map) == AFORC_ERROR_OVERFLOW);
    WORLD_TEST_CHECK(map == NULL && state.attempts == attempts_before_overflow);
    WORLD_TEST_CHECK(
        aforc_tilemap_create((AFORC_Size){4, 3}, 2U, 7U, &allocator, &map) ==
        AFORC_OK);
    WORLD_TEST_CHECK(state.live == 2U);
    WORLD_TEST_CHECK(aforc_tilemap_get_dimensions(map, &size, &layers) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(size.width == 4 && size.height == 3 && layers == 2U);
    WORLD_TEST_CHECK(aforc_tilemap_cell_count(map, &cells) == AFORC_OK &&
                     cells == 12U);
    WORLD_TEST_CHECK(aforc_tilemap_contains(map, (AFORC_Point){3, 2}));
    WORLD_TEST_CHECK(!aforc_tilemap_contains(map, (AFORC_Point){4, 2}));
    WORLD_TEST_CHECK(aforc_tilemap_get(map, 1U, (AFORC_Point){3, 2}, &tile) ==
                         AFORC_OK &&
                     tile == 7U);
    WORLD_TEST_CHECK(aforc_tilemap_fill_layer(map, 0U, 0U) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_tilemap_fill_rect(
                         map, 0U, (AFORC_Rect){1, 0, 2, 2}, 5U) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_tilemap_get(map, 0U, (AFORC_Point){2, 1}, &tile) ==
                         AFORC_OK &&
                     tile == 5U);
    WORLD_TEST_CHECK(aforc_tilemap_fill_rect(
                         map, 0U, (AFORC_Rect){4, 3, 0, 0}, 9U) == AFORC_OK);
    WORLD_TEST_CHECK(
        aforc_tilemap_fill_rect(map, 0U, (AFORC_Rect){0, 0, -1, 1}, 9U) ==
        AFORC_ERROR_INVALID_ARGUMENT);
    WORLD_TEST_CHECK(aforc_tilemap_set(map, 0U, (AFORC_Point){2, 0}, 1U) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(aforc_grid_point_blocked(map,
                                              0U,
                                              (AFORC_Point){2, 0},
                                              world_test_tile_nonzero,
                                              NULL,
                                              &blocked) == AFORC_OK &&
                     blocked);
    WORLD_TEST_CHECK(aforc_grid_rect_blocked(map,
                                             0U,
                                             (AFORC_Rect){0, 0, 4, 2},
                                             world_test_tile_nonzero,
                                             NULL,
                                             &blocked,
                                             &first) == AFORC_OK);
    WORLD_TEST_CHECK(blocked &&
                     aforc_world_point_equal(first, (AFORC_Point){1, 0}));
    WORLD_TEST_CHECK(
        aforc_grid_rect_blocked(
            map, 0U, (AFORC_Rect){0, 0, 4, 2}, NULL, NULL, &blocked, &first) ==
            AFORC_OK &&
        !blocked);
    WORLD_TEST_CHECK(aforc_grid_rect_blocked(map,
                                             0U,
                                             (AFORC_Rect){4, 3, 0, 0},
                                             world_test_tile_nonzero,
                                             NULL,
                                             &blocked,
                                             &first) == AFORC_OK &&
                     !blocked);
    aforc_tilemap_destroy(map);
    WORLD_TEST_CHECK(state.live == 0U);
    return true;
}

static bool test_camera(void)
{
    AFORC_Allocator allocator = aforc_allocator_default();
    AFORC_TileMap *map = NULL;
    AFORC_Camera camera;
    AFORC_Point point;
    AFORC_Rect view;

    WORLD_TEST_CHECK(
        aforc_tilemap_create((AFORC_Size){10, 8}, 1U, 0U, &allocator, &map) ==
        AFORC_OK);
    WORLD_TEST_CHECK(aforc_camera_init(&camera, (AFORC_Size){0, 3}) ==
                     AFORC_ERROR_INVALID_ARGUMENT);
    WORLD_TEST_CHECK(aforc_camera_init(&camera, (AFORC_Size){4, 3}) ==
                     AFORC_OK);
    camera.origin = (AFORC_Point){-5, 100};
    WORLD_TEST_CHECK(aforc_camera_clamp_to_map(&camera, map) == AFORC_OK);
    WORLD_TEST_CHECK(
        aforc_world_point_equal(camera.origin, (AFORC_Point){0, 5}));
    WORLD_TEST_CHECK(
        aforc_camera_center_on(&camera, (AFORC_Point){9, 7}, map) == AFORC_OK);
    WORLD_TEST_CHECK(
        aforc_world_point_equal(camera.origin, (AFORC_Point){6, 5}));
    WORLD_TEST_CHECK(aforc_camera_world_to_screen(
                         &camera, (AFORC_Point){9, 7}, &point) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_world_point_equal(point, (AFORC_Point){3, 2}));
    WORLD_TEST_CHECK(aforc_camera_world_to_screen(
                         &camera, (AFORC_Point){0, 0}, &point) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_world_point_equal(point, (AFORC_Point){-6, -5}));
    WORLD_TEST_CHECK(aforc_camera_screen_to_world(
                         &camera, (AFORC_Point){3, 2}, &point) == AFORC_OK);
    WORLD_TEST_CHECK(aforc_world_point_equal(point, (AFORC_Point){9, 7}));
    WORLD_TEST_CHECK(aforc_camera_view(&camera, &view) == AFORC_OK);
    WORLD_TEST_CHECK(view.x == 6 && view.y == 5 && view.width == 4 &&
                     view.height == 3);
    WORLD_TEST_CHECK(aforc_camera_set_viewport(&camera, (AFORC_Size){12, 10}) ==
                     AFORC_OK);
    WORLD_TEST_CHECK(aforc_camera_clamp_to_map(&camera, map) == AFORC_OK);
    WORLD_TEST_CHECK(
        aforc_world_point_equal(camera.origin, (AFORC_Point){0, 0}));
    camera.origin = (AFORC_Point){INT32_MIN, 0};
    WORLD_TEST_CHECK(aforc_camera_world_to_screen(
                         &camera, (AFORC_Point){INT32_MAX, 0}, &point) ==
                     AFORC_ERROR_OVERFLOW);
    aforc_tilemap_destroy(map);
    return true;
}

bool world_test_core_cases(void)
{
    return test_geometry() && test_tilemap_and_collision() && test_camera();
}
