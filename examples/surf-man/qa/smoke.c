/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man_internal.h"

#include <string.h>

static AFORC_Status surf_man_qa_smoke_error(AFORC_Error *error,
                                            AFORC_Status status,
                                            const char *message) {
    aforc_error_set(error, status, "surf-man qa", "%s", message);
    return status;
}

static AFORC_Status surf_man_qa_command_checks(SurfManApp *app,
                                               AFORC_Engine *engine,
                                               AFORC_Error *error) {
    const SurfManInputState saved_controls = app->controls;
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManOverlay saved_overlay = app->overlay;
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
    app->controls.vertical_lease = 2U;
    app->controls.horizontal_lease = 1U;
    app->controls.action_latched = true;
    app->controls.confirm_latched = true;
    app->controls.back_latched = true;
    surf_man_input_take_command(app, &first);
    surf_man_input_take_command(app, &second);
    surf_man_input_take_command(app, &third);
    if (first.vertical != 1 || first.horizontal != -1 || !first.action ||
        !first.confirm || !first.back || second.vertical != 1 ||
        second.horizontal != 0 || second.action || second.confirm ||
        second.back || third.vertical != 0 || third.horizontal != 0 ||
        third.action || third.confirm || third.back) {
        status = AFORC_ERROR_STATE;
    }

    if (status == AFORC_OK) {
        app->controls = (SurfManInputState){0};
        app->simulation.phase = SURF_MAN_RIDING;
        app->overlay = SURF_MAN_OVERLAY_NONE;
        app->focused = true;
        event.type = AFORC_INPUT_EVENT_KEY_DOWN;
        event.data.key.key = AFORC_KEY_SPACE;
        event.data.key.repeat = true;
        status = surf_man_app_handle_event(app, engine, &event);
        if (status == AFORC_OK && app->controls.action_latched) {
            status = AFORC_ERROR_STATE;
        }
    }
    if (status == AFORC_OK) {
        event.data.key.repeat = false;
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
    app->focused = saved_focused;
    app->visuals.dirty = saved_dirty;
    if (status != AFORC_OK) {
        return surf_man_qa_smoke_error(
            error,
            status,
            "input leases or one-shot latches were dispatched more than once");
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_qa_pause_checks(SurfManApp *app,
                                             AFORC_Engine *engine,
                                             AFORC_Error *error) {
    const SurfManSimulation saved_simulation = app->simulation;
    const SurfManInputState saved_controls = app->controls;
    const SurfManOverlay saved_overlay = app->overlay;
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
        .vertical_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .horizontal_lease = SURF_MAN_COMMAND_LEASE_TICKS,
        .action_latched = true,
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
         app->controls.action_latched)) {
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

    app->simulation = saved_simulation;
    app->controls = saved_controls;
    app->overlay = saved_overlay;
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
        status = surf_man_qa_pause_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_qa_offscreen_scene_checks(app, engine, error);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_checks(app, error);
    }
    return status;
}
