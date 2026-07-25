/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static AFORC_Cell game_cell(uint32_t codepoint,
                          AFORC_Color foreground,
                          AFORC_CellStyle style) {
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    cell.foreground = foreground;
    cell.style = style;
    return cell;
}

static AFORC_Status game_plot_cell(void *context,
                                 AFORC_Point position,
                                 AFORC_Cell cell) {
    Game *game = context;
    return aforc_renderer_put(game->renderer, position, cell);
}

static size_t game_map_index(const Game *game, AFORC_Point position) {
    return (size_t)(uint32_t)position.y *
               (size_t)(uint32_t)game->rules.map_width +
           (size_t)(uint32_t)position.x;
}

AFORC_Status game_emit_burst(Game *game, AFORC_Point point, bool strong) {
    AFORC_Cell cells[3];
    AFORC_ParticleEmitter emitter;
    size_t spawned = 0U;
    int32_t x = point.x * AFORC_EFFECT_FIXED_ONE;
    int32_t y = point.y * AFORC_EFFECT_FIXED_ONE;
    AFORC_Status status;

    if (game->particle_pool.active_count + 12U >
        game->particle_pool.capacity) {
        (void)aforc_particle_pool_clear(&game->particle_pool);
    }
    cells[0] = game_cell((uint32_t)'*',
                         aforc_color_indexed(strong ? 196U : 214U),
                         AFORC_STYLE_BOLD);
    cells[1] = game_cell((uint32_t)'+',
                         aforc_color_indexed(strong ? 203U : 220U),
                         AFORC_STYLE_BOLD);
    cells[2] = game_cell((uint32_t)'.',
                         aforc_color_indexed(215U),
                         AFORC_STYLE_DIM);
    emitter.x = (AFORC_ParticleI32Range){x, x};
    emitter.y = (AFORC_ParticleI32Range){y, y};
    emitter.velocity_x =
        (AFORC_ParticleI32Range){-3 * AFORC_EFFECT_FIXED_ONE,
                               3 * AFORC_EFFECT_FIXED_ONE};
    emitter.velocity_y =
        (AFORC_ParticleI32Range){-2 * AFORC_EFFECT_FIXED_ONE,
                               2 * AFORC_EFFECT_FIXED_ONE};
    emitter.acceleration_x = (AFORC_ParticleI32Range){0, 0};
    emitter.acceleration_y = (AFORC_ParticleI32Range){0, AFORC_EFFECT_FIXED_ONE};
    emitter.lifetime_ms = (AFORC_ParticleU32Range){180U, 520U};
    emitter.cells = cells;
    emitter.cell_count = sizeof(cells) / sizeof(cells[0]);
    status = aforc_particle_pool_emit(&game->particle_pool,
                                    &emitter,
                                    strong ? 12U : 7U,
                                    &spawned);
    if (status == AFORC_ERROR_LIMIT && spawned != 0U) {
        return AFORC_OK;
    }
    return status;
}

static AFORC_Status game_compute_visibility(Game *game,
                                          AFORC_Point player_position) {
    AFORC_Status status = aforc_fov_compute(game->map,
                                        0U,
                                        player_position,
                                        game->rules.fov_radius,
                                        game_tile_blocks,
                                        NULL,
                                        game->visibility,
                                        game->cell_count);

    if (status == AFORC_OK) {
        for (size_t index = 0U; index < game->cell_count; ++index) {
            if (game->visibility[index] != 0U) {
                game->explored[index] = 1U;
            }
        }
    }
    return status;
}

