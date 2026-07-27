/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct GameSmokeArchetypes {
    GamePosition *sentinel_position;
    GameActor *sentinel_actor;
    GamePosition *goblin_position;
    GameActor *goblin_actor;
} GameSmokeArchetypes;

static bool game_smoke_renderer_contains(Game *game, const char *text) {
    const AFORC_Size size = aforc_renderer_size(game->renderer);
    const size_t length = strlen(text);

    for (int32_t y = 0; y < size.height; ++y) {
        for (int32_t x = 0; x + (int32_t)length <= size.width; ++x) {
            size_t index = 0U;

            while (index < length) {
                AFORC_Cell cell;

                if (aforc_renderer_get(
                        game->renderer,
                        (AFORC_Point){x + (int32_t)index, y},
                        &cell) != AFORC_OK ||
                    cell.codepoint != (uint32_t)(unsigned char)text[index]) {
                    break;
                }
                ++index;
            }
            if (index == length) {
                return true;
            }
        }
    }
    return false;
}

static AFORC_Status game_smoke_archetypes(Game *game,
                                           bool isolate,
                                           GameSmokeArchetypes *out_enemies,
                                           AFORC_Error *error) {
    const AFORC_ComponentType types[2] = {game->position_type,
                                          game->actor_type};
    AFORC_EcsView *view = NULL;
    size_t relocated = 0U;
    bool has_value = false;
    AFORC_Status status;

    (void)memset(out_enemies, 0, sizeof(*out_enemies));
    status = aforc_ecs_view_create(game->ecs, types, 2U, &view);
    while (status == AFORC_OK) {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        GamePosition *position;
        GameActor *actor;

        status = aforc_ecs_view_next(view,
                                     &entity,
                                     components,
                                     &has_value);
        if (status != AFORC_OK || !has_value) {
            break;
        }
        position = components[0];
        actor = components[1];
        if (!actor->hostile) {
            continue;
        }
        if (actor->glyph == (uint32_t)'S' &&
            out_enemies->sentinel_actor == NULL) {
            out_enemies->sentinel_position = position;
            out_enemies->sentinel_actor = actor;
        } else if (actor->glyph == (uint32_t)'g' &&
                   out_enemies->goblin_actor == NULL) {
            out_enemies->goblin_position = position;
            out_enemies->goblin_actor = actor;
        }
        if (isolate) {
            position->point =
                (AFORC_Point){game->rules.map_width - 2 -
                                  (int32_t)(relocated % 16U),
                              game->rules.map_height - 2 -
                                  (int32_t)(relocated / 16U)};
            actor->hostile = false;
            ++relocated;
        }
    }
    aforc_ecs_view_destroy(view);
    if (status == AFORC_OK &&
        (out_enemies->sentinel_actor == NULL ||
         out_enemies->goblin_actor == NULL)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "floor did not contain both enemy archetypes");
    }
    if (status == AFORC_OK && isolate) {
        out_enemies->sentinel_actor->hostile = true;
        out_enemies->goblin_actor->hostile = true;
    }
    return status;
}

static AFORC_Status game_smoke_balanced_archetypes(Game *game,
                                                    AFORC_Error *error) {
    GameSmokeArchetypes enemies;
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Status status = game_smoke_archetypes(game,
                                                false,
                                                &enemies,
                                                error);

    if (status == AFORC_OK) {
        status = game_actor_components(game,
                                       game->player,
                                       &player_position,
                                       &player_actor);
    }
    if (status == AFORC_OK &&
        (game->rules.player_health < 24 || game->rules.player_health > 60 ||
         player_actor->maximum_health != game->rules.player_health ||
         enemies.goblin_actor->maximum_health < 5 ||
         enemies.goblin_actor->maximum_health > 16 ||
         enemies.goblin_actor->attack < 1 || enemies.goblin_actor->attack > 6 ||
         enemies.sentinel_actor->maximum_health <=
             enemies.goblin_actor->maximum_health ||
         enemies.sentinel_actor->maximum_health > 24 ||
         enemies.sentinel_actor->attack <= enemies.goblin_actor->attack ||
         enemies.sentinel_actor->attack > 8)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "player or enemy archetype balance is unbounded");
    }
    return status;
}

