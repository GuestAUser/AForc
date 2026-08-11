#include "fieldzero/qa.h"

#include <string.h>

enum
{
    FIELDZERO_SMOKE_FRAME_NS = 16666667,
    FIELDZERO_SMOKE_REGISTRATION_TICKS = 27,
    FIELDZERO_SMOKE_DISSOLVE_TICKS = 30,
    FIELDZERO_SMOKE_ROOM_TRANSITION_TICKS = 18,
    FIELDZERO_SMOKE_OVERFLOW_KEYS = 130
};

typedef struct FieldzeroSmokeClock
{
    uint64_t now_ns;
    uint64_t input_ms;
} FieldzeroSmokeClock;

static bool fieldzero_smoke_fail(AFORC_Error *error,
                                 AFORC_Status status,
                                 const char *message)
{
    if (error != NULL && error->message[0] == '\0')
    {
        aforc_error_set(error, status, "fieldzero-smoke", "%s", message);
    }
    return false;
}

static bool fieldzero_smoke_status(AFORC_Status status,
                                   AFORC_Error *error,
                                   const char *message)
{
    return status == AFORC_OK ? true
                              : fieldzero_smoke_fail(error, status, message);
}

static bool fieldzero_smoke_dispatch_queue(FieldzeroApp *app,
                                           AFORC_Error *error,
                                           bool require_event)
{
    AFORC_InputEvent event;
    size_t event_count = 0U;

    while (aforc_input_next_event(app->input, &event))
    {
        bool consumed = false;
        const AFORC_Status status =
            aforc_engine_dispatch_event(app->engine, &event, &consumed, error);

        if (status != AFORC_OK)
        {
            return fieldzero_smoke_fail(
                error, status, "decoded input dispatch failed");
        }
        ++event_count;
    }
    return !require_event || event_count != 0U
               ? true
               : fieldzero_smoke_fail(
                     error, AFORC_ERROR_STATE, "input produced no event");
}

static bool fieldzero_smoke_input(FieldzeroApp *app,
                                  FieldzeroSmokeClock *clock,
                                  const unsigned char *bytes,
                                  size_t size,
                                  AFORC_Error *error)
{
    ++clock->input_ms;
    if (!fieldzero_smoke_status(
            aforc_input_feed(app->input, bytes, size, clock->input_ms),
            error,
            "input decoder rejected smoke bytes"))
    {
        return false;
    }
    return fieldzero_smoke_dispatch_queue(app, error, true);
}

static bool fieldzero_smoke_release(FieldzeroApp *app,
                                    FieldzeroSmokeClock *clock,
                                    AFORC_Error *error)
{
    ++clock->input_ms;
    aforc_input_release_all(app->input, clock->input_ms);
    return fieldzero_smoke_dispatch_queue(app, error, false);
}

static bool fieldzero_smoke_frame(FieldzeroApp *app,
                                  FieldzeroSmokeClock *clock,
                                  AFORC_Error *error)
{
    clock->now_ns += FIELDZERO_SMOKE_FRAME_NS;
    return fieldzero_smoke_status(
        aforc_engine_frame(app->engine, clock->now_ns, error),
        error,
        "deterministic engine frame failed");
}

static bool fieldzero_smoke_frames(FieldzeroApp *app,
                                   FieldzeroSmokeClock *clock,
                                   size_t count,
                                   AFORC_Error *error)
{
    for (size_t frame = 0U; frame < count; ++frame)
    {
        if (!fieldzero_smoke_frame(app, clock, error))
        {
            return false;
        }
    }
    return true;
}

static void fieldzero_smoke_place_player(FieldzeroGame *game, AFORC_Point point)
{
    (void)memset(&game->player, 0, sizeof(game->player));
    game->player.x = point.x * FIELDZERO_FIXED_ONE;
    game->player.y = point.y * FIELDZERO_FIXED_ONE;
    game->player.facing = 1;
    game->player.grounded =
        fieldzero_game_cell_blocked(game, point.x, point.y + 1);
    game->player.dash_available = true;
    fieldzero_game_clear_actions(game);
}

static bool fieldzero_smoke_render_valid(FieldzeroApp *app, AFORC_Error *error)
{
    const AFORC_Status status = fieldzero_render_validate(
        app->renderer, &app->view, app->options.no_color);

    return status == AFORC_OK
               ? true
               : fieldzero_smoke_fail(
                     error, status, "offscreen render invariant failed");
}

