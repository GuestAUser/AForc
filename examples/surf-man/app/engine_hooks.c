/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"

const AFORC_SceneVTable surf_man_scene_vtable = {
    .fixed_update = surf_man_scene_fixed_update,
    .update = surf_man_scene_update,
    .render = surf_man_scene_render,
    .event = surf_man_scene_event,
};

static bool surf_man_app_can_advance(const SurfManApp *app)
{
    return app->focused && app->overlay == SURF_MAN_OVERLAY_NONE &&
           app->terminal_size.width >= SURF_MAN_MIN_COLUMNS &&
           app->terminal_size.height >= SURF_MAN_MIN_ROWS;
}

AFORC_Status surf_man_scene_fixed_update(AFORC_Scene *scene,
                                         AFORC_Engine *engine,
                                         double seconds,
                                         AFORC_Error *error)
{
    SurfManApp *app;
    SurfManCommand command;
    SurfManWaveSample action_sample;
    SurfManPhase previous_phase;
    uint32_t previous_maneuver_count;
    bool lip_action_consumed = false;
    bool previous_airborne;
    AFORC_Status status = AFORC_OK;

    (void)seconds;
    if (scene == NULL || engine == NULL || scene->user_data == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "surf-man",
                              "invalid fixed-update context");
    }
    app = scene->user_data;
    if (!app->initialized) {
        return surf_man_error(error,
                              AFORC_ERROR_STATE,
                              "surf-man",
                              "application is not initialized");
    }
    if (!surf_man_app_can_advance(app)) {
        return AFORC_OK;
    }

    previous_phase = app->simulation.phase;
    previous_maneuver_count = app->simulation.maneuver_count;
    previous_airborne = app->simulation.airborne;
    surf_man_input_take_command(app, &command);
    if (command.action && !previous_airborne &&
        (app->simulation.phase == SURF_MAN_RIDING ||
         app->simulation.phase == SURF_MAN_PRACTICE)) {
        status = surf_man_wave_sample(&app->simulation,
                                      app->simulation.line_position_q16,
                                      &action_sample);
        if (status == AFORC_OK) {
            lip_action_consumed = action_sample.lip && !action_sample.tube;
        }
    }
    if (status == AFORC_OK && app->simulation.phase != SURF_MAN_SHACK) {
        status = surf_man_simulation_step(&app->simulation, &command);
        if (status == AFORC_OK && command.action &&
            (lip_action_consumed ||
             (!previous_airborne && app->simulation.airborne))) {
            app->controls.action_lease = 0U;
            app->controls.action_tap = false;
        }
        if (status == AFORC_OK &&
            (app->simulation.phase != previous_phase ||
             (app->simulation.phase != SURF_MAN_WAVE_RECAP &&
              app->simulation.phase != SURF_MAN_DAY_RECAP))) {
            surf_man_visuals_mark_dirty(&app->visuals);
        }
    }
    if (status == AFORC_OK &&
        app->simulation.phase == SURF_MAN_WIPEOUT_RECOVERY &&
        previous_phase != SURF_MAN_WIPEOUT_RECOVERY) {
        status = surf_man_emit_spray(app, true);
    } else if (status == AFORC_OK &&
               app->simulation.maneuver_count != previous_maneuver_count) {
        status = surf_man_emit_spray(app, false);
    }
    if (status == AFORC_OK &&
        aforc_engine_fixed_tick(engine) %
                (SURF_MAN_FIXED_HZ / SURF_MAN_VISUAL_HZ) ==
            0U) {
        status = surf_man_visuals_step(app);
    }
    return status == AFORC_OK
               ? AFORC_OK
               : surf_man_error(error,
                                status,
                                "surf-man",
                                "fixed update failed");
}

AFORC_Status surf_man_scene_update(AFORC_Scene *scene,
                                   AFORC_Engine *engine,
                                   double seconds,
                                   AFORC_Error *error)
{
    (void)scene;
    (void)engine;
    (void)seconds;
    (void)error;
    return AFORC_OK;
}

AFORC_Status surf_man_scene_render(AFORC_Scene *scene,
                                   AFORC_Engine *engine,
                                   double interpolation,
                                   AFORC_Error *error)
{
    SurfManApp *app;

    (void)engine;
    if (scene == NULL || scene->user_data == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "surf-man",
                              "invalid render context");
    }
    app = scene->user_data;
    return surf_man_render_frame(app, interpolation, error);
}

