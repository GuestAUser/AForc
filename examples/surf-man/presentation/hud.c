/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

#include <inttypes.h>
#include <stdio.h>

static const char *wave_name(SurfManWaveKind kind) {
    switch (kind) {
    case SURF_MAN_WAVE_OPEN:
        return "OPEN";
    case SURF_MAN_WAVE_STEEP:
        return "STEEP";
    case SURF_MAN_WAVE_TUBE:
        return "TUBE";
    case SURF_MAN_WAVE_CHOP:
        return "CHOP";
    case SURF_MAN_WAVE_CLOSEOUT:
        return "CLOSEOUT";
    }
    return "UNKNOWN";
}

static const char *maneuver_name(SurfManManeuver maneuver) {
    switch (maneuver) {
    case SURF_MAN_MANEUVER_NONE:
        return "LINE";
    case SURF_MAN_MANEUVER_CARVE_LEFT:
        return "LEFT CARVE";
    case SURF_MAN_MANEUVER_CARVE_RIGHT:
        return "RIGHT CARVE";
    case SURF_MAN_MANEUVER_LIP_SNAP:
        return "LIP SNAP";
    case SURF_MAN_MANEUVER_AIR:
        return "AIR";
    case SURF_MAN_MANEUVER_TUBE:
        return "TUBE";
    }
    return "UNKNOWN";
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

static const char *default_message(SurfManPhase phase) {
    switch (phase) {
    case SURF_MAN_SHACK:
        return "SELECT A SESSION. ENTER CONFIRMS.";
    case SURF_MAN_COUNT_IN:
        return "SET YOUR LINE.";
    case SURF_MAN_RIDING:
        return "BANK MANEUVERS BEFORE THE SECTION CLOSES.";
    case SURF_MAN_WIPEOUT_RECOVERY:
        return "WIPEOUT: PENDING SCORE LOST.";
    case SURF_MAN_WAVE_RECAP:
        return "WAVE COMPLETE. REVIEW AND CONTINUE.";
    case SURF_MAN_DAY_RECAP:
        return "DAY COMPLETE. RETURN TO THE SHACK.";
    case SURF_MAN_PRACTICE:
        return "PRACTICE: WIPEOUTS ARE SAFE; ESC PAUSES.";
    }
    return "";
}

static AFORC_Status draw_flow_meter(SurfManApp *app,
                                    AFORC_Point position) {
    char value[16];
    AFORC_Status status = surf_man_draw_text(app,
                                             position,
                                             "FLOW [",
                                             SURF_MAN_TONE_INK,
                                             AFORC_STYLE_NONE);

    for (uint32_t index = 0U;
         status == AFORC_OK && index < SURF_MAN_FLOW_MAX;
         ++index) {
        const bool filled = index < app->simulation.flow;

        status = surf_man_draw_char(
            app,
            (AFORC_Point){position.x + 6 + (int32_t)index, position.y},
            filled ? (uint32_t)'=' : (uint32_t)'-',
            filled ? SURF_MAN_TONE_SIGNAL : SURF_MAN_TONE_FRAMEWORK,
            filled ? AFORC_STYLE_BOLD : AFORC_STYLE_DIM);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(
            app,
            (AFORC_Point){position.x + 6 + SURF_MAN_FLOW_MAX, position.y},
            (uint32_t)']',
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    (void)snprintf(value,
                   sizeof(value),
                   " %u/%u",
                   (unsigned int)app->simulation.flow,
                   (unsigned int)SURF_MAN_FLOW_MAX);
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){position.x + 7 + SURF_MAN_FLOW_MAX, position.y},
            value,
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    return status;
}

AFORC_Status surf_man_render_hud(SurfManApp *app,
                                  const SurfManLayout *layout) {
    char primary[128];
    char secondary[96];
    char state[128];
    char accessibility[128];
    const uint32_t seconds =
        (app->simulation.wave_ticks_remaining + SURF_MAN_FIXED_HZ - 1U) /
        SURF_MAN_FIXED_HZ;
    const int64_t speed_tenths =
        (int64_t)app->simulation.speed_q16 * 10 / SURF_MAN_Q16_ONE;
    const char *message = app->message[0] == '\0'
                              ? default_message(app->simulation.phase)
                              : app->message;
    AFORC_Status status;

    if (layout->hud.width >= 70) {
        (void)snprintf(primary,
                       sizeof(primary),
                       "DAY %u  WAVE %u/%u  %-8s  TIME %02us  SCORE %" PRIu64
                       "  BEST %" PRIu64,
                       app->simulation.day,
                       app->simulation.wave,
                       (unsigned int)SURF_MAN_WAVES_PER_DAY,
                       wave_name(app->simulation.wave_kind),
                       seconds,
                       app->simulation.day_score,
                       app->simulation.best_score);
    } else {
        (void)snprintf(primary,
                       sizeof(primary),
                       "DAY %u WAVE %u/%u %s TIME %us SCORE %" PRIu64,
                       app->simulation.day,
                       app->simulation.wave,
                       (unsigned int)SURF_MAN_WAVES_PER_DAY,
                       wave_name(app->simulation.wave_kind),
                       seconds,
                       app->simulation.day_score);
    }
    status = surf_man_draw_text(app,
                                (AFORC_Point){layout->hud.x + 1,
                                              layout->hud.y},
                                primary,
                                SURF_MAN_TONE_INK,
                                AFORC_STYLE_BOLD);
    if (status == AFORC_OK) {
        status = draw_flow_meter(
            app, (AFORC_Point){layout->hud.x + 1, layout->hud.y + 1});
    }
    (void)snprintf(secondary,
                   sizeof(secondary),
                   "SPEED %" PRId64 ".%" PRId64 "  PENDING +%" PRIu64,
                   speed_tenths / 10,
                   speed_tenths < 0 ? -(speed_tenths % 10)
                                     : speed_tenths % 10,
                   app->simulation.pending_score);
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){layout->hud.x + 20, layout->hud.y + 1},
            secondary,
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    if (layout->hud.width >= 70) {
        (void)snprintf(state,
                       sizeof(state),
                       "STATE %s  MOVE %s  RISK %s  AIR %s  TUBE %u.%us",
                       surf_man_phase_name(app->simulation.phase),
                       maneuver_name(app->simulation.last_maneuver),
                       app->simulation.risk_active ? "ACTIVE" : "CLEAR",
                       app->simulation.airborne ? "YES" : "NO",
                       app->simulation.tube_ticks / SURF_MAN_FIXED_HZ,
                       (app->simulation.tube_ticks % SURF_MAN_FIXED_HZ) * 10U /
                           SURF_MAN_FIXED_HZ);
    } else {
        (void)snprintf(state,
                       sizeof(state),
                       "STATE %s MOVE %s RISK:%s AIR:%s TUBE:%u.%us",
                       surf_man_phase_name(app->simulation.phase),
                       maneuver_name(app->simulation.last_maneuver),
                       app->simulation.risk_active ? "ACTIVE" : "CLEAR",
                       app->simulation.airborne ? "YES" : "NO",
                       app->simulation.tube_ticks / SURF_MAN_FIXED_HZ,
                       (app->simulation.tube_ticks % SURF_MAN_FIXED_HZ) * 10U /
                           SURF_MAN_FIXED_HZ);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 2},
                                    state,
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 3},
                                    (uint32_t)'!',
                                    SURF_MAN_TONE_SIGNAL,
                                    AFORC_STYLE_BOLD);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 3,
                                                  layout->hud.y + 3},
                                    message,
                                    SURF_MAN_TONE_INK,
                                    AFORC_STYLE_BOLD);
    }
    if (layout->hud.width >= 70) {
        (void)snprintf(accessibility,
                       sizeof(accessibility),
                       "ARROWS/WASD MOVE SPACE ? P | S%u T%u L:%s M:%s C:%s",
                       app->settings.speed_percent,
                       app->settings.timing_percent,
                       app->settings.landing_assist ? "ON" : "OFF",
                       app->settings.reduced_motion ? "RED" : "FULL",
                       color_name(app->settings.color_mode));
    } else {
        (void)snprintf(accessibility,
                       sizeof(accessibility),
                       "MOVE WASD SPACE ? P | S%u T%u L:%s M:%s C:%s",
                       app->settings.speed_percent,
                       app->settings.timing_percent,
                       app->settings.landing_assist ? "ON" : "OFF",
                       app->settings.reduced_motion ? "RED" : "FULL",
                       color_name(app->settings.color_mode));
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 4},
                                    accessibility,
                                    SURF_MAN_TONE_FRAMEWORK,
                                    AFORC_STYLE_DIM);
    }
    return status;
}