static AFORC_Status game_render_world(Game *game,
                                    AFORC_Size screen,
                                    int32_t map_rows) {
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Size viewport = {screen.width, map_rows};
    double exit_glow = 0.0;
    AFORC_Status status;

    if (viewport.width > game->rules.map_width) {
        viewport.width = game->rules.map_width;
    }
    if (viewport.height > game->rules.map_height) {
        viewport.height = game->rules.map_height;
    }
    status = game_actor_components(game,
                                   game->player,
                                   &player_position,
                                   &player_actor);
    if (status == AFORC_OK) {
        status = aforc_camera_set_viewport(&game->camera, viewport);
    }
    if (status == AFORC_OK) {
        status = aforc_camera_center_on(&game->camera,
                                      player_position->point,
                                      game->map);
    }
    if (status == AFORC_OK) {
        status = game_compute_visibility(game, player_position->point);
    }
    if (status == AFORC_OK) {
        status = aforc_tween_sample(&game->exit_tween, &exit_glow);
    }
    if (status != AFORC_OK) {
        return status;
    }
    for (int32_t screen_y = 0; screen_y < viewport.height; ++screen_y) {
        for (int32_t screen_x = 0; screen_x < viewport.width; ++screen_x) {
            const AFORC_Point world = {game->camera.origin.x + screen_x,
                                     game->camera.origin.y + screen_y};
            const size_t world_index = game_map_index(game, world);
            AFORC_Tile tile = TILE_WALL;
            AFORC_Cell cell;

            if (game->explored[world_index] == 0U) {
                continue;
            }
            status = aforc_tilemap_get(game->map, 0U, world, &tile);
            if (status != AFORC_OK) {
                return status;
            }
            if (tile == TILE_WALL) {
                cell = game_cell((uint32_t)'#',
                                 aforc_color_indexed(game->visibility[world_index]
                                                       ? 245U
                                                       : 238U),
                                 game->visibility[world_index]
                                     ? AFORC_STYLE_NONE
                                     : AFORC_STYLE_DIM);
            } else if (tile == TILE_EXIT) {
                cell = game_cell((uint32_t)'>',
                                 aforc_color_indexed(48U),
                                 exit_glow > 0.45 ? AFORC_STYLE_BOLD
                                                  : AFORC_STYLE_DIM);
            } else {
                cell = game_cell((uint32_t)'.',
                                 aforc_color_indexed(game->visibility[world_index]
                                                       ? 242U
                                                       : 236U),
                                 AFORC_STYLE_DIM);
            }
            status = aforc_renderer_put(game->renderer,
                                      (AFORC_Point){screen_x, screen_y},
                                      cell);
            if (status != AFORC_OK) {
                return status;
            }
        }
    }
    {
        const AFORC_ComponentType types[2] = {game->position_type,
                                            game->actor_type};
        AFORC_EcsView *view = NULL;
        bool has_value = false;

        status = aforc_ecs_view_create(game->ecs, types, 2U, &view);
        while (status == AFORC_OK) {
            AFORC_Entity entity = AFORC_ENTITY_INVALID;
            void *components[2] = {NULL, NULL};
            GamePosition *position;
            GameActor *actor;
            AFORC_Point screen_position;
            size_t world_index;

            status = aforc_ecs_view_next(view,
                                       &entity,
                                       components,
                                       &has_value);
            if (status != AFORC_OK || !has_value) {
                break;
            }
            position = components[0];
            actor = components[1];
            world_index = game_map_index(game, position->point);
            if (game->visibility[world_index] == 0U) {
                continue;
            }
            status = aforc_camera_world_to_screen(&game->camera,
                                                position->point,
                                                &screen_position);
            if (status == AFORC_OK && screen_position.x >= 0 &&
                screen_position.y >= 0 &&
                screen_position.x < viewport.width &&
                screen_position.y < viewport.height) {
                status = aforc_renderer_put(
                    game->renderer,
                    screen_position,
                    game_cell(actor->glyph, actor->color, AFORC_STYLE_BOLD));
            }
            if (status != AFORC_OK) {
                break;
            }
        }
        aforc_ecs_view_destroy(view);
    }
    if (status == AFORC_OK) {
        AFORC_ParticleDrawOptions options =
            aforc_particle_draw_options_default();

        options.offset = (AFORC_Point){-game->camera.origin.x,
                                     -game->camera.origin.y};
        options.clip = (AFORC_Rect){0, 0, viewport.width, viewport.height};
        options.clip_enabled = true;
        status = aforc_particle_pool_draw(&game->particle_pool,
                                        &options,
                                        game_plot_cell,
                                        game);
    }
    return status;
}

