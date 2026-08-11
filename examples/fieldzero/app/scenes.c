#include "fieldzero/app.h"

static AFORC_Status fieldzero_scene_error(AFORC_Error *error,
                                          AFORC_Status status,
                                          const char *message)
{
    if (error != NULL && error->message[0] == '\0')
    {
        aforc_error_set(error, status, "fieldzero", "%s", message);
    }
    return status;
}

static bool fieldzero_scene_event_is_input(const AFORC_InputEvent *event)
{
    return event->type == AFORC_INPUT_EVENT_KEY_DOWN ||
           event->type == AFORC_INPUT_EVENT_KEY_UP ||
           event->type == AFORC_INPUT_EVENT_RESIZE ||
           event->type == AFORC_INPUT_EVENT_FOCUS_IN ||
           event->type == AFORC_INPUT_EVENT_FOCUS_OUT;
}

static AFORC_Status fieldzero_sync_overlay(FieldzeroApp *app,
                                           AFORC_Engine *engine,
                                           const AFORC_InputEvent *event,
                                           bool *consumed,
                                           AFORC_Error *error)
{
    const bool before = fieldzero_view_has_overlay(&app->view);
    AFORC_Status status = AFORC_OK;

    fieldzero_app_handle_input(app, event, engine);
    if (!before && fieldzero_view_has_overlay(&app->view))
    {
        status = aforc_engine_request_push(engine, &app->overlay_scene, error);
    }
    else if (before && !fieldzero_view_has_overlay(&app->view))
    {
        status = aforc_engine_request_pop(engine, error);
    }
    *consumed = fieldzero_scene_event_is_input(event);
    return status;
}

static void fieldzero_scene_clear_actions(AFORC_Scene *scene,
                                          AFORC_Engine *engine)
{
    FieldzeroApp *app = scene->user_data;

    (void)engine;
    fieldzero_game_clear_actions(&app->game);
}

static AFORC_Status fieldzero_enter_screen(AFORC_Scene *scene,
                                           FieldzeroScreen screen)
{
    FieldzeroApp *app = scene->user_data;

    app->view.screen = screen;
    app->view.help_visible = false;
    app->view.paused = false;
    app->view.focus_paused = false;
    app->view.quit_confirmation = false;
    fieldzero_game_clear_actions(&app->game);
    return AFORC_OK;
}

static AFORC_Status fieldzero_title_enter(AFORC_Scene *scene,
                                          AFORC_Engine *engine,
                                          AFORC_Error *error)
{
    (void)engine;
    (void)error;
    return fieldzero_enter_screen(scene, FIELDZERO_SCREEN_TITLE);
}

static AFORC_Status fieldzero_play_enter(AFORC_Scene *scene,
                                         AFORC_Engine *engine,
                                         AFORC_Error *error)
{
    (void)engine;
    (void)error;
    return fieldzero_enter_screen(scene, FIELDZERO_SCREEN_PLAY);
}

static AFORC_Status fieldzero_completion_enter(AFORC_Scene *scene,
                                               AFORC_Engine *engine,
                                               AFORC_Error *error)
{
    (void)engine;
    (void)error;
    return fieldzero_enter_screen(scene, FIELDZERO_SCREEN_COMPLETE);
}

static AFORC_Status fieldzero_presentation_fixed(FieldzeroApp *app,
                                                 AFORC_Engine *engine,
                                                 AFORC_Error *error)
{
    const uint64_t fixed_tick = aforc_engine_fixed_tick(engine);
    AFORC_Status status;

    if (app->last_fixed_tick == fixed_tick)
    {
        return AFORC_OK;
    }
    status = fieldzero_presentation_update(
        &app->presentation, &app->game, fixed_tick);
    if (status == AFORC_OK)
    {
        app->last_fixed_tick = fixed_tick;
        return AFORC_OK;
    }
    return fieldzero_scene_error(
        error, status, "fixed presentation update failed");
}

