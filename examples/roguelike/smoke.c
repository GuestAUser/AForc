/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <string.h>

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

static AFORC_Status game_smoke_weak_burst_preserves_particles(
    Game *game,
    AFORC_Point point,
    AFORC_Error *error) {
    AFORC_ParticleDesc particle = {0};
    const size_t preserved_count = game->particle_pool.capacity - 11U;
    const size_t expected_count = preserved_count + 7U;
    AFORC_Status status = aforc_particle_pool_clear(&game->particle_pool);

    particle.lifetime_ms = 1000U;
    particle.cell = aforc_cell_default();
    while (status == AFORC_OK &&
           game->particle_pool.active_count < preserved_count) {
        status = aforc_particle_pool_spawn(&game->particle_pool,
                                           &particle,
                                           NULL);
    }
    if (status == AFORC_OK) {
        status = game_emit_burst(game, point, false);
    }
    if (status == AFORC_OK &&
        game->particle_pool.active_count != expected_count) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "weak burst discarded existing particles");
    }
    (void)aforc_particle_pool_clear(&game->particle_pool);
    return status;
}

static AFORC_Status game_smoke_save_failure_is_recoverable(
    Game *game,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const char *save_path = game->save_path;
    bool consumed = false;
    AFORC_Status status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_S;
    event.data.key.codepoint = (uint32_t)'S';
    game->save_path = "aforc-smoke-missing-directory/save.bin";
    status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    game->save_path = save_path;
    if (status == AFORC_OK &&
        (!consumed || strcmp(game->message, "Save failed: not found.") != 0)) {
        return game_error(error,
                          AFORC_ERROR_STATE,
                          "smoke",
                          "save failure did not remain in game");
    }
    return status;
}

AFORC_Status game_smoke_checks(Game *game,
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
    status = game_smoke_save_failure_is_recoverable(game, engine, error);
    if (status == AFORC_OK) {
        status = game_smoke_weak_burst_preserves_particles(game,
                                                           position->point,
                                                           error);
    }
    if (status == AFORC_OK) {
        status = game_emit_burst(game, position->point, true);
    }
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
