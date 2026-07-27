/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

static uint64_t game_enemy_perception(const GameActor *actor) {
    return actor->glyph == (uint32_t)'S' ? UINT64_C(12) : UINT64_C(7);
}

static const char *game_enemy_name(const GameActor *actor) {
    return actor->glyph == (uint32_t)'S' ? "sentinel" : "goblin";
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

AFORC_Status game_enemy_turns(Game *game) {
    AFORC_Entity enemies[GAME_MAX_ENEMIES];
    size_t enemy_count = 0U;
    size_t hit_count = 0U;
    int32_t total_damage = 0;
    const char *last_attacker = NULL;
    GameActor *player_actor = NULL;
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
            total_damage += enemy_actor->attack;
            ++hit_count;
            last_attacker = game_enemy_name(enemy_actor);
            (void)game_emit_burst(game, player_position->point, false);
            if (player_actor->health <= 0) {
                player_actor->health = 0;
                game->run_state = GAME_DEFEATED;
                game_set_message(game,
                                 "The %s hits for %d and ends your run on "
                                 "floor %u.",
                                 last_attacker,
                                 enemy_actor->attack,
                                 game->floor);
                break;
            }
        } else if (distance <= game_enemy_perception(enemy_actor)) {
            const AFORC_Point *path = NULL;
            size_t path_length = 0U;
            AFORC_PathOptions options = aforc_path_options_default();
            bool awareness_blocked = false;

            options.max_visited = 1800U;
            status = aforc_grid_raycast(game->map,
                                        0U,
                                        enemy_position->point,
                                        player_position->point,
                                        game_tile_blocks,
                                        NULL,
                                        &awareness_blocked,
                                        NULL);
            if (status == AFORC_OK && !awareness_blocked) {
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
            } else if (status == AFORC_OK) {
                continue;
            }
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
    if (status == AFORC_OK && hit_count == 1U &&
        game->run_state == GAME_PLAYING) {
        game_set_message(game,
                         "The %s hits for %d. Health: %d/%d.",
                         last_attacker,
                         total_damage,
                         player_actor->health,
                         player_actor->maximum_health);
    } else if (status == AFORC_OK && hit_count > 1U &&
               game->run_state == GAME_PLAYING) {
        game_set_message(game,
                         "%zu enemies hit for %d total. Health: %d/%d.",
                         hit_count,
                         total_damage,
                         player_actor->health,
                         player_actor->maximum_health);
    }
    return status;
}
