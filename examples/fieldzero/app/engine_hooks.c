#include "fieldzero/app.h"

static AFORC_Status fieldzero_hook_error(AFORC_Error *error,
                                         AFORC_Status status,
                                         const char *subsystem,
                                         const char *message)
{
    aforc_error_set(error, status, subsystem, "%s", message);
    return status;
}

AFORC_Status fieldzero_app_poll_events(void *context,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error)
{
    FieldzeroApp *app = context;
    AFORC_InputEvent event;
    AFORC_Status status = aforc_input_begin_frame(app->input);
    bool overflowed = status == AFORC_ERROR_LIMIT;

    if (overflowed)
    {
        status = AFORC_OK;
    }
    if (status == AFORC_OK)
    {
        status = aforc_input_poll(app->input, app->terminal, 0);
        if (status == AFORC_ERROR_LIMIT)
        {
            overflowed = true;
            status = AFORC_OK;
        }
    }
    if (status == AFORC_ERROR_INTERRUPTED ||
        status == AFORC_ERROR_END_OF_STREAM)
    {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (status != AFORC_OK)
    {
        return fieldzero_hook_error(
            error, status, "input", "terminal input poll failed");
    }
    status = fieldzero_app_begin_frame(context, engine, error);
    while (status == AFORC_OK && aforc_input_next_event(app->input, &event))
    {
        const FieldzeroScreen screen_before = app->view.screen;
        bool consumed = false;

        status = aforc_engine_dispatch_event(engine, &event, &consumed, error);
        if (status == AFORC_OK &&
            (app->view.screen != screen_before ||
             aforc_engine_state(engine) == AFORC_ENGINE_STOPPING))
        {
            break;
        }
    }
    if (overflowed)
    {
        while (aforc_input_next_event(app->input, &event))
        {
        }
        if (status == AFORC_OK)
        {
            fieldzero_app_reconcile_input(app);
        }
    }
    return status;
}

AFORC_Status fieldzero_app_begin_frame(void *context,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error)
{
    FieldzeroApp *app = context;
    bool changed = false;
    bool was_too_small;
    AFORC_Status status = AFORC_OK;
    AFORC_Size size;

    (void)engine;
    if (app == NULL || app->renderer == NULL)
    {
        return fieldzero_hook_error(error,
                                    AFORC_ERROR_STATE,
                                    "fieldzero",
                                    "frame hook has no renderer");
    }
    was_too_small = app->view.terminal_too_small;
    if (app->terminal != NULL)
    {
        status = aforc_renderer_resize_to_terminal(
            app->renderer, app->terminal, &changed);
        if (status != AFORC_OK)
        {
            return fieldzero_hook_error(
                error, status, "renderer", "terminal resize failed");
        }
    }
    size = aforc_renderer_size(app->renderer);
    app->view.terminal_too_small =
        size.width < FIELDZERO_MIN_WIDTH || size.height < FIELDZERO_MIN_HEIGHT;
    if (changed)
    {
        aforc_renderer_invalidate(app->renderer);
    }
    if (changed || (!was_too_small && app->view.terminal_too_small))
    {
        fieldzero_game_clear_actions(&app->game);
    }
    return AFORC_OK;
}

AFORC_Status
fieldzero_app_present(void *context, AFORC_Engine *engine, AFORC_Error *error)
{
    FieldzeroApp *app = context;
    AFORC_Status status;

    (void)engine;
    if (app->terminal == NULL)
    {
        return AFORC_OK;
    }
    status = aforc_renderer_present(app->renderer, app->terminal);
    return status == AFORC_OK
               ? AFORC_OK
               : fieldzero_hook_error(
                     error, status, "renderer", "frame presentation failed");
}
