/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static AFORC_Rect centered_rect(const SurfManLayout *layout,
                                 int32_t width,
                                int32_t height) {
    return (AFORC_Rect){(layout->screen.width - width) / 2,
                        layout->play.y + (layout->play.height - height) / 2,
                        width,
                        height};
}

static int32_t centered_text_x(AFORC_Rect rect, const char *text) {
    int32_t x = rect.x + (rect.width - (int32_t)strlen(text)) / 2;

    if (x < rect.x + 1) {
        x = rect.x + 1;
    }
    return x;
}

static const char *color_name(SurfManColorMode mode) {
    switch (mode) {
    case SURF_MAN_COLOR_STANDARD:
        return "STANDARD";
    case SURF_MAN_COLOR_HIGH_CONTRAST:
        return "HIGH";
    case SURF_MAN_COLOR_NONE:
        return "NONE";
    }
    return "UNKNOWN";
}

AFORC_Status surf_man_render_menu(SurfManApp *app,
                                  const SurfManLayout *layout) {
    static const char *const labels[] = {
        "SURF DAY", "PRACTICE", "HELP", "ACCESSIBILITY", "QUIT"};
    const int32_t width = 24;
    const int32_t height = 9;
    const AFORC_Rect panel = {
        layout->play.x + layout->play.width - width,
        layout->play.y + 1,
        width,
        height};
    AFORC_Status status = surf_man_draw_panel(app, panel, "SESSION");

    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(labels) / sizeof(labels[0]);
         ++index) {
        const bool selected = (size_t)app->menu_item == index;
        const int32_t row = panel.y + 2 + (int32_t)index;

        status = surf_man_draw_char(app,
                                    (AFORC_Point){panel.x + 2, row},
                                    selected ? (uint32_t)'>' : (uint32_t)' ',
                                    selected ? SURF_MAN_TONE_SIGNAL
                                             : SURF_MAN_TONE_FRAMEWORK,
                                    selected ? AFORC_STYLE_BOLD
                                             : AFORC_STYLE_NONE);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){panel.x + 4, row},
                labels[index],
                selected ? SURF_MAN_TONE_INK : SURF_MAN_TONE_FRAMEWORK,
                selected ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE);
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){panel.x + 2,
                                                  panel.y + height - 2},
                                    "UP/DOWN  ENTER",
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_DIM);
    }
    return status;
}

static AFORC_Status render_help(SurfManApp *app,
                                const SurfManLayout *layout) {
    static const char *const lines[] = {
        "W/S CLIMBS HIGH OR DROPS LOW ON THE WAVE FACE.",
        "A/D BUILDS LINE MOMENTUM; REVERSE TO CARVE.",
        "HIGH LIP: SPACE LAUNCHES. LOW LIP: SPACE SNAPS.",
        "IN AIR: A/D ROTATES; SPACE GRABS.",
        "TAP SPACE ONCE TO COMMIT THROUGH A TUBE.",
        "STAY HIGH OR MOVE YOUR LINE TO CLEAR HAZARDS.",
        "LAND LEVEL; HOLD A CLEAN LINE 2.5S TO BANK.",
        "NO CHORDS, RAPID MASHING, OR RELEASE TIMING.",
        "PRACTICE: PAUSE, THEN CHOOSE END SESSION."};
    const AFORC_Rect panel = centered_rect(layout, 56, 14);
    AFORC_Status status = surf_man_draw_panel(app, panel, "HELP");

    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(lines) / sizeof(lines[0]);
         ++index) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){panel.x + 3, panel.y + 2 + (int32_t)index},
            lines[index],
            index == 7U ? SURF_MAN_TONE_SIGNAL : SURF_MAN_TONE_INK,
            index == 7U ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){panel.x + 3, panel.y + 12},
            "ESC/ENTER/?/P BACK",
            SURF_MAN_TONE_FRAMEWORK,
            AFORC_STYLE_DIM);
    }
    return status;
}

