#include "fieldzero/qa.h"

#include <stdio.h>
#include <string.h>

enum
{
    FIELDZERO_REGRESSION_DASH_TICKS = 7
};

enum
{
    FIELDZERO_REGRESSION_ROUTE_LEFT = 1U << 0,
    FIELDZERO_REGRESSION_ROUTE_RIGHT = 1U << 1,
    FIELDZERO_REGRESSION_ROUTE_JUMP = 1U << 2,
    FIELDZERO_REGRESSION_ROUTE_DASH = 1U << 3,
    FIELDZERO_REGRESSION_MAX_ROUTE_RUNS = 43
};

typedef struct FieldzeroRegressionRouteRun
{
    uint8_t input;
    uint8_t ticks;
} FieldzeroRegressionRouteRun;

static bool fieldzero_regression_fill_all(FieldzeroGame *game, AFORC_Tile tile)
{
    return aforc_tilemap_fill_layer(game->static_map, 0U, tile) == AFORC_OK &&
           aforc_tilemap_fill_layer(game->active_map, 0U, tile) == AFORC_OK &&
           aforc_tilemap_fill_layer(game->staging_map, 0U, tile) == AFORC_OK;
}

static bool fieldzero_regression_rect_all(FieldzeroGame *game,
                                          AFORC_Rect rectangle,
                                          AFORC_Tile tile)
{
    return aforc_tilemap_fill_rect(game->static_map, 0U, rectangle, tile) ==
               AFORC_OK &&
           aforc_tilemap_fill_rect(game->active_map, 0U, rectangle, tile) ==
               AFORC_OK &&
           aforc_tilemap_fill_rect(game->staging_map, 0U, rectangle, tile) ==
               AFORC_OK;
}

static bool fieldzero_regression_flat_world(FieldzeroGame *game,
                                            int32_t floor_y)
{
    if (!fieldzero_regression_fill_all(game, 0U))
    {
        return false;
    }
    if (floor_y >= 0 && floor_y < FIELDZERO_ARENA_HEIGHT &&
        !fieldzero_regression_rect_all(
            game, (AFORC_Rect){0, floor_y, FIELDZERO_ARENA_WIDTH, 1}, 1U))
    {
        return false;
    }
    game->phase = FIELDZERO_PHASE_ACTIVE;
    game->phase_ticks = 0U;
    game->transition_direction = 0;
    fieldzero_game_clear_actions(game);
    return true;
}

static void fieldzero_regression_place_player(FieldzeroGame *game,
                                              int32_t x,
                                              int32_t y,
                                              bool grounded)
{
    (void)memset(&game->player, 0, sizeof(game->player));
    game->player.x = x * FIELDZERO_FIXED_ONE;
    game->player.y = y * FIELDZERO_FIXED_ONE;
    game->player.facing = 1;
    game->player.grounded = grounded;
    game->player.dash_available = true;
    fieldzero_game_clear_actions(game);
}

static bool fieldzero_regression_tick_many(FieldzeroGame *game, size_t count)
{
    for (size_t tick = 0U; tick < count; ++tick)
    {
        if (fieldzero_game_tick(game) != AFORC_OK)
        {
            return false;
        }
    }
    return true;
}

static bool
fieldzero_regression_run_route(FieldzeroGame *game,
                               const FieldzeroRegressionRouteRun
                                   route[FIELDZERO_REGRESSION_MAX_ROUTE_RUNS],
                               uint8_t *out_registrations)
{
    uint8_t previous_input = 0U;

    *out_registrations = 0U;
    for (size_t run = 0U; run < FIELDZERO_REGRESSION_MAX_ROUTE_RUNS; ++run)
    {
        if (route[run].ticks == 0U)
        {
            return true;
        }
        for (uint8_t tick = 0U; tick < route[run].ticks; ++tick)
        {
            const FieldzeroGamePhase phase = game->phase;

            fieldzero_game_set_move(game, 0, false);
            if ((route[run].input & FIELDZERO_REGRESSION_ROUTE_LEFT) != 0U)
            {
                fieldzero_game_set_move(game, -1, true);
            }
            else if ((route[run].input & FIELDZERO_REGRESSION_ROUTE_RIGHT) !=
                     0U)
            {
                fieldzero_game_set_move(game, 1, true);
            }
            if ((route[run].input & FIELDZERO_REGRESSION_ROUTE_JUMP) != 0U)
            {
                fieldzero_game_press_jump(game);
            }
            else
            {
                fieldzero_game_release_jump(game);
            }
            if ((route[run].input & FIELDZERO_REGRESSION_ROUTE_DASH) != 0U &&
                (previous_input & FIELDZERO_REGRESSION_ROUTE_DASH) == 0U)
            {
                fieldzero_game_press_dash(game);
            }
            if (fieldzero_game_tick(game) != AFORC_OK)
            {
                return false;
            }
            if (phase == FIELDZERO_PHASE_ACTIVE &&
                game->phase == FIELDZERO_PHASE_REGISTERING)
            {
                ++*out_registrations;
            }
            previous_input = route[run].input;
        }
    }
    return false;
}