static AFORC_Status game_smoke_viewport_freezes_turn(Game *game,
                                                      AFORC_Engine *engine,
                                                      AFORC_Error *error) {
    AFORC_InputEvent event = {0};
    const AFORC_Size saved_size = aforc_renderer_size(game->renderer);
    const uint32_t turn = game->turn;
    bool consumed = false;
    AFORC_Status status = aforc_renderer_resize(
        game->renderer,
        (AFORC_Size){GAME_MIN_COLUMNS - 1, GAME_MIN_ROWS});
    AFORC_Status restore_status;

    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_SPACE;
    if (status == AFORC_OK) {
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    }
    restore_status = aforc_renderer_resize(game->renderer, saved_size);
    if (status == AFORC_OK) {
        status = restore_status;
    }
    if (status == AFORC_OK &&
        (!consumed || game->turn != turn ||
         strcmp(game->message,
                "Resize terminal to at least 40x14 before acting.") != 0)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "unsupported viewport advanced a turn");
    }
    return status;
}

static AFORC_Status game_smoke_queued_turns_are_bounded(Game *game,
                                                        AFORC_Engine *engine,
                                                        AFORC_Error *error) {
    static const unsigned char burst[] = "..";
    AFORC_InputEvent discarded;
    const uint32_t turn = game->turn;
    size_t remaining = 0U;
    AFORC_Status status;

    aforc_input_release_all(game->input, 1000U);
    while (aforc_input_next_event(game->input, &discarded)) {
    }
    status = aforc_input_begin_frame(game->input);
    if (status == AFORC_OK) {
        status = aforc_input_feed(game->input,
                                  burst,
                                  sizeof(burst) - 1U,
                                  1001U);
    }
    if (status == AFORC_OK) {
        status = game_dispatch_input_queue(game, engine, error);
    }
    remaining = aforc_input_event_count(game->input);
    if (status == AFORC_OK &&
        (game->turn != turn + 1U || remaining == 0U)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "one frame drained multiple queued turns");
    }
    if (status == AFORC_OK) {
        status = aforc_input_begin_frame(game->input);
    }
    if (status == AFORC_OK) {
        status = game_dispatch_input_queue(game, engine, error);
    }
    if (status == AFORC_OK && game->turn != turn + 2U) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "queued turn was not retained for the next frame");
    }
    while (aforc_input_next_event(game->input, &discarded)) {
    }
    return status;
}

static AFORC_Status game_smoke_terminal_states_can_load(Game *game,
                                                        AFORC_Engine *engine,
                                                        AFORC_Error *error) {
    char smoke_save_path[96];
    AFORC_InputEvent event = {0};
    const char *saved_path = game->save_path;
    const uint32_t saved_floor = game->floor;
    const uint32_t saved_score = game->score;
    const uint32_t saved_turn = game->turn;
    bool consumed = false;
    bool stored = false;
    AFORC_Status status;

    (void)snprintf(smoke_save_path,
                   sizeof(smoke_save_path),
                   "aforc-smoke-ended-run-%ld-%" PRIu64 ".sav",
                   (long)getpid(),
                   game->seed);
    (void)remove(smoke_save_path);
    game->save_path = smoke_save_path;
    status = game_save(game);
    stored = status == AFORC_OK;
    event.type = AFORC_INPUT_EVENT_KEY_DOWN;
    event.data.key.key = AFORC_KEY_L;
    event.data.key.codepoint = (uint32_t)'L';
    game->run_state = GAME_DEFEATED;
    game->score = saved_score ^ 1U;
    if (status == AFORC_OK) {
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    }
    if (status == AFORC_OK &&
        (!consumed || game->run_state != GAME_PLAYING ||
         game->floor != saved_floor || game->score != saved_score ||
         game->turn != saved_turn)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "defeat state could not restore a saved run");
    }
    game->run_state = GAME_VICTORIOUS;
    game->score = saved_score ^ 1U;
    consumed = false;
    if (status == AFORC_OK) {
        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
    }
    if (status == AFORC_OK &&
        (!consumed || game->run_state != GAME_PLAYING ||
         game->floor != saved_floor || game->score != saved_score ||
         game->turn != saved_turn)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "victory state could not restore a saved run");
    }
    game->save_path = saved_path;
    if (stored && remove(smoke_save_path) != 0 && status == AFORC_OK) {
        status = game_error(error,
                            AFORC_ERROR_IO,
                            "smoke",
                            "ended-run smoke save could not be removed");
    }
    return status;
}