static bool fieldzero_smoke_input_contracts(FieldzeroApp *app,
                                            FieldzeroSmokeClock *clock,
                                            AFORC_Error *error)
{
    static const unsigned char left_down[] = "\x1b[97;1:1u\x1b[1;1:1D";
    static const unsigned char left_a_up[] = "\x1b[97;1:3u";
    static const unsigned char left_arrow_up[] = "\x1b[1;1:3D";
    static const unsigned char right_down[] = "\x1b[100;1:1u\x1b[1;1:1C";
    static const unsigned char right_d_up[] = "\x1b[100;1:3u";
    static const unsigned char right_arrow_up[] = "\x1b[1;1:3C";
    static const unsigned char jump_down[] = "\x1b[32;1:1u\x1b[122;1:1u";
    static const unsigned char jump_space_up[] = "\x1b[32;1:3u";
    static const unsigned char jump_z_up[] = "\x1b[122;1:3u";
    static const unsigned char overflow_left[] = "\x1b[97;1:1u";
    static const unsigned char overflow_jump[] = "\x1b[32;1:1u";
    static const unsigned char overflow_dash[] = "\x1b[120;1:1u";
    unsigned char overflow[FIELDZERO_SMOKE_OVERFLOW_KEYS];
    uint64_t dropped_before;

    if (!fieldzero_smoke_input(
            app, clock, left_down, sizeof(left_down) - 1U, error) ||
        !app->game.actions.left ||
        !fieldzero_smoke_input(
            app, clock, left_a_up, sizeof(left_a_up) - 1U, error) ||
        !app->game.actions.left ||
        !fieldzero_smoke_input(
            app, clock, left_arrow_up, sizeof(left_arrow_up) - 1U, error) ||
        app->game.actions.left ||
        !fieldzero_smoke_input(
            app, clock, right_down, sizeof(right_down) - 1U, error) ||
        !app->game.actions.right ||
        !fieldzero_smoke_input(
            app, clock, right_d_up, sizeof(right_d_up) - 1U, error) ||
        !app->game.actions.right ||
        !fieldzero_smoke_input(
            app, clock, right_arrow_up, sizeof(right_arrow_up) - 1U, error) ||
        app->game.actions.right ||
        !fieldzero_smoke_input(
            app, clock, jump_down, sizeof(jump_down) - 1U, error) ||
        !app->game.actions.jump_held || !app->game.actions.jump_pressed ||
        !fieldzero_smoke_input(
            app, clock, jump_space_up, sizeof(jump_space_up) - 1U, error) ||
        !app->game.actions.jump_held || app->game.actions.jump_released ||
        !fieldzero_smoke_input(
            app, clock, jump_z_up, sizeof(jump_z_up) - 1U, error) ||
        app->game.actions.jump_held || !app->game.actions.jump_released)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "input aliases lost held state");
    }

    fieldzero_game_clear_actions(&app->game);
    if (aforc_input_begin_frame(app->input) != AFORC_OK)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "overflow input frame did not begin");
    }
    (void)memset(overflow, (unsigned char)'1', sizeof(overflow));
    dropped_before = aforc_input_dropped_events(app->input);
    ++clock->input_ms;
    if (aforc_input_feed(
            app->input, overflow, sizeof(overflow), clock->input_ms) !=
            AFORC_ERROR_LIMIT ||
        aforc_input_feed(app->input,
                         overflow_left,
                         sizeof(overflow_left) - 1U,
                         ++clock->input_ms) != AFORC_ERROR_LIMIT ||
        aforc_input_feed(app->input,
                         overflow_jump,
                         sizeof(overflow_jump) - 1U,
                         ++clock->input_ms) != AFORC_ERROR_LIMIT ||
        aforc_input_feed(app->input,
                         overflow_dash,
                         sizeof(overflow_dash) - 1U,
                         ++clock->input_ms) != AFORC_ERROR_LIMIT ||
        aforc_input_dropped_events(app->input) == dropped_before ||
        !fieldzero_smoke_dispatch_queue(app, error, true))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "input queue did not report overflow");
    }
    fieldzero_app_reconcile_input(app);
    if (!app->game.actions.left || app->game.actions.right ||
        !app->game.actions.jump_held || !app->game.actions.jump_pressed ||
        !app->game.actions.dash_pressed ||
        !fieldzero_smoke_release(app, clock, error) || app->game.actions.left ||
        app->game.actions.jump_held || !app->game.actions.jump_released)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "overflow input state was not recovered");
    }
    fieldzero_game_clear_actions(&app->game);
    return true;
}

