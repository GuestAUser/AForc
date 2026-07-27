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

static void surf_man_move_menu(SurfManApp *app, int direction)
{
    int item = (int)app->menu_item + direction;

    if (item < 0) {
        item = (int)SURF_MAN_MENU_COUNT - 1;
    } else if (item >= (int)SURF_MAN_MENU_COUNT) {
        item = 0;
    }
    app->menu_item = (SurfManMenuItem)item;
    surf_man_visuals_mark_dirty(&app->visuals);
}

static void surf_man_move_pause_menu(SurfManApp *app, int direction)
{
    int item = (int)app->pause_item + direction;

    if (item < 0) {
        item = (int)SURF_MAN_PAUSE_COUNT - 1;
    } else if (item >= (int)SURF_MAN_PAUSE_COUNT) {
        item = 0;
    }
    app->pause_item = (SurfManPauseItem)item;
    surf_man_visuals_mark_dirty(&app->visuals);
}

static void surf_man_move_accessibility_menu(SurfManApp *app, int direction)
{
    int item = (int)app->accessibility_item + direction;

    if (item < 0) {
        item = (int)SURF_MAN_ACCESSIBILITY_COUNT - 1;
    } else if (item >= (int)SURF_MAN_ACCESSIBILITY_COUNT) {
        item = 0;
    }
    app->accessibility_item = (SurfManAccessibilityItem)item;
    surf_man_visuals_mark_dirty(&app->visuals);
}

static void surf_man_close_overlay(SurfManApp *app)
{
    const SurfManOverlay closed = app->overlay;
    const SurfManOverlay target =
        (closed == SURF_MAN_OVERLAY_HELP ||
         closed == SURF_MAN_OVERLAY_ACCESSIBILITY) &&
                app->overlay_return != SURF_MAN_OVERLAY_NONE
            ? app->overlay_return
            : SURF_MAN_OVERLAY_NONE;

    app->overlay = surf_man_size_supported(app->terminal_size)
                       ? target
                       : SURF_MAN_OVERLAY_RESIZE;
    app->overlay_return = SURF_MAN_OVERLAY_NONE;
    if (closed == SURF_MAN_OVERLAY_ACCESSIBILITY &&
        app->simulation.phase == SURF_MAN_SHACK &&
        app->overlay == SURF_MAN_OVERLAY_NONE) {
        app->menu_item = SURF_MAN_MENU_ACCESSIBILITY;
    }
    surf_man_clear_controls(app);
    surf_man_visuals_mark_dirty(&app->visuals);
}

static AFORC_Status surf_man_start_session(SurfManApp *app, bool practice)
{
    AFORC_Status status = surf_man_simulation_start_day(&app->simulation,
                                                        practice);

    if (status == AFORC_OK) {
        status = surf_man_simulation_start_wave(&app->simulation);
    }
    if (status == AFORC_OK) {
        app->overlay = SURF_MAN_OVERLAY_NONE;
        app->overlay_return = SURF_MAN_OVERLAY_NONE;
        surf_man_clear_controls(app);
        if (practice) {
            surf_man_set_message(app, "Practice session started.");
        } else {
            surf_man_set_message(app,
                                 "Day %u started.",
                                 app->simulation.day);
        }
        surf_man_visuals_mark_dirty(&app->visuals);
    }
    return status;
}

static AFORC_Status surf_man_activate_menu(SurfManApp *app,
                                            AFORC_Engine *engine)
{
    switch (app->menu_item) {
        case SURF_MAN_MENU_SURF:
            return surf_man_start_session(app, false);
        case SURF_MAN_MENU_PRACTICE:
            return surf_man_start_session(app, true);
        case SURF_MAN_MENU_HELP:
            app->overlay = SURF_MAN_OVERLAY_HELP;
            app->overlay_return = SURF_MAN_OVERLAY_NONE;
            break;
        case SURF_MAN_MENU_ACCESSIBILITY:
            app->overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
            app->overlay_return = SURF_MAN_OVERLAY_NONE;
            app->accessibility_item = SURF_MAN_ACCESSIBILITY_SPEED;
            break;
        case SURF_MAN_MENU_QUIT:
            aforc_engine_request_quit(engine);
            break;
        case SURF_MAN_MENU_COUNT:
            return AFORC_ERROR_STATE;
    }
    surf_man_clear_controls(app);
    surf_man_visuals_mark_dirty(&app->visuals);
    return AFORC_OK;
}