static AFORC_Status game_smoke_enemy_perception(Game *game,
                                                AFORC_Error *error) {
    GameSmokeArchetypes enemies;
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Status status = game_smoke_archetypes(game, true, &enemies, error);
    AFORC_Status reset_status;

    if (status == AFORC_OK) {
        status = game_actor_components(game,
                                       game->player,
                                       &player_position,
                                       &player_actor);
    }
    for (int32_t x = 2; status == AFORC_OK && x <= 12; ++x) {
        status = aforc_tilemap_set(game->map,
                                   0U,
                                   (AFORC_Point){x, 2},
                                   TILE_FLOOR);
        if (status == AFORC_OK) {
            status = aforc_tilemap_set(game->map,
                                       0U,
                                       (AFORC_Point){x, 4},
                                       TILE_FLOOR);
        }
    }
    for (int32_t y = 2; status == AFORC_OK && y <= 4; ++y) {
        status = aforc_tilemap_set(game->map,
                                   0U,
                                   (AFORC_Point){12, y},
                                   TILE_FLOOR);
    }
    if (status == AFORC_OK) {
        player_position->point = (AFORC_Point){12, 2};
        enemies.sentinel_position->point = (AFORC_Point){2, 2};
        enemies.goblin_position->point = (AFORC_Point){2, 4};
        status = game_enemy_turns(game);
    }
    if (status == AFORC_OK &&
        (!aforc_world_point_equal(enemies.sentinel_position->point,
                                  (AFORC_Point){3, 2}) ||
         !aforc_world_point_equal(enemies.goblin_position->point,
                                  (AFORC_Point){2, 4}))) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "enemy archetypes did not use distinct perception");
    }
    if (status == AFORC_OK) {
        player_position->point = (AFORC_Point){10, 2};
        enemies.sentinel_position->point = (AFORC_Point){2, 2};
        enemies.goblin_actor->hostile = false;
        status = aforc_tilemap_set(game->map,
                                   0U,
                                   (AFORC_Point){6, 2},
                                   TILE_WALL);
    }
    if (status == AFORC_OK) {
        status = game_enemy_turns(game);
    }
    if (status == AFORC_OK &&
        !aforc_world_point_equal(enemies.sentinel_position->point,
                                 (AFORC_Point){2, 2})) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "wall did not block long-range enemy awareness");
    }
    reset_status = game_new_run(game);
    if (status == AFORC_OK) {
        status = reset_status;
    }
    return status;
}

static AFORC_Status game_smoke_turn_feedback(Game *game,
                                             AFORC_Error *error) {
    GameSmokeArchetypes enemies;
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Status status = game_smoke_archetypes(game, true, &enemies, error);
    AFORC_Status reset_status;

    if (status == AFORC_OK) {
        status = game_actor_components(game,
                                       game->player,
                                       &player_position,
                                       &player_actor);
    }
    if (status == AFORC_OK) {
        status = aforc_tilemap_set(game->map,
                                   0U,
                                   (AFORC_Point){10, 10},
                                   TILE_FLOOR);
    }
    if (status == AFORC_OK) {
        status = aforc_tilemap_set(game->map,
                                   0U,
                                   (AFORC_Point){11, 10},
                                   TILE_FLOOR);
    }
    if (status == AFORC_OK) {
        player_position->point = (AFORC_Point){10, 10};
        player_actor->health = player_actor->maximum_health;
        enemies.sentinel_position->point = (AFORC_Point){11, 10};
        enemies.sentinel_actor->health =
            enemies.sentinel_actor->maximum_health;
        enemies.goblin_actor->hostile = false;
        status = game_move_player(game, (AFORC_Point){1, 0});
    }
    if (status == AFORC_OK &&
        (strstr(game->message, "You strike") == NULL ||
         strstr(game->message, "sentinel hits") == NULL)) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "turn summary lost action or retaliation feedback");
    }
    reset_status = game_new_run(game);
    if (status == AFORC_OK) {
        status = reset_status;
    }
    return status;
}

