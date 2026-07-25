/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

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

static void game_add_score(Game *game, uint32_t amount) {
    if (game->score > UINT32_MAX - amount) {
        game->score = UINT32_MAX;
        return;
    }
    game->score += amount;
}

static AFORC_Status game_take_turn(Game *game) {
    if (game->turn != UINT32_MAX) {
        ++game->turn;
    }
    return game_enemy_turns(game);
}

AFORC_Status game_move_player(Game *game, AFORC_Point delta) {
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
            game_add_score(game, 100U);
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

AFORC_Status game_wait_turn(Game *game) {
    game_set_message(game, "You wait and listen.");
    return game_take_turn(game);
}

AFORC_Status game_descend(Game *game) {
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
        game_add_score(game, 1000U);
        game->run_state = GAME_VICTORIOUS;
        game_set_message(game,
                         "The final seal breaks. Victory with %u points!",
                         game->score);
        return AFORC_OK;
    }
    game_add_score(game, 250U);
    actor->health += 3;
    if (actor->health > actor->maximum_health) {
        actor->health = actor->maximum_health;
    }
    return game_generate_floor(game, game->floor + 1U, actor->health);
}