static AFORC_Status fieldzero_static_fixed_update(AFORC_Scene *scene,
                                                  AFORC_Engine *engine,
                                                  double seconds,
                                                  AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;

    (void)seconds;
    return app->view.terminal_too_small
               ? AFORC_OK
               : fieldzero_presentation_fixed(app, engine, error);
}

static AFORC_Status fieldzero_play_fixed_update(AFORC_Scene *scene,
                                                AFORC_Engine *engine,
                                                double seconds,
                                                AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    AFORC_Status status;

    (void)seconds;
    if (app->view.terminal_too_small || fieldzero_view_has_overlay(&app->view))
    {
        return AFORC_OK;
    }
    status = fieldzero_game_tick(&app->game);
    if (status != AFORC_OK)
    {
        return fieldzero_scene_error(error, status, "fixed game update failed");
    }
    status = fieldzero_presentation_fixed(app, engine, error);
    if (status == AFORC_OK && app->game.phase == FIELDZERO_PHASE_COMPLETE)
    {
        app->view.screen = FIELDZERO_SCREEN_COMPLETE;
        status =
            aforc_engine_request_replace(engine, &app->completion_scene, error);
        if (status != AFORC_OK)
        {
            app->view.screen = FIELDZERO_SCREEN_PLAY;
        }
    }
    return status;
}

static AFORC_Status fieldzero_scene_render(AFORC_Scene *scene,
                                           AFORC_Engine *engine,
                                           double interpolation,
                                           AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    AFORC_Status status;

    (void)engine;
    (void)interpolation;
    status = fieldzero_render(
        app->renderer, &app->game, &app->presentation, &app->view);
    return status == AFORC_OK
               ? AFORC_OK
               : fieldzero_scene_error(error, status, "frame render failed");
}

static AFORC_Status fieldzero_title_event(AFORC_Scene *scene,
                                          AFORC_Engine *engine,
                                          const void *event_data,
                                          bool *consumed,
                                          AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    const AFORC_InputEvent *event = event_data;

    *consumed = false;
    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN && !event->data.key.repeat &&
        !fieldzero_view_has_overlay(&app->view))
    {
        const uint32_t codepoint = fieldzero_event_codepoint(event);

        if (event->data.key.key == AFORC_KEY_ENTER ||
            event->data.key.key == AFORC_KEY_SPACE ||
            fieldzero_codepoint_is(codepoint, 'z'))
        {
            AFORC_Status status;

            app->view.screen = FIELDZERO_SCREEN_PLAY;
            status =
                aforc_engine_request_replace(engine, &app->play_scene, error);
            if (status != AFORC_OK)
            {
                app->view.screen = FIELDZERO_SCREEN_TITLE;
            }
            *consumed = true;
            return status;
        }
    }
    return fieldzero_sync_overlay(app, engine, event, consumed, error);
}

static AFORC_Status fieldzero_play_event(AFORC_Scene *scene,
                                         AFORC_Engine *engine,
                                         const void *event_data,
                                         bool *consumed,
                                         AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    const AFORC_InputEvent *event = event_data;

    *consumed = false;
    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN && !event->data.key.repeat &&
        !fieldzero_view_has_overlay(&app->view) &&
        !app->view.terminal_too_small &&
        fieldzero_codepoint_is(fieldzero_event_codepoint(event), 'r'))
    {
        const AFORC_Status status = fieldzero_game_restart_room(&app->game);

        fieldzero_game_clear_actions(&app->game);
        *consumed = true;
        return status == AFORC_OK ? AFORC_OK
                                  : fieldzero_scene_error(
                                        error, status, "room restart failed");
    }
    return fieldzero_sync_overlay(app, engine, event, consumed, error);
}

