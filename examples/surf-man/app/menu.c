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

static void surf_man_close_overlay(SurfManApp *app)
{
    const SurfManOverlay closed = app->overlay;

    app->overlay = surf_man_size_supported(app->terminal_size)
                       ? SURF_MAN_OVERLAY_NONE
                       : SURF_MAN_OVERLAY_RESIZE;
    if (closed == SURF_MAN_OVERLAY_ACCESSIBILITY &&
        app->simulation.phase == SURF_MAN_SHACK) {
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
            break;
        case SURF_MAN_MENU_ACCESSIBILITY:
            app->overlay = SURF_MAN_OVERLAY_ACCESSIBILITY;
            app->menu_item = SURF_MAN_MENU_SURF;
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

static void surf_man_adjust_setting(SurfManApp *app, int direction)
{
    switch (app->menu_item) {
        case SURF_MAN_MENU_SURF:
            app->settings.speed_percent =
                app->settings.speed_percent == 100U ? 75U : 100U;
            break;
        case SURF_MAN_MENU_PRACTICE:
            app->settings.timing_percent =
                app->settings.timing_percent == 100U ? 150U : 100U;
            break;
        case SURF_MAN_MENU_HELP:
            app->settings.landing_assist = !app->settings.landing_assist;
            break;
        case SURF_MAN_MENU_ACCESSIBILITY:
            app->settings.reduced_motion = !app->settings.reduced_motion;
            break;
        case SURF_MAN_MENU_QUIT: {
            int mode = (int)app->settings.color_mode + direction;

            if (mode < 0) {
                mode = (int)SURF_MAN_COLOR_NONE;
            } else if (mode > (int)SURF_MAN_COLOR_NONE) {
                mode = (int)SURF_MAN_COLOR_STANDARD;
            }
            app->settings.color_mode = (SurfManColorMode)mode;
            break;
        }
        case SURF_MAN_MENU_COUNT:
            return;
    }
    app->simulation.settings = app->settings;
    surf_man_visuals_mark_dirty(&app->visuals);
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
    if (input->key == AFORC_KEY_ESCAPE ||
        (app->overlay == SURF_MAN_OVERLAY_PAUSE && input->pause) ||
        (app->overlay == SURF_MAN_OVERLAY_HELP &&
         (input->key == AFORC_KEY_ENTER ||
          input->codepoint == (uint32_t)'?' || input->pause))) {
        surf_man_close_overlay(app);
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_PAUSE) {
        if (input->key == AFORC_KEY_ENTER || input->pause) {
            surf_man_close_overlay(app);
        } else if (input->codepoint == (uint32_t)'?') {
            app->overlay = SURF_MAN_OVERLAY_HELP;
            surf_man_visuals_mark_dirty(&app->visuals);
        }
        return AFORC_OK;
    }
    if (app->overlay == SURF_MAN_OVERLAY_ACCESSIBILITY) {
        if (input->vertical != 0) {
            surf_man_move_menu(app, input->vertical);
        } else if (input->horizontal != 0) {
            surf_man_adjust_setting(app, input->horizontal);
        } else if (input->key == AFORC_KEY_ENTER ||
                   input->key == AFORC_KEY_SPACE) {
            surf_man_adjust_setting(app, 1);
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
