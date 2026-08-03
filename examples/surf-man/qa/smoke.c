/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"
#include "surf_man/qa.h"

#include <string.h>

enum { SURF_MAN_QA_LIP_SEARCH_STEPS = 1024 };

static AFORC_Status surf_man_qa_smoke_error(AFORC_Error *error,
                                            AFORC_Status status,
                                            const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static AFORC_Status surf_man_qa_send_timed_key(SurfManApp *app,
                                               AFORC_Engine *engine,
                                               AFORC_InputEventType type,
                                               AFORC_Key key,
                                               uint32_t codepoint,
                                               bool repeat,
                                               uint64_t timestamp_ms) {
    AFORC_InputEvent event = {0};

    event.type = type;
    event.timestamp_ms = timestamp_ms;
    event.data.key.key = key;
    event.data.key.codepoint = codepoint;
    event.data.key.repeat = repeat;
    return surf_man_app_handle_event(app, engine, &event);
}

static AFORC_Status surf_man_qa_send_key(SurfManApp *app,
                                         AFORC_Engine *engine,
                                         AFORC_InputEventType type,
                                         AFORC_Key key,
                                         uint32_t codepoint,
                                         bool repeat) {
    return surf_man_qa_send_timed_key(
        app, engine, type, key, codepoint, repeat, 0U);
}

static AFORC_Status surf_man_qa_command_checks(SurfManApp *app,
                                               AFORC_Engine *engine,
                                               AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManOverlay saved_overlay = app->overlay;
    const uint64_t saved_pause_pressed_at_ms = app->pause_pressed_at_ms;
    const bool saved_pause_repeat_armed = app->pause_repeat_armed;
    const bool saved_focused = app->focused;
    const bool saved_dirty = app->visuals.dirty;
    SurfManCommand first;
    SurfManCommand second;
    SurfManCommand third;
    AFORC_InputEvent event = {0};
    AFORC_Status status = AFORC_OK;

    app->controls = (SurfManInputState){0};
    app->controls.vertical = 1;
    app->controls.horizontal = -1;
    app->controls.confirm_latched = true;
    app->controls.back_latched = true;
    surf_man_input_take_command(app, &first);
    surf_man_input_take_command(app, &second);
    surf_man_input_take_command(app, &third);
    if (first.vertical != 1 || first.horizontal != -1 || first.action ||
        !first.confirm || !first.back || second.vertical != 1 ||
        second.horizontal != -1 || second.action || second.confirm ||
        second.back || third.vertical != 1 || third.horizontal != -1 ||
        third.action || third.confirm || third.back) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        app->simulation.phase = SURF_MAN_RIDING;
        app->overlay = SURF_MAN_OVERLAY_NONE;
        app->focused = true;
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_LEFT;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        surf_man_input_take_command(app, &second);
        if (first.horizontal != -1 || second.horizontal != 0) {
            status = AFORC_ERROR_STATE;
        }
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        surf_man_input_take_command(app, &second);
        if (!first.action || second.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        surf_man_input_take_command(app, &first);
        if (!first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        if (first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        surf_man_input_take_command(app, &first);
        if (!first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.type = AFORC_INPUT_EVENT_KEY_UP;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &first);
        if (first.action) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_P;
        event.data.key.codepoint = (uint32_t)'p';
        status = surf_man_app_handle_event(app, engine, &event);
        if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_app_handle_event(app, engine, &event);
        if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_NONE) {
            status = AFORC_ERROR_STATE;
        }
    }

    app->controls = saved_controls;
    app->simulation = saved_simulation;
    app->overlay = saved_overlay;
    app->pause_pressed_at_ms = saved_pause_pressed_at_ms;
    app->pause_repeat_armed = saved_pause_repeat_armed;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "input taps, leases, releases, or one-shot controls were incorrect");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_repeat_navigation_checks(
    SurfManApp *app,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManSettings saved_settings = app->settings;
    const SurfManOverlay saved_overlay = app->overlay;
    const SurfManOverlay saved_overlay_return = app->overlay_return;
    const SurfManMenuItem saved_menu_item = app->menu_item;
    const SurfManPauseItem saved_pause_item = app->pause_item;
    const SurfManAccessibilityItem saved_accessibility_item =
        app->accessibility_item;
    const uint64_t saved_pause_pressed_at_ms = app->pause_pressed_at_ms;
    const bool saved_pause_repeat_armed = app->pause_repeat_armed;
    const bool saved_focused = app->focused;
    const bool saved_dirty = app->visuals.dirty;
    AFORC_Status status;

    app->controls = (SurfManInputState){0};
    app->simulation.phase = SURF_MAN_SHACK;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->menu_item = SURF_MAN_MENU_SURF;
    app->focused = true;
    status = surf_man_qa_send_key(app,
                                  engine,
                                  AFORC_INPUT_EVENT_KEY_DOWN,
                                  AFORC_KEY_DOWN,
                                  0U,
                                  false);
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      true);
    }
    if (status == AFORC_OK && app->menu_item != SURF_MAN_MENU_HELP) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->simulation.phase = SURF_MAN_RIDING;
        app->overlay = SURF_MAN_OVERLAY_PAUSE;
        app->pause_item = SURF_MAN_PAUSE_RESUME;
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      true);
    }
    if (status == AFORC_OK &&
        app->pause_item != SURF_MAN_PAUSE_ACCESSIBILITY) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->overlay = SURF_MAN_OVERLAY_NONE;
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_P,
                                            (uint32_t)'p',
                                            false,
                                            UINT64_C(1000));
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_P,
                                            (uint32_t)'p',
                                            true,
                                            UINT64_C(1100));
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_NONE) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_P,
                                            (uint32_t)'p',
                                            false,
                                            UINT64_C(2000));
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_P,
                                            (uint32_t)'p',
                                            true,
                                            UINT64_C(2300));
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_NONE,
                                            (uint32_t)'?',
                                            true,
                                            UINT64_C(2350));
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        app->overlay = SURF_MAN_OVERLAY_HELP;
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_ENTER,
                                            0U,
                                            true,
                                            UINT64_C(2400));
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_HELP) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        app->overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_ESCAPE,
                                            0U,
                                            true,
                                            UINT64_C(2450));
    }
    if (status == AFORC_OK &&
        app->overlay != SURF_MAN_OVERLAY_ACCESSIBILITY) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        app->accessibility_item = SURF_MAN_ACCESSIBILITY_SPEED;
        app->settings.speed_percent = 100U;
        app->simulation.settings = app->settings;
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_RIGHT,
                                            0U,
                                            true,
                                            UINT64_C(2500));
    }
    if (status == AFORC_OK && app->settings.speed_percent != 100U) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_timed_key(app,
                                            engine,
                                            AFORC_INPUT_EVENT_KEY_DOWN,
                                            AFORC_KEY_RIGHT,
                                            0U,
                                            false,
                                            UINT64_C(2550));
    }
    if (status == AFORC_OK && app->settings.speed_percent != 75U) {
        status = AFORC_ERROR_STATE;
    }

    app->controls = saved_controls;
    app->simulation = saved_simulation;
    app->settings = saved_settings;
    app->overlay = saved_overlay;
    app->overlay_return = saved_overlay_return;
    app->menu_item = saved_menu_item;
    app->pause_item = saved_pause_item;
    app->accessibility_item = saved_accessibility_item;
    app->pause_pressed_at_ms = saved_pause_pressed_at_ms;
    app->pause_repeat_armed = saved_pause_repeat_armed;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "legacy repeat navigation or one-shot repeat filtering was incorrect");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_direction_alias_checks(SurfManApp *app,
                                                        AFORC_Engine *engine,
                                                        AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManOverlay saved_overlay = app->overlay;
    const bool saved_focused = app->focused;
    SurfManCommand command;
    AFORC_Status status = AFORC_OK;

    app->controls = (SurfManInputState){0};
    app->simulation.phase = SURF_MAN_RIDING;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->focused = true;
    status = surf_man_qa_send_key(app,
                                  engine,
                                  AFORC_INPUT_EVENT_KEY_DOWN,
                                  AFORC_KEY_A,
                                  (uint32_t)'a',
                                  false);
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_LEFT,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_UP,
                                      AFORC_KEY_LEFT,
                                      0U,
                                      false);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK &&
         tick < (uint32_t)SURF_MAN_COMMAND_LEASE_TICKS + 2U;
         ++tick) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != -1) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_UP,
                                      AFORC_KEY_A,
                                      (uint32_t)'a',
                                      false);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != 0) {
            status = AFORC_ERROR_STATE;
        }
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_LEFT,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != -1) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_RIGHT,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != 1) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_UP,
                                      AFORC_KEY_RIGHT,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != -1) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_UP,
                                      AFORC_KEY_LEFT,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        surf_man_input_take_command(app, &command);
        if (command.horizontal != 0) {
            status = AFORC_ERROR_STATE;
        }
    }

    app->controls = saved_controls;
    app->simulation = saved_simulation;
    app->overlay = saved_overlay;
    app->focused = saved_focused;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "direction aliases or opposite-key releases lost held input");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_pause_menu_checks(SurfManApp *app,
                                                   AFORC_Engine *engine,
                                                   AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManOverlay saved_overlay = app->overlay;
    const SurfManOverlay saved_overlay_return = app->overlay_return;
    const SurfManMenuItem saved_menu_item = app->menu_item;
    const SurfManPauseItem saved_pause_item = app->pause_item;
    const SurfManAccessibilityItem saved_accessibility_item =
        app->accessibility_item;
    const uint64_t saved_pause_pressed_at_ms = app->pause_pressed_at_ms;
    const bool saved_pause_repeat_armed = app->pause_repeat_armed;
    const bool saved_focused = app->focused;
    const char *failure = "pause menu input returned an error";
    uint64_t frame_index;
    AFORC_Status status;

    app->controls = (SurfManInputState){0};
    app->simulation.phase = SURF_MAN_RIDING;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->focused = true;
    status = surf_man_qa_send_key(app,
                                  engine,
                                  AFORC_INPUT_EVENT_KEY_DOWN,
                                  AFORC_KEY_P,
                                  (uint32_t)'p',
                                  false);
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_NONE,
                                      (uint32_t)'?',
                                      false);
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_HELP) {
        status = AFORC_ERROR_STATE;
        failure = "Pause Help action did not open Help";
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_ESCAPE,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
        status = AFORC_ERROR_STATE;
        failure = "closing Help resumed instead of returning to Pause";
    }

    for (uint32_t press = 0U; status == AFORC_OK && press < 2U; ++press) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_ENTER,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK &&
        app->overlay != SURF_MAN_OVERLAY_ACCESSIBILITY) {
        status = AFORC_ERROR_STATE;
        failure = "Pause Accessibility action did not open Accessibility";
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_ESCAPE,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
        status = AFORC_ERROR_STATE;
        failure = "closing Accessibility resumed instead of returning to Pause";
    }

    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&app->simulation, true);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&app->simulation);
    }
    if (status == AFORC_OK) {
        app->simulation.phase = SURF_MAN_PRACTICE;
        app->simulation.pending_score = 250U;
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_ENTER,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK &&
        (app->overlay != SURF_MAN_OVERLAY_NONE ||
         app->simulation.phase != SURF_MAN_SHACK || app->simulation.practice ||
         app->simulation.wave != 0U || app->simulation.day_score != 0U ||
         app->simulation.pending_score != 0U)) {
        status = AFORC_ERROR_STATE;
        failure = "Practice End Session retained active session telemetry";
    }

    if (status == AFORC_OK) {
        app->simulation.phase = SURF_MAN_RIDING;
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_P,
                                      (uint32_t)'p',
                                      false);
    }
    for (uint32_t press = 0U; status == AFORC_OK && press < 4U; ++press) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_DOWN,
                                      0U,
                                      false);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_send_key(app,
                                      engine,
                                      AFORC_INPUT_EVENT_KEY_DOWN,
                                      AFORC_KEY_ENTER,
                                      0U,
                                      false);
    }
    frame_index = aforc_engine_frame_index(engine);
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, UINT64_C(1000000000), error);
    }
    if (status == AFORC_OK &&
        aforc_engine_frame_index(engine) != frame_index) {
        status = AFORC_ERROR_STATE;
        failure = "Pause Quit action did not request engine shutdown";
    }

    app->controls = saved_controls;
    app->simulation = saved_simulation;
    app->overlay = saved_overlay;
    app->overlay_return = saved_overlay_return;
    app->menu_item = saved_menu_item;
    app->pause_item = saved_pause_item;
    app->accessibility_item = saved_accessibility_item;
    app->pause_pressed_at_ms = saved_pause_pressed_at_ms;
    app->pause_repeat_armed = saved_pause_repeat_armed;
    app->focused = saved_focused;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            failure);
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_seek_safe_lip(
    SurfManSimulation *simulation) {
    const int32_t search_step_q16 = SURF_MAN_Q16_ONE / 4;

    for (uint32_t step = 0U; step < SURF_MAN_QA_LIP_SEARCH_STEPS; ++step) {
        SurfManWaveSample sample;
        AFORC_Status status;

        simulation->distance_q16 = (int32_t)step * search_step_q16;
        status = surf_man_wave_sample(
            simulation, simulation->line_position_q16, &sample);
        if (status != AFORC_OK) {
            return status;
        }
        if (sample.lip && !sample.hazard && !sample.tube) {
            return AFORC_OK;
        }
    }
    return AFORC_ERROR_NOT_FOUND;
}

