/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <string.h>

typedef struct GameRoom {
    AFORC_Rect bounds;
    AFORC_Point center;
} GameRoom;

static AFORC_Status game_random_range(AFORC_Rng *rng,
                                    uint32_t minimum,
                                    uint32_t maximum,
                                    uint32_t *out_value) {
    uint32_t offset = 0U;
    AFORC_Status status;

    if (minimum > maximum || out_value == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_rng_bounded_u32(rng, maximum - minimum + 1U, &offset);
    if (status == AFORC_OK) {
        *out_value = minimum + offset;
    }
    return status;
}

static bool game_rooms_overlap(GameRoom left, GameRoom right) {
    const int64_t left_right =
        (int64_t)left.bounds.x + (int64_t)left.bounds.width + 1;
    const int64_t left_bottom =
        (int64_t)left.bounds.y + (int64_t)left.bounds.height + 1;
    const int64_t right_right =
        (int64_t)right.bounds.x + (int64_t)right.bounds.width + 1;
    const int64_t right_bottom =
        (int64_t)right.bounds.y + (int64_t)right.bounds.height + 1;

    return (int64_t)left.bounds.x - 1 < right_right &&
           (int64_t)right.bounds.x - 1 < left_right &&
           (int64_t)left.bounds.y - 1 < right_bottom &&
           (int64_t)right.bounds.y - 1 < left_bottom;
}

static AFORC_Status game_carve_corridor(Game *game,
                                      AFORC_Point start,
                                      AFORC_Point end,
                                      bool horizontal_first) {
    AFORC_Point cursor = start;
    AFORC_Status status = AFORC_OK;

    while (!aforc_world_point_equal(cursor, end)) {
        if ((horizontal_first && cursor.x != end.x) || cursor.y == end.y) {
            cursor.x += cursor.x < end.x ? 1 : -1;
        } else {
            cursor.y += cursor.y < end.y ? 1 : -1;
        }
        status = aforc_tilemap_set(game->map, 0U, cursor, TILE_FLOOR);
        if (status != AFORC_OK) {
            return status;
        }
    }
    return AFORC_OK;
}

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
        uint32_t room_index = 0U;
        uint32_t x = 0U;
        uint32_t y = 0U;
        AFORC_Point point;
        AFORC_Entity occupant = AFORC_ENTITY_INVALID;
        bool occupied = false;
        GameActor enemy;
        AFORC_Status status;

        ++attempts;
        status = game_random_range(rng,
                                   1U,
                                   (uint32_t)room_count - 1U,
                                   &room_index);
        if (status == AFORC_OK) {
            const GameRoom *room = &rooms[room_index];
            status = game_random_range(
                rng,
                (uint32_t)(room->bounds.x + 1),
                (uint32_t)(room->bounds.x + room->bounds.width - 2),
                &x);
            if (status == AFORC_OK) {
                status = game_random_range(
                    rng,
                    (uint32_t)(room->bounds.y + 1),
                    (uint32_t)(room->bounds.y + room->bounds.height - 2),
                    &y);
            }
        }
        if (status != AFORC_OK) {
            return status;
        }
        point = (AFORC_Point){(int32_t)x, (int32_t)y};
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

AFORC_Status game_generate_floor(Game *game,
                               uint32_t floor,
                               int32_t player_health) {
    GameRoom rooms[GAME_MAX_ROOMS];
    size_t room_count = 0U;
    uint32_t attempts = 0U;
    AFORC_TileMap *new_map = NULL;
    AFORC_Rng rng;
    AFORC_Status status;

    /* Each floor has its own derived stream so save loading can regenerate it. */
    status = aforc_rng_seed(&rng,
                          game->seed ^
                              (UINT64_C(0x9e3779b97f4a7c15) * floor),
                          UINT64_C(0xda3e39cb94b95bdb) + floor);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_tilemap_create((AFORC_Size){game->rules.map_width,
                                           game->rules.map_height},
                                1U,
                                TILE_WALL,
                                &game->allocator,
                                &new_map);
    if (status != AFORC_OK) {
        return status;
    }
    aforc_tilemap_destroy(game->map);
    game->map = new_map;
    status = aforc_ecs_clear(game->ecs);
    if (status != AFORC_OK) {
        return status;
    }
    game->floor = floor;
    while (room_count < game->rules.room_count &&
           attempts < game->rules.room_count * 16U) {
        uint32_t width = 0U;
        uint32_t height = 0U;
        uint32_t x = 0U;
        uint32_t y = 0U;
        uint32_t horizontal_first = 0U;
        GameRoom candidate;
        bool overlaps = false;

        ++attempts;
        status = game_random_range(&rng, 6U, 12U, &width);
        if (status == AFORC_OK) {
            status = game_random_range(&rng, 5U, 9U, &height);
        }
        if (status == AFORC_OK) {
            status = game_random_range(
                &rng,
                1U,
                (uint32_t)game->rules.map_width - width - 2U,
                &x);
        }
        if (status == AFORC_OK) {
            status = game_random_range(
                &rng,
                1U,
                (uint32_t)game->rules.map_height - height - 2U,
                &y);
        }
        if (status != AFORC_OK) {
            return status;
        }
        candidate.bounds =
            (AFORC_Rect){(int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height};
        candidate.center =
            (AFORC_Point){(int32_t)(x + width / 2U),
                        (int32_t)(y + height / 2U)};
        for (size_t index = 0U; index < room_count; ++index) {
            if (game_rooms_overlap(candidate, rooms[index])) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) {
            continue;
        }
        status = aforc_tilemap_fill_rect(game->map,
                                       0U,
                                       candidate.bounds,
                                       TILE_FLOOR);
        if (status != AFORC_OK) {
            return status;
        }
        if (room_count != 0U) {
            status = aforc_rng_bounded_u32(&rng, 2U, &horizontal_first);
            if (status == AFORC_OK) {
                status = game_carve_corridor(game,
                                             rooms[room_count - 1U].center,
                                             candidate.center,
                                             horizontal_first != 0U);
            }
            if (status != AFORC_OK) {
                return status;
            }
        }
        rooms[room_count++] = candidate;
    }
    if (room_count < 2U) {
        return AFORC_ERROR_STATE;
    }
    game->exit_position = rooms[room_count - 1U].center;
    status = aforc_tilemap_set(game->map,
                             0U,
                             game->exit_position,
                             TILE_EXIT);
    if (status != AFORC_OK) {
        return status;
    }
    {
        GameActor player;

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
    }
    if (status == AFORC_OK) {
        status = game_spawn_enemies(game, &rng, rooms, room_count);
    }
    if (status != AFORC_OK) {
        return status;
    }
    (void)memset(game->visibility, 0, game->cell_count);
    (void)memset(game->explored, 0, game->cell_count);
    (void)aforc_particle_pool_clear(&game->particle_pool);
    game->run_state = GAME_PLAYING;
    game->help_visible = false;
    game_set_message(game,
                     "Floor %u: find the green exit and press >.",
                     game->floor);
    return AFORC_OK;
}

AFORC_Status game_new_run(Game *game) {
    game->score = 0U;
    game->turn = 0U;
    return game_generate_floor(game, 1U, game->rules.player_health);
}