static AFORC_Status game_render_help(const AFORC_UICanvas *canvas,
                                   AFORC_Size screen) {
    static const char *const lines[] = {
        "ARROW / WASD / HJKL   Move or attack",
        ". / SPACE             Wait one turn",
        ">                     Descend on the exit",
        "S / L                 Save / load run",
        "?                     Close this help",
        "Q / ESC               Quit"
    };
    AFORC_Rect panel;
    AFORC_UIPanelStyle style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ', aforc_color_indexed(220U), AFORC_STYLE_BOLD),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    int32_t width = screen.width > 56 ? 56 : screen.width;
    int32_t height = 10;
    AFORC_Status status;

    if (height > screen.height) {
        height = screen.height;
    }
    status = aforc_ui_layout_anchor(
        (AFORC_Rect){0, 0, screen.width, screen.height},
        (AFORC_Size){width, height},
        AFORC_UI_ANCHOR_CENTER,
        &panel);
    if (status == AFORC_OK) {
        status = aforc_ui_draw_panel(canvas, panel, &style);
    }
    if (status == AFORC_OK) {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 1, panel.width - 2, 1},
            "AFORC ROGUELIKE CONTROLS",
            sizeof("AFORC ROGUELIKE CONTROLS") - 1U,
            AFORC_UI_ALIGN_CENTER,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(220U),
                      AFORC_STYLE_BOLD));
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(lines) / sizeof(lines[0]);
         ++index) {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 3,
                       panel.y + 3 + (int32_t)index,
                       panel.width - 6,
                       1},
            lines[index],
            strlen(lines[index]),
            AFORC_UI_ALIGN_START,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(index == 4U ? 220U : 252U),
                      index == 4U ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE));
    }
    return status;
}

static AFORC_Status game_render_overlay(Game *game,
                                      const AFORC_UICanvas *canvas,
                                      AFORC_Size screen) {
    const bool victory = game->run_state == GAME_VICTORIOUS;
    const char *title = victory ? "VICTORY" : "RUN ENDED";
    const char *instruction = "Press R for a new run or Q to quit";
    AFORC_Rect panel;
    AFORC_UIPanelStyle style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ',
                  aforc_color_indexed(victory ? 220U : 203U),
                  AFORC_STYLE_BOLD),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    int32_t width = screen.width > 48 ? 48 : screen.width;
    AFORC_Status status = aforc_ui_layout_anchor(
        (AFORC_Rect){0, 0, screen.width, screen.height},
        (AFORC_Size){width, 7},
        AFORC_UI_ANCHOR_CENTER,
        &panel);

    if (status == AFORC_OK) {
        status = aforc_ui_draw_panel(canvas, panel, &style);
    }
    if (status == AFORC_OK) {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 2, panel.width - 2, 1},
            title,
            strlen(title),
            AFORC_UI_ALIGN_CENTER,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(victory ? 220U : 203U),
                      AFORC_STYLE_BOLD));
    }
    if (status == AFORC_OK) {
        status = aforc_ui_draw_label(
            canvas,
            (AFORC_Rect){panel.x + 1, panel.y + 4, panel.width - 2, 1},
            instruction,
            strlen(instruction),
            AFORC_UI_ALIGN_CENTER,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(252U),
                      AFORC_STYLE_NONE));
    }
    return status;
}