bool fieldzero_smoke_drive(FieldzeroApp *app, AFORC_Error *error)
{
    static const unsigned char start_input[] = " ";
    static const unsigned char move_input[] = "d";
    static const unsigned char jump_input[] = "z";
    static const unsigned char dash_input[] = "x";
    static const unsigned char help_input[] = "?";
    FieldzeroSmokeClock clock = {0};
    AFORC_TileMap *registration_active = NULL;
    AFORC_TileMap *registration_staging = NULL;
    FieldzeroCheckpoint checkpoint;
    AFORC_Cell original_cell;
    uint64_t registration_digest = 0U;
    uint64_t paused_digest = 0U;
    uint64_t small_digest = 0U;
    int32_t start_x = 0;
    uint32_t falls = 0U;

    if (app == NULL || error == NULL || !app->initialized ||
        app->renderer == NULL || app->input == NULL || app->engine == NULL ||
        app->terminal != NULL)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_INVALID_ARGUMENT, "invalid offscreen app");
    }
    if (app->view.screen != FIELDZERO_SCREEN_TITLE ||
        aforc_engine_scene(app->engine) != &app->title_scene ||
        app->presentation.reduced_motion != app->options.reduced_motion ||
        app->presentation.no_color != app->options.no_color ||
        fieldzero_game_state_digest(&app->game) == 0U ||
        fieldzero_game_collision_digest(&app->game) == 0U)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "initial smoke state is inconsistent");
    }
    if (!fieldzero_smoke_render_valid(app, error) ||
        !fieldzero_smoke_input(
            app, &clock, start_input, sizeof(start_input) - 1U, error) ||
        !fieldzero_smoke_release(app, &clock, error) ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        app->view.screen != FIELDZERO_SCREEN_PLAY ||
        aforc_engine_scene(app->engine) != &app->play_scene)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "title did not enter gameplay");
    }
    if (!fieldzero_smoke_input_contracts(app, &clock, error))
    {
        return false;
    }

    start_x = app->game.player.x;
    if (!fieldzero_smoke_input(
            app, &clock, move_input, sizeof(move_input) - 1U, error) ||
        !fieldzero_smoke_frames(app, &clock, 12U, error) ||
        app->game.player.x <= start_x || app->game.player.velocity_x <= 0 ||
        !fieldzero_smoke_release(app, &clock, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "movement sequence did not accelerate");
    }
    if (!fieldzero_smoke_input(
            app, &clock, jump_input, sizeof(jump_input) - 1U, error) ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        app->game.player.velocity_y >= 0 ||
        !fieldzero_smoke_release(app, &clock, error) ||
        !fieldzero_smoke_frame(app, &clock, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "jump input did not reach simulation");
    }
    if (!fieldzero_smoke_input(
            app, &clock, dash_input, sizeof(dash_input) - 1U, error) ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        app->game.player.dash_ticks == 0U || app->game.player.velocity_y != 0 ||
        app->game.player.dash_available ||
        !fieldzero_smoke_release(app, &clock, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "air dash did not execute");
    }

    registration_active = app->game.active_map;
    registration_staging = app->game.staging_map;
    fieldzero_smoke_place_player(&app->game, app->game.room->marks[0]);
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        app->game.phase != FIELDZERO_PHASE_REGISTERING ||
        app->game.active_map != registration_active)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "registration did not begin at mark");
    }
    registration_digest = fieldzero_game_collision_digest(&app->game);
    for (size_t tick = 0U; tick + 1U < FIELDZERO_SMOKE_REGISTRATION_TICKS;
         ++tick)
    {
        if (!fieldzero_smoke_frame(app, &clock, error) ||
            app->game.phase != FIELDZERO_PHASE_REGISTERING ||
            app->game.active_map != registration_active ||
            fieldzero_game_collision_digest(&app->game) != registration_digest)
        {
            return fieldzero_smoke_fail(
                error,
                AFORC_ERROR_STATE,
                "registration collision was not atomic");
        }
    }
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        app->game.phase != FIELDZERO_PHASE_ACTIVE ||
        app->game.room_state != 1U ||
        app->game.active_map != registration_staging ||
        app->game.staging_map != registration_active ||
        fieldzero_game_collision_digest(&app->game) == registration_digest)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "registration did not commit safely");
    }

    checkpoint = app->game.checkpoint;
    falls = app->game.falls;
    app->game.player.y = (FIELDZERO_ARENA_HEIGHT + 1) * FIELDZERO_FIXED_ONE;
    app->game.player.velocity_x = 0;
    app->game.player.velocity_y = 0;
    fieldzero_game_clear_actions(&app->game);
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        app->game.phase != FIELDZERO_PHASE_DISSOLVING ||
        app->game.falls != falls + 1U ||
        !fieldzero_smoke_frames(
            app, &clock, FIELDZERO_SMOKE_DISSOLVE_TICKS - 1U, error) ||
        app->game.phase != FIELDZERO_PHASE_DISSOLVING ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        app->game.phase != FIELDZERO_PHASE_ACTIVE ||
        app->game.player.x != checkpoint.x * FIELDZERO_FIXED_ONE ||
        app->game.player.y != checkpoint.y * FIELDZERO_FIXED_ONE ||
        app->game.room_state != checkpoint.room_state)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "fall did not restore checkpoint");
    }

    fieldzero_smoke_place_player(&app->game, app->game.room->exit);
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        app->game.phase != FIELDZERO_PHASE_ROOM_TRANSITION ||
        (app->game.completed_rooms & UINT16_C(1)) == 0U ||
        !fieldzero_smoke_frames(
            app, &clock, FIELDZERO_SMOKE_ROOM_TRANSITION_TICKS, error) ||
        app->game.room_index != 1U || app->game.phase != FIELDZERO_PHASE_ACTIVE)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "room transition did not complete");
    }

    if (!fieldzero_smoke_input(
            app, &clock, help_input, sizeof(help_input) - 1U, error) ||
        !app->view.help_visible)
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "help overlay did not open");
    }
    paused_digest = fieldzero_game_state_digest(&app->game);
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        fieldzero_game_state_digest(&app->game) != paused_digest ||
        !fieldzero_smoke_release(app, &clock, error) ||
        !fieldzero_smoke_input(
            app, &clock, help_input, sizeof(help_input) - 1U, error) ||
        app->view.help_visible || !fieldzero_smoke_release(app, &clock, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "help overlay did not freeze and close");
    }

    if (!fieldzero_smoke_status(
            aforc_renderer_resize(app->renderer, (AFORC_Size){79, 23}),
            error,
            "small viewport resize failed"))
    {
        return false;
    }
    small_digest = fieldzero_game_state_digest(&app->game);
    if (!fieldzero_smoke_frame(app, &clock, error) ||
        !app->view.terminal_too_small ||
        fieldzero_game_state_digest(&app->game) != small_digest ||
        !fieldzero_smoke_render_valid(app, error) ||
        !fieldzero_smoke_status(
            aforc_renderer_resize(app->renderer, (AFORC_Size){120, 40}),
            error,
            "large viewport resize failed") ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        app->view.terminal_too_small ||
        !fieldzero_smoke_render_valid(app, error) ||
        !fieldzero_smoke_status(
            aforc_renderer_resize(app->renderer, (AFORC_Size){100, 30}),
            error,
            "canonical viewport resize failed") ||
        !fieldzero_smoke_frame(app, &clock, error) ||
        !fieldzero_smoke_render_valid(app, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "viewport layout invariant failed");
    }

    if (app->options.reduced_motion &&
        (app->presentation.camera_impulse_x != 0 ||
         app->presentation.camera_impulse_y != 0))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "reduced motion retained camera impulse");
    }
    if (!fieldzero_smoke_status(aforc_renderer_get(app->renderer,
                                                   (AFORC_Point){0, 0},
                                                   &original_cell),
                                error,
                                "renderer probe failed"))
    {
        return false;
    }
    AFORC_Cell broken_cell = original_cell;

    broken_cell.style |= AFORC_STYLE_HIDDEN;
    if (!fieldzero_smoke_status(
            aforc_renderer_put(app->renderer, (AFORC_Point){0, 0}, broken_cell),
            error,
            "renderer invariant probe could not be written") ||
        fieldzero_render_validate(
            app->renderer, &app->view, app->options.no_color) == AFORC_OK ||
        !fieldzero_smoke_status(
            aforc_renderer_put(
                app->renderer, (AFORC_Point){0, 0}, original_cell),
            error,
            "renderer invariant probe could not be restored") ||
        !fieldzero_smoke_render_valid(app, error))
    {
        return fieldzero_smoke_fail(
            error, AFORC_ERROR_STATE, "broken render invariant was accepted");
    }
    return true;
}
