/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"

#include <string.h>

enum {
    SURF_MAN_HOLD_UP_ARROW = 1U << 0,
    SURF_MAN_HOLD_W = 1U << 1,
    SURF_MAN_HOLD_DOWN_ARROW = 1U << 2,
    SURF_MAN_HOLD_S = 1U << 3,
    SURF_MAN_HOLD_LEFT_ARROW = 1U << 4,
    SURF_MAN_HOLD_A = 1U << 5,
    SURF_MAN_HOLD_RIGHT_ARROW = 1U << 6,
    SURF_MAN_HOLD_D = 1U << 7,
    SURF_MAN_HOLD_VERTICAL_NEGATIVE =
        SURF_MAN_HOLD_UP_ARROW | SURF_MAN_HOLD_W,
    SURF_MAN_HOLD_VERTICAL_POSITIVE =
        SURF_MAN_HOLD_DOWN_ARROW | SURF_MAN_HOLD_S,
    SURF_MAN_HOLD_HORIZONTAL_NEGATIVE =
        SURF_MAN_HOLD_LEFT_ARROW | SURF_MAN_HOLD_A,
    SURF_MAN_HOLD_HORIZONTAL_POSITIVE =
        SURF_MAN_HOLD_RIGHT_ARROW | SURF_MAN_HOLD_D,
    SURF_MAN_HOLD_VERTICAL =
        SURF_MAN_HOLD_VERTICAL_NEGATIVE | SURF_MAN_HOLD_VERTICAL_POSITIVE,
    SURF_MAN_HOLD_HORIZONTAL =
        SURF_MAN_HOLD_HORIZONTAL_NEGATIVE | SURF_MAN_HOLD_HORIZONTAL_POSITIVE,
    SURF_MAN_PAUSE_REPEAT_TAP_WINDOW_MS = 200
};

static void surf_man_clear_controls(SurfManApp *app)
{
    (void)memset(&app->controls, 0, sizeof(app->controls));
}

static bool surf_man_size_supported(AFORC_Size size)
{
    return size.width >= SURF_MAN_MIN_COLUMNS &&
           size.height >= SURF_MAN_MIN_ROWS;
}

static uint32_t surf_man_event_codepoint(const AFORC_InputEvent *event)
{
    uint32_t codepoint = event->data.key.codepoint;

    if (codepoint == 0U && event->data.key.key >= AFORC_KEY_A &&
        event->data.key.key <= AFORC_KEY_Z) {
        codepoint = (uint32_t)event->data.key.key;
        if ((event->data.key.modifiers & AFORC_MOD_SHIFT) == 0U) {
            codepoint += (uint32_t)('a' - 'A');
        }
    }
    return codepoint;
}

static bool surf_man_codepoint_is(uint32_t codepoint, char letter)
{
    return codepoint == (uint32_t)letter ||
           codepoint == (uint32_t)(letter - ('a' - 'A'));
}

static uint16_t surf_man_direction_source(const AFORC_InputEvent *event)
{
    const uint32_t codepoint = surf_man_event_codepoint(event);

    if (event->data.key.key == AFORC_KEY_UP) {
        return SURF_MAN_HOLD_UP_ARROW;
    }
    if (surf_man_codepoint_is(codepoint, 'w')) {
        return SURF_MAN_HOLD_W;
    }
    if (event->data.key.key == AFORC_KEY_DOWN) {
        return SURF_MAN_HOLD_DOWN_ARROW;
    }
    if (surf_man_codepoint_is(codepoint, 's')) {
        return SURF_MAN_HOLD_S;
    }
    if (event->data.key.key == AFORC_KEY_LEFT) {
        return SURF_MAN_HOLD_LEFT_ARROW;
    }
    if (surf_man_codepoint_is(codepoint, 'a')) {
        return SURF_MAN_HOLD_A;
    }
    if (event->data.key.key == AFORC_KEY_RIGHT) {
        return SURF_MAN_HOLD_RIGHT_ARROW;
    }
    if (surf_man_codepoint_is(codepoint, 'd')) {
        return SURF_MAN_HOLD_D;
    }
    return 0U;
}

