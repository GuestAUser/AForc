/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const AFORC_SceneVTable game_scene_vtable = {
    NULL,
    NULL,
    NULL,
    NULL,
    game_scene_fixed_update,
    game_scene_update,
    game_scene_render,
    game_scene_event
};

static AFORC_Status game_dispatch_input_queue(Game *game,
                                             AFORC_Engine *engine,
                                             AFORC_Error *error) {
    AFORC_InputEvent event;
    AFORC_Status status = AFORC_OK;

    while (aforc_input_next_event(game->input, &event)) {
        bool consumed = false;

        status = aforc_engine_dispatch_event(engine,
                                           &event,
                                           &consumed,
                                           error);
        if (status != AFORC_OK) {
            break;
        }
    }
    return status;
}

static AFORC_Status game_poll_events(void *context,
                                   AFORC_Engine *engine,
                                   AFORC_Error *error) {
    Game *game = context;
    AFORC_Status status = aforc_input_begin_frame(game->input);

    if (status == AFORC_OK) {
        status = aforc_input_poll(game->input, game->terminal, 0);
    }
    if (status == AFORC_ERROR_INTERRUPTED || status == AFORC_ERROR_END_OF_STREAM) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (status != AFORC_OK) {
        return game_error(error, status, "input", "terminal input poll failed");
    }
    return game_dispatch_input_queue(game, engine, error);
}

static AFORC_Status game_begin_frame(void *context,
                                   AFORC_Engine *engine,
                                   AFORC_Error *error) {
    Game *game = context;
    bool changed = false;
    AFORC_Status status;

    (void)engine;
    /* A NULL terminal keeps the same frame hook path for off-screen smoke. */
    if (game->terminal == NULL) {
        return AFORC_OK;
    }
    status = aforc_renderer_resize_to_terminal(game->renderer,
                                             game->terminal,
                                             &changed);
    if (status != AFORC_OK) {
        return game_error(error, status, "renderer", "terminal resize failed");
    }
    if (changed) {
        aforc_renderer_invalidate(game->renderer);
    }
    return AFORC_OK;
}

static AFORC_Status game_present(void *context,
                               AFORC_Engine *engine,
                               AFORC_Error *error) {
    Game *game = context;
    AFORC_Status status;

    (void)engine;
    if (game->terminal == NULL) {
        return AFORC_OK;
    }
    status = aforc_renderer_present(game->renderer, game->terminal);
    return status == AFORC_OK
               ? AFORC_OK
               : game_error(error, status, "renderer", "present failed");
}

static AFORC_Status game_smoke_repeated_direction_is_dispatched(
    Game *game,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const bool help_visible = game->help_visible;
    bool consumed = false;
    AFORC_Status status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_UP;
    event.data.key.repeat = true;
    game->help_visible = true;
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    game->help_visible = help_visible;
    if (status == AFORC_OK && !consumed) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "repeated direction keydown was ignored");
    }
    return status;
}

static AFORC_Status game_smoke_checks(Game *game,
                                    AFORC_Engine *engine,
                                    AFORC_Error *error) {
    static const unsigned char wait_input[] = ".";
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    AFORC_Point *path = NULL;
    size_t path_length = 0U;
    size_t path_capacity = 0U;
    AFORC_PathOptions path_options = aforc_path_options_default();
    AFORC_AssetBlob save_blob = {NULL, 0U};
    AFORC_Cell hud_cell;
    uint32_t turn_before = game->turn;
    uint32_t saved_floor = game->floor;
    uint32_t saved_score = game->score;
    uint32_t saved_turn = game->turn;
    uint64_t saved_seed = game->seed;
    int32_t saved_health = 0;
    const char *save_path = game->save_path;
    AFORC_Status status;

    if (aforc_ecs_entity_count(game->ecs) < 2U) {
        return AFORC_ERROR_STATE;
    }
    status = game_actor_components(game,
                                   game->player,
                                   &position,
                                   &actor);
    if (status == AFORC_OK) {
        saved_health = actor->health;
        path_options.max_visited = game->cell_count;
        status = aforc_pathfind_astar(game->map,
                                    0U,
                                    position->point,
                                    game->exit_position,
                                    game_tile_blocks,
                                    NULL,
                                    &path_options,
                                    NULL,
                                    0U,
                                    &path_length);
    }
    if (status == AFORC_ERROR_LIMIT && path_length >= 2U &&
        path_length <= game->cell_count) {
        path_capacity = path_length;
        status = aforc_alloc_array(&game->allocator,
                                 path_capacity,
                                 sizeof(*path),
                                 (void **)&path);
    }
    if (status == AFORC_OK) {
        status = aforc_pathfind_astar(game->map,
                                    0U,
                                    position->point,
                                    game->exit_position,
                                    game_tile_blocks,
                                    NULL,
                                    &path_options,
                                    path,
                                    path_capacity,
                                    &path_length);
    }
    if (status != AFORC_OK || path_length < 2U) {
        aforc_free(&game->allocator, path);
        return status == AFORC_OK ? AFORC_ERROR_STATE : status;
    }
    status = game_encode_save(game, &save_blob);
    if (status == AFORC_OK) {
        AFORC_SaveReader reader;

        status = aforc_save_reader_init(&reader,
                                      save_blob.data,
                                      save_blob.size,
                                      GAME_SAVE_MAX_BYTES,
                                      GAME_SAVE_SCHEMA,
                                      GAME_SAVE_SCHEMA);
        if (status == AFORC_OK && reader.schema_version != GAME_SAVE_SCHEMA) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = game_decode_save(game, save_blob.data, save_blob.size);
    }
    aforc_asset_blob_release(&save_blob);
    if (status == AFORC_OK) {
        status = game_actor_components(game,
                                       game->player,
                                       &position,
                                       &actor);
    }
    if (status == AFORC_OK &&
        (game->seed != saved_seed || game->floor != saved_floor ||
         game->score != saved_score || game->turn != saved_turn ||
         actor->health != saved_health)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        game->save_path = "aforc-smoke-missing-directory/save.bin";
        status = game_load(game);
        game->save_path = save_path;
        if (status == AFORC_OK &&
            strcmp(game->message,
                   "No saved run found at "
                   "aforc-smoke-missing-directory/save.bin.") != 0) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status != AFORC_OK) {
        aforc_free(&game->allocator, path);
        return status;
    }
    status = game_emit_burst(game, position->point, true);
    if (status == AFORC_OK) {
        status = aforc_input_begin_frame(game->input);
    }
    if (status == AFORC_OK) {
        status = aforc_input_feed(game->input,
                                wait_input,
                                sizeof(wait_input) - 1U,
                                1U);
    }
    if (status == AFORC_OK) {
        status = aforc_input_flush(game->input, 2U);
    }
    if (status == AFORC_OK) {
        status = game_dispatch_input_queue(game, engine, error);
    }
    if (status == AFORC_OK && game->turn == turn_before) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = game_smoke_repeated_direction_is_dispatched(game,
                                                              engine,
                                                              error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, UINT64_C(16666667), error);
    }
    if (status == AFORC_OK) {
        AFORC_Size screen = aforc_renderer_size(game->renderer);

        status = aforc_renderer_get(
            game->renderer,
            (AFORC_Point){0, screen.height - GAME_HUD_ROWS},
            &hud_cell);
        if (status == AFORC_OK && hud_cell.codepoint != (uint32_t)'+') {
            status = AFORC_ERROR_STATE;
        }
    }
    aforc_free(&game->allocator, path);
    return status;
}

