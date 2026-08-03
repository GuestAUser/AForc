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

static const char *line_name(const SurfManSimulation *simulation) {
    if (simulation->line_position_q16 < -SURF_MAN_Q16_ONE / 2) {
        return "LEFT";
    }
    if (simulation->line_position_q16 > SURF_MAN_Q16_ONE / 2) {
        return "RIGHT";
    }
    return "CENTER";
}

static const char *face_name(const SurfManSimulation *simulation) {
    const int64_t air_threshold_q16 =
        (int64_t)simulation->rules.air_face_threshold_q16 * 100 /
        simulation->settings.timing_percent;
    const int64_t hazard_threshold_q16 =
        (int64_t)simulation->rules.hazard_face_threshold_q16 * 100 /
        simulation->settings.timing_percent;

    if (simulation->wave_face_offset_q16 >= air_threshold_q16) {
        return "HIGH";
    }
    if (simulation->wave_face_offset_q16 < hazard_threshold_q16) {
        return "LOW";
    }
    return "MID";
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
        return "GET READY. RIDE CONTROLS START AT GO.";
    case SURF_MAN_RIDING:
        return "CLIMB, DROP, THEN REVERSE MOMENTUM TO CARVE.";
    case SURF_MAN_WIPEOUT_RECOVERY:
        return "WIPEOUT: PENDING SCORE LOST.";
    case SURF_MAN_WAVE_RECAP:
        return "WAVE COMPLETE. REVIEW AND CONTINUE.";
    case SURF_MAN_DAY_RECAP:
        return "DAY COMPLETE. RETURN TO THE SHACK.";
    case SURF_MAN_PRACTICE:
        return "PRACTICE: P/ESC PAUSE, THEN END SESSION.";
    }
    return "";
}

static const char *hud_message(const SurfManApp *app) {
    switch (app->simulation.phase) {
    case SURF_MAN_COUNT_IN:
    case SURF_MAN_RIDING:
    case SURF_MAN_WIPEOUT_RECOVERY:
    case SURF_MAN_PRACTICE:
        return app->simulation.award[0] == '\0'
                   ? default_message(app->simulation.phase)
                   : app->simulation.award;
    case SURF_MAN_SHACK:
    case SURF_MAN_WAVE_RECAP:
    case SURF_MAN_DAY_RECAP:
        return app->message[0] == '\0'
                   ? default_message(app->simulation.phase)
                   : app->message;
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

static AFORC_Status draw_bank_progress(SurfManApp *app,
                                       AFORC_Point position,
                                       bool compact) {
    char prefix[48];
    char suffix[48];
    const uint32_t delay = app->simulation.rules.bank_delay_ticks;
    const uint32_t ticks = app->simulation.bank_ticks < delay
                               ? app->simulation.bank_ticks
                               : delay;
    const uint32_t progress = app->simulation.pending_score == 0U
                                  ? 0U
                                  : delay - ticks;
    const uint32_t filled =
        delay == 0U ? 0U : (uint32_t)((uint64_t)progress * 5U / delay);
    const uint32_t percent =
        delay == 0U ? 0U : (uint32_t)((uint64_t)progress * 100U / delay);
    AFORC_Status status;

    if (compact) {
        (void)snprintf(prefix, sizeof(prefix), "BANK [");
    } else {
        (void)snprintf(prefix,
                       sizeof(prefix),
                       "PENDING +%" PRIu64 "  BANK [",
                       app->simulation.pending_score);
    }
    status = surf_man_draw_text(
        app, position, prefix, SURF_MAN_TONE_INK, AFORC_STYLE_NONE);
    for (uint32_t index = 0U; status == AFORC_OK && index < 5U; ++index) {
        const bool active = index < filled;

        status = surf_man_draw_char(
            app,
            (AFORC_Point){position.x + (int32_t)strlen(prefix) +
                              (int32_t)index,
                          position.y},
            active ? (uint32_t)'=' : (uint32_t)'-',
            active ? SURF_MAN_TONE_SIGNAL : SURF_MAN_TONE_FRAMEWORK,
            active ? AFORC_STYLE_BOLD : AFORC_STYLE_DIM);
    }
    if (compact) {
        (void)snprintf(suffix,
                       sizeof(suffix),
                       "] %u%% P+%" PRIu64,
                       percent,
                       app->simulation.pending_score);
    } else {
        (void)snprintf(suffix, sizeof(suffix), "] %u%%", percent);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){position.x + (int32_t)strlen(prefix) + 5,
                          position.y},
            suffix,
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    return status;
}

static AFORC_Status render_shack_hud(SurfManApp *app,
                                      const SurfManLayout *layout) {
    char primary[96];
    char settings[128];
    const char *message = hud_message(app);
    AFORC_Status status;

    (void)snprintf(primary,
                   sizeof(primary),
                   layout->hud.width >= 70
                       ? "SHACK MENU  BEST SCORE %" PRIu64
                       : "SHACK MENU  BEST %" PRIu64,
                   app->simulation.best_score);
    status = surf_man_draw_text(app,
                                (AFORC_Point){layout->hud.x + 1,
                                              layout->hud.y},
                                primary,
                                SURF_MAN_TONE_INK,
                                AFORC_STYLE_BOLD);
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){layout->hud.x + 1, layout->hud.y + 1},
            "CHOOSE SURF DAY OR PRACTICE",
            SURF_MAN_TONE_SIGNAL,
            AFORC_STYLE_BOLD);
    }
    if (layout->hud.width >= 70) {
        (void)snprintf(settings,
                       sizeof(settings),
                       "SPEED %u%%  TIMING %u%%  ASSIST %s  MOTION %s  COLOR %s",
                       app->settings.speed_percent,
                       app->settings.timing_percent,
                       app->settings.landing_assist ? "ON" : "OFF",
                       app->settings.reduced_motion ? "REDUCED" : "FULL",
                       color_name(app->settings.color_mode));
    } else {
        (void)snprintf(settings,
                       sizeof(settings),
                       "SPD %u%% TIMING %u%% A:%s M:%s C:%s",
                       app->settings.speed_percent,
                       app->settings.timing_percent,
                       app->settings.landing_assist ? "ON" : "OFF",
                       app->settings.reduced_motion ? "RED" : "FULL",
                       color_name(app->settings.color_mode));
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 2},
                                    settings,
                                    SURF_MAN_TONE_INK,
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
    if (status == AFORC_OK) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){layout->hud.x + 1, layout->hud.y + 4},
            "UP/DOWN SELECT  ENTER START  ? HELP  Q QUIT",
            SURF_MAN_TONE_INK,
            AFORC_STYLE_NONE);
    }
    return status;
}

