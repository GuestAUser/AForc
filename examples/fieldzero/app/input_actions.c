#include "fieldzero/app.h"

static bool fieldzero_is_left(const AFORC_InputEvent *event, uint32_t codepoint)
{
    return event->data.key.key == AFORC_KEY_LEFT ||
           fieldzero_codepoint_is(codepoint, 'a');
}

static bool fieldzero_is_right(const AFORC_InputEvent *event,
                               uint32_t codepoint)
{
    return event->data.key.key == AFORC_KEY_RIGHT ||
           fieldzero_codepoint_is(codepoint, 'd');
}

static bool fieldzero_is_jump(const AFORC_InputEvent *event, uint32_t codepoint)
{
    return event->data.key.key == AFORC_KEY_SPACE ||
           fieldzero_codepoint_is(codepoint, 'z');
}

static bool fieldzero_any_key_held(const FieldzeroApp *app,
                                   AFORC_Key first,
                                   AFORC_Key second)
{
    return aforc_input_key_held(app->input, first) ||
           aforc_input_key_held(app->input, second);
}

static void fieldzero_handle_key_up(FieldzeroApp *app,
                                    const AFORC_InputEvent *event)
{
    const uint32_t codepoint = fieldzero_event_codepoint(event);

    if (fieldzero_is_left(event, codepoint))
    {
        fieldzero_game_set_move(
            &app->game,
            -1,
            fieldzero_any_key_held(app, AFORC_KEY_A, AFORC_KEY_LEFT));
    }
    if (fieldzero_is_right(event, codepoint))
    {
        fieldzero_game_set_move(
            &app->game,
            1,
            fieldzero_any_key_held(app, AFORC_KEY_D, AFORC_KEY_RIGHT));
    }
    if (fieldzero_is_jump(event, codepoint) &&
        !fieldzero_any_key_held(app, AFORC_KEY_SPACE, AFORC_KEY_Z))
    {
        fieldzero_game_release_jump(&app->game);
    }
}

static void
fieldzero_handle_back(FieldzeroApp *app, AFORC_Engine *engine, bool quit_key)
{
    if (app->view.quit_confirmation)
    {
        if (quit_key)
        {
            aforc_engine_request_quit(engine);
        }
        else
        {
            app->view.quit_confirmation = false;
        }
    }
    else if (app->view.help_visible)
    {
        app->view.help_visible = false;
    }
    else if (app->view.paused)
    {
        app->view.paused = false;
    }
    else if (app->view.focus_paused)
    {
        return;
    }
    else if (app->view.screen == FIELDZERO_SCREEN_PLAY)
    {
        app->view.quit_confirmation = true;
    }
    else
    {
        aforc_engine_request_quit(engine);
    }
    fieldzero_game_clear_actions(&app->game);
}

void fieldzero_app_reconcile_input(FieldzeroApp *app)
{
    bool jump_held;
    bool jump_pressed;
    bool jump_released;

    if (app == NULL || app->input == NULL)
    {
        return;
    }
    if (app->view.screen != FIELDZERO_SCREEN_PLAY ||
        fieldzero_view_has_overlay(&app->view) ||
        app->view.terminal_too_small ||
        app->game.phase != FIELDZERO_PHASE_ACTIVE)
    {
        fieldzero_game_clear_actions(&app->game);
        return;
    }
    fieldzero_game_set_move(
        &app->game,
        -1,
        fieldzero_any_key_held(app, AFORC_KEY_A, AFORC_KEY_LEFT));
    fieldzero_game_set_move(
        &app->game,
        1,
        fieldzero_any_key_held(app, AFORC_KEY_D, AFORC_KEY_RIGHT));
    jump_held = fieldzero_any_key_held(app, AFORC_KEY_SPACE, AFORC_KEY_Z);
    jump_pressed = aforc_input_key_pressed(app->input, AFORC_KEY_SPACE) ||
                   aforc_input_key_pressed(app->input, AFORC_KEY_Z);
    jump_released = aforc_input_key_released(app->input, AFORC_KEY_SPACE) ||
                    aforc_input_key_released(app->input, AFORC_KEY_Z);
    if (jump_pressed)
    {
        fieldzero_game_press_jump(&app->game);
    }
    if (!jump_held && jump_released)
    {
        fieldzero_game_release_jump(&app->game);
    }
    else
    {
        app->game.actions.jump_held = jump_held;
    }
    if (aforc_input_key_pressed(app->input, AFORC_KEY_K))
    {
        fieldzero_game_press_dash(&app->game);
    }
}

void fieldzero_app_handle_input(FieldzeroApp *app,
                                const AFORC_InputEvent *event,
                                AFORC_Engine *engine)
{
    uint32_t codepoint;

    if (app == NULL || event == NULL || engine == NULL)
    {
        return;
    }
    if (event->type == AFORC_INPUT_EVENT_FOCUS_OUT)
    {
        app->view.focus_paused = true;
        fieldzero_game_clear_actions(&app->game);
        return;
    }
    if (event->type == AFORC_INPUT_EVENT_FOCUS_IN)
    {
        app->view.focus_paused = false;
        return;
    }
    if (event->type == AFORC_INPUT_EVENT_KEY_UP)
    {
        fieldzero_handle_key_up(app, event);
        return;
    }
    if (event->type != AFORC_INPUT_EVENT_KEY_DOWN)
    {
        return;
    }

    codepoint = fieldzero_event_codepoint(event);
    if (!event->data.key.repeat && (event->data.key.key == AFORC_KEY_ESCAPE ||
                                    fieldzero_codepoint_is(codepoint, 'q')))
    {
        fieldzero_handle_back(
            app, engine, fieldzero_codepoint_is(codepoint, 'q'));
        return;
    }
    if (!event->data.key.repeat && codepoint == (uint32_t)'?' &&
        !app->view.quit_confirmation)
    {
        app->view.help_visible = !app->view.help_visible;
        fieldzero_game_clear_actions(&app->game);
        return;
    }
    if (!event->data.key.repeat && fieldzero_codepoint_is(codepoint, 'p') &&
        app->view.screen == FIELDZERO_SCREEN_PLAY &&
        !app->view.quit_confirmation)
    {
        app->view.paused = !app->view.paused;
        fieldzero_game_clear_actions(&app->game);
        return;
    }
    if (app->view.screen != FIELDZERO_SCREEN_PLAY ||
        fieldzero_view_has_overlay(&app->view) ||
        app->view.terminal_too_small ||
        app->game.phase != FIELDZERO_PHASE_ACTIVE)
    {
        return;
    }
    if (fieldzero_is_left(event, codepoint))
    {
        fieldzero_game_set_move(&app->game, -1, true);
    }
    else if (fieldzero_is_right(event, codepoint))
    {
        fieldzero_game_set_move(&app->game, 1, true);
    }
    else if (fieldzero_is_jump(event, codepoint))
    {
        if (!event->data.key.repeat)
        {
            fieldzero_game_press_jump(&app->game);
        }
    }
    else if (fieldzero_codepoint_is(codepoint, 'k') && !event->data.key.repeat)
    {
        fieldzero_game_press_dash(&app->game);
    }
}