int game_run_smoke(uint64_t seed) {
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_RendererConfig renderer_config = aforc_renderer_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    Game game = {0};
    AFORC_Status status;
    size_t entity_count = 0U;
    bool game_initialized = false;

    aforc_error_clear(&error);
    renderer_config.size = (AFORC_Size){80, 28};
    status = aforc_renderer_create(&renderer, &renderer_config);
    if (status == AFORC_OK) {
        status = aforc_input_create(&input, &input_config);
    }
    if (status == AFORC_OK) {
        status = game_initialize(&game,
                                 renderer,
                                 input,
                                 NULL,
                                 seed,
                                 true);
        game_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.user_data = &game;
        engine_config.target_frames_per_second = 0U;
        engine_config.hooks.context = &game;
        engine_config.hooks.begin_frame = game_begin_frame;
        engine_config.hooks.present = game_present;
        status = aforc_engine_create(&engine_config,
                                   &engine,
                                   &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &game.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_checks(&game, engine, &error);
    }
    if (status == AFORC_OK) {
        entity_count = aforc_ecs_entity_count(game.ecs);
        (void)printf("roguelike smoke: ok seed=%" PRIu64
                     " floor=%u entities=%zu\n",
                     game.seed,
                     game.floor,
                     entity_count);
    } else {
        (void)fprintf(stderr,
                      "roguelike smoke: %s%s%s\n",
                      aforc_status_string(status),
                      error.message[0] == '\0' ? "" : ": ",
                      error.message);
    }
    aforc_engine_destroy(engine);
    if (game_initialized) {
        game_dispose(&game);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int game_run_interactive(uint64_t seed) {
    AFORC_Terminal *terminal = NULL;
    AFORC_Renderer *renderer = NULL;
    AFORC_Input *input = NULL;
    AFORC_Engine *engine = NULL;
    AFORC_TerminalConfig terminal_config = aforc_terminal_config_default();
    AFORC_InputConfig input_config = aforc_input_config_default();
    AFORC_EngineConfig engine_config = aforc_engine_config_default();
    AFORC_Error error;
    Game game;
    bool game_initialized = false;
    AFORC_Status status;

    aforc_error_clear(&error);
    status = aforc_terminal_open(&terminal, &terminal_config);
    if (status == AFORC_OK) {
        status = aforc_renderer_create_for_terminal(&renderer,
                                                  terminal,
                                                  &engine_config.allocator);
    }
    if (status == AFORC_OK) {
        status = aforc_input_create(&input, &input_config);
    }
    if (status == AFORC_OK) {
        status = game_initialize(&game,
                                 renderer,
                                 input,
                                 terminal,
                                 seed,
                                 false);
        game_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        engine_config.user_data = &game;
        engine_config.hooks.context = &game;
        engine_config.hooks.poll_events = game_poll_events;
        engine_config.hooks.begin_frame = game_begin_frame;
        engine_config.hooks.present = game_present;
        status = aforc_engine_create(&engine_config, &engine, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &game.scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_run(engine, &error);
    }
    aforc_engine_destroy(engine);
    if (game_initialized) {
        game_dispose(&game);
    }
    aforc_input_destroy(input);
    aforc_renderer_destroy(renderer);
    aforc_terminal_close(terminal);
    if (status != AFORC_OK) {
        (void)fprintf(stderr,
                      "aforc-roguelike: %s%s%s\n",
                      aforc_status_string(status),
                      error.message[0] == '\0' ? "" : ": ",
                      error.message);
    }
    return status == AFORC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