static const FieldzeroRegressionRouteRun fieldzero_completion_routes
    [FIELDZERO_ROOM_COUNT][FIELDZERO_REGRESSION_MAX_ROUTE_RUNS] = {
        {{14U, 45U}, {5U, 9U},  {0U, 2U},  {6U, 29U},  {10U, 7U}, {5U, 1U},
         {10U, 10U}, {0U, 28U}, {6U, 4U},  {10U, 27U}, {6U, 2U},  {8U, 10U},
         {0U, 1U},   {4U, 30U}, {14U, 8U}, {0U, 1U},   {4U, 30U}, {14U, 12U},
         {0U, 2U},   {13U, 2U}, {8U, 22U}, {0U, 1U}},
        {{6U, 8U},
         {14U, 26U},
         {0U, 1U},
         {6U, 33U},
         {0U, 1U},
         {6U, 37U},
         {0U, 28U},
         {6U, 4U},
         {14U, 27U},
         {2U, 8U},
         {14U, 30U},
         {0U, 1U},
         {6U, 10U},
         {2U, 7U},
         {10U, 16U},
         {0U, 2U},
         {6U, 10U},
         {2U, 12U},
         {0U, 5U}},
        {{6U, 32U}, {0U, 1U},  {12U, 9U},  {6U, 4U},   {14U, 20U},
         {0U, 28U}, {6U, 5U},  {14U, 16U}, {0U, 1U},   {14U, 10U},
         {8U, 31U}, {0U, 2U},  {4U, 30U},  {14U, 19U}, {0U, 2U},
         {6U, 30U}, {8U, 25U}, {12U, 3U},  {9U, 19U},  {1U, 1U}},
        {{0U, 1U}, {14U, 32U}, {0U, 1U}, {12U, 11U}, {8U, 1U}, {6U, 1U},
         {0U, 5U}, {14U, 12U}, {1U, 1U}, {0U, 27U},  {6U, 4U}, {10U, 35U},
         {0U, 1U}, {12U, 11U}, {9U, 1U}, {12U, 1U},  {0U, 6U}, {10U, 16U},
         {0U, 2U}, {6U, 30U},  {8U, 8U}, {0U, 28U},  {6U, 5U}, {14U, 27U},
         {0U, 1U}, {6U, 29U},  {1U, 1U}},
        {{6U, 4U},
         {10U, 20U},
         {6U, 34U},
         {0U, 1U},
         {4U, 30U},
         {14U, 13U},
         {0U, 28U},
         {6U, 4U},
         {14U, 24U},
         {0U, 1U},
         {14U, 10U},
         {0U, 9U},
         {10U, 30U},
         {0U, 1U},
         {5U, 19U},
         {10U, 23U},
         {0U, 2U},
         {5U, 3U},
         {0U, 20U}},
        {{14U, 30U}, {9U, 2U},   {4U, 1U},  {10U, 7U}, {6U, 1U},   {13U, 2U},
         {0U, 1U},   {12U, 11U}, {5U, 4U},  {9U, 7U},  {0U, 10U},  {6U, 37U},
         {0U, 1U},   {6U, 44U},  {2U, 1U},  {6U, 18U}, {10U, 12U}, {0U, 28U},
         {6U, 6U},   {8U, 36U},  {0U, 1U},  {6U, 5U},  {14U, 34U}, {1U, 1U},
         {6U, 10U},  {8U, 8U},   {9U, 21U}, {4U, 1U},  {10U, 18U}, {1U, 13U},
         {0U, 1U}},
        {{6U, 14U},  {14U, 42U}, {0U, 1U},  {5U, 1U},   {12U, 14U}, {6U, 9U},
         {8U, 26U},  {2U, 42U},  {0U, 31U}, {2U, 19U},  {14U, 34U}, {0U, 1U},
         {6U, 33U},  {0U, 28U},  {10U, 7U}, {14U, 33U}, {12U, 3U},  {2U, 1U},
         {14U, 15U}, {9U, 15U},  {2U, 13U}, {0U, 1U}},
        {{6U, 5U},   {8U, 20U}, {14U, 14U}, {9U, 6U},   {12U, 3U}, {1U, 19U},
         {0U, 1U},   {6U, 1U},  {10U, 10U}, {13U, 11U}, {2U, 26U}, {0U, 5U},
         {13U, 2U},  {8U, 7U},  {13U, 12U}, {0U, 28U},  {10U, 6U}, {4U, 8U},
         {14U, 15U}, {0U, 1U},  {4U, 30U},  {8U, 10U},  {2U, 8U},  {6U, 41U},
         {0U, 1U},   {12U, 8U}, {9U, 21U},  {0U, 1U}},
        {{6U, 5U},   {8U, 20U}, {14U, 12U}, {9U, 9U},   {0U, 1U}, {6U, 30U},
         {14U, 13U}, {0U, 29U}, {6U, 30U},  {12U, 19U}, {1U, 2U}, {5U, 3U},
         {2U, 19U},  {0U, 1U},  {6U, 30U},  {8U, 7U},   {0U, 1U}, {6U, 38U},
         {0U, 35U},  {9U, 19U}, {6U, 6U},   {14U, 12U}, {0U, 9U}, {9U, 6U},
         {6U, 12U},  {1U, 8U},  {6U, 2U},   {14U, 23U}, {0U, 1U}},
        {{0U, 2U},  {2U, 53U}, {0U, 2U},   {13U, 3U}, {0U, 19U},  {1U, 13U},
         {0U, 2U},  {6U, 10U}, {10U, 35U}, {0U, 28U}, {14U, 39U}, {0U, 2U},
         {6U, 40U}, {0U, 28U}, {6U, 39U},  {0U, 1U},  {14U, 18U}, {0U, 4U},
         {13U, 2U}, {9U, 19U}, {0U, 1U}},
        {{14U, 30U}, {9U, 2U},   {4U, 1U},  {10U, 7U},  {6U, 1U},   {13U, 2U},
         {0U, 1U},   {12U, 11U}, {5U, 4U},  {9U, 7U},   {0U, 10U},  {6U, 37U},
         {0U, 1U},   {5U, 4U},   {0U, 4U},  {14U, 31U}, {0U, 1U},   {14U, 10U},
         {9U, 16U},  {4U, 1U},   {10U, 1U}, {0U, 28U},  {2U, 37U},  {8U, 12U},
         {2U, 18U},  {1U, 9U},   {0U, 28U}, {14U, 43U}, {0U, 1U},   {14U, 33U},
         {0U, 1U},   {5U, 1U},   {8U, 10U}, {6U, 13U},  {10U, 11U}, {14U, 4U},
         {9U, 18U},  {12U, 2U},  {0U, 1U}},
        {{6U, 6U},   {12U, 29U}, {5U, 9U},  {14U, 10U}, {0U, 2U},  {14U, 4U},
         {1U, 5U},   {14U, 31U}, {0U, 28U}, {6U, 43U},  {0U, 1U},  {12U, 14U},
         {8U, 6U},   {0U, 28U},  {6U, 4U},  {10U, 20U}, {6U, 12U}, {10U, 7U},
         {5U, 1U},   {10U, 2U},  {12U, 9U}, {8U, 16U},  {6U, 1U},  {9U, 8U},
         {10U, 12U}, {14U, 37U}, {0U, 1U}},
};

