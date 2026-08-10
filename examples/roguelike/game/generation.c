/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

#include <string.h>

static AFORC_Status game_random_range(AFORC_Rng *rng,
                                      uint32_t minimum,
                                      uint32_t maximum,
                                      uint32_t *out_value)
{
    uint32_t offset = 0U;
    AFORC_Status status;

    if (minimum > maximum || out_value == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_rng_bounded_u32(rng, maximum - minimum + 1U, &offset);
    if (status == AFORC_OK)
    {
        *out_value = minimum + offset;
    }
    return status;
}

static uint32_t game_particle_seed(uint64_t seed, uint32_t floor)
{
    const uint64_t mixed =
        seed ^ (seed >> 32U) ^ (UINT64_C(0x9e3779b97f4a7c15) * floor);

    return (uint32_t)(mixed ^ (mixed >> 32U));
}

static bool game_rooms_overlap(GameRoom left, GameRoom right)
{
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
                                        bool horizontal_first)
{
    AFORC_Point cursor = start;
    AFORC_Status status = AFORC_OK;

    while (!aforc_world_point_equal(cursor, end))
    {
        if ((horizontal_first && cursor.x != end.x) || cursor.y == end.y)
        {
            cursor.x += cursor.x < end.x ? 1 : -1;
        }
        else
        {
            cursor.y += cursor.y < end.y ? 1 : -1;
        }
        status = aforc_tilemap_set(game->map, 0U, cursor, TILE_FLOOR);
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}

AFORC_Status
game_generate_floor(Game *game, uint32_t floor, int32_t player_health)
{
    GameRoom rooms[GAME_MAX_ROOMS];
    size_t room_count = 0U;
    uint32_t attempts = 0U;
    AFORC_TileMap *new_map = NULL;
    AFORC_Rng rng;
    AFORC_Status status;

    /* Each floor has its own derived stream so save loading can regenerate it.
     */
    status = aforc_rng_seed(&rng,
                            game->seed ^ (UINT64_C(0x9e3779b97f4a7c15) * floor),
                            UINT64_C(0xda3e39cb94b95bdb) + floor);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_tilemap_create(
        (AFORC_Size){game->rules.map_width, game->rules.map_height},
        1U,
        TILE_WALL,
        &game->allocator,
        &new_map);
    if (status != AFORC_OK)
    {
        return status;
    }
    aforc_tilemap_destroy(game->map);
    game->map = new_map;
    status = aforc_ecs_clear(game->ecs);
    if (status != AFORC_OK)
    {
        return status;
    }
    game->floor = floor;
    while (room_count < game->rules.room_count &&
           attempts < game->rules.room_count * 16U)
    {
        uint32_t width = 0U;
        uint32_t height = 0U;
        uint32_t x = 0U;
        uint32_t y = 0U;
        uint32_t horizontal_first = 0U;
        GameRoom candidate;
        bool overlaps = false;

        ++attempts;
        status = game_random_range(&rng, 6U, 12U, &width);
        if (status == AFORC_OK)
        {
            status = game_random_range(&rng, 5U, 9U, &height);
        }
        if (status == AFORC_OK)
        {
            status = game_random_range(
                &rng, 1U, (uint32_t)game->rules.map_width - width - 2U, &x);
        }
        if (status == AFORC_OK)
        {
            status = game_random_range(
                &rng, 1U, (uint32_t)game->rules.map_height - height - 2U, &y);
        }
        if (status != AFORC_OK)
        {
            return status;
        }
        candidate.bounds = (AFORC_Rect){
            (int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height};
        candidate.center = (AFORC_Point){(int32_t)(x + width / 2U),
                                         (int32_t)(y + height / 2U)};
        for (size_t index = 0U; index < room_count; ++index)
        {
            if (game_rooms_overlap(candidate, rooms[index]))
            {
                overlaps = true;
                break;
            }
        }
        if (overlaps)
        {
            continue;
        }
        status = aforc_tilemap_fill_rect(
            game->map, 0U, candidate.bounds, TILE_FLOOR);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (room_count != 0U)
        {
            status = aforc_rng_bounded_u32(&rng, 2U, &horizontal_first);
            if (status == AFORC_OK)
            {
                status = game_carve_corridor(game,
                                             rooms[room_count - 1U].center,
                                             candidate.center,
                                             horizontal_first != 0U);
            }
            if (status != AFORC_OK)
            {
                return status;
            }
        }
        rooms[room_count++] = candidate;
    }
    if (room_count < 2U)
    {
        return AFORC_ERROR_STATE;
    }
    game->exit_position = rooms[room_count - 1U].center;
    status = aforc_tilemap_set(game->map, 0U, game->exit_position, TILE_EXIT);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = game_populate_floor(game, &rng, rooms, room_count, player_health);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(game->visibility, 0, game->cell_count);
    (void)memset(game->explored, 0, game->cell_count);
    game->visibility_valid = false;
    status = aforc_particle_pool_clear(&game->particle_pool);
    if (status == AFORC_OK)
    {
        status = aforc_particle_pool_reseed(
            &game->particle_pool, game_particle_seed(game->seed, floor));
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    game->run_state = GAME_PLAYING;
    game->help_visible = false;
    game_set_message(
        game, "Floor %u: find the green exit and press >.", game->floor);
    return AFORC_OK;
}

AFORC_Status game_new_run(Game *game)
{
    game->score = 0U;
    game->turn = 0U;
    return game_generate_floor(game, 1U, game->rules.player_health);
}