static AFORC_Status surf_man_adjust_setting(SurfManApp *app, int direction)
{
    switch (app->accessibility_item) {
        case SURF_MAN_ACCESSIBILITY_SPEED:
            app->settings.speed_percent =
                app->settings.speed_percent == 100U ? 75U : 100U;
            break;
        case SURF_MAN_ACCESSIBILITY_TIMING:
            app->settings.timing_percent =
                app->settings.timing_percent == 100U ? 150U : 100U;
            break;
        case SURF_MAN_ACCESSIBILITY_LANDING:
            app->settings.landing_assist = !app->settings.landing_assist;
            break;
        case SURF_MAN_ACCESSIBILITY_MOTION:
            app->settings.reduced_motion = !app->settings.reduced_motion;
            break;
        case SURF_MAN_ACCESSIBILITY_COLOR: {
            int mode = (int)app->settings.color_mode + direction;
            AFORC_Status status;

            if (mode < 0) {
                mode = (int)SURF_MAN_COLOR_NONE;
            } else if (mode > (int)SURF_MAN_COLOR_NONE) {
                mode = (int)SURF_MAN_COLOR_STANDARD;
            }
            status = aforc_particle_pool_clear(&app->visuals.particle_pool);
            if (status != AFORC_OK) {
                return status;
            }
            app->settings.color_mode = (SurfManColorMode)mode;
            break;
        }
        case SURF_MAN_ACCESSIBILITY_COUNT:
            return AFORC_ERROR_STATE;
    }
    app->simulation.settings = app->settings;
    surf_man_visuals_mark_dirty(&app->visuals);
    return AFORC_OK;
}

static AFORC_Status surf_man_activate_pause(SurfManApp *app,
                                             AFORC_Engine *engine)
{
    switch (app->pause_item) {
        case SURF_MAN_PAUSE_RESUME:
            surf_man_close_overlay(app);
            return AFORC_OK;
        case SURF_MAN_PAUSE_HELP:
            app->overlay = SURF_MAN_OVERLAY_HELP;
            app->overlay_return = SURF_MAN_OVERLAY_PAUSE;
            break;
        case SURF_MAN_PAUSE_ACCESSIBILITY:
            app->overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
            app->overlay_return = SURF_MAN_OVERLAY_PAUSE;
            app->accessibility_item = SURF_MAN_ACCESSIBILITY_SPEED;
            break;
        case SURF_MAN_PAUSE_END_SESSION: {
            const bool was_practice = app->simulation.practice;
            AFORC_Status status =
                surf_man_simulation_end_session(&app->simulation);

            if (status == AFORC_OK) {
                status = aforc_particle_pool_clear(
                    &app->visuals.particle_pool);
            }
            if (status != AFORC_OK) {
                return status;
            }
            app->overlay = SURF_MAN_OVERLAY_NONE;
            app->overlay_return = SURF_MAN_OVERLAY_NONE;
            app->menu_item = was_practice ? SURF_MAN_MENU_PRACTICE
                                          : SURF_MAN_MENU_SURF;
            surf_man_set_message(app,
                                 was_practice
                                     ? "Practice ended. Choose another session."
                                     : "Session ended. Choose SURF or PRACTICE.");
            break;
        }
        case SURF_MAN_PAUSE_QUIT:
            aforc_engine_request_quit(engine);
            break;
        case SURF_MAN_PAUSE_COUNT:
            return AFORC_ERROR_STATE;
    }
    surf_man_clear_controls(app);
    surf_man_visuals_mark_dirty(&app->visuals);
    return AFORC_OK;
}

AFORC_Status surf_man_menu_handle_modal_key(
    SurfManApp *app,
    AFORC_Engine *engine,
    const SurfManInputKey *input)
{
    if (input->quit) {
        aforc_engine_request_quit(engine);
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_RESIZE) {
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_PAUSE) {
        if (input->key == AFORC_KEY_ESCAPE || input->pause) {
            surf_man_close_overlay(app);
        } else if (input->vertical != 0) {
            surf_man_move_pause_menu(app, input->vertical);
        } else if (input->key == AFORC_KEY_ENTER ||
                   input->key == AFORC_KEY_SPACE) {
            return surf_man_activate_pause(app, engine);
        } else if (input->codepoint == (uint32_t)'?') {
            app->overlay = SURF_MAN_OVERLAY_HELP;
            app->overlay_return = SURF_MAN_OVERLAY_PAUSE;
            surf_man_visuals_mark_dirty(&app->visuals);
        }
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_HELP) {
        if (input->key == AFORC_KEY_ESCAPE ||
            input->key == AFORC_KEY_ENTER ||
            input->codepoint == (uint32_t)'?' || input->pause) {
            surf_man_close_overlay(app);
        }
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_ACCESSIBILITY) {
        if (input->key == AFORC_KEY_ESCAPE || input->pause) {
            surf_man_close_overlay(app);
        } else if (input->vertical != 0) {
            surf_man_move_accessibility_menu(app, input->vertical);
        } else if (input->horizontal != 0) {
            return surf_man_adjust_setting(app, input->horizontal);
        } else if (input->key == AFORC_KEY_ENTER ||
                   input->key == AFORC_KEY_SPACE) {
            return surf_man_adjust_setting(app, 1);
        }
    }
    return AFORC_OK;
}

AFORC_Status surf_man_menu_handle_shack_key(
    SurfManApp *app,
    AFORC_Engine *engine,
    const SurfManInputKey *input)
{
    if (input->repeat) {
        return AFORC_OK;
    }
    if (input->vertical != 0) {
        surf_man_move_menu(app, input->vertical);
        return AFORC_OK;
    }
    if (input->key == AFORC_KEY_ENTER || input->key == AFORC_KEY_SPACE) {
        return surf_man_activate_menu(app, engine);
    }
    if (input->key == AFORC_KEY_ESCAPE) {
        aforc_engine_request_quit(engine);
    }
    return AFORC_OK;
}
