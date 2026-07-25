/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

static AFORC_Status render_error(AFORC_Error *error,
                                 AFORC_Status status,
                                 const char *message) {
    if (error != NULL) {
        aforc_error_set(error,
                        status,
                        "surf-man.presentation",
                        "%s",
                        message);
    }
    return status;
}

AFORC_Status surf_man_render_frame(SurfManApp *app,
                                   double interpolation,
                                   AFORC_Error *error) {
    AFORC_Size size;
    SurfManLayout layout;
    AFORC_Status status;

    (void)interpolation;
    if (app == NULL || app->renderer == NULL) {
        return render_error(error,
                            AFORC_ERROR_INVALID_ARGUMENT,
                            "render requires an app and renderer");
    }
    if (!app->visuals.initialized) {
        return render_error(error,
                            AFORC_ERROR_STATE,
                            "visual state is not initialized");
    }
    if (!app->visuals.dirty) {
        return AFORC_OK;
    }
    size = aforc_renderer_size(app->renderer);
    status = aforc_renderer_clear(
        app->renderer,
        surf_man_tone_cell(app,
                           (uint32_t)' ',
                           SURF_MAN_TONE_CANVAS,
                           AFORC_STYLE_NONE));
    if (status != AFORC_OK) {
        return render_error(error, status, "canvas clear failed");
    }
    if (size.width < SURF_MAN_MIN_COLUMNS || size.height < SURF_MAN_MIN_ROWS) {
        status = surf_man_render_resize(app, size);
        if (status != AFORC_OK) {
            return render_error(error, status, "resize notice failed");
        }
        app->visuals.dirty = false;
        return AFORC_OK;
    }
    layout = surf_man_layout_for_size(size);
    status = surf_man_draw_instrument(app, &layout);
    if (status == AFORC_OK && app->simulation.phase == SURF_MAN_SHACK) {
        status = surf_man_render_shack_art(app, &layout);
        if (status == AFORC_OK && app->overlay == SURF_MAN_OVERLAY_NONE) {
            status = surf_man_render_menu(app, &layout);
        }
    } else if (status == AFORC_OK) {
        status = surf_man_render_wave_art(app, &layout);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_hud(app, &layout);
    }
    if (status == AFORC_OK && app->overlay != SURF_MAN_OVERLAY_NONE) {
        status = surf_man_render_overlay(app, &layout);
    } else if (status == AFORC_OK) {
        status = surf_man_render_phase_modal(app, &layout);
    }
    if (status != AFORC_OK) {
        return render_error(error, status, "frame composition failed");
    }
    app->visuals.dirty = false;
    return AFORC_OK;
}