static int8_t surf_man_axis_from_holds(uint16_t holds,
                                       uint16_t negative_holds,
                                       uint16_t positive_holds,
                                       int8_t preference)
{
    const bool negative = (holds & negative_holds) != 0U;
    const bool positive = (holds & positive_holds) != 0U;

    if (negative && positive) {
        return preference > 0 ? 1 : -1;
    }
    if (negative) {
        return -1;
    }
    return positive ? 1 : 0;
}

static void surf_man_refresh_directions(SurfManInputState *controls)
{
    controls->vertical = surf_man_axis_from_holds(
        controls->directional_holds,
        SURF_MAN_HOLD_VERTICAL_NEGATIVE,
        SURF_MAN_HOLD_VERTICAL_POSITIVE,
        controls->vertical_preference);
    controls->horizontal = surf_man_axis_from_holds(
        controls->directional_holds,
        SURF_MAN_HOLD_HORIZONTAL_NEGATIVE,
        SURF_MAN_HOLD_HORIZONTAL_POSITIVE,
        controls->horizontal_preference);
}

static void surf_man_open_pause(SurfManApp *app)
{
    app->overlay = SURF_MAN_OVERLAY_PAUSE;
    app->overlay_return = SURF_MAN_OVERLAY_NONE;
    app->pause_item = SURF_MAN_PAUSE_RESUME;
    surf_man_clear_controls(app);
    surf_man_visuals_mark_dirty(&app->visuals);
}

static bool surf_man_is_interrupt_key(const AFORC_InputEvent *event,
                                      uint32_t codepoint)
{
    return codepoint == UINT32_C(3) ||
           (event->data.key.key == AFORC_KEY_C &&
            (event->data.key.modifiers & AFORC_MOD_CTRL) != 0U);
}

static int surf_man_direction(const AFORC_InputEvent *event,
                              AFORC_Key negative_key,
                              char negative_letter,
                              AFORC_Key positive_key,
                              char positive_letter)
{
    const AFORC_Key key = event->data.key.key;
    const uint32_t codepoint = surf_man_event_codepoint(event);

    if (key == negative_key ||
        surf_man_codepoint_is(codepoint, negative_letter)) {
        return -1;
    }
    if (key == positive_key ||
        surf_man_codepoint_is(codepoint, positive_letter)) {
        return 1;
    }
    return 0;
}

static SurfManInputKey surf_man_normalize_key(
    const AFORC_InputEvent *event)
{
    const uint32_t codepoint = surf_man_event_codepoint(event);

    return (SurfManInputKey){
        .key = event->data.key.key,
        .codepoint = codepoint,
        .vertical = surf_man_direction(event,
                                       AFORC_KEY_UP,
                                       'w',
                                       AFORC_KEY_DOWN,
                                       's'),
        .horizontal = surf_man_direction(event,
                                         AFORC_KEY_LEFT,
                                         'a',
                                         AFORC_KEY_RIGHT,
                                         'd'),
        .repeat = event->data.key.repeat,
        .quit = surf_man_codepoint_is(codepoint, 'q') ||
                surf_man_is_interrupt_key(event, codepoint),
        .pause = surf_man_codepoint_is(codepoint, 'p'),
    };
}

static bool surf_man_accept_pause_press(SurfManApp *app,
                                        const AFORC_InputEvent *event,
                                        bool repeat)
{
    uint64_t elapsed_ms;

    if (!repeat) {
        app->pause_pressed_at_ms = event->timestamp_ms;
        app->pause_repeat_armed = true;
        return true;
    }
    if (!app->pause_repeat_armed ||
        event->timestamp_ms < app->pause_pressed_at_ms) {
        app->pause_repeat_armed = false;
        return false;
    }
    elapsed_ms = event->timestamp_ms - app->pause_pressed_at_ms;
    app->pause_repeat_armed = false;
    return elapsed_ms <= SURF_MAN_PAUSE_REPEAT_TAP_WINDOW_MS;
}

