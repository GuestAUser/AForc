/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_WORLD_H
#define AFORC_WORLD_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t AFORC_Tile;

typedef struct AFORC_TileMap AFORC_TileMap;
typedef struct AFORC_PathWorkspace AFORC_PathWorkspace;

typedef struct AFORC_Camera {
    AFORC_Point origin;
    AFORC_Size viewport;
} AFORC_Camera;

typedef bool (*AFORC_TileTestFn)(AFORC_Tile tile,
                               uint32_t layer,
                               AFORC_Point position,
                               void *context);

/* A NULL tile test invokes no callback; every tile is passable/transparent. */

typedef enum AFORC_PathFlag {
    AFORC_PATH_NONE = 0,
    AFORC_PATH_ALLOW_DIAGONAL = UINT32_C(1) << 0,
    /* Has no effect unless AFORC_PATH_ALLOW_DIAGONAL is also set. */
    AFORC_PATH_PREVENT_CORNER_CUTTING = UINT32_C(1) << 1
} AFORC_PathFlag;

typedef struct AFORC_PathOptions {
    uint32_t flags;
    /* Bounds expanded cells, including endpoints; zero permits every cell. */
    size_t max_visited;
} AFORC_PathOptions;

/* Geometry helpers return AFORC_ERROR_OVERFLOW rather than wrapping int32_t. */
AFORC_API bool aforc_world_point_equal(AFORC_Point left, AFORC_Point right);
AFORC_API AFORC_Status aforc_world_point_add(AFORC_Point left,
                                       AFORC_Point right,
                                       AFORC_Point *out_sum);
AFORC_API uint64_t aforc_world_point_manhattan(AFORC_Point left,
                                           AFORC_Point right);

AFORC_API bool aforc_world_rect_is_empty(AFORC_Rect rect);
AFORC_API bool aforc_world_rect_intersects(AFORC_Rect left, AFORC_Rect right);
AFORC_API AFORC_Status aforc_world_rect_translate(AFORC_Rect rect,
                                            AFORC_Point delta,
                                            AFORC_Rect *out_rect);

/*
 * A tile map owns layer_count dense row-major layers. size dimensions and
 * layer_count must be positive. The allocator is copied; its context must
 * remain valid until destroy. cell_count reports cells per layer.
 */
AFORC_API AFORC_Status aforc_tilemap_create(AFORC_Size size,
                                      uint32_t layer_count,
                                      AFORC_Tile initial_tile,
                                      const AFORC_Allocator *allocator,
                                      AFORC_TileMap **out_map);
AFORC_API void aforc_tilemap_destroy(AFORC_TileMap *map);
AFORC_API AFORC_Status aforc_tilemap_get_dimensions(const AFORC_TileMap *map,
                                              AFORC_Size *out_size,
                                              uint32_t *out_layer_count);
AFORC_API AFORC_Status aforc_tilemap_cell_count(const AFORC_TileMap *map,
                                          size_t *out_count);
AFORC_API bool aforc_tilemap_contains(const AFORC_TileMap *map, AFORC_Point point);
AFORC_API AFORC_Status aforc_tilemap_get(const AFORC_TileMap *map,
                                   uint32_t layer,
                                   AFORC_Point point,
                                   AFORC_Tile *out_tile);
AFORC_API AFORC_Status aforc_tilemap_set(AFORC_TileMap *map,
                                   uint32_t layer,
                                   AFORC_Point point,
                                   AFORC_Tile tile);
AFORC_API AFORC_Status aforc_tilemap_fill_layer(AFORC_TileMap *map,
                                          uint32_t layer,
                                          AFORC_Tile tile);
/* rect must be non-negative and wholly contained; an empty rect is a no-op. */
AFORC_API AFORC_Status aforc_tilemap_fill_rect(AFORC_TileMap *map,
                                         uint32_t layer,
                                         AFORC_Rect rect,
                                         AFORC_Tile tile);

AFORC_API AFORC_Status aforc_camera_init(AFORC_Camera *camera, AFORC_Size viewport);
AFORC_API AFORC_Status aforc_camera_set_viewport(AFORC_Camera *camera,
                                           AFORC_Size viewport);
AFORC_API AFORC_Status aforc_camera_clamp_to_map(AFORC_Camera *camera,
                                           const AFORC_TileMap *map);
AFORC_API AFORC_Status aforc_camera_center_on(AFORC_Camera *camera,
                                       AFORC_Point target,
                                       const AFORC_TileMap *map);
