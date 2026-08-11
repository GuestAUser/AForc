#include "fieldzero/game.h"

#include <limits.h>
#include <stdint.h>

enum
{
    FIELDZERO_FIXED_HALF = FIELDZERO_FIXED_ONE / 2,
    FIELDZERO_CORNER_THRESHOLD = FIELDZERO_FIXED_ONE / 4,
    FIELDZERO_COLLISION_TICKS_PER_SECOND = FIELDZERO_FIXED_UPDATES_PER_SECOND,
    FIELDZERO_SWEEP_LIMIT = FIELDZERO_ARENA_WIDTH + FIELDZERO_ARENA_HEIGHT + 2
};

AFORC_Status fieldzero_collision_move(FieldzeroGame *game);

static bool fieldzero_tile_blocks(AFORC_Tile tile,
                                  uint32_t layer,
                                  AFORC_Point position,
                                  void *context)
{
    (void)layer;
    (void)position;
    (void)context;
    return tile != 0U;
}

static const AFORC_TileMap *fieldzero_collision_map(const FieldzeroGame *game)
{
    if (game->phase == FIELDZERO_PHASE_REGISTERING)
    {
        return game->static_map;
    }
    return game->active_map;
}

static AFORC_Status fieldzero_collision_cell_blocked(const FieldzeroGame *game,
                                                     int32_t x,
                                                     int32_t y,
                                                     bool *out_blocked)
{
    const AFORC_TileMap *map;
    AFORC_Status status;

    if (game == NULL || out_blocked == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (x < 0 || x >= FIELDZERO_ARENA_WIDTH || y < 0)
    {
        *out_blocked = true;
        return AFORC_OK;
    }
    if (y >= FIELDZERO_ARENA_HEIGHT)
    {
        *out_blocked = false;
        return AFORC_OK;
    }
    map = fieldzero_collision_map(game);
    if (map == NULL)
    {
        return AFORC_ERROR_STATE;
    }
    status = aforc_grid_point_blocked(
        map, 0U, (AFORC_Point){x, y}, fieldzero_tile_blocks, NULL, out_blocked);
    if (status != AFORC_OK)
    {
        return status;
    }
    return AFORC_OK;
}

bool fieldzero_game_cell_blocked(const FieldzeroGame *game,
                                 int32_t x,
                                 int32_t y)
{
    bool blocked = true;

    return fieldzero_collision_cell_blocked(game, x, y, &blocked) != AFORC_OK ||
           blocked;
}

static int32_t fieldzero_fixed_to_cell(int32_t value)
{
    const int64_t wide = value;

    if (wide >= 0)
    {
        return (int32_t)((wide + FIELDZERO_FIXED_HALF) / FIELDZERO_FIXED_ONE);
    }
    return (int32_t)-((-wide + FIELDZERO_FIXED_HALF) / FIELDZERO_FIXED_ONE);
}

static AFORC_Status
fieldzero_fixed_add(int32_t value, int32_t delta, int32_t *out_sum)
{
    const int64_t sum = (int64_t)value + delta;

    if (out_sum == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (sum < INT32_MIN || sum > INT32_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_sum = (int32_t)sum;
    return AFORC_OK;
}

static AFORC_Status fieldzero_cell_center(int32_t cell, int32_t *out_center)
{
    const int64_t center = (int64_t)cell * FIELDZERO_FIXED_ONE;

    if (out_center == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (center < INT32_MIN || center > INT32_MAX)
    {
        return AFORC_ERROR_OVERFLOW;
    }
    *out_center = (int32_t)center;
    return AFORC_OK;
}

static AFORC_Status fieldzero_collision_move_horizontal(FieldzeroGame *game)
{
    FieldzeroPlayer *player = &game->player;
    int32_t target;
    int32_t current_cell;
    int32_t target_cell;
    int32_t cell_y;
    int32_t direction;
    int32_t candidate;
    int32_t swept = 0;
    AFORC_Status status;

    status = fieldzero_fixed_add(player->x,
                                 player->velocity_x /
                                     FIELDZERO_COLLISION_TICKS_PER_SECOND,
                                 &target);
    if (status != AFORC_OK)
    {
        return status;
    }
    current_cell = fieldzero_fixed_to_cell(player->x);
    target_cell = fieldzero_fixed_to_cell(target);
    cell_y = fieldzero_fixed_to_cell(player->y);
    direction = target_cell > current_cell   ? 1
                : target_cell < current_cell ? -1
                                             : 0;
    candidate = current_cell + direction;
    while (direction != 0 && candidate != target_cell + direction)
    {
        bool blocked;

        if (swept >= FIELDZERO_SWEEP_LIMIT)
        {
            return AFORC_ERROR_LIMIT;
        }
        status =
            fieldzero_collision_cell_blocked(game, candidate, cell_y, &blocked);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (blocked)
        {
            status = fieldzero_cell_center(candidate - direction, &player->x);
            if (status != AFORC_OK)
            {
                return status;
            }
            player->velocity_x = 0;
            player->dash_ticks = 0U;
            return AFORC_OK;
        }
        candidate += direction;
        ++swept;
    }
    player->x = target;
    return AFORC_OK;
}

static AFORC_Status fieldzero_collision_correct_corner(FieldzeroGame *game,
                                                       int32_t blocked_y,
                                                       bool *out_corrected)
{
    FieldzeroPlayer *player = &game->player;
    const int32_t cell_x = fieldzero_fixed_to_cell(player->x);
    const int32_t cell_y = blocked_y + 1;
    int32_t center_x;
    int32_t offset;
    int32_t direction;
    bool blocked;
    AFORC_Status status;

    if (out_corrected == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_corrected = false;
    status = fieldzero_cell_center(cell_x, &center_x);
    if (status != AFORC_OK)
    {
        return status;
    }
    offset = player->x - center_x;
    direction = offset >= FIELDZERO_CORNER_THRESHOLD    ? 1
                : offset <= -FIELDZERO_CORNER_THRESHOLD ? -1
                                                        : 0;
    if (direction == 0)
    {
        return AFORC_OK;
    }
    status = fieldzero_collision_cell_blocked(
        game, cell_x + direction, cell_y, &blocked);
    if (status != AFORC_OK || blocked)
    {
        return status;
    }
    status = fieldzero_collision_cell_blocked(
        game, cell_x + direction, blocked_y, &blocked);
    if (status != AFORC_OK || blocked)
    {
        return status;
    }
    status = fieldzero_cell_center(cell_x + direction, &player->x);
    if (status == AFORC_OK)
    {
        *out_corrected = true;
    }
    return status;
}

static AFORC_Status fieldzero_collision_move_vertical(FieldzeroGame *game)
{
    FieldzeroPlayer *player = &game->player;
    int32_t target;
    int32_t current_cell;
    int32_t target_cell;
    int32_t direction;
    int32_t candidate;
    int32_t swept = 0;
    bool corrected_corner = false;
    AFORC_Status status;

    current_cell = fieldzero_fixed_to_cell(player->y);
    if (player->velocity_y >= 0)
    {
        const int32_t cell_x = fieldzero_fixed_to_cell(player->x);
        int32_t center_y;
        bool supported;

        status = fieldzero_cell_center(current_cell, &center_y);
        if (status != AFORC_OK)
        {
            return status;
        }
        status = fieldzero_collision_cell_blocked(
            game, cell_x, current_cell + 1, &supported);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (supported && (player->velocity_y > 0 || player->y == center_y))
        {
            status = fieldzero_cell_center(current_cell, &player->y);
            if (status != AFORC_OK)
            {
                return status;
            }
            player->velocity_y = 0;
            player->grounded = true;
            return AFORC_OK;
        }
    }

    status = fieldzero_fixed_add(player->y,
                                 player->velocity_y /
                                     FIELDZERO_COLLISION_TICKS_PER_SECOND,
                                 &target);
    if (status != AFORC_OK)
    {
        return status;
    }
    target_cell = fieldzero_fixed_to_cell(target);
    direction = target_cell > current_cell   ? 1
                : target_cell < current_cell ? -1
                                             : 0;
    candidate = current_cell + direction;
    while (direction != 0 && candidate != target_cell + direction)
    {
        const int32_t cell_x = fieldzero_fixed_to_cell(player->x);
        bool blocked;

        if (swept >= FIELDZERO_SWEEP_LIMIT)
        {
            return AFORC_ERROR_LIMIT;
        }
        status =
            fieldzero_collision_cell_blocked(game, cell_x, candidate, &blocked);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (blocked && direction < 0 && !corrected_corner)
        {
            status = fieldzero_collision_correct_corner(
                game, candidate, &corrected_corner);
            if (status != AFORC_OK)
            {
                return status;
            }
            blocked = !corrected_corner;
        }
        if (blocked)
        {
            status = fieldzero_cell_center(candidate - direction, &player->y);
            if (status != AFORC_OK)
            {
                return status;
            }
            player->velocity_y = 0;
            player->grounded = direction > 0;
            return AFORC_OK;
        }
        candidate += direction;
        ++swept;
    }
    player->y = target;
    if (player->velocity_y > 0)
    {
        const int32_t cell_x = fieldzero_fixed_to_cell(player->x);
        const int32_t cell_y = fieldzero_fixed_to_cell(player->y);
        bool supported;

        status = fieldzero_collision_cell_blocked(
            game, cell_x, cell_y + 1, &supported);
        if (status != AFORC_OK)
        {
            return status;
        }
        if (supported)
        {
            status = fieldzero_cell_center(cell_y, &player->y);
            if (status != AFORC_OK)
            {
                return status;
            }
            player->velocity_y = 0;
            player->grounded = true;
        }
    }
    return AFORC_OK;
}

static AFORC_Status fieldzero_collision_refresh_contacts(FieldzeroGame *game)
{
    FieldzeroPlayer *player = &game->player;
    const int32_t cell_x = fieldzero_fixed_to_cell(player->x);
    const int32_t cell_y = fieldzero_fixed_to_cell(player->y);
    bool blocked_below;
    bool blocked_left;
    bool blocked_right;
    int32_t center_y;
    AFORC_Status status;

    status = fieldzero_collision_cell_blocked(
        game, cell_x, cell_y + 1, &blocked_below);
    if (status == AFORC_OK)
    {
        status = fieldzero_collision_cell_blocked(
            game, cell_x - 1, cell_y, &blocked_left);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_collision_cell_blocked(
            game, cell_x + 1, cell_y, &blocked_right);
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    status = fieldzero_cell_center(cell_y, &center_y);
    if (status != AFORC_OK)
    {
        return status;
    }
    player->grounded =
        blocked_below && player->velocity_y == 0 && player->y == center_y;
    player->wall_left = blocked_left;
    player->wall_right = blocked_right;
    return AFORC_OK;
}

AFORC_Status fieldzero_collision_move(FieldzeroGame *game)
{
    AFORC_Status status;

    if (game == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = fieldzero_collision_move_horizontal(game);
    if (status == AFORC_OK)
    {
        status = fieldzero_collision_move_vertical(game);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_collision_refresh_contacts(game);
    }
    return status;
}