static bool fieldzero_regression_content(void)
{
    const FieldzeroRoomDefinition *room = fieldzero_content_room(0U);
    FieldzeroRoomDefinition malformed;

    if (!fieldzero_content_validate_all() || room == NULL)
    {
        return false;
    }
    malformed = *room;
    malformed.bands[0].offsets[1].x = 1;
    return !fieldzero_content_validate_room(&malformed, room->sector);
}

static bool fieldzero_regression_acceleration(void)
{
    FieldzeroGame game = {0};
    int32_t previous_velocity = 0;
    int32_t running_velocity = 0;
    bool passed = fieldzero_game_init(&game, UINT64_C(1)) == AFORC_OK;

    if (passed)
    {
        passed = fieldzero_regression_flat_world(&game, 12);
        fieldzero_regression_place_player(&game, 20, 11, true);
        fieldzero_game_set_move(&game, 1, true);
    }
    for (size_t tick = 0U; passed && tick < 14U; ++tick)
    {
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 game.player.velocity_x >= previous_velocity;
        previous_velocity = game.player.velocity_x;
    }
    if (passed)
    {
        running_velocity = game.player.velocity_x;
        fieldzero_game_set_move(&game, 1, false);
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 running_velocity > 0 && game.player.velocity_x >= 0 &&
                 game.player.velocity_x < running_velocity;
    }
    fieldzero_game_dispose(&game);
    return passed;
}

static bool fieldzero_regression_jump_contract(void)
{
    FieldzeroGame held = {0};
    FieldzeroGame released = {0};
    FieldzeroGame coyote = {0};
    FieldzeroGame buffered = {0};
    bool passed = fieldzero_game_init(&held, UINT64_C(2)) == AFORC_OK &&
                  fieldzero_game_init(&released, UINT64_C(2)) == AFORC_OK &&
                  fieldzero_game_init(&coyote, UINT64_C(2)) == AFORC_OK &&
                  fieldzero_game_init(&buffered, UINT64_C(2)) == AFORC_OK;

    if (passed)
    {
        passed = fieldzero_regression_flat_world(&held, 12) &&
                 fieldzero_regression_flat_world(&released, 12) &&
                 fieldzero_regression_flat_world(&coyote, 12) &&
                 fieldzero_regression_flat_world(&buffered, 12);
    }
    if (passed)
    {
        fieldzero_regression_place_player(&held, 20, 11, true);
        fieldzero_regression_place_player(&released, 20, 11, true);
        fieldzero_game_press_jump(&held);
        fieldzero_game_press_jump(&released);
        passed = fieldzero_game_tick(&held) == AFORC_OK &&
                 fieldzero_game_tick(&released) == AFORC_OK &&
                 held.player.velocity_y < 0 &&
                 held.player.velocity_y == released.player.velocity_y;
    }
    if (passed)
    {
        fieldzero_game_release_jump(&released);
        passed = fieldzero_game_tick(&held) == AFORC_OK &&
                 fieldzero_game_tick(&released) == AFORC_OK &&
                 held.player.velocity_y < released.player.velocity_y &&
                 released.player.velocity_y < 0;
    }
    if (passed)
    {
        fieldzero_regression_place_player(&coyote, 30, 11, true);
        passed = fieldzero_regression_fill_all(&coyote, 0U) &&
                 fieldzero_game_tick(&coyote) == AFORC_OK &&
                 !coyote.player.grounded && coyote.player.coyote_ticks > 0U;
    }
    if (passed)
    {
        fieldzero_game_press_jump(&coyote);
        passed = fieldzero_game_tick(&coyote) == AFORC_OK &&
                 coyote.player.velocity_y < 0 &&
                 coyote.player.coyote_ticks == 0U;
    }
    if (passed)
    {
        fieldzero_regression_place_player(&buffered, 40, 10, false);
        buffered.player.velocity_y = 60 * FIELDZERO_FIXED_ONE;
        fieldzero_game_press_jump(&buffered);
        passed = fieldzero_game_tick(&buffered) == AFORC_OK &&
                 buffered.player.y == 11 * FIELDZERO_FIXED_ONE &&
                 buffered.player.velocity_y < 0 &&
                 buffered.player.jump_buffer_ticks == 0U &&
                 !buffered.player.grounded;
    }
    fieldzero_game_dispose(&buffered);
    fieldzero_game_dispose(&coyote);
    fieldzero_game_dispose(&released);
    fieldzero_game_dispose(&held);
    return passed;
}

