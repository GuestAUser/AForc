#include "fieldzero/game.h"

static uint64_t fieldzero_digest_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t fieldzero_digest_u64(uint64_t hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
    {
        hash = fieldzero_digest_byte(hash, (uint8_t)(value >> shift));
    }
    return hash;
}

static uint64_t fieldzero_digest_player(uint64_t hash,
                                        const FieldzeroPlayer *player)
{
    hash = fieldzero_digest_u64(hash, (uint32_t)player->x);
    hash = fieldzero_digest_u64(hash, (uint32_t)player->y);
    hash = fieldzero_digest_u64(hash, (uint32_t)player->velocity_x);
    hash = fieldzero_digest_u64(hash, (uint32_t)player->velocity_y);
    hash = fieldzero_digest_u64(hash, (uint8_t)player->facing);
    hash = fieldzero_digest_u64(hash, player->coyote_ticks);
    hash = fieldzero_digest_u64(hash, player->jump_buffer_ticks);
    hash = fieldzero_digest_u64(hash, player->wall_lock_ticks);
    hash = fieldzero_digest_u64(hash, player->dash_ticks);
    hash = fieldzero_digest_u64(hash, player->grounded ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, player->wall_left ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, player->wall_right ? 1U : 0U);
    return fieldzero_digest_u64(hash, player->dash_available ? 1U : 0U);
}

uint64_t fieldzero_game_collision_digest(const FieldzeroGame *game)
{
    const AFORC_TileMap *map;
    uint64_t hash = UINT64_C(14695981039346656037);

    if (game == NULL)
    {
        return 0U;
    }
    map = game->phase == FIELDZERO_PHASE_REGISTERING ? game->static_map
                                                     : game->active_map;
    if (map == NULL)
    {
        return 0U;
    }
    hash = fieldzero_digest_u64(hash, FIELDZERO_ARENA_WIDTH);
    hash = fieldzero_digest_u64(hash, FIELDZERO_ARENA_HEIGHT);
    for (int32_t y = 0; y < FIELDZERO_ARENA_HEIGHT; ++y)
    {
        for (int32_t x = 0; x < FIELDZERO_ARENA_WIDTH; ++x)
        {
            AFORC_Tile tile = 0U;

            if (aforc_tilemap_get(map, 0U, (AFORC_Point){x, y}, &tile) !=
                AFORC_OK)
            {
                return 0U;
            }
            hash = fieldzero_digest_u64(hash, tile);
        }
    }
    return hash;
}

uint64_t fieldzero_game_state_digest(const FieldzeroGame *game)
{
    uint64_t hash = UINT64_C(14695981039346656037);

    if (game == NULL)
    {
        return 0U;
    }
    hash = fieldzero_digest_u64(hash, game->falls);
    hash = fieldzero_digest_u64(hash, game->completed_rooms);
    hash = fieldzero_digest_u64(hash, game->collected_memories);
    for (size_t index = 0U; index < FIELDZERO_ROOM_COUNT; ++index)
    {
        hash = fieldzero_digest_u64(hash, game->room_states[index]);
    }
    hash = fieldzero_digest_u64(hash, game->room_index);
    hash = fieldzero_digest_u64(hash, game->room_state);
    hash = fieldzero_digest_u64(hash, game->registration_target_state);
    hash = fieldzero_digest_u64(hash, (uint32_t)game->phase);
    hash = fieldzero_digest_u64(hash, game->phase_ticks);
    hash = fieldzero_digest_u64(hash, (uint8_t)game->transition_direction);
    hash = fieldzero_digest_u64(hash, game->memory_collected_here ? 1U : 0U);
    hash = fieldzero_digest_player(hash, &game->player);
    hash = fieldzero_digest_u64(hash, (uint32_t)game->checkpoint.x);
    hash = fieldzero_digest_u64(hash, (uint32_t)game->checkpoint.y);
    hash = fieldzero_digest_u64(hash, game->checkpoint.room_state);
    hash = fieldzero_digest_u64(hash, game->actions.left ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, game->actions.right ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, game->actions.jump_held ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, game->actions.jump_pressed ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, game->actions.jump_released ? 1U : 0U);
    hash = fieldzero_digest_u64(hash, game->actions.dash_pressed ? 1U : 0U);
    return fieldzero_digest_u64(hash, fieldzero_game_collision_digest(game));
}