/* Coordinate transforms translate only; they do not clip to map or viewport. */
AFORC_API AFORC_Status aforc_camera_world_to_screen(const AFORC_Camera *camera,
                                              AFORC_Point world,
                                              AFORC_Point *out_screen);
AFORC_API AFORC_Status aforc_camera_screen_to_world(const AFORC_Camera *camera,
                                              AFORC_Point screen,
                                              AFORC_Point *out_world);
AFORC_API AFORC_Status aforc_camera_view(const AFORC_Camera *camera,
                                   AFORC_Rect *out_view);

AFORC_API AFORC_Status aforc_grid_point_blocked(const AFORC_TileMap *map,
                                          uint32_t layer,
                                          AFORC_Point point,
                                          AFORC_TileTestFn is_blocked,
                                          void *context,
                                          bool *out_blocked);
/* Cells are tested in row-major order; out_first_blocked is optional. */
AFORC_API AFORC_Status aforc_grid_rect_blocked(const AFORC_TileMap *map,
                                         uint32_t layer,
                                         AFORC_Rect rect,
                                         AFORC_TileTestFn is_blocked,
                                         void *context,
                                         bool *out_blocked,
                                         AFORC_Point *out_first_blocked);
/*
 * Supercover traversal includes endpoints and corner-touching cells. out_hit
 * is optional and receives end when the ray is clear.
 */
AFORC_API AFORC_Status aforc_grid_raycast(const AFORC_TileMap *map,
                                    uint32_t layer,
                                    AFORC_Point start,
                                    AFORC_Point end,
                                    AFORC_TileTestFn is_blocked,
                                    void *context,
                                    bool *out_blocked,
                                    AFORC_Point *out_hit);

AFORC_API AFORC_PathOptions aforc_path_options_default(void);
/*
 * A path workspace owns reusable A* scratch storage. The allocator is copied;
 * its context must remain valid until destroy. Reserve at least the map's cell
 * count before searching. A sufficient reservation makes searches allocation
 * free.
 */
AFORC_API AFORC_Status aforc_path_workspace_create(
    const AFORC_Allocator *allocator,
    AFORC_PathWorkspace **out_workspace);
AFORC_API AFORC_Status aforc_path_workspace_reserve(
    AFORC_PathWorkspace *workspace,
    size_t cell_capacity);
AFORC_API void aforc_path_workspace_destroy(AFORC_PathWorkspace *workspace);
/*
 * Returns a workspace-owned path including start and goal. The borrowed path
 * remains valid until the next search, reserve, or destroy on that workspace.
 * Cardinal steps cost 10 and diagonal steps cost 14. Ties resolve
 * deterministically; default movement is four-directional.
 */
AFORC_API AFORC_Status aforc_pathfind_astar_workspace(
    AFORC_PathWorkspace *workspace,
    const AFORC_TileMap *map,
    uint32_t layer,
    AFORC_Point start,
    AFORC_Point goal,
    AFORC_TileTestFn is_blocked,
    void *context,
    const AFORC_PathOptions *options,
    const AFORC_Point **out_points,
    size_t *out_length);
/*
 * The returned path includes start and goal. If point_capacity is too small,
 * out_length receives the required count and AFORC_ERROR_LIMIT is returned.
 * Passing NULL with zero capacity is therefore a supported sizing query.
 * Cardinal steps cost 10 and diagonal steps cost 14. Ties resolve
 * deterministically; default movement is four-directional.
 */
AFORC_API AFORC_Status aforc_pathfind_astar(const AFORC_TileMap *map,
                                      uint32_t layer,
                                      AFORC_Point start,
                                      AFORC_Point goal,
                                      AFORC_TileTestFn is_blocked,
                                      void *context,
                                      const AFORC_PathOptions *options,
                                      AFORC_Point *out_points,
                                      size_t point_capacity,
                                      size_t *out_length);

/*
 * Writes one byte per row-major map cell for the selected layer. Opaque cells
 * can be visible, while cells behind them are shadowed. The radius is
 * Euclidean and the origin is always visible. Complex scans may allocate
 * temporary task storage through the map's allocator.
 */
AFORC_API AFORC_Status aforc_fov_compute(const AFORC_TileMap *map,
                                   uint32_t layer,
                                   AFORC_Point origin,
                                   uint32_t radius,
                                   AFORC_TileTestFn is_opaque,
                                   void *context,
                                   uint8_t *out_visibility,
                                   size_t visibility_size);

#ifdef __cplusplus
}
#endif

#endif
