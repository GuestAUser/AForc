/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

static AFORC_Status game_point_blocked(Game *game,
                                     AFORC_Point point,
                                     bool *out_blocked) {
    if (!aforc_tilemap_contains(game->map, point)) {
        *out_blocked = true;
        return AFORC_OK;
    }
    return aforc_grid_point_blocked(game->map,
                                  0U,
                                  point,
                                  game_tile_blocks,
                                  NULL,
                                  out_blocked);
}

static AFORC_Status game_gather_enemies(Game *game,
                                      AFORC_Entity *entities,
                                      size_t capacity,
                                      size_t *out_count) {
    const AFORC_ComponentType types[2] = {game->position_type,
                                        game->actor_type};
    AFORC_EcsView *view = NULL;
    size_t count = 0U;
    AFORC_Status status = aforc_ecs_view_create(game->ecs, types, 2U, &view);

    while (status == AFORC_OK) {
        AFORC_Entity entity = AFORC_ENTITY_INVALID;
        void *components[2] = {NULL, NULL};
        bool has_value = false;
        GameActor *actor;

        status = aforc_ecs_view_next(view,
                                   &entity,
                                   components,
                                   &has_value);
        if (status != AFORC_OK || !has_value) {
            break;
        }
        actor = components[1];
        if (!actor->hostile) {
            continue;
        }
        if (count == capacity) {
            status = AFORC_ERROR_LIMIT;
            break;
        }
        entities[count++] = entity;
    }
    aforc_ecs_view_destroy(view);
    if (status == AFORC_OK) {
        *out_count = count;
    }
    return status;
}

static AFORC_Status game_enemy_turns(Game *game) {
    AFORC_Entity enemies[GAME_MAX_ENEMIES];
    size_t enemy_count = 0U;
    AFORC_Status status;

    /* A handle snapshot permits mutation while preserving deterministic order. */
    status = game_gather_enemies(game,
                                 enemies,
                                 GAME_MAX_ENEMIES,
                                 &enemy_count);
    for (size_t index = 0U; status == AFORC_OK && index < enemy_count; ++index) {
        GamePosition *enemy_position = NULL;
        GameActor *enemy_actor = NULL;
        GamePosition *player_position = NULL;
        GameActor *player_actor = NULL;
        uint64_t distance;

        if (!aforc_ecs_entity_alive(game->ecs, enemies[index])) {
            continue;
        }
        status = game_actor_components(game,
                                       enemies[index],
                                       &enemy_position,
                                       &enemy_actor);
        if (status == AFORC_OK) {
            status = game_actor_components(game,
                                           game->player,
                                           &player_position,
                                           &player_actor);
        }
        if (status != AFORC_OK) {
            break;
        }
        distance = aforc_world_point_manhattan(enemy_position->point,
                                             player_position->point);
        if (distance == 1U) {
            player_actor->health -= enemy_actor->attack;
            (void)game_emit_burst(game, player_position->point, false);
            if (player_actor->health <= 0) {
                player_actor->health = 0;
                game->run_state = GAME_DEFEATED;
                game_set_message(game,
                                 "The sentinels ended your run on floor %u.",
                                 game->floor);
                break;
            }
            game_set_message(game,
                             "A sentinel hits for %d. Health: %d/%d.",
                             enemy_actor->attack,
                             player_actor->health,
                             player_actor->maximum_health);
        } else if (distance <= 20U) {
            const AFORC_Point *path = NULL;
            size_t path_length = 0U;
            AFORC_PathOptions options = aforc_path_options_default();

            options.max_visited = 1800U;
            status = aforc_pathfind_astar_workspace(
                game->path_workspace,
                game->map,
                0U,
                enemy_position->point,
                player_position->point,
                game_tile_blocks,
                NULL,
                &options,
                &path,
                &path_length);
            if (status == AFORC_ERROR_NOT_FOUND || status == AFORC_ERROR_LIMIT) {
                status = AFORC_OK;
                continue;
            }
            if (status == AFORC_OK && path_length > 1U) {
                AFORC_Entity occupant = AFORC_ENTITY_INVALID;
                bool occupied = false;

                status = game_entity_at(game,
                                        path[1],
                                        &occupant,
                                        &occupied);
                if (status == AFORC_OK && !occupied) {
                    enemy_position->point = path[1];
                }
            }
        }
    }
    return status;
}

static AFORC_Status game_take_turn(Game *game) {
    ++game->turn;
    return game_enemy_turns(game);
}