static bool fieldzero_regression_wall_and_dash(void)
{
    FieldzeroGame wall = {0};
    FieldzeroGame dash = {0};
    FieldzeroPresentation presentation = {0};
    const FieldzeroOptions options = {.seed = UINT64_C(3)};
    size_t dash_ticks = 0U;
    bool passed =
        fieldzero_game_init(&wall, UINT64_C(3)) == AFORC_OK &&
        fieldzero_game_init(&dash, UINT64_C(3)) == AFORC_OK &&
        fieldzero_presentation_init(&presentation, &options) == AFORC_OK;

    if (passed)
    {
        passed = fieldzero_regression_flat_world(&wall, -1) &&
                 fieldzero_regression_rect_all(
                     &wall, (AFORC_Rect){8, 0, 1, FIELDZERO_ARENA_HEIGHT}, 1U);
        fieldzero_regression_place_player(&wall, 7, 8, false);
        wall.player.velocity_y = 12 * FIELDZERO_FIXED_ONE;
    }
    if (passed)
    {
        passed = fieldzero_game_tick(&wall) == AFORC_OK &&
                 wall.player.wall_right &&
                 wall.player.velocity_y <= 6 * FIELDZERO_FIXED_ONE;
    }
    if (passed)
    {
        fieldzero_game_press_jump(&wall);
        passed = fieldzero_game_tick(&wall) == AFORC_OK &&
                 wall.player.velocity_x < 0 && wall.player.velocity_y < 0 &&
                 wall.player.facing == -1 && wall.player.wall_lock_ticks > 0U;
    }
    if (passed)
    {
        passed = fieldzero_regression_flat_world(&dash, -1);
        fieldzero_regression_place_player(&dash, 30, 8, false);
        fieldzero_game_set_move(&dash, -1, true);
        fieldzero_game_press_dash(&dash);
        passed =
            fieldzero_game_tick(&dash) == AFORC_OK &&
            dash.player.velocity_x < 0 && dash.player.velocity_y == 0 &&
            dash.player.dash_ticks > 0U && !dash.player.dash_available &&
            fieldzero_presentation_update(&presentation, &dash, 3U) == AFORC_OK;
        dash_ticks = 1U;
    }
    while (passed && dash.player.dash_ticks > 0U && dash_ticks < 20U)
    {
        passed = fieldzero_game_tick(&dash) == AFORC_OK;
        ++dash_ticks;
    }
    if (passed)
    {
        fieldzero_game_press_dash(&dash);
        passed = dash_ticks == FIELDZERO_REGRESSION_DASH_TICKS &&
                 fieldzero_game_tick(&dash) == AFORC_OK &&
                 dash.player.dash_ticks == 0U && !dash.player.dash_available;
    }
    if (passed)
    {
        passed = fieldzero_regression_rect_all(
            &dash, (AFORC_Rect){0, 9, FIELDZERO_ARENA_WIDTH, 1}, 1U);
        dash.player.y = 8 * FIELDZERO_FIXED_ONE;
        dash.player.velocity_y = 0;
        fieldzero_game_set_move(&dash, 0, false);
        passed = passed && fieldzero_game_tick(&dash) == AFORC_OK &&
                 dash.player.grounded && dash.player.dash_available;
    }
    fieldzero_presentation_dispose(&presentation);
    fieldzero_game_dispose(&dash);
    fieldzero_game_dispose(&wall);
    return passed;
}

static bool fieldzero_regression_completion_routes(void)
{
    bool passed = true;

    for (size_t room_index = 0U; passed && room_index < FIELDZERO_ROOM_COUNT;
         ++room_index)
    {
        FieldzeroGame game = {0};
        const uint16_t room_bit = (uint16_t)(UINT16_C(1) << room_index);
        uint8_t registrations = 0U;

        passed =
            fieldzero_game_init(&game, UINT64_C(2026)) == AFORC_OK &&
            fieldzero_game_enter_room(&game, room_index, true) == AFORC_OK &&
            fieldzero_regression_run_route(
                &game,
                fieldzero_completion_routes[room_index],
                &registrations) &&
            game.phase == FIELDZERO_PHASE_ROOM_TRANSITION &&
            registrations == fieldzero_room_mark_count(game.room) &&
            game.room_state + 1U == game.room->state_count &&
            game.checkpoint.room_state == game.room_state && game.falls == 0U &&
            (game.completed_rooms & room_bit) != 0U;
        fieldzero_game_dispose(&game);
    }
    return passed;
}