static AFORC_Status surf_man_handle_key_down(SurfManApp *app,
                                              AFORC_Engine *engine,
                                              const AFORC_InputEvent *event)
{
    SurfManInputKey input = surf_man_normalize_key(event);

    if (input.pause) {
        input.pause = surf_man_accept_pause_press(app, event, input.repeat);
    } else {
        app->pause_repeat_armed = false;
    }

    if (app->overlay != SURF_MAN_OVERLAY_NONE) {
        return surf_man_menu_handle_modal_key(app, engine, &input);
    }
    if (input.quit) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (!input.repeat && input.codepoint == (uint32_t)'?') {
        app->overlay = SURF_MAN_OVERLAY_HELP;
        app->overlay_return = SURF_MAN_OVERLAY_NONE;
        surf_man_clear_controls(app);
        surf_man_visuals_mark_dirty(&app->visuals);
        return AFORC_OK;
    }
    if (!input.repeat && input.pause &&
        app->simulation.phase != SURF_MAN_SHACK) {
        surf_man_open_pause(app);
        return AFORC_OK;
    }
    if (app->simulation.phase == SURF_MAN_SHACK) {
        return surf_man_menu_handle_shack_key(app, engine, &input);
    }
    if (!input.repeat && input.key == AFORC_KEY_ESCAPE) {
        if (app->simulation.phase == SURF_MAN_COUNT_IN ||
            app->simulation.phase == SURF_MAN_WAVE_RECAP ||
            app->simulation.phase == SURF_MAN_DAY_RECAP) {
            app->controls.back_latched = true;
        } else {
            surf_man_open_pause(app);
        }
    } else if (input.vertical != 0) {
        const uint16_t source = surf_man_direction_source(event);
        const bool newly_held =
            (app->controls.directional_holds & source) == 0U;

        app->controls.directional_holds |= source;
        if (!input.repeat || newly_held) {
            app->controls.vertical_preference = (int8_t)input.vertical;
        }
        surf_man_refresh_directions(&app->controls);
        if (!input.repeat) {
            app->controls.vertical_tap = (int8_t)input.vertical;
        }
    } else if (input.horizontal != 0) {
        const uint16_t source = surf_man_direction_source(event);
        const bool newly_held =
            (app->controls.directional_holds & source) == 0U;

        app->controls.directional_holds |= source;
        if (!input.repeat || newly_held) {
            app->controls.horizontal_preference = (int8_t)input.horizontal;
        }
        surf_man_refresh_directions(&app->controls);
        if (!input.repeat) {
            app->controls.horizontal_tap = (int8_t)input.horizontal;
        }
    } else if (input.key == AFORC_KEY_SPACE &&
               (app->simulation.phase == SURF_MAN_RIDING ||
                app->simulation.phase == SURF_MAN_PRACTICE)) {
        app->controls.action_lease = SURF_MAN_COMMAND_LEASE_TICKS;
        if (!input.repeat) {
            app->controls.action_tap = true;
        }
    } else if (!input.repeat && input.key == AFORC_KEY_ENTER) {
        app->controls.confirm_latched = true;
    } else if (!input.repeat && input.key == AFORC_KEY_BACKSPACE) {
        app->controls.back_latched = true;
    }
    return AFORC_OK;
}

static void surf_man_handle_key_up(SurfManApp *app,
                                   const AFORC_InputEvent *event)
{
    const uint32_t codepoint = surf_man_event_codepoint(event);
    const uint16_t source = surf_man_direction_source(event);

    app->controls.directional_holds &= (uint16_t)~source;
    surf_man_refresh_directions(&app->controls);
    if (event->data.key.key == AFORC_KEY_P ||
        surf_man_codepoint_is(codepoint, 'p')) {
        app->pause_repeat_armed = false;
    }
    if (event->data.key.key == AFORC_KEY_SPACE || codepoint == (uint32_t)' ') {
        app->controls.action_lease = 0U;
    }
}

