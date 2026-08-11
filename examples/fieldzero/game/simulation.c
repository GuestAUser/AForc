#include "fieldzero/game.h"

#include <string.h>

enum
{
    FIELDZERO_TICKS_PER_SECOND = FIELDZERO_FIXED_UPDATES_PER_SECOND,
    FIELDZERO_MAX_RUN_SPEED = 16 * FIELDZERO_FIXED_ONE,
    FIELDZERO_GROUND_ACCELERATION =
        (70 * FIELDZERO_FIXED_ONE + FIELDZERO_TICKS_PER_SECOND / 2) /
        FIELDZERO_TICKS_PER_SECOND,
    FIELDZERO_AIR_ACCELERATION =
        (40 * FIELDZERO_FIXED_ONE + FIELDZERO_TICKS_PER_SECOND / 2) /
        FIELDZERO_TICKS_PER_SECOND,
    FIELDZERO_GROUND_DECELERATION =
        (90 * FIELDZERO_FIXED_ONE + FIELDZERO_TICKS_PER_SECOND / 2) /
        FIELDZERO_TICKS_PER_SECOND,
    FIELDZERO_JUMP_SPEED = 18 * FIELDZERO_FIXED_ONE,
    FIELDZERO_GRAVITY =
        (50 * FIELDZERO_FIXED_ONE + FIELDZERO_TICKS_PER_SECOND / 2) /
        FIELDZERO_TICKS_PER_SECOND,
    FIELDZERO_MAX_FALL_SPEED = 24 * FIELDZERO_FIXED_ONE,
    FIELDZERO_WALL_SLIDE_SPEED = 6 * FIELDZERO_FIXED_ONE,
    FIELDZERO_WALL_JUMP_SPEED_X = 12 * FIELDZERO_FIXED_ONE,
    FIELDZERO_WALL_JUMP_SPEED_Y = 17 * FIELDZERO_FIXED_ONE,
    FIELDZERO_DASH_SPEED = 24 * FIELDZERO_FIXED_ONE,
    FIELDZERO_COYOTE_TICKS = (120 * FIELDZERO_TICKS_PER_SECOND + 500) / 1000,
    FIELDZERO_JUMP_BUFFER_TICKS =
        (120 * FIELDZERO_TICKS_PER_SECOND + 500) / 1000,
    FIELDZERO_WALL_LOCK_TICKS = (80 * FIELDZERO_TICKS_PER_SECOND + 500) / 1000,
    FIELDZERO_DASH_DURATION_TICKS =
        (120 * FIELDZERO_TICKS_PER_SECOND + 500) / 1000
};

static int fieldzero_movement_intent(const FieldzeroActions *actions)
{
    return (actions->right ? 1 : 0) - (actions->left ? 1 : 0);
}

static int32_t fieldzero_approach(int32_t value, int32_t target, int32_t amount)
{
    if (value < target)
    {
        const int32_t remaining = target - value;

        return value + (remaining < amount ? remaining : amount);
    }
    if (value > target)
    {
        const int32_t remaining = value - target;

        return value - (remaining < amount ? remaining : amount);
    }
    return value;
}

static int fieldzero_wall_jump_direction(const FieldzeroPlayer *player,
                                         int intent)
{
    if (player->wall_left && !player->wall_right)
    {
        return 1;
    }
    if (player->wall_right && !player->wall_left)
    {
        return -1;
    }
    if (player->wall_left && player->wall_right)
    {
        if (intent != 0)
        {
            return -intent;
        }
        return player->facing < 0 ? 1 : -1;
    }
    return 0;
}