AFORC_Status surf_man_scene_event(AFORC_Scene *scene,
                                  AFORC_Engine *engine,
                                  const void *event_data,
                                  bool *consumed,
                                  AFORC_Error *error)
{
    const AFORC_InputEvent *event = event_data;
    SurfManApp *app;
    AFORC_Status status;

    if (consumed != NULL) {
        *consumed = false;
    }
    if (scene == NULL || scene->user_data == NULL || engine == NULL ||
        event == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "surf-man",
                              "invalid input event context");
    }
    app = scene->user_data;
    status = surf_man_app_handle_event(app, engine, event);
    if (status != AFORC_OK) {
        return surf_man_error(error,
                              status,
                              "surf-man",
                              "input event handling failed");
    }
    if (consumed != NULL) {
        *consumed = event->type == AFORC_INPUT_EVENT_KEY_DOWN ||
                    event->type == AFORC_INPUT_EVENT_KEY_UP ||
                    event->type == AFORC_INPUT_EVENT_RESIZE ||
                    event->type == AFORC_INPUT_EVENT_FOCUS_IN ||
                    event->type == AFORC_INPUT_EVENT_FOCUS_OUT;
    }
    return AFORC_OK;
}

static AFORC_Status surf_man_dispatch_input(SurfManApp *app,
                                            AFORC_Engine *engine,
                                            AFORC_Error *error)
{
    AFORC_InputEvent event;

    while (aforc_input_next_event(app->input, &event)) {
        bool consumed = false;
        AFORC_Status status = aforc_engine_dispatch_event(engine,
                                                          &event,
                                                          &consumed,
                                                          error);

        if (status != AFORC_OK) {
            return status;
        }
    }
    return AFORC_OK;
}

AFORC_Status surf_man_poll_events(void *context,
                                  AFORC_Engine *engine,
                                  AFORC_Error *error)
{
    SurfManApp *app = context;
    AFORC_Status status;

    if (app == NULL || engine == NULL || app->input == NULL ||
        app->terminal == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "input",
                              "invalid input polling context");
    }
    status = aforc_input_begin_frame(app->input);
    if (status == AFORC_ERROR_LIMIT) {
        surf_man_set_message(app, "Input queue saturated; newest event dropped.");
        status = AFORC_OK;
    }
    if (status == AFORC_OK) {
        status = aforc_input_poll(app->input, app->terminal, 0);
    }
    if (status == AFORC_ERROR_INTERRUPTED ||
        status == AFORC_ERROR_END_OF_STREAM) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (status == AFORC_ERROR_LIMIT) {
        surf_man_set_message(app, "Input queue saturated; newest event dropped.");
        status = AFORC_OK;
    }
    if (status != AFORC_OK) {
        return surf_man_error(error,
                              status,
                              "input",
                              "terminal input poll failed");
    }
    return surf_man_dispatch_input(app, engine, error);
}

AFORC_Status surf_man_begin_frame(void *context,
                                  AFORC_Engine *engine,
                                  AFORC_Error *error)
{
    SurfManApp *app = context;
    AFORC_Size size;
    bool changed = false;
    AFORC_Status status = AFORC_OK;

    if (app == NULL || engine == NULL || app->renderer == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "renderer",
                              "invalid begin-frame context");
    }
    if (app->terminal != NULL) {
        status = aforc_renderer_resize_to_terminal(app->renderer,
                                                   app->terminal,
                                                   &changed);
    }
    if (status != AFORC_OK) {
        return surf_man_error(error,
                              status,
                              "renderer",
                              "terminal resize failed");
    }
    size = aforc_renderer_size(app->renderer);
    if (changed || size.width != app->terminal_size.width ||
        size.height != app->terminal_size.height) {
        AFORC_InputEvent event = {0};

        event.type = AFORC_INPUT_EVENT_RESIZE;
        event.data.resize.size = size;
        status = surf_man_app_handle_event(app, engine, &event);
    }
    return status == AFORC_OK
               ? AFORC_OK
               : surf_man_error(error,
                                status,
                                "renderer",
                                "resize event handling failed");
}

AFORC_Status surf_man_present(void *context,
                              AFORC_Engine *engine,
                              AFORC_Error *error)
{
    SurfManApp *app = context;
    AFORC_Status status;

    (void)engine;
    if (app == NULL || app->renderer == NULL) {
        return surf_man_error(error,
                              AFORC_ERROR_INVALID_ARGUMENT,
                              "renderer",
                              "invalid present context");
    }
    if (app->terminal == NULL) {
        return AFORC_OK;
    }
    status = aforc_renderer_present(app->renderer, app->terminal);
    return status == AFORC_OK
               ? AFORC_OK
               : surf_man_error(error,
                                status,
                                "renderer",
                                "terminal presentation failed");
}
