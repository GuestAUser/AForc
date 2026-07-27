/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

AFORC_Status surf_man_error(AFORC_Error *error,
                            AFORC_Status status,
                            const char *subsystem,
                            const char *message)
{
    aforc_error_set(error, status, subsystem, "%s", message);
    return status;
}

void surf_man_set_message(SurfManApp *app, const char *format, ...)
{
    va_list arguments;

    if (app == NULL || format == NULL) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(app->message,
                    sizeof(app->message),
                    format,
                    arguments);
    va_end(arguments);
}

AFORC_Status surf_man_app_init(SurfManApp *app,
                               AFORC_Renderer *renderer,
                               AFORC_Input *input,
                               AFORC_Terminal *terminal,
                               uint64_t seed,
                               bool smoke)
{
    AFORC_Status status;

    if (app == NULL || renderer == NULL || input == NULL ||
        (!smoke && terminal == NULL)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    (void)memset(app, 0, sizeof(*app));
    app->allocator = aforc_allocator_default();
    app->terminal = terminal;
    app->renderer = renderer;
    app->input = input;
    app->seed = seed;
    app->smoke = smoke;
    app->focused = true;
    app->menu_item = SURF_MAN_MENU_SURF;
    app->pause_item = SURF_MAN_PAUSE_RESUME;
    app->accessibility_item = SURF_MAN_ACCESSIBILITY_SPEED;
    app->terminal_size = aforc_renderer_size(renderer);
    app->settings = surf_man_settings_default();
    app->scene.vtable = &surf_man_scene_vtable;
    app->scene.user_data = app;

    status = surf_man_simulation_init(&app->simulation,
                                      seed,
                                      &app->settings);
    if (status == AFORC_OK) {
        app->settings = app->simulation.settings;
        status = surf_man_visuals_init(
            &app->visuals,
            (uint32_t)(seed ^ (seed >> 32U)));
    }
    if (status != AFORC_OK) {
        surf_man_visuals_dispose(&app->visuals);
        return status;
    }

    if (app->terminal_size.width < SURF_MAN_MIN_COLUMNS ||
        app->terminal_size.height < SURF_MAN_MIN_ROWS) {
        app->overlay = SURF_MAN_OVERLAY_RESIZE;
        surf_man_set_message(app,
                             "Resize terminal to at least %dx%d.",
                             SURF_MAN_MIN_COLUMNS,
                             SURF_MAN_MIN_ROWS);
    } else {
        surf_man_set_message(app, "Choose SURF or PRACTICE.");
    }
    app->initialized = true;
    return AFORC_OK;
}

void surf_man_app_dispose(SurfManApp *app)
{
    if (app == NULL) {
        return;
    }
    surf_man_visuals_dispose(&app->visuals);
    (void)memset(app, 0, sizeof(*app));
}