static AFORC_Status render_accessibility(SurfManApp *app,
                                         const SurfManLayout *layout) {
    char rows[5][48];
    const AFORC_Rect panel = centered_rect(layout, 54, 11);
    AFORC_Status status = surf_man_draw_panel(app, panel, "ACCESSIBILITY");

    (void)snprintf(rows[0], sizeof(rows[0]), "SPEED             %u%%", app->settings.speed_percent);
    (void)snprintf(rows[1], sizeof(rows[1]), "TIMING WINDOW     %u%%", app->settings.timing_percent);
    (void)snprintf(rows[2], sizeof(rows[2]), "LANDING ASSIST    %s", app->settings.landing_assist ? "ON" : "OFF");
    (void)snprintf(rows[3], sizeof(rows[3]), "MOTION            %s", app->settings.reduced_motion ? "REDUCED" : "FULL");
    (void)snprintf(rows[4], sizeof(rows[4]), "COLOR             %s", color_name(app->settings.color_mode));
    for (size_t index = 0U; status == AFORC_OK && index < 5U; ++index) {
        const bool selected = (size_t)app->accessibility_item == index;
        const int32_t row = panel.y + 2 + (int32_t)index;

        status = surf_man_draw_char(app,
                                    (AFORC_Point){panel.x + 3, row},
                                    selected ? (uint32_t)'>' : (uint32_t)' ',
                                    selected ? SURF_MAN_TONE_SIGNAL
                                             : SURF_MAN_TONE_FRAMEWORK,
                                    selected ? AFORC_STYLE_BOLD
                                             : AFORC_STYLE_NONE);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){panel.x + 5, row},
                rows[index],
                selected ? SURF_MAN_TONE_INK : SURF_MAN_TONE_FRAMEWORK,
                selected ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE);
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){panel.x + 3, panel.y + 8},
            "UP/DOWN SELECT  LEFT/RIGHT CHANGE  ESC BACK",
            SURF_MAN_TONE_FRAMEWORK,
            AFORC_STYLE_DIM);
    }
    return status;
}

static AFORC_Status render_pause(SurfManApp *app,
                                  const SurfManLayout *layout) {
    static const char *const labels[] = {
        "RESUME", "HELP", "ACCESSIBILITY", "END SESSION", "QUIT"};
    const AFORC_Rect panel = centered_rect(layout, 48, 12);
    AFORC_Status status = surf_man_draw_panel(app, panel, "PAUSED");

    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(labels) / sizeof(labels[0]);
         ++index) {
        const bool selected = (size_t)app->pause_item == index;
        const int32_t row = panel.y + 2 + (int32_t)index;

        status = surf_man_draw_char(app,
                                    (AFORC_Point){panel.x + 3, row},
                                    selected ? (uint32_t)'>' : (uint32_t)' ',
                                    selected ? SURF_MAN_TONE_SIGNAL
                                             : SURF_MAN_TONE_FRAMEWORK,
                                    selected ? AFORC_STYLE_BOLD
                                             : AFORC_STYLE_NONE);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){panel.x + 5, row},
                labels[index],
                selected ? SURF_MAN_TONE_INK : SURF_MAN_TONE_FRAMEWORK,
                selected ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE);
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){panel.x + 10, panel.y + 8},
            "AUTHORITATIVE TIME STOPPED",
            SURF_MAN_TONE_SIGNAL,
            AFORC_STYLE_BOLD);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){panel.x + 3, panel.y + 9},
            "UP/DOWN SELECT  ENTER ACT  P/ESC RESUME",
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    return status;
}

AFORC_Status surf_man_render_overlay(SurfManApp *app,
                                     const SurfManLayout *layout) {
    switch (app->overlay) {
    case SURF_MAN_OVERLAY_NONE:
        return AFORC_OK;
    case SURF_MAN_OVERLAY_HELP:
        return render_help(app, layout);
    case SURF_MAN_OVERLAY_ACCESSIBILITY:
        return render_accessibility(app, layout);
    case SURF_MAN_OVERLAY_PAUSE:
        return render_pause(app, layout);
    case SURF_MAN_OVERLAY_RESIZE:
        return surf_man_render_resize(app,
                                      (AFORC_Size){layout->screen.width,
                                                   layout->screen.height});
    }
    return AFORC_ERROR_STATE;
}