static AFORC_Status surf_man_qa_prepare_action_probe(
    SurfManApp *probe,
    const SurfManApp *app) {
    AFORC_Status status;

    probe->controls = (SurfManInputState){0};
    status = surf_man_simulation_init(
        &probe->simulation, app->seed, &probe->settings);
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_day(&probe->simulation, false);
    }
    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&probe->simulation);
    }
    if (status == AFORC_OK) {
        probe->simulation.phase = SURF_MAN_RIDING;
        probe->simulation.phase_tick = 0U;
        probe->simulation.wave_kind = SURF_MAN_WAVE_STEEP;
        status = surf_man_qa_seek_safe_lip(&probe->simulation);
    }
    return status;
}

static AFORC_Status surf_man_qa_action_transition_checks(
    const SurfManApp *app,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    SurfManApp probe = {0};
    AFORC_InputEvent event = {0};
    uint32_t initial_maneuver_count = 0U;
    uint32_t snapped_maneuver_count = 0U;
    uint64_t snapped_score = 0U;
    bool visuals_initialized = false;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.settings = surf_man_settings_default();
    probe.settings.reduced_motion = true;
    probe.overlay = SURF_MAN_OVERLAY_NONE;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;
    status = surf_man_qa_prepare_action_probe(&probe, app);
    if (status == AFORC_OK) {
        status = surf_man_visuals_init(&probe.visuals, UINT32_C(0xa17e57));
        visuals_initialized = status == AFORC_OK;
    }
    if (status == AFORC_OK) {
        probe.simulation.wave_face_offset_q16 =
            (probe.simulation.rules.air_face_threshold_q16 +
             probe.simulation.rules.hazard_face_threshold_q16) /
            2;
        initial_maneuver_count = probe.simulation.maneuver_count;
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.airborne ||
         probe.simulation.last_maneuver != SURF_MAN_MANEUVER_LIP_SNAP ||
         probe.simulation.maneuver_count != initial_maneuver_count + 1U ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        snapped_maneuver_count = probe.simulation.maneuver_count;
        snapped_score = probe.simulation.pending_score;
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_COMMAND_LEASE_TICKS;
         ++tick) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
        if (status == AFORC_OK &&
            (probe.simulation.maneuver_count != snapped_maneuver_count ||
             probe.simulation.pending_score != snapped_score)) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_seek_safe_lip(&probe.simulation);
    }
    if (status == AFORC_OK) {
        probe.simulation.wave_face_offset_q16 =
            (probe.simulation.rules.air_face_threshold_q16 +
             probe.simulation.rules.hazard_face_threshold_q16) /
            2;
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.airborne ||
         probe.simulation.last_maneuver != SURF_MAN_MANEUVER_LIP_SNAP ||
         probe.simulation.maneuver_count != snapped_maneuver_count + 1U ||
         probe.simulation.pending_score <= snapped_score ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        status = surf_man_qa_prepare_action_probe(&probe, app);
    }
    if (status == AFORC_OK) {
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (!probe.simulation.airborne || probe.simulation.grabbed ||
         probe.controls.action_lease != 0U || probe.controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (!probe.simulation.airborne || probe.simulation.grabbed)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(&probe, engine, &event);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK && !probe.simulation.grabbed) {
        status = AFORC_ERROR_STATE;
    }

    if (visuals_initialized) {
        surf_man_visuals_dispose(&probe.visuals);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "snap or launch action leaked across fixed ticks");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_visual_timing_checks(SurfManApp *app,
                                                      AFORC_Engine *engine,
                                                      AFORC_Error *error) {
    SurfManApp probe = {0};
    AFORC_ParticleDesc particle = {0};
    size_t particle_index = 0U;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.simulation = app->simulation;
    probe.simulation.phase = SURF_MAN_SHACK;
    probe.settings = surf_man_settings_default();
    probe.settings.reduced_motion = true;
    probe.simulation.settings = probe.settings;
    probe.overlay = SURF_MAN_OVERLAY_NONE;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;

    status = surf_man_visuals_init(&probe.visuals, UINT32_C(0x51f15e));
    if (status == AFORC_OK) {
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK && probe.visuals.dirty) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_COUNT_IN;
        probe.simulation.phase_tick = 0U;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || !probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_WAVE_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.controls = (SurfManInputState){0};
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_DAY_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase_tick != 1U || probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.simulation.phase = SURF_MAN_WAVE_RECAP;
        probe.simulation.phase_tick = 0U;
        probe.simulation.wave = SURF_MAN_WAVES_PER_DAY;
        probe.controls.confirm_latched = true;
        probe.visuals.dirty = false;
        status = surf_man_scene_fixed_update(
            &probe.scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (probe.simulation.phase != SURF_MAN_DAY_RECAP ||
         !probe.visuals.dirty)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        probe.settings.reduced_motion = false;
        probe.simulation.settings = probe.settings;
        probe.simulation.phase = SURF_MAN_RIDING;
        probe.visuals.visual_tick = 0U;
        probe.visuals.dirty = false;
        particle.lifetime_ms = 2000U;
        particle.cell = aforc_cell_default();
        status = aforc_particle_pool_spawn(&probe.visuals.particle_pool,
                                           &particle,
                                           &particle_index);
    }
    for (uint32_t tick = 0U;
         status == AFORC_OK && tick < SURF_MAN_VISUAL_HZ;
         ++tick) {
        status = surf_man_visuals_step(&probe);
    }
    if (status == AFORC_OK &&
        (!probe.visuals.particles[particle_index].active ||
         probe.visuals.particles[particle_index].age_ms != 1000U)) {
        status = AFORC_ERROR_STATE;
    }
    surf_man_visuals_dispose(&probe.visuals);
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "visual timing, recap dirtiness, or reduced-motion tracking was incorrect");
    }
    return AFORC_OK;
}