static bool fieldzero_regression_collision_and_corner(void)
{
    FieldzeroGame sweep = {0};
    FieldzeroGame corner = {0};
    int32_t corner_start = 0;
    bool passed = fieldzero_game_init(&sweep, UINT64_C(4)) == AFORC_OK &&
                  fieldzero_game_init(&corner, UINT64_C(4)) == AFORC_OK;

    if (passed)
    {
        passed = fieldzero_regression_flat_world(&sweep, 12) &&
                 fieldzero_regression_rect_all(
                     &sweep, (AFORC_Rect){30, 0, 1, 12}, 1U);
        fieldzero_regression_place_player(&sweep, 5, 11, true);
        sweep.player.velocity_x =
            40 * FIELDZERO_FIXED_UPDATES_PER_SECOND * FIELDZERO_FIXED_ONE;
        sweep.player.wall_lock_ticks = 2U;
    }
    if (passed)
    {
        passed =
            fieldzero_game_tick(&sweep) == AFORC_OK &&
            sweep.player.x == 29 * FIELDZERO_FIXED_ONE &&
            sweep.player.velocity_x == 0 &&
            fieldzero_game_cell_blocked(&sweep, -1, 5) &&
            !fieldzero_game_cell_blocked(&sweep, 5, FIELDZERO_ARENA_HEIGHT);
    }
    if (passed)
    {
        passed = fieldzero_regression_flat_world(&corner, -1) &&
                 fieldzero_regression_rect_all(
                     &corner, (AFORC_Rect){10, 5, 1, 1}, 1U);
        fieldzero_regression_place_player(&corner, 10, 6, false);
        corner.player.x += FIELDZERO_FIXED_ONE / 3;
        corner.player.velocity_y =
            -FIELDZERO_FIXED_UPDATES_PER_SECOND * FIELDZERO_FIXED_ONE;
        corner_start = corner.player.x;
    }
    if (passed)
    {
        passed = fieldzero_game_tick(&corner) == AFORC_OK &&
                 corner.player.x == 11 * FIELDZERO_FIXED_ONE &&
                 corner.player.x - corner_start > 0 &&
                 corner.player.x - corner_start <= FIELDZERO_FIXED_ONE;
    }
    fieldzero_game_dispose(&corner);
    fieldzero_game_dispose(&sweep);
    return passed;
}

static bool fieldzero_regression_fall_checkpoint(void)
{
    FieldzeroGame game = {0};
    FieldzeroCheckpoint checkpoint;
    uint32_t falls = 0U;
    bool passed = fieldzero_game_init(&game, UINT64_C(5)) == AFORC_OK;

    if (passed)
    {
        checkpoint = game.checkpoint;
        falls = game.falls;
        game.player.y = (FIELDZERO_ARENA_HEIGHT + 1) * FIELDZERO_FIXED_ONE;
        game.player.velocity_x = 0;
        game.player.velocity_y = 0;
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_DISSOLVING &&
                 game.falls == falls + 1U;
    }
    if (passed)
    {
        passed = fieldzero_regression_tick_many(
                     &game, FIELDZERO_DISSOLVE_TICKS - 1U) &&
                 game.phase == FIELDZERO_PHASE_DISSOLVING &&
                 fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_ACTIVE &&
                 game.player.x == checkpoint.x * FIELDZERO_FIXED_ONE &&
                 game.player.y == checkpoint.y * FIELDZERO_FIXED_ONE &&
                 game.room_state == checkpoint.room_state &&
                 game.player.dash_available;
    }
    fieldzero_game_dispose(&game);
    return passed;
}

