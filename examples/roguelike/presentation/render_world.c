/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

static size_t game_map_index(const Game *game, AFORC_Point position)
{
    return (size_t)(uint32_t)position.y *
               (size_t)(uint32_t)game->rules.map_width +
           (size_t)(uint32_t)position.x;
}

static AFORC_Status game_compute_visibility(Game *game,
                                            AFORC_Point player_position)
{
    /* Static floor geometry lets animation-only frames reuse the prior FOV. */
    if (game->visibility_valid &&
        aforc_world_point_equal(game->visibility_origin, player_position))
    {
        return AFORC_OK;
    }

    AFORC_Status status = aforc_fov_compute(game->map,
                                            0U,
                                            player_position,
                                            game->rules.fov_radius,
                                            game_tile_blocks,
                                            NULL,
                                            game->visibility,
                                            game->cell_count);

    if (status == AFORC_OK)
    {
        for (size_t index = 0U; index < game->cell_count; ++index)
        {
            if (game->visibility[index] != 0U)
            {
                game->explored[index] = 1U;
            }
        }
        game->visibility_origin = player_position;
        game->visibility_valid = true;
    }
    return status;
}

AFORC_Status game_render_world(Game *game, AFORC_Size screen, int32_t map_rows)
{
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Size viewport = {screen.width, map_rows};
    AFORC_Point screen_origin;
    double exit_glow = 0.0;
    bool has_actor = false;
    AFORC_Status status;

    if (viewport.width > game->rules.map_width)
    {
        viewport.width = game->rules.map_width;
    }
    if (viewport.height > game->rules.map_height)
    {
        viewport.height = game->rules.map_height;
    }
    screen_origin = (AFORC_Point){(screen.width - viewport.width) / 2,
                                  (map_rows - viewport.height) / 2};
    status = game_actor_components(
        game, game->player, &player_position, &player_actor);
    if (status == AFORC_OK)
    {
        status = aforc_camera_set_viewport(&game->camera, viewport);
    }
    if (status == AFORC_OK)
    {
        status = aforc_camera_center_on(
            &game->camera, player_position->point, game->map);
    }
    if (status == AFORC_OK)
    {
        status = game_compute_visibility(game, player_position->point);
    }
    if (status == AFORC_OK)
    {
        status = aforc_tween_sample(&game->exit_tween, &exit_glow);
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    for (int32_t screen_y = 0; screen_y < viewport.height; ++screen_y)
    {
        for (int32_t screen_x = 0; screen_x < viewport.width; ++screen_x)
        {
            const AFORC_Point world = {game->camera.origin.x + screen_x,
                                       game->camera.origin.y + screen_y};
            const size_t world_index = game_map_index(game, world);
            AFORC_Tile tile = TILE_WALL;
            AFORC_Cell cell;

            if (game->explored[world_index] == 0U)
            {
                continue;
            }
            status = aforc_tilemap_get(game->map, 0U, world, &tile);
            if (status != AFORC_OK)
            {
                return status;
            }
            if (tile == TILE_WALL)
            {
                cell =
                    game_cell((uint32_t)'#',
                              aforc_color_indexed(
                                  game->visibility[world_index] ? 245U : 238U),
                              game->visibility[world_index] ? AFORC_STYLE_NONE
                                                            : AFORC_STYLE_DIM);
            }
            else if (tile == TILE_EXIT)
            {
                cell = game_cell((uint32_t)'>',
                                 aforc_color_indexed(48U),
                                 exit_glow > 0.45 ? AFORC_STYLE_BOLD
                                                  : AFORC_STYLE_DIM);
            }
            else
            {
                cell =
                    game_cell((uint32_t)'.',
                              aforc_color_indexed(
                                  game->visibility[world_index] ? 242U : 236U),
                              AFORC_STYLE_DIM);
            }
            status =
                aforc_renderer_put(game->renderer,
                                   (AFORC_Point){screen_origin.x + screen_x,
                                                 screen_origin.y + screen_y},
                                   cell);
            if (status != AFORC_OK)
            {
                return status;
            }
        }
    }
    status = aforc_ecs_view_reset(game->render_view);
    while (status == AFORC_OK)
    {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        GamePosition *position;
        GameActor *actor;
        AFORC_Point screen_position;
        size_t world_index;

        status = aforc_ecs_view_next(
            game->render_view, &entity, components, &has_actor);
        if (status != AFORC_OK || !has_actor)
        {
            break;
        }
        position = components[0];
        actor = components[1];
        world_index = game_map_index(game, position->point);
        if (game->visibility[world_index] == 0U)
        {
            continue;
        }
        status = aforc_camera_world_to_screen(
            &game->camera, position->point, &screen_position);
        if (status == AFORC_OK && screen_position.x >= 0 &&
            screen_position.y >= 0 && screen_position.x < viewport.width &&
            screen_position.y < viewport.height)
        {
            screen_position.x += screen_origin.x;
            screen_position.y += screen_origin.y;
            status = aforc_renderer_put(
                game->renderer,
                screen_position,
                game_cell(actor->glyph, actor->color, AFORC_STYLE_BOLD));
        }
        if (status != AFORC_OK)
        {
            break;
        }
    }
    if (status == AFORC_OK)
    {
        AFORC_ParticleDrawOptions options =
            aforc_particle_draw_options_default();

        options.offset = (AFORC_Point){screen_origin.x - game->camera.origin.x,
                                       screen_origin.y - game->camera.origin.y};
        options.clip = (AFORC_Rect){
            screen_origin.x, screen_origin.y, viewport.width, viewport.height};
        options.clip_enabled = true;
        status = aforc_particle_pool_draw(
            &game->particle_pool, &options, game_plot_cell, game);
    }
    return status;
}
