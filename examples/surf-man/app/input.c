/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"

#include <string.h>

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

static AFORC_Status surf_man_handle_key_down(SurfManApp *app,
                                              AFORC_Engine *engine,
                                              const AFORC_InputEvent *event)
{
    const SurfManInputKey input = surf_man_normalize_key(event);

    if (app->overlay != SURF_MAN_OVERLAY_NONE) {
        return input.repeat
                   ? AFORC_OK
                   : surf_man_menu_handle_modal_key(app, engine, &input);
    }
    if (input.quit) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (!input.repeat && input.codepoint == (uint32_t)'?') {
        app->overlay = SURF_MAN_OVERLAY_HELP;
        surf_man_clear_controls(app);
        surf_man_visuals_mark_dirty(&app->visuals);
        return AFORC_OK;
    }
    if (!input.repeat && input.pause &&
        app->simulation.phase != SURF_MAN_SHACK) {
        app->overlay = SURF_MAN_OVERLAY_PAUSE;
        surf_man_clear_controls(app);
        surf_man_visuals_mark_dirty(&app->visuals);
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
            app->overlay = SURF_MAN_OVERLAY_PAUSE;
            surf_man_clear_controls(app);
            surf_man_visuals_mark_dirty(&app->visuals);
        }
    } else if (input.vertical != 0) {
        app->controls.vertical = (int8_t)input.vertical;
        app->controls.vertical_lease = SURF_MAN_COMMAND_LEASE_TICKS;
    } else if (input.horizontal != 0) {
        app->controls.horizontal = (int8_t)input.horizontal;
        app->controls.horizontal_lease = SURF_MAN_COMMAND_LEASE_TICKS;
    } else if (!input.repeat && input.key == AFORC_KEY_SPACE) {
        app->controls.action_latched = true;
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
    const int vertical = surf_man_direction(event,
                                             AFORC_KEY_UP,
                                             'w',
                                             AFORC_KEY_DOWN,
                                             's');
    const int horizontal = surf_man_direction(event,
                                               AFORC_KEY_LEFT,
                                               'a',
                                               AFORC_KEY_RIGHT,
                                               'd');

    if (vertical != 0 && app->controls.vertical == vertical) {
        app->controls.vertical = 0;
        app->controls.vertical_lease = 0U;
    }
    if (horizontal != 0 && app->controls.horizontal == horizontal) {
        app->controls.horizontal = 0;
        app->controls.horizontal_lease = 0U;
    }
}

static void surf_man_handle_resize(SurfManApp *app, AFORC_Size size)
{
    const bool was_resize = app->overlay == SURF_MAN_OVERLAY_RESIZE;

    app->terminal_size = size;
    surf_man_clear_controls(app);
    if (!surf_man_size_supported(size)) {
        app->overlay = SURF_MAN_OVERLAY_RESIZE;
        surf_man_set_message(app,
                             "Resize terminal to at least %dx%d.",
                             SURF_MAN_MIN_COLUMNS,
                             SURF_MAN_MIN_ROWS);
    } else if (was_resize) {
        app->overlay = !app->focused &&
                               app->simulation.phase != SURF_MAN_SHACK
                           ? SURF_MAN_OVERLAY_PAUSE
                           : SURF_MAN_OVERLAY_NONE;
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
            if (app->overlay == SURF_MAN_OVERLAY_NONE &&
                app->simulation.phase != SURF_MAN_SHACK) {
                app->overlay = SURF_MAN_OVERLAY_PAUSE;
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

    out_command->vertical = app->controls.vertical;
    out_command->horizontal = app->controls.horizontal;
    out_command->action = app->controls.action_latched;
    out_command->confirm = app->controls.confirm_latched;
    out_command->back = app->controls.back_latched;

    app->controls.action_latched = false;
    app->controls.confirm_latched = false;
    app->controls.back_latched = false;
    if (app->controls.vertical_lease > 0U &&
        --app->controls.vertical_lease == 0U) {
        app->controls.vertical = 0;
    }
    if (app->controls.horizontal_lease > 0U &&
        --app->controls.horizontal_lease == 0U) {
        app->controls.horizontal = 0;
    }
}
