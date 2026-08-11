#include "fieldzero/game.h"

#include <string.h>

enum
{
    GAME_TILE_EMPTY = 0,
    GAME_TILE_SOLID = 1
};

static AFORC_Status game_fill_geometry(AFORC_TileMap *map,
                                       const FieldzeroRoomDefinition *room,
                                       uint8_t state,
                                       bool include_bands)
{
    AFORC_Status status;
    size_t index;

    if (map == NULL || room == NULL || state >= room->state_count)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_tilemap_fill_layer(map, 0U, GAME_TILE_EMPTY);
    for (index = 0U; status == AFORC_OK && index < room->static_rectangle_count;
         ++index)
    {
        status = aforc_tilemap_fill_rect(
            map, 0U, room->static_rectangles[index], GAME_TILE_SOLID);
    }
    if (!include_bands)
    {
        return status;
    }
    for (index = 0U; status == AFORC_OK && index < room->band_count; ++index)
    {
        const FieldzeroBand *band = &room->bands[index];
        size_t rectangle_index;

        for (rectangle_index = 0U;
             status == AFORC_OK && rectangle_index < band->rectangle_count;
             ++rectangle_index)
        {
            AFORC_Rect translated;

            status =
                aforc_world_rect_translate(band->rectangles[rectangle_index],
                                           band->offsets[state],
                                           &translated);
            if (status == AFORC_OK)
            {
                status = aforc_tilemap_fill_rect(
                    map, 0U, translated, GAME_TILE_SOLID);
            }
        }
    }
    return status;
}

static void
game_reset_player(FieldzeroGame *game, AFORC_Point point, int8_t facing)
{
    (void)memset(&game->player, 0, sizeof(game->player));
    game->player.x = point.x * FIELDZERO_FIXED_ONE;
    game->player.y = point.y * FIELDZERO_FIXED_ONE;
    game->player.facing = facing;
    game->player.grounded =
        fieldzero_game_cell_blocked(game, point.x, point.y + 1);
    game->player.dash_available = true;
}

static AFORC_Point game_reverse_spawn(const FieldzeroRoomDefinition *room)
{
    const AFORC_Point inward = {room->spawn.x - room->entry.x,
                                room->spawn.y - room->entry.y};

    return (AFORC_Point){room->exit.x - inward.x, room->exit.y - inward.y};
}

static bool game_map_cell_blocked(const AFORC_TileMap *map, AFORC_Point point)
{
    AFORC_Tile tile = GAME_TILE_SOLID;

    return map == NULL ||
           aforc_tilemap_get(map, 0U, point, &tile) != AFORC_OK ||
           tile != GAME_TILE_EMPTY;
}

static bool game_release_cell_valid(const AFORC_TileMap *map, AFORC_Point point)
{
    const AFORC_Point below = {point.x, point.y + 1};

    return map != NULL && aforc_tilemap_contains(map, point) &&
           aforc_tilemap_contains(map, below) &&
           !game_map_cell_blocked(map, point) &&
           game_map_cell_blocked(map, below);
}