static AFORC_Status game_move_player(Game *game, AFORC_Point delta) {
    GamePosition *player_position = NULL;
    GameActor *player_actor = NULL;
    AFORC_Point destination;
    AFORC_Entity target = AFORC_ENTITY_INVALID;
    bool occupied = false;
    bool blocked = false;
    AFORC_Status status = game_actor_components(game,
                                              game->player,
                                              &player_position,
                                              &player_actor);

    if (status == AFORC_OK) {
        status = aforc_world_point_add(player_position->point,
                                     delta,
                                     &destination);
    }
    if (status == AFORC_OK) {
        status = game_point_blocked(game, destination, &blocked);
    }
    if (status != AFORC_OK) {
        return status;
    }
    if (blocked) {
        game_set_message(game, "Stone blocks the way.");
        return AFORC_OK;
    }
    status = game_entity_at(game, destination, &target, &occupied);
    if (status != AFORC_OK) {
        return status;
    }
    if (occupied) {
        GamePosition *target_position = NULL;
        GameActor *target_actor = NULL;

        if (aforc_entity_equal(target, game->player)) {
            return AFORC_OK;
        }
        status = game_actor_components(game,
                                       target,
                                       &target_position,
                                       &target_actor);
        if (status != AFORC_OK || !target_actor->hostile) {
            return status;
        }
        target_actor->health -= player_actor->attack;
        (void)game_emit_burst(game, target_position->point, true);
        if (target_actor->health <= 0) {
            status = aforc_ecs_destroy_entity(game->ecs, target);
            if (status != AFORC_OK) {
                return status;
            }
            game->score += 100U;
            game_set_message(game, "Sentinel defeated. Score: %u.", game->score);
        } else {
            game_set_message(game,
                             "You strike for %d. Sentinel health: %d.",
                             player_actor->attack,
                             target_actor->health);
        }
    } else {
        player_position->point = destination;
        if (aforc_world_point_equal(destination, game->exit_position)) {
            game_set_message(game, "The exit hums here. Press > to descend.");
        } else {
            game_set_message(game, "You advance through the ruins.");
        }
    }
    return game_take_turn(game);
}

static AFORC_Status game_descend(Game *game) {
    GamePosition *position = NULL;
    GameActor *actor = NULL;
    AFORC_Status status = game_actor_components(game,
                                              game->player,
                                              &position,
                                              &actor);

    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_world_point_equal(position->point, game->exit_position)) {
        game_set_message(game, "Stand on the green > before descending.");
        return AFORC_OK;
    }
    if (game->floor == game->rules.final_floor) {
        game->score += 1000U;
        game->run_state = GAME_VICTORIOUS;
        game_set_message(game,
                         "The final seal breaks. Victory with %u points!",
                         game->score);
        return AFORC_OK;
    }
    game->score += 250U;
    actor->health += 3;
    if (actor->health > actor->maximum_health) {
        actor->health = actor->maximum_health;
    }
    return game_generate_floor(game, game->floor + 1U, actor->health);
}

static uint32_t game_event_codepoint(const AFORC_InputEvent *event) {
    uint32_t codepoint = event->data.key.codepoint;

    if (codepoint == 0U && event->data.key.key >= AFORC_KEY_A &&
        event->data.key.key <= AFORC_KEY_Z) {
        codepoint = (uint32_t)event->data.key.key;
        if ((event->data.key.modifiers & AFORC_MOD_SHIFT) == 0U) {
            codepoint += (uint32_t)('a' - 'A');
        }
    }
    return codepoint;
}

static AFORC_Status game_handle_key(Game *game,
                                  AFORC_Engine *engine,
                                  const AFORC_InputEvent *event,
                                  bool *out_consumed) {
    const AFORC_Key key = event->data.key.key;
    const uint32_t codepoint = game_event_codepoint(event);
    AFORC_Point movement = {0, 0};
    bool move = false;
    AFORC_Status status = AFORC_OK;

    *out_consumed = true;
    if (key == AFORC_KEY_ESCAPE || codepoint == (uint32_t)'q' ||
        codepoint == (uint32_t)'Q') {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (codepoint == (uint32_t)'?') {
        game->help_visible = !game->help_visible;
        game_set_message(game,
                         game->help_visible ? "Help opened." : "Help closed.");
        return AFORC_OK;
    }
    if (game->help_visible) {
        return AFORC_OK;
    }
    if (game->run_state != GAME_PLAYING) {
        if (codepoint == (uint32_t)'r' || codepoint == (uint32_t)'R') {
            return game_new_run(game);
        }
        game_set_message(game, "Press R for a new run or Q to quit.");
        return AFORC_OK;
    }
    if (codepoint == (uint32_t)'S') {
        status = game_save(game);
    } else if (codepoint == (uint32_t)'L') {
        status = game_load(game);
    } else if (codepoint == (uint32_t)'>') {
        status = game_descend(game);
    } else if (codepoint == (uint32_t)'.' || key == AFORC_KEY_SPACE) {
        game_set_message(game, "You wait and listen.");
        status = game_take_turn(game);
    } else {
        if (key == AFORC_KEY_UP || codepoint == (uint32_t)'w' ||
            codepoint == (uint32_t)'k') {
            movement.y = -1;
            move = true;
        } else if (key == AFORC_KEY_DOWN || codepoint == (uint32_t)'s' ||
                   codepoint == (uint32_t)'j') {
            movement.y = 1;
            move = true;
        } else if (key == AFORC_KEY_LEFT || codepoint == (uint32_t)'a' ||
                   codepoint == (uint32_t)'h') {
            movement.x = -1;
            move = true;
        } else if (key == AFORC_KEY_RIGHT || codepoint == (uint32_t)'d' ||
                   codepoint == (uint32_t)'l') {
            movement.x = 1;
            move = true;
        }
        if (move) {
            status = game_move_player(game, movement);
        } else {
            game_set_message(game, "Unknown key. Press ? for controls.");
        }
    }
    return status;
}

AFORC_Status game_scene_event(AFORC_Scene *scene,
                            AFORC_Engine *engine,
                            const void *event_data,
                            bool *consumed,
                            AFORC_Error *error) {
    Game *game = scene->user_data;
    const AFORC_InputEvent *event = event_data;
    AFORC_Status status = AFORC_OK;

    *consumed = false;
    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN) {
        status = game_handle_key(game, engine, event, consumed);
    }
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "roguelike",
                          "could not process input event");
    }
    return AFORC_OK;
}