static bool surf_man_qa_renderer_uses_color_mode(
    const AFORC_Renderer *renderer,
    AFORC_ColorMode mode) {
    const AFORC_Size size = aforc_renderer_size(renderer);

    for (int32_t y = 0; y < size.height; ++y) {
        for (int32_t x = 0; x < size.width; ++x) {
            AFORC_Cell cell;

            if (aforc_renderer_get(
                    renderer, (AFORC_Point){x, y}, &cell) != AFORC_OK ||
                cell.foreground.mode != mode || cell.background.mode != mode) {
                return false;
            }
        }
    }
    return true;
}

static AFORC_Status surf_man_qa_color_particle_checks(
    const SurfManApp *app,
    AFORC_Engine *engine,
    AFORC_Error *error) {
    SurfManApp probe = {0};
    SurfManInputKey input = {0};
    AFORC_ParticleDesc particle = {0};
    bool visuals_initialized = false;
    AFORC_Status status;

    probe.scene.vtable = &surf_man_scene_vtable;
    probe.scene.user_data = &probe;
    probe.renderer = app->renderer;
    probe.simulation = app->simulation;
    probe.simulation.phase = SURF_MAN_RIDING;
    probe.settings = surf_man_settings_default();
    probe.settings.color_mode = SURF_MAN_COLOR_HIGH_CONTRAST;
    probe.simulation.settings = probe.settings;
    probe.overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
    probe.accessibility_item = SURF_MAN_ACCESSIBILITY_COLOR;
    probe.terminal_size = (AFORC_Size){SURF_MAN_TARGET_COLUMNS,
                                      SURF_MAN_TARGET_ROWS};
    probe.focused = true;
    probe.initialized = true;

    status = surf_man_visuals_init(&probe.visuals, UINT32_C(0xc0104));
    visuals_initialized = status == AFORC_OK;
    particle.position.x =
        (SURF_MAN_TARGET_COLUMNS - 10) * AFORC_EFFECT_FIXED_ONE;
    particle.position.y = AFORC_EFFECT_FIXED_ONE;
    particle.lifetime_ms = 1000U;
    particle.cell = surf_man_cell(
        (uint32_t)'*', UINT8_C(77), AFORC_STYLE_BOLD);
    if (status == AFORC_OK) {
        status = aforc_particle_pool_spawn(
            &probe.visuals.particle_pool, &particle, NULL);
    }
    input.horizontal = 1;
    if (status == AFORC_OK) {
        status = surf_man_menu_handle_modal_key(&probe, engine, &input);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_frame(&probe, 0.0, error);
    }
    if (status == AFORC_OK &&
        (probe.settings.color_mode != SURF_MAN_COLOR_NONE ||
         probe.visuals.particle_pool.active_count != 0U ||
         !surf_man_qa_renderer_uses_color_mode(
             probe.renderer, AFORC_COLOR_DEFAULT))) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        status = surf_man_menu_handle_modal_key(&probe, engine, &input);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_frame(&probe, 0.0, error);
    }
    if (status == AFORC_OK &&
        (probe.settings.color_mode != SURF_MAN_COLOR_STANDARD ||
         !surf_man_qa_renderer_uses_color_mode(
             probe.renderer, AFORC_COLOR_INDEXED))) {
        status = AFORC_ERROR_STATE;
    }

    if (visuals_initialized) {
        surf_man_visuals_dispose(&probe.visuals);
    }
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "color-mode transition retained stale particles or cell colors");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_pause_checks(SurfManApp *app,
                                              AFORC_Engine *engine,
                                             AFORC_Error *error) {
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManInputState saved_controls = app->controls;
    const SurfManOverlay saved_overlay = app->overlay;
    const SurfManOverlay saved_overlay_return = app->overlay_return;
    const AFORC_Size saved_size = app->terminal_size;
    const bool saved_focused = app->focused;
    const bool saved_dirty = app->visuals.dirty;
    char saved_message[SURF_MAN_MESSAGE_CAPACITY];
    AFORC_InputEvent event = {0};
    uint64_t paused_hash;
    AFORC_Status status;

    (void)memcpy(saved_message, app->message, sizeof(saved_message));
    app->simulation.phase = SURF_MAN_RIDING;
    app->overlay = SURF_MAN_OVERLAY_NONE;
    app->focused = true;
    app->controls = (SurfManInputState){
        .vertical = 1,
        .horizontal = -1,
        .action_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .action_tap = true,
    };
    event.type = AFORC_INPUT_EVENT_FOCUS_OUT;
    status = surf_man_app_handle_event(app, engine, &event);
    paused_hash = surf_man_simulation_hash(&app->simulation);
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &app->scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
         (app->focused || app->overlay != SURF_MAN_OVERLAY_PAUSE ||
          surf_man_simulation_hash(&app->simulation) != paused_hash ||
          app->controls.vertical != 0 || app->controls.horizontal != 0 ||
         app->controls.action_lease != 0U || app->controls.action_tap)) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->focused = true;
        app->overlay = SURF_MAN_OVERLAY_NONE;
        event = (AFORC_InputEvent){0};
        event.type = AFORC_INPUT_EVENT_RESIZE;
        event.data.resize.size = (AFORC_Size){
            SURF_MAN_MIN_COLUMNS - 1,
            SURF_MAN_MIN_ROWS - 1,
        };
        status = surf_man_app_handle_event(app, engine, &event);
        paused_hash = surf_man_simulation_hash(&app->simulation);
    }
    if (status == AFORC_OK) {
        status = surf_man_scene_fixed_update(
            &app->scene,
            engine,
            1.0 / (double)SURF_MAN_FIXED_HZ,
            error);
    }
    if (status == AFORC_OK &&
        (app->overlay != SURF_MAN_OVERLAY_RESIZE ||
         surf_man_simulation_hash(&app->simulation) != paused_hash)) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        app->overlay = SURF_MAN_OVERLAY_HELP;
        app->overlay_return = SURF_MAN_OVERLAY_PAUSE;
        event.data.resize.size = (AFORC_Size){
            SURF_MAN_MIN_COLUMNS - 1,
            SURF_MAN_MIN_ROWS - 1,
        };
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK) {
        event.data.resize.size = (AFORC_Size){
            SURF_MAN_MIN_COLUMNS,
            SURF_MAN_MIN_ROWS,
        };
        status = surf_man_app_handle_event(app, engine, &event);
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_PAUSE) {
        status = AFORC_ERROR_STATE;
    }

    app->simulation = saved_simulation;
    app->controls = saved_controls;
    app->overlay = saved_overlay;
    app->overlay_return = saved_overlay_return;
    app->terminal_size = saved_size;
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    (void)memcpy(app->message, saved_message, sizeof(saved_message));
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "focus loss or undersized resize advanced authoritative state");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_offscreen_scene_checks(SurfManApp *app,
                                                       AFORC_Engine *engine,
                                                       AFORC_Error *error) {
    AFORC_Status status;

    if (!app->smoke || app->terminal != NULL ||
        aforc_engine_scene(engine) != &app->scene ||
        app->scene.vtable != &surf_man_scene_vtable ||
        app->scene.user_data != app) {
        return surf_man_qa_smoke_error(
            error,
            AFORC_ERROR_STATE,
            "smoke mode did not use the real app scene off-screen");
    }
    status = surf_man_scene_render(&app->scene, engine, 0.0, error);
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error, status, "off-screen real-scene render failed");
    }
    return AFORC_OK;
}

AFORC_Status surf_man_smoke_checks(SurfManApp *app,
                                   AFORC_Engine *engine,
                                   AFORC_Error *error) {
    AFORC_Status status;

    if (app == NULL || engine == NULL || !app->initialized) {
        return surf_man_qa_smoke_error(
            error, AFORC_ERROR_INVALID_ARGUMENT, "invalid smoke QA context");
    }
    status = surf_man_simulation_checks(app->seed, error);
    if (status == AFORC_OK) {
        status = surf_man_qa_command_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_repeat_navigation_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_direction_alias_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_pause_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_visual_timing_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_action_transition_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_color_particle_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_offscreen_scene_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_checks(app, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_pause_menu_checks(app, engine, error);
    }
    return status;
}