static AFORC_Status game_render_hud(Game *game,
                                  AFORC_Size screen,
                                  int32_t map_rows) {
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    AFORC_UICanvas canvas;
    AFORC_UIPanelStyle panel_style = aforc_ui_panel_style_ascii(
        game_cell((uint32_t)' ', aforc_color_indexed(37U), AFORC_STYLE_NONE),
        game_cell((uint32_t)' ', aforc_color_indexed(252U), AFORC_STYLE_NONE),
        true);
    AFORC_UIProgressStyle health_style = {
        game_cell((uint32_t)'=', aforc_color_indexed(196U), AFORC_STYLE_BOLD),
        game_cell((uint32_t)'-', aforc_color_indexed(244U), AFORC_STYLE_DIM)};
    char status_text[96];
    AFORC_Status status = game_actor_components(game,
                                              game->player,
                                              &position,
                                              &actor);

    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ui_canvas_init(
        &canvas,
        (AFORC_Rect){0, 0, screen.width, screen.height},
        game_plot_cell,
        game);
    if (status == AFORC_OK) {
        status = aforc_ui_draw_panel(
            &canvas,
            (AFORC_Rect){0, map_rows, screen.width, GAME_HUD_ROWS},
            &panel_style);
    }
    (void)snprintf(status_text,
                   sizeof(status_text),
                   "AFORC RUINS  Floor %u/%u  Turn %u  Score %u  Seed %" PRIu64,
                   game->floor,
                   game->rules.final_floor,
                   game->turn,
                   game->score,
                   game->seed);
    if (status == AFORC_OK) {
        status = aforc_ui_draw_label(
            &canvas,
            (AFORC_Rect){2, map_rows + 1, screen.width - 4, 1},
            status_text,
            strlen(status_text),
            AFORC_UI_ALIGN_START,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(220U),
                      AFORC_STYLE_BOLD));
    }
    if (status == AFORC_OK) {
        int32_t progress_width = screen.width > 30 ? 22 : screen.width - 4;

        status = aforc_ui_draw_progress(
            &canvas,
            (AFORC_Rect){2, map_rows + 2, progress_width, 1},
            (uint64_t)(uint32_t)actor->health,
            (uint64_t)(uint32_t)actor->maximum_health,
            &health_style);
    }
    if (status == AFORC_OK) {
        status = aforc_ui_draw_label(
            &canvas,
            (AFORC_Rect){2, map_rows + 3, screen.width - 4, 1},
            game->message,
            strlen(game->message),
            AFORC_UI_ALIGN_START,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(game->run_state == GAME_DEFEATED
                                            ? 203U
                                            : 252U),
                      game->run_state == GAME_DEFEATED ? AFORC_STYLE_BOLD
                                                        : AFORC_STYLE_NONE));
    }
    if (status != AFORC_OK) {
        return status;
    }
    if (game->help_visible) {
        status = game_render_help(&canvas, screen);
    } else if (game->run_state != GAME_PLAYING) {
        status = game_render_overlay(game, &canvas, screen);
    }
    return status;
}

AFORC_Status game_scene_fixed_update(AFORC_Scene *scene,
                                   AFORC_Engine *engine,
                                   double seconds,
                                   AFORC_Error *error) {
    Game *game = scene->user_data;
    uint32_t milliseconds = (uint32_t)(seconds * 1000.0 + 0.5);
    AFORC_Status status;

    (void)engine;
    if (milliseconds == 0U) {
        milliseconds = 1U;
    }
    status = aforc_particle_pool_update(&game->particle_pool, milliseconds);
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "effects",
                          "particle update failed");
    }
    return AFORC_OK;
}

AFORC_Status game_scene_update(AFORC_Scene *scene,
                             AFORC_Engine *engine,
                             double seconds,
                             AFORC_Error *error) {
    Game *game = scene->user_data;
    uint64_t milliseconds = (uint64_t)(seconds * 1000.0 + 0.5);
    AFORC_Status status;

    (void)engine;
    status = aforc_tween_update(&game->exit_tween, milliseconds);
    if (status == AFORC_OK && game->exit_tween.finished) {
        status = aforc_tween_restart(&game->exit_tween);
    }
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "effects",
                          "exit tween update failed");
    }
    return AFORC_OK;
}

AFORC_Status game_scene_render(AFORC_Scene *scene,
                             AFORC_Engine *engine,
                             double interpolation,
                             AFORC_Error *error) {
    Game *game = scene->user_data;
    AFORC_Size screen = aforc_renderer_size(game->renderer);
    AFORC_Cell background =
        game_cell((uint32_t)' ', aforc_color_default(), AFORC_STYLE_NONE);
    int32_t map_rows;
    AFORC_Status status;

    (void)engine;
    (void)interpolation;
    status = aforc_renderer_clear(game->renderer, background);
    if (status != AFORC_OK) {
        return game_error(error, status, "renderer", "clear failed");
    }
    if (screen.width < GAME_MIN_COLUMNS || screen.height < GAME_MIN_ROWS) {
        static const char message[] = "Resize terminal to at least 40x14";
        AFORC_Point position = {1, screen.height / 2};

        status = aforc_renderer_draw_ascii(
            game->renderer,
            position,
            message,
            sizeof(message) - 1U,
            game_cell((uint32_t)' ',
                      aforc_color_indexed(203U),
                      AFORC_STYLE_BOLD));
        return status == AFORC_OK
                   ? AFORC_OK
                   : game_error(error,
                                status,
                                "renderer",
                                "resize message failed");
    }
    map_rows = screen.height - GAME_HUD_ROWS;
    status = game_render_world(game, screen, map_rows);
    if (status == AFORC_OK) {
        status = game_render_hud(game, screen, map_rows);
    }
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "roguelike",
                          "frame composition failed");
    }
    return AFORC_OK;
}