static AFORC_Status game_smoke_help_and_overlay(Game *game,
                                                AFORC_Engine *engine,
                                                AFORC_Error *error) {
    const GameRunState saved_run_state = game->run_state;
    const bool saved_help_visible = game->help_visible;
    AFORC_Status status;

    game->run_state = GAME_PLAYING;
    game->help_visible = false;
    status = game_scene_render(&game->scene, engine, 0.0, error);
    if (status == AFORC_OK &&
        !game_smoke_renderer_contains(game, "HP ")) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "HUD health meter was not text-labelled");
    }
    game->run_state = GAME_DEFEATED;
    if (status == AFORC_OK) {
        status = game_scene_render(&game->scene, engine, 0.0, error);
    }
    if (status == AFORC_OK &&
        !game_smoke_renderer_contains(game, "L Load save")) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "ended-run overlay did not expose load");
    }
    game->run_state = GAME_PLAYING;
    game->help_visible = true;
    if (status == AFORC_OK) {
        status = game_scene_render(&game->scene, engine, 0.0, error);
    }
    if (status == AFORC_OK &&
        !game_smoke_renderer_contains(game, "LOAD L (after run ends)")) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "help did not explain ended-run loading");
    }
    game->run_state = saved_run_state;
    game->help_visible = saved_help_visible;
    return status;
}

static AFORC_Status game_smoke_maturity_stage(AFORC_Status status,
                                               AFORC_Error *error,
                                               const char *message) {
    if (status != AFORC_OK && error->message[0] == '\0') {
        return game_error(error, status, "smoke", message);
    }
    return status;
}

static AFORC_Status game_smoke_burst_preserves_particles(
    Game *game,
    AFORC_Point point,
    bool strong,
    AFORC_Error *error) {
    AFORC_ParticleDesc particle = {0};
    const size_t burst_count = strong ? 12U : 7U;
    const size_t free_count = strong ? burst_count - 1U : burst_count;
    const size_t preserved_count = game->particle_pool.capacity - free_count;
    const size_t expected_count = preserved_count + free_count;
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
        status = game_emit_burst(game, point, strong);
    }
    if (status == AFORC_OK &&
        game->particle_pool.active_count != expected_count) {
        status = game_error(error,
                            AFORC_ERROR_STATE,
                            "smoke",
                            "burst discarded existing particles");
    }
    (void)aforc_particle_pool_clear(&game->particle_pool);
    return status;
}

static AFORC_Status game_smoke_score_saturates(Game *game,
                                                AFORC_Error *error) {
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    AFORC_Point saved_position;
    uint32_t saved_floor = game->floor;
    uint32_t saved_score = game->score;
    GameRunState saved_run_state = game->run_state;
    char saved_message[GAME_MESSAGE_CAPACITY];
    AFORC_Status status = game_actor_components(game,
                                                 game->player,
                                                 &position,
                                                 &actor);

    if (status != AFORC_OK) {
        return status;
    }
    saved_position = position->point;
    (void)memcpy(saved_message, game->message, sizeof(saved_message));
    position->point = game->exit_position;
    game->floor = game->rules.final_floor;
    game->score = UINT32_MAX - 999U;
    status = game_descend(game);
    if (status == AFORC_OK) {
        if (game->score != UINT32_MAX || game->run_state != GAME_VICTORIOUS) {
            status = game_error(error,
                                AFORC_ERROR_STATE,
                                "smoke",
                                "score overflowed at victory");
        }
    }
    position->point = saved_position;
    game->floor = saved_floor;
    game->score = saved_score;
    game->run_state = saved_run_state;
    (void)memcpy(game->message, saved_message, sizeof(saved_message));
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
    uint32_t saved_particle_random_state = game->particle_pool.random_state;
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
        status = game_emit_burst(game, position->point, true);
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
         actor->health != saved_health || game->particle_pool.active_count != 0U ||
         game->particle_pool.random_state != saved_particle_random_state)) {
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
    status = game_runtime_smoke_checks(game, engine, error);
    if (status == AFORC_OK) {
        status = game_smoke_burst_preserves_particles(game,
                                                       position->point,
                                                       false,
                                                       error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_burst_preserves_particles(game,
                                                       position->point,
                                                       true,
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
    if (status == AFORC_OK) {
        status = game_smoke_score_saturates(game, error);
    }
    if (status == AFORC_OK) {
        status = game_smoke_balanced_archetypes(game, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "archetype balance check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_viewport_freezes_turn(game, engine, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "viewport freeze check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_queued_turns_are_bounded(game, engine, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "queued turn check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_terminal_states_can_load(game, engine, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "ended-run load check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_enemy_perception(game, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "enemy perception check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_turn_feedback(game, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "turn feedback check failed");
    }
    if (status == AFORC_OK) {
        status = game_smoke_help_and_overlay(game, engine, error);
        status = game_smoke_maturity_stage(status,
                                           error,
                                           "help and overlay check failed");
    }
    aforc_free(&game->allocator, path);
    return status;
}