static AFORC_Status fieldzero_completion_event(AFORC_Scene *scene,
                                               AFORC_Engine *engine,
                                               const void *event_data,
                                               bool *consumed,
                                               AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    const AFORC_InputEvent *event = event_data;

    *consumed = false;
    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN && !event->data.key.repeat &&
        !fieldzero_view_has_overlay(&app->view) &&
        fieldzero_codepoint_is(fieldzero_event_codepoint(event), 'r'))
    {
        AFORC_Status status = fieldzero_game_restart_run(&app->game);

        if (status == AFORC_OK)
        {
            app->view.screen = FIELDZERO_SCREEN_PLAY;
            status =
                aforc_engine_request_replace(engine, &app->play_scene, error);
            if (status != AFORC_OK)
            {
                app->view.screen = FIELDZERO_SCREEN_COMPLETE;
            }
        }
        *consumed = true;
        return status == AFORC_OK
                   ? AFORC_OK
                   : fieldzero_scene_error(
                         error, status, "completed run restart failed");
    }
    return fieldzero_sync_overlay(app, engine, event, consumed, error);
}

static AFORC_Status fieldzero_overlay_event(AFORC_Scene *scene,
                                            AFORC_Engine *engine,
                                            const void *event_data,
                                            bool *consumed,
                                            AFORC_Error *error)
{
    FieldzeroApp *app = scene->user_data;
    const AFORC_InputEvent *event = event_data;

    if (event->type == AFORC_INPUT_EVENT_KEY_DOWN && !event->data.key.repeat &&
        app->view.paused && !app->view.help_visible &&
        !app->view.focus_paused && !app->view.quit_confirmation &&
        !app->view.terminal_too_small &&
        fieldzero_codepoint_is(fieldzero_event_codepoint(event), 'r'))
    {
        AFORC_Status status = fieldzero_game_restart_room(&app->game);

        if (status == AFORC_OK)
        {
            app->view.paused = false;
            status = aforc_engine_request_pop(engine, error);
        }
        *consumed = true;
        return status == AFORC_OK
                   ? AFORC_OK
                   : fieldzero_scene_error(
                         error, status, "paused room restart failed");
    }
    return fieldzero_sync_overlay(app, engine, event, consumed, error);
}

static const AFORC_SceneVTable fieldzero_title_vtable = {
    .enter = fieldzero_title_enter,
    .leave = fieldzero_scene_clear_actions,
    .pause = fieldzero_scene_clear_actions,
    .resume = fieldzero_scene_clear_actions,
    .fixed_update = fieldzero_static_fixed_update,
    .render = fieldzero_scene_render,
    .event = fieldzero_title_event,
};

static const AFORC_SceneVTable fieldzero_play_vtable = {
    .enter = fieldzero_play_enter,
    .leave = fieldzero_scene_clear_actions,
    .pause = fieldzero_scene_clear_actions,
    .resume = fieldzero_scene_clear_actions,
    .fixed_update = fieldzero_play_fixed_update,
    .render = fieldzero_scene_render,
    .event = fieldzero_play_event,
};

static const AFORC_SceneVTable fieldzero_completion_vtable = {
    .enter = fieldzero_completion_enter,
    .leave = fieldzero_scene_clear_actions,
    .pause = fieldzero_scene_clear_actions,
    .resume = fieldzero_scene_clear_actions,
    .fixed_update = fieldzero_static_fixed_update,
    .render = fieldzero_scene_render,
    .event = fieldzero_completion_event,
};

static const AFORC_SceneVTable fieldzero_overlay_vtable = {
    .leave = fieldzero_scene_clear_actions,
    .event = fieldzero_overlay_event,
};

void fieldzero_app_configure_scenes(FieldzeroApp *app)
{
    app->title_scene = (AFORC_Scene){&fieldzero_title_vtable, app, 0U};
    app->play_scene = (AFORC_Scene){&fieldzero_play_vtable, app, 0U};
    app->completion_scene =
        (AFORC_Scene){&fieldzero_completion_vtable, app, 0U};
    app->overlay_scene =
        (AFORC_Scene){&fieldzero_overlay_vtable, app, AFORC_SCENE_RENDER_BELOW};
}
