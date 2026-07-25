/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

static AFORC_Status game_spawn_enemies(Game *game,
                                       AFORC_Rng *rng,
                                       const GameRoom *rooms,
                                       size_t room_count) {
    uint32_t desired = game->rules.enemy_count + (game->floor - 1U) * 2U;
    uint32_t spawned = 0U;
    uint32_t attempts = 0U;

    if (desired > GAME_MAX_ENEMIES) {
        desired = GAME_MAX_ENEMIES;
    }
    while (spawned < desired && attempts < desired * 20U) {
        uint32_t room_offset = 0U;
        uint32_t x_offset = 0U;
        uint32_t y_offset = 0U;
        AFORC_Point point;
        AFORC_Entity occupant = AFORC_ENTITY_INVALID;
        bool occupied = false;
        GameActor enemy;
        AFORC_Status status;

        ++attempts;
        status = aforc_rng_bounded_u32(rng,
                                       (uint32_t)room_count - 1U,
                                       &room_offset);
        if (status == AFORC_OK) {
            const GameRoom *room = &rooms[room_offset + 1U];

            status = aforc_rng_bounded_u32(
                rng,
                (uint32_t)room->bounds.width - 2U,
                &x_offset);
            if (status == AFORC_OK) {
                status = aforc_rng_bounded_u32(
                    rng,
                    (uint32_t)room->bounds.height - 2U,
                    &y_offset);
            }
            point = (AFORC_Point){room->bounds.x + 1 + (int32_t)x_offset,
                                  room->bounds.y + 1 + (int32_t)y_offset};
        }
        if (status != AFORC_OK) {
            return status;
        }
        status = game_entity_at(game, point, &occupant, &occupied);
        if (status != AFORC_OK) {
            return status;
        }
        if (occupied || aforc_world_point_equal(point, game->exit_position)) {
            continue;
        }
        enemy.health = game->rules.enemy_health + (int32_t)game->floor;
        enemy.maximum_health = enemy.health;
        enemy.attack = game->rules.enemy_attack + (int32_t)(game->floor / 2U);
        enemy.glyph = spawned % 3U == 0U ? (uint32_t)'S' : (uint32_t)'g';
        enemy.color = spawned % 3U == 0U ? aforc_color_indexed(141U)
                                         : aforc_color_indexed(203U);
        enemy.hostile = true;
        status = game_create_actor(game, point, enemy, NULL);
        if (status != AFORC_OK) {
            return status;
        }
        ++spawned;
    }
    return spawned == desired ? AFORC_OK : AFORC_ERROR_LIMIT;
}

AFORC_Status game_populate_floor(Game *game,
                                 AFORC_Rng *rng,
                                 const GameRoom *rooms,
                                 size_t room_count,
                                 int32_t player_health) {
    GameActor player;
    AFORC_Status status;

    player.maximum_health = game->rules.player_health;
    player.health = player_health > player.maximum_health
                        ? player.maximum_health
                        : player_health;
    if (player.health < 1) {
        player.health = 1;
    }
    player.attack = game->rules.player_attack;
    player.glyph = (uint32_t)'@';
    player.color = aforc_color_indexed(220U);
    player.hostile = false;
    status = game_create_actor(game,
                               rooms[0].center,
                               player,
                               &game->player);
    if (status == AFORC_OK) {
        status = game_spawn_enemies(game, rng, rooms, room_count);
    }
    return status;
}