static void surf_man_handle_resize(SurfManApp *app, AFORC_Size size)
{
    const bool was_resize = app->overlay == SURF_MAN_OVERLAY_RESIZE;
    const bool return_to_pause =
        app->overlay == SURF_MAN_OVERLAY_PAUSE ||
        app->overlay_return == SURF_MAN_OVERLAY_PAUSE ||
        (!app->focused && app->simulation.phase != SURF_MAN_SHACK);

    app->terminal_size = size;
    surf_man_clear_controls(app);
    app->pause_repeat_armed = false;
    if (!surf_man_size_supported(size)) {
        app->overlay = SURF_MAN_OVERLAY_RESIZE;
        app->overlay_return = return_to_pause ? SURF_MAN_OVERLAY_PAUSE
                                              : SURF_MAN_OVERLAY_NONE;
        surf_man_set_message(app,
                             "Resize terminal to at least %dx%d.",
                             SURF_MAN_MIN_COLUMNS,
                             SURF_MAN_MIN_ROWS);
    } else if (was_resize) {
        app->overlay = return_to_pause ? SURF_MAN_OVERLAY_PAUSE
                                       : SURF_MAN_OVERLAY_NONE;
        app->overlay_return = SURF_MAN_OVERLAY_NONE;
        if (app->overlay == SURF_MAN_OVERLAY_PAUSE) {
            app->pause_item = SURF_MAN_PAUSE_RESUME;
        }
        surf_man_set_message(app, "Terminal size restored.");
    }
    surf_man_visuals_mark_dirty(&app->visuals);
}

AFORC_Status surf_man_app_handle_event(SurfManApp *app,
                                       AFORC_Engine *engine,
                                       const AFORC_InputEvent *event)
{
    if (app == NULL || engine == NULL || event == NULL || !app->initialized) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    switch (event->type) {
        case AFORC_INPUT_EVENT_KEY_DOWN:
            return app->focused
                       ? surf_man_handle_key_down(app, engine, event)
                       : AFORC_OK;
        case AFORC_INPUT_EVENT_KEY_UP:
            surf_man_handle_key_up(app, event);
            break;
        case AFORC_INPUT_EVENT_RESIZE:
            surf_man_handle_resize(app, event->data.resize.size);
            break;
        case AFORC_INPUT_EVENT_FOCUS_OUT:
            app->focused = false;
            surf_man_clear_controls(app);
            app->pause_repeat_armed = false;
            if (app->overlay == SURF_MAN_OVERLAY_NONE &&
                app->simulation.phase != SURF_MAN_SHACK) {
                app->overlay = SURF_MAN_OVERLAY_PAUSE;
                app->overlay_return = SURF_MAN_OVERLAY_NONE;
                app->pause_item = SURF_MAN_PAUSE_RESUME;
            }
            if (app->overlay != SURF_MAN_OVERLAY_RESIZE) {
                surf_man_set_message(app, "Paused: terminal focus lost.");
            }
            surf_man_visuals_mark_dirty(&app->visuals);
            break;
        case AFORC_INPUT_EVENT_FOCUS_IN:
            app->focused = true;
            surf_man_visuals_mark_dirty(&app->visuals);
            break;
        default:
            break;
    }
    return AFORC_OK;
}

void surf_man_input_take_command(SurfManApp *app,
                                 SurfManCommand *out_command)
{
    if (out_command == NULL) {
        return;
    }
    surf_man_command_clear(out_command);
    if (app == NULL || !app->initialized) {
        return;
    }

    out_command->vertical = app->controls.vertical_tap != 0
                                ? app->controls.vertical_tap
                                : app->controls.vertical;
    out_command->horizontal = app->controls.horizontal_tap != 0
                                  ? app->controls.horizontal_tap
                                  : app->controls.horizontal;
    out_command->action = app->controls.action_tap ||
                          app->controls.action_lease > 0U;
    out_command->confirm = app->controls.confirm_latched;
    out_command->back = app->controls.back_latched;

    app->controls.vertical_tap = 0;
    app->controls.horizontal_tap = 0;
    app->controls.action_tap = false;
    app->controls.confirm_latched = false;
    app->controls.back_latched = false;
    if (app->controls.action_lease > 0U) {
        --app->controls.action_lease;
    }
}
