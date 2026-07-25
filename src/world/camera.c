/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "world_private.h"

/*
 * Integer viewport transforms over a tile map.
 *
 * Cameras own no map data. Centering and clamping keep the origin inside the
 * map even when the viewport is larger, while coordinate conversion remains
 * a pure translation with explicit overflow reporting.
 */

static bool camera_viewport_is_valid(AFORC_Size viewport) {
    return viewport.width > 0 && viewport.height > 0;
}

static int32_t world_camera_clamp_axis(int64_t value, int32_t map_extent,
                                       int32_t viewport_extent) {
    const int32_t maximum = map_extent > viewport_extent
                                ? map_extent - viewport_extent
                                : 0;
    if (value < 0) {
        return 0;
    }
    if (value > (int64_t)maximum) {
        return maximum;
    }
    return (int32_t)value;
}

AFORC_Status aforc_camera_init(AFORC_Camera *camera, AFORC_Size viewport) {
    if (camera == NULL || !camera_viewport_is_valid(viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    camera->origin.x = 0;
    camera->origin.y = 0;
    camera->viewport = viewport;
    return AFORC_OK;
}

AFORC_Status aforc_camera_set_viewport(AFORC_Camera *camera, AFORC_Size viewport) {
    if (camera == NULL || !camera_viewport_is_valid(viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    camera->viewport = viewport;
    return AFORC_OK;
}

AFORC_Status aforc_camera_clamp_to_map(AFORC_Camera *camera,
                                   const AFORC_TileMap *map) {
    if (camera == NULL || map == NULL ||
        !camera_viewport_is_valid(camera->viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    camera->origin.x = world_camera_clamp_axis(
        camera->origin.x, map->size.width, camera->viewport.width);
    camera->origin.y = world_camera_clamp_axis(
        camera->origin.y, map->size.height, camera->viewport.height);
    return AFORC_OK;
}

AFORC_Status aforc_camera_center_on(AFORC_Camera *camera, AFORC_Point target,
                                const AFORC_TileMap *map) {
    const int64_t candidate_x = camera == NULL
                                    ? 0
                                    : (int64_t)target.x -
                                          camera->viewport.width / 2;
    const int64_t candidate_y = camera == NULL
                                    ? 0
                                    : (int64_t)target.y -
                                          camera->viewport.height / 2;

    if (camera == NULL || map == NULL ||
        !camera_viewport_is_valid(camera->viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    camera->origin.x = world_camera_clamp_axis(
        candidate_x, map->size.width, camera->viewport.width);
    camera->origin.y = world_camera_clamp_axis(
        candidate_y, map->size.height, camera->viewport.height);
    return AFORC_OK;
}

/* Conversion translates coordinates only; int32 overflow remains explicit. */
AFORC_Status aforc_camera_world_to_screen(const AFORC_Camera *camera,
                                      AFORC_Point world,
                                      AFORC_Point *out_screen) {
    if (camera == NULL || out_screen == NULL ||
        !camera_viewport_is_valid(camera->viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return aforc_world_point_from_i64_internal(
        (int64_t)world.x - camera->origin.x,
        (int64_t)world.y - camera->origin.y, out_screen);
}

AFORC_Status aforc_camera_screen_to_world(const AFORC_Camera *camera,
                                      AFORC_Point screen,
                                      AFORC_Point *out_world) {
    if (camera == NULL || out_world == NULL ||
        !camera_viewport_is_valid(camera->viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return aforc_world_point_from_i64_internal(
        (int64_t)screen.x + camera->origin.x,
        (int64_t)screen.y + camera->origin.y, out_world);
}

AFORC_Status aforc_camera_view(const AFORC_Camera *camera, AFORC_Rect *out_view) {
    if (camera == NULL || out_view == NULL ||
        !camera_viewport_is_valid(camera->viewport)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    out_view->x = camera->origin.x;
    out_view->y = camera->origin.y;
    out_view->width = camera->viewport.width;
    out_view->height = camera->viewport.height;
    return AFORC_OK;
}