static bool fieldzero_try_jump(FieldzeroGame *game, int intent)
{
    FieldzeroPlayer *player = &game->player;
    int direction;

    if (player->jump_buffer_ticks == 0U)
    {
        return false;
    }
    if (player->grounded || player->coyote_ticks > 0U)
    {
        player->velocity_y = -FIELDZERO_JUMP_SPEED;
        player->grounded = false;
        player->coyote_ticks = 0U;
        player->jump_buffer_ticks = 0U;
        return true;
    }
    if (player->wall_lock_ticks > 0U)
    {
        return false;
    }
    direction = fieldzero_wall_jump_direction(player, intent);
    if (direction == 0)
    {
        return false;
    }
    player->velocity_x = direction * FIELDZERO_WALL_JUMP_SPEED_X;
    player->velocity_y = -FIELDZERO_WALL_JUMP_SPEED_Y;
    player->facing = (int8_t)direction;
    player->wall_lock_ticks = FIELDZERO_WALL_LOCK_TICKS;
    player->coyote_ticks = 0U;
    player->jump_buffer_ticks = 0U;
    player->grounded = false;
    return true;
}

static void fieldzero_apply_horizontal_control(FieldzeroPlayer *player,
                                               int intent)
{
    int32_t target;
    int32_t acceleration;

    if (player->wall_lock_ticks > 0U)
    {
        return;
    }
    if (intent == 0)
    {
        if (player->grounded)
        {
            player->velocity_x = fieldzero_approach(
                player->velocity_x, 0, FIELDZERO_GROUND_DECELERATION);
        }
        return;
    }
    target = intent * FIELDZERO_MAX_RUN_SPEED;
    acceleration = player->grounded ? FIELDZERO_GROUND_ACCELERATION
                                    : FIELDZERO_AIR_ACCELERATION;
    player->velocity_x =
        fieldzero_approach(player->velocity_x, target, acceleration);
}

static bool fieldzero_begin_dash(FieldzeroGame *game, int intent)
{
    FieldzeroPlayer *player = &game->player;
    int direction;

    if (!game->actions.dash_pressed || !player->dash_available ||
        player->grounded || player->dash_ticks > 0U)
    {
        return false;
    }
    direction = intent != 0 ? intent : player->facing;
    if (direction == 0)
    {
        direction = 1;
    }
    player->facing = (int8_t)direction;
    player->velocity_x = direction * FIELDZERO_DASH_SPEED;
    player->velocity_y = 0;
    player->dash_ticks = FIELDZERO_DASH_DURATION_TICKS;
    player->dash_available = false;
    player->coyote_ticks = 0U;
    return true;
}

static void fieldzero_apply_gravity(FieldzeroPlayer *player)
{
    player->velocity_y = fieldzero_approach(
        player->velocity_y, FIELDZERO_MAX_FALL_SPEED, FIELDZERO_GRAVITY);
    if ((player->wall_left || player->wall_right) &&
        player->velocity_y > FIELDZERO_WALL_SLIDE_SPEED)
    {
        player->velocity_y = FIELDZERO_WALL_SLIDE_SPEED;
    }
}

static void fieldzero_finish_dash_tick(FieldzeroPlayer *player)
{
    if (player->dash_ticks == 0U)
    {
        return;
    }
    --player->dash_ticks;
    if (player->dash_ticks == 0U)
    {
        player->velocity_x =
            fieldzero_approach(player->velocity_x,
                               0,
                               FIELDZERO_DASH_SPEED - FIELDZERO_MAX_RUN_SPEED);
    }
}

static void fieldzero_clear_transient_actions(FieldzeroActions *actions)
{
    actions->jump_pressed = false;
    actions->jump_released = false;
    actions->dash_pressed = false;
}