static AFORC_Status contextual_action(const SurfManApp *app,
                                       const char **out_action) {
    SurfManWaveSample sample;
    AFORC_Status status;

    if (app->simulation.phase != SURF_MAN_RIDING &&
        app->simulation.phase != SURF_MAN_PRACTICE) {
        *out_action = "WAIT FOR GO";
        return AFORC_OK;
    }
    if (app->simulation.airborne) {
        *out_action = app->simulation.grabbed ? "AIR: HOLD THE GRAB"
                                              : "AIR: SPACE GRAB";
        return AFORC_OK;
    }
    status = surf_man_wave_sample(&app->simulation,
                                  app->simulation.line_position_q16,
                                  &sample);
    if (status != AFORC_OK) {
        return status;
    }
    if (sample.hazard) {
        *out_action = "HAZARD: CLIMB OR CHANGE LINE";
    } else if (sample.tube) {
        *out_action = app->simulation.tube_ticks == 0U
                          ? "TUBE: TAP SPACE TO COMMIT"
                          : "TUBE: COMMITTED - HOLD LINE";
    } else if (sample.lip) {
        const int64_t threshold_q16 =
            (int64_t)app->simulation.rules.air_face_threshold_q16 * 100 /
            app->simulation.settings.timing_percent;

        *out_action = app->simulation.wave_face_offset_q16 >= threshold_q16
                          ? "LIP: SPACE LAUNCH"
                          : "LIP: SPACE SNAP";
    } else if (app->simulation.pending_score > 0U) {
        *out_action = "BANK: HOLD A CLEAN LINE";
    } else {
        *out_action = "ACTION: SPACE AT LIP OR TUBE";
    }
    return AFORC_OK;
}

static size_t gauge_index(int32_t value, int32_t limit, size_t slots) {
    int64_t bounded = value;
    int64_t scaled;

    if (slots < 2U || limit <= 0) {
        return 0U;
    }
    if (bounded < -(int64_t)limit) {
        bounded = -(int64_t)limit;
    } else if (bounded > limit) {
        bounded = limit;
    }
    scaled = (bounded + limit) * (int64_t)(slots - 1U);
    scaled = (scaled + limit) / ((int64_t)limit * 2);
    return (size_t)scaled;
}