AFORC_Status surf_man_render_phase_modal(SurfManApp *app,
                                         const SurfManLayout *layout) {
    AFORC_Rect panel;
    char text[96];
    AFORC_Status status;

    if (app->simulation.phase == SURF_MAN_COUNT_IN) {
        const uint32_t total = app->simulation.rules.count_in_ticks;
        const uint32_t remaining = app->simulation.phase_tick < total
                                       ? total - app->simulation.phase_tick
                                       : 0U;
        uint32_t count = 0U;

        if (total > 0U && remaining > 0U) {
            count = (remaining * 3U + total - 1U) / total;
        }
        panel = (AFORC_Rect){layout->screen.width - 23,
                             layout->play.y + 3,
                             22,
                             5};
        status = surf_man_draw_panel(app, panel, "COUNT-IN");
        (void)snprintf(text, sizeof(text), count == 0U ? "GO" : "%u", count);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 10,
                                                      panel.y + 2},
                                        text,
                                        SURF_MAN_TONE_SIGNAL,
                                        AFORC_STYLE_BOLD);
        }
        return status;
    }
    if (app->simulation.phase == SURF_MAN_WIPEOUT_RECOVERY) {
        panel = (AFORC_Rect){layout->screen.width - 35,
                             layout->play.y + 3,
                             34,
                             6};
        status = surf_man_draw_panel(app, panel, "WIPEOUT");
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 8,
                                                      panel.y + 2},
                                        "PENDING SCORE LOST",
                                        SURF_MAN_TONE_SIGNAL,
                                        AFORC_STYLE_BOLD);
        }
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 3,
                                                      panel.y + 4},
                                        "RECOVERY PAUSES WITH THE GAME",
                                        SURF_MAN_TONE_INK,
                                        AFORC_STYLE_NONE);
        }
        return status;
    }
    if (app->simulation.phase == SURF_MAN_WAVE_RECAP) {
        panel = centered_rect(layout, 46, 8);
        status = surf_man_draw_panel(app, panel, "WAVE RECAP");
        (void)snprintf(text,
                       sizeof(text),
                       "BANKED SCORE  %" PRIu64,
                       app->simulation.day_score);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 2},
                                        text,
                                        SURF_MAN_TONE_SIGNAL,
                                        AFORC_STYLE_BOLD);
        }
        (void)snprintf(text,
                       sizeof(text),
                       "MANEUVERS     %u",
                       app->simulation.maneuver_count);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 4},
                                        text,
                                        SURF_MAN_TONE_INK,
                                        AFORC_STYLE_NONE);
        }
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 6},
                                        "ENTER NEXT WAVE   ESC SHACK",
                                        SURF_MAN_TONE_FRAMEWORK,
                                        AFORC_STYLE_NONE);
        }
        return status;
    }
    if (app->simulation.phase == SURF_MAN_DAY_RECAP) {
        panel = centered_rect(layout, 46, 8);
        status = surf_man_draw_panel(app, panel, "DAY RECAP");
        (void)snprintf(text,
                       sizeof(text),
                       "DAY SCORE   %" PRIu64,
                       app->simulation.day_score);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 2},
                                        text,
                                        SURF_MAN_TONE_SIGNAL,
                                        AFORC_STYLE_BOLD);
        }
        (void)snprintf(text,
                       sizeof(text),
                       "BEST SCORE  %" PRIu64,
                       app->simulation.best_score);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 4},
                                        text,
                                        SURF_MAN_TONE_INK,
                                        AFORC_STYLE_NONE);
        }
        if (status == AFORC_OK) {
            status = surf_man_draw_text(app,
                                        (AFORC_Point){panel.x + 4,
                                                      panel.y + 6},
                                        "ENTER RETURN TO SHACK",
                                        SURF_MAN_TONE_FRAMEWORK,
                                        AFORC_STYLE_NONE);
        }
        return status;
    }
    return AFORC_OK;
}

AFORC_Status surf_man_render_resize(SurfManApp *app, AFORC_Size size) {
    static const char requirement[] = "RESIZE TERMINAL TO AT LEAST 60x20";
    char current[48];
    int32_t requirement_x;
    int32_t current_x;
    int32_t row;
    AFORC_Status status;

    if (app == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (size.width >= 24 && size.height >= 7) {
        const int32_t panel_width = size.width > 48 ? 48 : size.width - 2;
        const AFORC_Rect panel = {(size.width - panel_width) / 2,
                                  (size.height - 7) / 2,
                                  panel_width,
                                  7};
        const char *needed =
            panel_width >= (int32_t)sizeof(requirement) + 1
                ? requirement
                : "NEED 60x20";
        const char *paused =
            panel_width >= 23 ? "GAMEPLAY TIME PAUSED" : "GAMEPLAY PAUSED";

        status = surf_man_draw_panel(app, panel, "RESIZE");
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){centered_text_x(panel, needed), panel.y + 2},
                needed,
                SURF_MAN_TONE_SIGNAL,
                AFORC_STYLE_BOLD);
        }
        (void)snprintf(current,
                       sizeof(current),
                       "CURRENT %dx%d",
                       size.width,
                       size.height);
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){centered_text_x(panel, current), panel.y + 3},
                current,
                SURF_MAN_TONE_INK,
                AFORC_STYLE_NONE);
        }
        if (status == AFORC_OK) {
            status = surf_man_draw_text(
                app,
                (AFORC_Point){centered_text_x(panel, paused), panel.y + 5},
                paused,
                SURF_MAN_TONE_FRAMEWORK,
                AFORC_STYLE_NONE);
        }
        return status;
    }
    requirement_x = (size.width - (int32_t)(sizeof(requirement) - 1U)) / 2;
    if (requirement_x < 0) {
        requirement_x = 0;
    }
    row = size.height > 2 ? size.height / 2 - 1 : 0;
    status = surf_man_draw_text(app,
                                (AFORC_Point){requirement_x, row},
                                requirement,
                                SURF_MAN_TONE_SIGNAL,
                                AFORC_STYLE_BOLD);
    (void)snprintf(current,
                   sizeof(current),
                   "CURRENT %dx%d",
                   size.width,
                   size.height);
    current_x = (size.width - (int32_t)strlen(current)) / 2;
    if (current_x < 0) {
        current_x = 0;
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){current_x, row + 2},
                                    current,
                                    SURF_MAN_TONE_INK,
                                    AFORC_STYLE_NONE);
    }
    return status;
}
