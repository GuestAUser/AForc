#include "fieldzero/game.h"

#include <string.h>

static AFORC_Point game_player_cell(const FieldzeroGame *game)
{
    return (AFORC_Point){fieldzero_fixed_to_cell(game->player.x),
                         fieldzero_fixed_to_cell(game->player.y)};
}

static uint16_t game_room_bit(uint8_t room_index)
{
    return (uint16_t)(UINT16_C(1) << room_index);
}

static void game_freeze(FieldzeroGame *game)
{
    game->player.velocity_x = 0;
    game->player.velocity_y = 0;
    game->player.jump_buffer_ticks = 0U;
    game->player.dash_ticks = 0U;
    (void)memset(&game->actions, 0, sizeof(game->actions));
}

static void game_begin_dissolve(FieldzeroGame *game)
{
    if (game->falls != UINT32_MAX)
    {
        ++game->falls;
    }
    game_freeze(game);
    game->phase = FIELDZERO_PHASE_DISSOLVING;
    game->phase_ticks = 0U;
    game->transition_direction = 0;
}

static void game_begin_room_transition(FieldzeroGame *game, int8_t direction)
{
    game_freeze(game);
    game->phase = FIELDZERO_PHASE_ROOM_TRANSITION;
    game->phase_ticks = 0U;
    game->transition_direction = direction;
}

static AFORC_Status game_tick_active_progression(FieldzeroGame *game)
{
    AFORC_Point player_cell;
    uint16_t room_bit;

    player_cell = game_player_cell(game);
    if (player_cell.y >= FIELDZERO_ARENA_HEIGHT)
    {
        game_begin_dissolve(game);
        return AFORC_OK;
    }
    room_bit = game_room_bit(game->room_index);
    if (fieldzero_room_has_memory(game->room) && !game->memory_collected_here &&
        aforc_world_point_equal(player_cell, game->room->memory))
    {
        game->collected_memories |= room_bit;
        game->memory_collected_here = true;
    }
    if (game->room_state < game->room->state_count - 1U &&
        game->room_state < fieldzero_room_mark_count(game->room) &&
        aforc_world_point_equal(player_cell,
                                game->room->marks[game->room_state]))
    {
        return fieldzero_game_begin_registration(game);
    }
    if (game->room_state + 1U == game->room->state_count &&
        aforc_world_point_equal(player_cell, game->room->exit))
    {
        game->completed_rooms |= room_bit;
        game_begin_room_transition(game, 1);
    }
    else if (game->room_index > 0U &&
             aforc_world_point_equal(player_cell, game->room->entry))
    {
        game_begin_room_transition(game, -1);
    }
    return AFORC_OK;
}

static AFORC_Status game_finish_room_transition(FieldzeroGame *game)
{
    const FieldzeroRoomDefinition *next_room;
    FieldzeroSector previous_sector;
    size_t next_index;
    int8_t direction;
    AFORC_Status status;

    direction = game->transition_direction;
    if (direction > 0 && game->room_index + 1U >= FIELDZERO_ROOM_COUNT)
    {
        game->phase = FIELDZERO_PHASE_COMPLETE;
        game->phase_ticks = 0U;
        game->transition_direction = 0;
        return AFORC_OK;
    }
    if ((direction < 0 && game->room_index == 0U) || direction == 0)
    {
        return AFORC_ERROR_STATE;
    }
    next_index = direction > 0 ? (size_t)game->room_index + 1U
                               : (size_t)game->room_index - 1U;
    next_room = fieldzero_content_room(next_index);
    if (next_room == NULL)
    {
        return AFORC_ERROR_STATE;
    }
    previous_sector = game->room->sector;
    status = fieldzero_game_enter_room(game, next_index, direction > 0);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (previous_sector != next_room->sector)
    {
        game->phase = FIELDZERO_PHASE_SECTOR_TRANSITION;
        game->phase_ticks = 0U;
        game->transition_direction = direction;
    }
    return AFORC_OK;
}

AFORC_Status fieldzero_game_tick_progression(FieldzeroGame *game)
{
    if (game == NULL || game->room == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    switch (game->phase)
    {
        case FIELDZERO_PHASE_ACTIVE:
            return game_tick_active_progression(game);
        case FIELDZERO_PHASE_REGISTERING:
            return fieldzero_game_tick_registration(game);
        case FIELDZERO_PHASE_DISSOLVING:
            game_freeze(game);
            ++game->phase_ticks;
            if (game->phase_ticks >= FIELDZERO_DISSOLVE_TICKS)
            {
                return fieldzero_game_restore_checkpoint(game);
            }
            return AFORC_OK;
        case FIELDZERO_PHASE_ROOM_TRANSITION:
            game_freeze(game);
            ++game->phase_ticks;
            if (game->phase_ticks >= FIELDZERO_ROOM_TRANSITION_TICKS)
            {
                return game_finish_room_transition(game);
            }
            return AFORC_OK;
        case FIELDZERO_PHASE_SECTOR_TRANSITION:
            game_freeze(game);
            ++game->phase_ticks;
            if (game->phase_ticks >= FIELDZERO_SECTOR_TRANSITION_TICKS)
            {
                game->phase = FIELDZERO_PHASE_ACTIVE;
                game->phase_ticks = 0U;
                game->transition_direction = 0;
            }
            return AFORC_OK;
        case FIELDZERO_PHASE_COMPLETE:
            return AFORC_OK;
        default:
            return AFORC_ERROR_STATE;
    }
}