static void format_gauge(char *gauge,
                         size_t slots,
                         int32_t value,
                         int32_t limit,
                         int32_t input,
                         char negative,
                         char positive) {
    const size_t marker = gauge_index(value, limit, slots);
    char glyph = 'O';

    if (input < 0) {
        glyph = negative;
    } else if (input > 0) {
        glyph = positive;
    }
    for (size_t index = 0U; index < slots; ++index) {
        gauge[index] = index == marker ? glyph : '-';
    }
    gauge[slots] = '\0';
}

AFORC_Status surf_man_render_hud(SurfManApp *app,
                                  const SurfManLayout *layout) {
    char primary[128];
    char state[128];
    char context[128];
    char line_gauge[10];
    char face_gauge[6];
    const uint32_t seconds =
        (app->simulation.wave_ticks_remaining + SURF_MAN_FIXED_HZ - 1U) /
        SURF_MAN_FIXED_HZ;
    const int64_t speed_tenths =
        (int64_t)app->simulation.speed_q16 * 10 / SURF_MAN_Q16_ONE;
    const char *message = hud_message(app);
    const char *action = "";
    AFORC_Status status;

    if (app->simulation.phase == SURF_MAN_SHACK) {
        return render_shack_hud(app, layout);
    }

    if (app->simulation.phase == SURF_MAN_PRACTICE &&
        layout->hud.width >= 70) {
        (void)snprintf(primary,
                       sizeof(primary),
                       "PRACTICE  %-8s  TIME %02us  SCORE %" PRIu64
                       "  BEST %" PRIu64,
                       wave_name(app->simulation.wave_kind),
                       seconds,
                       app->simulation.day_score,
                       app->simulation.best_score);
    } else if (app->simulation.phase == SURF_MAN_PRACTICE) {
        (void)snprintf(primary,
                       sizeof(primary),
                       "PRACTICE %s TIME %us SCORE %" PRIu64,
                       wave_name(app->simulation.wave_kind),
                       seconds,
                       app->simulation.day_score);
    } else if (layout->hud.width >= 70) {
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
    format_gauge(line_gauge,
                 sizeof(line_gauge) - 1U,
                 app->simulation.line_position_q16,
                 app->simulation.rules.line_position_limit_q16,
                 app->controls.horizontal,
                 '<',
                 '>');
    format_gauge(face_gauge,
                 sizeof(face_gauge) - 1U,
                 app->simulation.wave_face_offset_q16,
                 app->simulation.rules.wave_face_offset_limit_q16,
                 app->controls.vertical,
                 '^',
                 'v');
    if (layout->hud.width >= 70) {
        (void)snprintf(state,
                       sizeof(state),
                       "LINE [%s] %-6s  FACE [%s] %-4s  SPEED %" PRId64
                       ".%" PRId64,
                       line_gauge,
                       line_name(&app->simulation),
                       face_gauge,
                       face_name(&app->simulation),
                       speed_tenths / 10,
                       speed_tenths < 0 ? -(speed_tenths % 10)
                                         : speed_tenths % 10);
    } else {
        (void)snprintf(state,
                       sizeof(state),
                       "LINE[%s] FACE[%s] SPD %" PRId64 ".%" PRId64,
                       line_gauge,
                       face_gauge,
                       speed_tenths / 10,
                       speed_tenths < 0 ? -(speed_tenths % 10)
                                         : speed_tenths % 10);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 1},
                                    state,
                                    SURF_MAN_TONE_INK,
                                    AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = draw_flow_meter(app,
                                 (AFORC_Point){layout->hud.x + 1,
                                               layout->hud.y + 2});
    }
    if (status == AFORC_OK) {
        status = draw_bank_progress(
            app,
            (AFORC_Point){layout->hud.x + 20, layout->hud.y + 2},
            layout->hud.width < 70);
    }
    status = status == AFORC_OK ? contextual_action(app, &action) : status;
    (void)snprintf(context,
                   sizeof(context),
                   "RISK %s | %s",
                   app->simulation.risk_active ? "ACTIVE" : "CLEAR",
                   action);
    if (status == AFORC_OK) {
        status = surf_man_draw_char(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 3},
                                    (uint32_t)'>',
                                    SURF_MAN_TONE_SIGNAL,
                                    AFORC_STYLE_BOLD);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 3,
                                                  layout->hud.y + 3},
                                    context,
                                    SURF_MAN_TONE_INK,
                                    AFORC_STYLE_BOLD);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_text(app,
                                    (AFORC_Point){layout->hud.x + 1,
                                                  layout->hud.y + 4},
                                    message,
                                    SURF_MAN_TONE_INK,
                                    AFORC_STYLE_NONE);
    }
    return status;
}