static bool fieldzero_regression_registration(void)
{
    FieldzeroGame game = {0};
    FieldzeroGame unsafe = {0};
    AFORC_TileMap *active_before = NULL;
    AFORC_TileMap *staging_before = NULL;
    AFORC_TileMap *unsafe_active = NULL;
    uint64_t registering_digest = 0U;
    bool passed = fieldzero_game_init(&game, UINT64_C(6)) == AFORC_OK &&
                  fieldzero_game_init(&unsafe, UINT64_C(6)) == AFORC_OK;

    if (passed)
    {
        active_before = game.active_map;
        staging_before = game.staging_map;
        passed = fieldzero_game_begin_registration(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_REGISTERING &&
                 game.registration_target_state == 1U;
        registering_digest = fieldzero_game_collision_digest(&game);
    }
    for (size_t tick = 0U; passed && tick + 1U < FIELDZERO_REGISTRATION_TICKS;
         ++tick)
    {
        fieldzero_game_set_move(&game, 1, true);
        fieldzero_game_press_jump(&game);
        fieldzero_game_press_dash(&game);
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_REGISTERING &&
                 game.room_state == 0U && game.active_map == active_before &&
                 game.staging_map == staging_before &&
                 fieldzero_game_collision_digest(&game) == registering_digest &&
                 !game.actions.left && !game.actions.right &&
                 !game.actions.jump_held && !game.actions.jump_pressed &&
                 !game.actions.jump_released && !game.actions.dash_pressed;
    }
    if (passed)
    {
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_ACTIVE &&
                 game.room_state == 1U && game.room_states[0] == 1U &&
                 game.active_map == staging_before &&
                 game.staging_map == active_before &&
                 game.checkpoint.x == game.room->marks[0].x &&
                 game.checkpoint.y == game.room->marks[0].y &&
                 game.checkpoint.room_state == 1U &&
                 fieldzero_game_collision_digest(&game) != registering_digest;
    }
    if (passed)
    {
        const AFORC_Point release = unsafe.room->marks[0];

        unsafe_active = unsafe.active_map;
        passed = fieldzero_game_begin_registration(&unsafe) == AFORC_OK &&
                 aforc_tilemap_set(unsafe.staging_map, 0U, release, 1U) ==
                     AFORC_OK &&
                 fieldzero_regression_tick_many(
                     &unsafe, FIELDZERO_REGISTRATION_TICKS - 1U);
    }
    if (passed)
    {
        passed = fieldzero_game_tick(&unsafe) == AFORC_ERROR_STATE &&
                 unsafe.phase == FIELDZERO_PHASE_REGISTERING &&
                 unsafe.room_state == 0U && unsafe.active_map == unsafe_active;
    }
    fieldzero_game_dispose(&unsafe);
    fieldzero_game_dispose(&game);
    return passed;
}

static bool fieldzero_regression_prepare_exit(FieldzeroGame *game)
{
    const uint8_t final_state = (uint8_t)(game->room->state_count - 1U);

    game->room_state = final_state;
    game->room_states[game->room_index] = final_state;
    game->registration_target_state = final_state;
    if (fieldzero_game_rebuild_maps(game) != AFORC_OK)
    {
        return false;
    }
    fieldzero_regression_place_player(
        game, game->room->exit.x, game->room->exit.y, true);
    game->checkpoint.room_state = final_state;
    return true;
}

static bool fieldzero_regression_progression(void)
{
    FieldzeroGame game = {0};
    const uint64_t seed = UINT64_C(7);
    bool passed = fieldzero_game_init(&game, seed) == AFORC_OK;

    if (passed)
    {
        fieldzero_regression_place_player(
            &game, game.room->memory.x, game.room->memory.y, true);
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 game.memory_collected_here &&
                 (game.collected_memories & UINT16_C(1)) != 0U;
    }
    if (passed)
    {
        passed = fieldzero_regression_prepare_exit(&game) &&
                 fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_ROOM_TRANSITION &&
                 (game.completed_rooms & UINT16_C(1)) != 0U &&
                 fieldzero_regression_tick_many(
                     &game, FIELDZERO_ROOM_TRANSITION_TICKS) &&
                 game.room_index == 1U && game.phase == FIELDZERO_PHASE_ACTIVE;
    }
    if (passed)
    {
        passed = fieldzero_regression_prepare_exit(&game) &&
                 fieldzero_game_tick(&game) == AFORC_OK &&
                 fieldzero_regression_tick_many(
                     &game, FIELDZERO_ROOM_TRANSITION_TICKS) &&
                 game.room_index == 2U &&
                 game.phase == FIELDZERO_PHASE_SECTOR_TRANSITION;
    }
    if (passed)
    {
        passed = fieldzero_regression_tick_many(
                     &game, FIELDZERO_SECTOR_TRANSITION_TICKS - 1U) &&
                 game.phase == FIELDZERO_PHASE_SECTOR_TRANSITION &&
                 fieldzero_game_tick(&game) == AFORC_OK &&
                 game.phase == FIELDZERO_PHASE_ACTIVE;
    }
    if (passed)
    {
        passed = fieldzero_game_enter_room(
                     &game, FIELDZERO_ROOM_COUNT - 1U, true) == AFORC_OK &&
                 fieldzero_regression_prepare_exit(&game);
        game.falls = 9U;
        game.collected_memories = UINT16_C(0x03ff);
    }
    if (passed)
    {
        passed = fieldzero_game_tick(&game) == AFORC_OK &&
                 fieldzero_regression_tick_many(
                     &game, FIELDZERO_ROOM_TRANSITION_TICKS) &&
                 game.phase == FIELDZERO_PHASE_COMPLETE &&
                 (game.completed_rooms &
                  (uint16_t)(UINT16_C(1) << (FIELDZERO_ROOM_COUNT - 1U))) != 0U;
    }
    if (passed)
    {
        passed = fieldzero_game_restart_run(&game) == AFORC_OK &&
                 game.seed == seed && game.room_index == 0U &&
                 game.room_state == 0U &&
                 game.phase == FIELDZERO_PHASE_ACTIVE && game.falls == 0U &&
                 game.completed_rooms == 0U && game.collected_memories == 0U;
        for (size_t index = 0U; passed && index < FIELDZERO_ROOM_COUNT; ++index)
        {
            passed = game.room_states[index] == 0U;
        }
    }
    fieldzero_game_dispose(&game);
    return passed;
}

static bool fieldzero_regression_seed_independence(void)
{
    FieldzeroGame first = {0};
    FieldzeroGame second = {0};
    bool passed = fieldzero_game_init(&first, UINT64_C(2026)) == AFORC_OK &&
                  fieldzero_game_init(&second, UINT64_C(2027)) == AFORC_OK;

    if (passed)
    {
        passed = first.seed != second.seed &&
                 fieldzero_game_collision_digest(&first) ==
                     fieldzero_game_collision_digest(&second) &&
                 fieldzero_game_state_digest(&first) ==
                     fieldzero_game_state_digest(&second);
    }
    if (passed)
    {
        fieldzero_game_set_move(&first, 1, true);
        fieldzero_game_set_move(&second, 1, true);
        fieldzero_game_press_jump(&first);
        fieldzero_game_press_jump(&second);
        passed = fieldzero_regression_tick_many(&first, 12U) &&
                 fieldzero_regression_tick_many(&second, 12U) &&
                 fieldzero_game_state_digest(&first) ==
                     fieldzero_game_state_digest(&second) &&
                 fieldzero_game_collision_digest(&first) ==
                     fieldzero_game_collision_digest(&second);
    }
    for (size_t room_index = 0U; passed && room_index < FIELDZERO_ROOM_COUNT;
         ++room_index)
    {
        passed =
            fieldzero_game_enter_room(&first, room_index, true) == AFORC_OK &&
            fieldzero_game_enter_room(&second, room_index, true) == AFORC_OK;
        for (uint8_t state = 0U; passed && state < first.room->state_count;
             ++state)
        {
            first.room_state = state;
            first.room_states[room_index] = state;
            first.registration_target_state = state;
            second.room_state = state;
            second.room_states[room_index] = state;
            second.registration_target_state = state;
            passed = fieldzero_game_rebuild_maps(&first) == AFORC_OK &&
                     fieldzero_game_rebuild_maps(&second) == AFORC_OK &&
                     fieldzero_game_collision_digest(&first) ==
                         fieldzero_game_collision_digest(&second) &&
                     fieldzero_game_state_digest(&first) ==
                         fieldzero_game_state_digest(&second);
        }
    }
    fieldzero_game_dispose(&second);
    fieldzero_game_dispose(&first);
    return passed;
}

static uint64_t fieldzero_regression_hash_u64(uint64_t hash, uint64_t value)
{
    for (unsigned int shift = 0U; shift < 64U; shift += 8U)
    {
        hash = (hash ^ (uint8_t)(value >> shift)) * UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t
fieldzero_regression_renderer_digest(const AFORC_Renderer *renderer)
{
    const AFORC_Size size = aforc_renderer_size(renderer);
    uint64_t hash = UINT64_C(14695981039346656037);

    hash = fieldzero_regression_hash_u64(hash, (uint32_t)size.width);
    hash = fieldzero_regression_hash_u64(hash, (uint32_t)size.height);
    for (int32_t y = 0; y < size.height; ++y)
    {
        for (int32_t x = 0; x < size.width; ++x)
        {
            AFORC_Cell cell;

            if (aforc_renderer_get(renderer, (AFORC_Point){x, y}, &cell) !=
                AFORC_OK)
            {
                return 0U;
            }
            hash = fieldzero_regression_hash_u64(hash, cell.codepoint);
            hash = fieldzero_regression_hash_u64(hash, cell.style);
            hash = fieldzero_regression_hash_u64(
                hash, (uint32_t)cell.foreground.mode);
            hash = fieldzero_regression_hash_u64(hash, cell.foreground.red);
            hash = fieldzero_regression_hash_u64(hash, cell.foreground.green);
            hash = fieldzero_regression_hash_u64(hash, cell.foreground.blue);
            hash = fieldzero_regression_hash_u64(
                hash, (uint32_t)cell.background.mode);
            hash = fieldzero_regression_hash_u64(hash, cell.background.red);
            hash = fieldzero_regression_hash_u64(hash, cell.background.green);
            hash = fieldzero_regression_hash_u64(hash, cell.background.blue);
        }
    }
    return hash;
}

static bool
fieldzero_regression_render_case(AFORC_Renderer *renderer,
                                 FieldzeroGame *game,
                                 const FieldzeroPresentation *presentation,
                                 FieldzeroViewState *view,
                                 AFORC_Size size)
{
    view->terminal_too_small =
        size.width < FIELDZERO_MIN_WIDTH || size.height < FIELDZERO_MIN_HEIGHT;
    return aforc_renderer_resize(renderer, size) == AFORC_OK &&
           fieldzero_render(renderer, game, presentation, view) == AFORC_OK &&
           fieldzero_render_validate(renderer, view, presentation->no_color) ==
               AFORC_OK;
}

static bool fieldzero_regression_rendering(void)
{
    static const AFORC_Size sizes[] = {
        {79, 23}, {80, 24}, {100, 30}, {120, 40}};
    FieldzeroGame game = {0};
    FieldzeroPresentation normal = {0};
    FieldzeroPresentation monochrome = {0};
    FieldzeroPresentation reduced = {0};
    FieldzeroOptions normal_options = {.seed = UINT64_C(2026)};
    FieldzeroOptions monochrome_options = {.seed = UINT64_C(2026),
                                           .no_color = true};
    FieldzeroOptions reduced_options = {.seed = UINT64_C(2026),
                                        .reduced_motion = true};
    FieldzeroViewState view = {.screen = FIELDZERO_SCREEN_PLAY};
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell original;
    uint64_t state_digest = 0U;
    uint64_t collision_digest = 0U;
    uint64_t render_first = 0U;
    uint64_t render_second = 0U;
    uint64_t render_other_seed = 0U;
    bool normal_initialized = false;
    bool monochrome_initialized = false;
    bool reduced_initialized = false;
    bool passed = fieldzero_game_init(&game, normal_options.seed) == AFORC_OK;

    config.size = (AFORC_Size){100, 30};
    if (passed)
    {
        passed = aforc_renderer_create(&renderer, &config) == AFORC_OK;
    }
    if (passed)
    {
        passed =
            fieldzero_presentation_init(&normal, &normal_options) == AFORC_OK;
        normal_initialized = passed;
    }
    state_digest = fieldzero_game_state_digest(&game);
    collision_digest = fieldzero_game_collision_digest(&game);
    for (size_t index = 0U; passed && index < sizeof(sizes) / sizeof(sizes[0]);
         ++index)
    {
        view.screen = FIELDZERO_SCREEN_PLAY;
        passed = fieldzero_regression_render_case(
            renderer, &game, &normal, &view, sizes[index]);
    }
    if (passed)
    {
        view.screen = FIELDZERO_SCREEN_TITLE;
        passed = fieldzero_regression_render_case(
            renderer, &game, &normal, &view, (AFORC_Size){100, 30});
    }
    if (passed)
    {
        view.screen = FIELDZERO_SCREEN_COMPLETE;
        passed = fieldzero_regression_render_case(
            renderer, &game, &normal, &view, (AFORC_Size){100, 30});
    }
    if (passed)
    {
        view.screen = FIELDZERO_SCREEN_PLAY;
        passed = fieldzero_regression_render_case(
            renderer, &game, &normal, &view, (AFORC_Size){100, 30});
        render_first = fieldzero_regression_renderer_digest(renderer);
        passed = passed &&
                 fieldzero_render(renderer, &game, &normal, &view) == AFORC_OK;
        render_second = fieldzero_regression_renderer_digest(renderer);
        game.seed = UINT64_C(2027);
        passed = passed &&
                 fieldzero_render(renderer, &game, &normal, &view) == AFORC_OK;
        render_other_seed = fieldzero_regression_renderer_digest(renderer);
        game.seed = normal_options.seed;
        passed = passed && render_first != 0U &&
                 render_first == render_second &&
                 render_first != render_other_seed &&
                 fieldzero_game_state_digest(&game) == state_digest &&
                 fieldzero_game_collision_digest(&game) == collision_digest;
    }
    if (passed)
    {
        passed =
            fieldzero_render(renderer, &game, &normal, &view) == AFORC_OK &&
            aforc_renderer_get(renderer, (AFORC_Point){0, 0}, &original) ==
                AFORC_OK;
    }
    if (passed)
    {
        AFORC_Cell broken = original;

        broken.codepoint = UINT32_C(0x00e9);
        passed = aforc_renderer_put(renderer, (AFORC_Point){0, 0}, broken) ==
                     AFORC_OK &&
                 fieldzero_render_validate(renderer, &view, false) ==
                     AFORC_ERROR_FORMAT;
    }
    if (passed)
    {
        AFORC_Cell broken = original;

        broken.codepoint = (uint32_t)'!';
        broken.foreground = aforc_color_rgb(0x62U, 0xd7U, 0x76U);
        passed = aforc_renderer_put(renderer, (AFORC_Point){0, 0}, broken) ==
                     AFORC_OK &&
                 fieldzero_render_validate(renderer, &view, false) ==
                     AFORC_ERROR_STATE;
    }
    if (passed)
    {
        passed = fieldzero_presentation_init(&monochrome,
                                             &monochrome_options) == AFORC_OK;
        monochrome_initialized = passed;
    }
    if (passed)
    {
        passed =
            fieldzero_regression_render_case(
                renderer, &game, &monochrome, &view, (AFORC_Size){100, 30}) &&
            aforc_renderer_get(renderer, (AFORC_Point){0, 0}, &original) ==
                AFORC_OK;
    }
    if (passed)
    {
        AFORC_Cell broken = original;

        broken.foreground = aforc_color_rgb(1U, 2U, 3U);
        passed = aforc_renderer_put(renderer, (AFORC_Point){0, 0}, broken) ==
                     AFORC_OK &&
                 fieldzero_render_validate(renderer, &view, true) ==
                     AFORC_ERROR_STATE;
    }
    if (passed)
    {
        passed =
            fieldzero_presentation_init(&reduced, &reduced_options) == AFORC_OK;
        reduced_initialized = passed;
    }
    for (uint64_t tick = 1U; passed && tick <= 120U; ++tick)
    {
        passed =
            fieldzero_presentation_update(&reduced, &game, tick) == AFORC_OK &&
            reduced.camera_impulse_x == 0 && reduced.camera_impulse_y == 0;
    }
    if (passed)
    {
        view.screen = FIELDZERO_SCREEN_PLAY;
        passed = fieldzero_regression_render_case(
                     renderer, &game, &reduced, &view, (AFORC_Size){100, 30}) &&
                 fieldzero_game_state_digest(&game) == state_digest &&
                 fieldzero_game_collision_digest(&game) == collision_digest &&
                 fieldzero_render_validate(NULL, &view, false) ==
                     AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (reduced_initialized)
    {
        fieldzero_presentation_dispose(&reduced);
    }
    if (monochrome_initialized)
    {
        fieldzero_presentation_dispose(&monochrome);
    }
    if (normal_initialized)
    {
        fieldzero_presentation_dispose(&normal);
    }
    aforc_renderer_destroy(renderer);
    fieldzero_game_dispose(&game);
    return passed;
}

static bool fieldzero_regression_result(const char *name, bool passed)
{
    if (!passed)
    {
        (void)fprintf(stderr, "fieldzero regression failed: %s\n", name);
    }
    return passed;
}

bool fieldzero_run_regressions(void)
{
    bool passed = true;

    passed =
        fieldzero_regression_result("content metadata and four-cell offsets",
                                    fieldzero_regression_content()) &&
        passed;
    passed = fieldzero_regression_result("acceleration and deceleration",
                                         fieldzero_regression_acceleration()) &&
             passed;
    passed =
        fieldzero_regression_result("jump buffer, coyote time, and release",
                                    fieldzero_regression_jump_contract()) &&
        passed;
    passed =
        fieldzero_regression_result("wall contact, wall kick, and dash",
                                    fieldzero_regression_wall_and_dash()) &&
        passed;
    passed = fieldzero_regression_result(
                 "all-room registration and completion routes",
                 fieldzero_regression_completion_routes()) &&
             passed;
    passed = fieldzero_regression_result(
                 "swept collision and corner correction",
                 fieldzero_regression_collision_and_corner()) &&
             passed;
    passed =
        fieldzero_regression_result("fall and checkpoint restoration",
                                    fieldzero_regression_fall_checkpoint()) &&
        passed;
    passed =
        fieldzero_regression_result("registration atomicity and release safety",
                                    fieldzero_regression_registration()) &&
        passed;
    passed = fieldzero_regression_result("progression, completion, and restart",
                                         fieldzero_regression_progression()) &&
             passed;
    passed =
        fieldzero_regression_result("all-state seed collision independence",
                                    fieldzero_regression_seed_independence()) &&
        passed;
    passed =
        fieldzero_regression_result("layout, ASCII, color, and reduced motion",
                                    fieldzero_regression_rendering()) &&
        passed;
    return passed;
}