static AFORC_Status fieldzero_tick_active(FieldzeroGame *game)
{
    FieldzeroPlayer *player = &game->player;
    const int intent = fieldzero_movement_intent(&game->actions);
    bool jumped = false;
    bool post_move_jump = false;
    bool dashing;
    AFORC_Status status;

    if (game->actions.jump_pressed)
    {
        player->jump_buffer_ticks = FIELDZERO_JUMP_BUFFER_TICKS;
    }
    if (player->grounded)
    {
        player->coyote_ticks = FIELDZERO_COYOTE_TICKS;
    }
    if (player->dash_ticks == 0U && player->wall_lock_ticks == 0U &&
        intent != 0)
    {
        player->facing = (int8_t)intent;
    }
    if (player->dash_ticks == 0U)
    {
        jumped = fieldzero_try_jump(game, intent);
        if (!jumped)
        {
            (void)fieldzero_begin_dash(game, intent);
        }
    }
    dashing = player->dash_ticks > 0U;
    if (dashing)
    {
        player->velocity_x = player->facing * FIELDZERO_DASH_SPEED;
        player->velocity_y = 0;
    }
    else
    {
        fieldzero_apply_horizontal_control(player, intent);
        if (game->actions.jump_released && player->velocity_y < 0)
        {
            player->velocity_y /= 2;
        }
        fieldzero_apply_gravity(player);
    }
    status = fieldzero_collision_move(game);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (dashing)
    {
        fieldzero_finish_dash_tick(player);
    }
    if (player->grounded)
    {
        player->dash_available = true;
        player->dash_ticks = 0U;
        player->wall_lock_ticks = 0U;
        player->coyote_ticks = FIELDZERO_COYOTE_TICKS;
    }
    else
    {
        if ((player->wall_left || player->wall_right) &&
            player->dash_ticks == 0U &&
            player->velocity_y > FIELDZERO_WALL_SLIDE_SPEED)
        {
            player->velocity_y = FIELDZERO_WALL_SLIDE_SPEED;
        }
    }
    if (!jumped && player->dash_ticks == 0U)
    {
        post_move_jump = fieldzero_try_jump(game, intent);
        jumped = post_move_jump;
    }
    if (post_move_jump && game->actions.jump_released && player->velocity_y < 0)
    {
        player->velocity_y /= 2;
    }
    if (!player->grounded && !jumped && player->coyote_ticks > 0U)
    {
        --player->coyote_ticks;
    }
    if (player->jump_buffer_ticks > 0U)
    {
        --player->jump_buffer_ticks;
    }
    if (player->wall_lock_ticks > 0U)
    {
        --player->wall_lock_ticks;
    }
    return AFORC_OK;
}

void fieldzero_game_clear_actions(FieldzeroGame *game)
{
    if (game != NULL)
    {
        (void)memset(&game->actions, 0, sizeof(game->actions));
    }
}

void fieldzero_game_set_move(FieldzeroGame *game, int direction, bool held)
{
    if (game == NULL)
    {
        return;
    }
    if (direction < 0)
    {
        game->actions.left = held;
    }
    else if (direction > 0)
    {
        game->actions.right = held;
    }
    else if (!held)
    {
        game->actions.left = false;
        game->actions.right = false;
    }
}

void fieldzero_game_press_jump(FieldzeroGame *game)
{
    if (game == NULL)
    {
        return;
    }
    if (!game->actions.jump_held)
    {
        game->actions.jump_pressed = true;
    }
    game->actions.jump_held = true;
}

void fieldzero_game_release_jump(FieldzeroGame *game)
{
    if (game == NULL)
    {
        return;
    }
    if (game->actions.jump_held)
    {
        game->actions.jump_released = true;
    }
    game->actions.jump_held = false;
}

void fieldzero_game_press_dash(FieldzeroGame *game)
{
    if (game != NULL)
    {
        game->actions.dash_pressed = true;
    }
}

AFORC_Status fieldzero_game_tick(FieldzeroGame *game)
{
    AFORC_Status status;

    if (game == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (game->phase == FIELDZERO_PHASE_REGISTERING)
    {
        fieldzero_game_clear_actions(game);
        return fieldzero_game_tick_registration(game);
    }
    if (game->phase != FIELDZERO_PHASE_ACTIVE)
    {
        fieldzero_game_clear_actions(game);
        return fieldzero_game_tick_progression(game);
    }
    status = fieldzero_tick_active(game);
    if (status == AFORC_OK)
    {
        status = fieldzero_game_tick_progression(game);
    }
    fieldzero_clear_transient_actions(&game->actions);
    return status;
}