AFORC_Status fieldzero_game_init(FieldzeroGame *game, uint64_t seed)
{
    const AFORC_Size size = {FIELDZERO_ARENA_WIDTH, FIELDZERO_ARENA_HEIGHT};
    AFORC_Allocator allocator;
    AFORC_Status status;

    if (game == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(game, 0, sizeof(*game));
    game->seed = seed;
    if (!fieldzero_content_validate_all())
    {
        return AFORC_ERROR_STATE;
    }
    allocator = aforc_allocator_default();
    status = aforc_tilemap_create(
        size, 1U, GAME_TILE_EMPTY, &allocator, &game->static_map);
    if (status == AFORC_OK)
    {
        status = aforc_tilemap_create(
            size, 1U, GAME_TILE_EMPTY, &allocator, &game->active_map);
    }
    if (status == AFORC_OK)
    {
        status = aforc_tilemap_create(
            size, 1U, GAME_TILE_EMPTY, &allocator, &game->staging_map);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_game_restart_run(game);
    }
    if (status != AFORC_OK)
    {
        fieldzero_game_dispose(game);
    }
    return status;
}

void fieldzero_game_dispose(FieldzeroGame *game)
{
    if (game == NULL)
    {
        return;
    }
    aforc_tilemap_destroy(game->staging_map);
    aforc_tilemap_destroy(game->active_map);
    aforc_tilemap_destroy(game->static_map);
    (void)memset(game, 0, sizeof(*game));
}

AFORC_Status fieldzero_game_restart_run(FieldzeroGame *game)
{
    if (game == NULL || game->static_map == NULL || game->active_map == NULL ||
        game->staging_map == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    game->falls = 0U;
    game->completed_rooms = 0U;
    game->collected_memories = 0U;
    (void)memset(game->room_states, 0, sizeof(game->room_states));
    return fieldzero_game_enter_room(game, 0U, true);
}

AFORC_Status fieldzero_game_enter_room(FieldzeroGame *game,
                                       size_t room_index,
                                       bool from_left)
{
    const FieldzeroRoomDefinition *room;
    AFORC_Point checkpoint;
    AFORC_Status status;
    uint16_t room_bit;

    if (game == NULL || game->static_map == NULL || game->active_map == NULL ||
        game->staging_map == NULL || room_index >= FIELDZERO_ROOM_COUNT)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    room = fieldzero_content_room(room_index);
    if (room == NULL || room->state_count == 0U ||
        game->room_states[room_index] >= room->state_count)
    {
        return AFORC_ERROR_STATE;
    }
    game->room = room;
    game->room_index = (uint8_t)room_index;
    game->room_state = game->room_states[room_index];
    game->registration_target_state = game->room_state;
    status = fieldzero_game_rebuild_maps(game);
    if (status != AFORC_OK)
    {
        return status;
    }
    checkpoint = from_left ? room->spawn : game_reverse_spawn(room);
    game->checkpoint.x = checkpoint.x;
    game->checkpoint.y = checkpoint.y;
    game->checkpoint.room_state = game->room_state;
    game_reset_player(game, checkpoint, from_left ? 1 : -1);
    game->phase = FIELDZERO_PHASE_ACTIVE;
    game->phase_ticks = 0U;
    game->transition_direction = 0;
    room_bit = (uint16_t)(UINT16_C(1) << game->room_index);
    game->memory_collected_here = (game->collected_memories & room_bit) != 0U;
    (void)memset(&game->actions, 0, sizeof(game->actions));
    return AFORC_OK;
}

AFORC_Status fieldzero_game_restart_room(FieldzeroGame *game)
{
    if (game == NULL || game->room == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return fieldzero_game_enter_room(game, game->room_index, true);
}

AFORC_Status fieldzero_game_rebuild_maps(FieldzeroGame *game)
{
    AFORC_Status status;

    if (game == NULL || game->room == NULL || game->static_map == NULL ||
        game->active_map == NULL || game->staging_map == NULL ||
        game->room_state >= game->room->state_count)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = game_fill_geometry(
        game->static_map, game->room, game->room_state, false);
    if (status == AFORC_OK)
    {
        status = game_fill_geometry(
            game->active_map, game->room, game->room_state, true);
    }
    if (status == AFORC_OK)
    {
        status = game_fill_geometry(
            game->staging_map, game->room, game->room_state, true);
    }
    return status;
}

AFORC_Status fieldzero_game_begin_registration(FieldzeroGame *game)
{
    AFORC_Point anchor;
    AFORC_Status status;
    uint8_t target_state;

    if (game == NULL || game->room == NULL || game->staging_map == NULL ||
        game->phase != FIELDZERO_PHASE_ACTIVE ||
        game->room->state_count == 0U ||
        game->room_state + 1U >= game->room->state_count ||
        game->room_state >= fieldzero_room_mark_count(game->room))
    {
        return AFORC_ERROR_STATE;
    }
    target_state = (uint8_t)(game->room_state + 1U);
    status =
        game_fill_geometry(game->staging_map, game->room, target_state, true);
    if (status != AFORC_OK)
    {
        return status;
    }
    anchor = game->room->marks[game->room_state];
    if (!game_release_cell_valid(game->staging_map, anchor))
    {
        return AFORC_ERROR_STATE;
    }
    game_reset_player(
        game, anchor, game->player.facing == 0 ? 1 : game->player.facing);
    game->registration_target_state = target_state;
    game->phase = FIELDZERO_PHASE_REGISTERING;
    game->phase_ticks = 0U;
    game->transition_direction = 0;
    (void)memset(&game->actions, 0, sizeof(game->actions));
    return AFORC_OK;
}

AFORC_Status fieldzero_game_tick_registration(FieldzeroGame *game)
{
    AFORC_Point release;
    AFORC_TileMap *previous_map;

    if (game == NULL || game->room == NULL || game->staging_map == NULL ||
        game->active_map == NULL ||
        game->phase != FIELDZERO_PHASE_REGISTERING ||
        game->registration_target_state >= game->room->state_count)
    {
        return AFORC_ERROR_STATE;
    }
    game->player.velocity_x = 0;
    game->player.velocity_y = 0;
    (void)memset(&game->actions, 0, sizeof(game->actions));
    ++game->phase_ticks;
    if (game->phase_ticks < FIELDZERO_REGISTRATION_TICKS)
    {
        return AFORC_OK;
    }
    release = (AFORC_Point){game->player.x / FIELDZERO_FIXED_ONE,
                            game->player.y / FIELDZERO_FIXED_ONE};
    if (!game_release_cell_valid(game->staging_map, release))
    {
        return AFORC_ERROR_STATE;
    }
    previous_map = game->active_map;
    game->active_map = game->staging_map;
    game->staging_map = previous_map;
    game->room_state = game->registration_target_state;
    game->room_states[game->room_index] = game->room_state;
    game->checkpoint.x = release.x;
    game->checkpoint.y = release.y;
    game->checkpoint.room_state = game->room_state;
    game->phase = FIELDZERO_PHASE_ACTIVE;
    game->phase_ticks = 0U;
    game->player.grounded =
        fieldzero_game_cell_blocked(game, release.x, release.y + 1);
    return AFORC_OK;
}

AFORC_Status fieldzero_game_restore_checkpoint(FieldzeroGame *game)
{
    AFORC_Point point;
    AFORC_Status status;

    if (game == NULL || game->room == NULL ||
        game->checkpoint.room_state >= game->room->state_count)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    point = (AFORC_Point){game->checkpoint.x, game->checkpoint.y};
    game->room_state = game->checkpoint.room_state;
    game->room_states[game->room_index] = game->room_state;
    game->registration_target_state = game->room_state;
    status = fieldzero_game_rebuild_maps(game);
    if (status != AFORC_OK)
    {
        return status;
    }
    game_reset_player(
        game, point, game->player.facing == 0 ? 1 : game->player.facing);
    game->phase = FIELDZERO_PHASE_ACTIVE;
    game->phase_ticks = 0U;
    game->transition_direction = 0;
    (void)memset(&game->actions, 0, sizeof(game->actions));
    return AFORC_OK;
}
