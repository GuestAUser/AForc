/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

enum {
    WAVE_FOCAL_CORE_RADIUS = 3,
    WAVE_FOCAL_FADE_RADIUS = 7
};

static int32_t clamp_i32(int32_t value, int32_t minimum, int32_t maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int32_t offset_q16(int32_t columns) {
    const int64_t value = (int64_t)columns * SURF_MAN_Q16_ONE / 2;

    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static uint32_t wave_glyph(const SurfManWaveSample *sample,
                           int32_t previous_row,
                           int32_t row,
                           int32_t next_row) {
    if (sample->hazard) {
        return (uint32_t)'+';
    }
    if (sample->tube) {
        return (uint32_t)'=';
    }
    if (sample->pocket) {
        return (uint32_t)'#';
    }
    if (sample->lip) {
        return (uint32_t)'^';
    }
    if (sample->foam) {
        return (uint32_t)'*';
    }
    if (row < previous_row && row < next_row) {
        return (uint32_t)'^';
    }
    if (row > previous_row && row > next_row) {
        return (uint32_t)'_';
    }
    if (next_row < row || previous_row > row) {
        return (uint32_t)'/';
    }
    if (next_row > row || previous_row < row) {
        return (uint32_t)'\\';
    }
    if (sample->slope_q16 >= SURF_MAN_Q16_ONE / 8) {
        return (uint32_t)'/';
    }
    if (sample->slope_q16 <= -SURF_MAN_Q16_ONE / 8) {
        return (uint32_t)'\\';
    }
    return (uint32_t)'-';
}

static SurfManTone wave_tone(const SurfManWaveSample *sample) {
    if (sample->hazard) {
        return SURF_MAN_TONE_SIGNAL;
    }
    if (sample->pocket || sample->tube) {
        return SURF_MAN_TONE_INK;
    }
    if (sample->lip) {
        return SURF_MAN_TONE_SIGNAL;
    }
    return SURF_MAN_TONE_FRAMEWORK;
}

static uint64_t decorative_phase(const SurfManApp *app, uint32_t period) {
    if (app->settings.reduced_motion) {
        return 0U;
    }
    return app->visuals.visual_tick / period;
}

static uint64_t wave_motion_phase(const SurfManApp *app,
                                  uint32_t slowest_period) {
    const uint32_t speed_units = (uint32_t)clamp_i32(
        app->simulation.speed_q16 / SURF_MAN_Q16_ONE, 0, 8);
    const uint32_t period = slowest_period > speed_units
                                ? slowest_period - speed_units
                                : 1U;

    if (app->settings.reduced_motion) {
        return 0U;
    }
    return app->visuals.visual_tick / period;
}

static AFORC_Status draw_pattern_row(SurfManApp *app,
                                     const SurfManLayout *layout,
                                     int32_t row,
                                     const char *pattern,
                                     uint64_t phase,
                                     SurfManTone tone,
                                     AFORC_CellStyle style) {
    const size_t pattern_length = strlen(pattern);
    const size_t phase_offset = (size_t)(phase % pattern_length);
    AFORC_Status status = AFORC_OK;

    if (row < layout->play.y ||
        row >= layout->play.y + layout->play.height) {
        return AFORC_OK;
    }
    for (int32_t column = 0;
         status == AFORC_OK && column < layout->play.width;
         ++column) {
        const size_t index = ((size_t)column + phase_offset) % pattern_length;
        const unsigned char glyph = (unsigned char)pattern[index];

        if (glyph != (unsigned char)' ') {
            status = surf_man_draw_char(
                app,
                (AFORC_Point){layout->play.x + column, row},
                glyph,
                tone,
                style);
        }
    }
    return status;
}

static uint32_t wave_depth_glyph(const SurfManWaveSample *sample,
                                 size_t depth,
                                 int32_t column,
                                 uint64_t phase) {
    const uint64_t flow_position = (uint64_t)(uint32_t)column + phase +
                                   depth * 5U;
    const uint64_t flow_period = 13U + depth * 4U;

    if (sample->hazard) {
        static const char hazard[] = "|V^";

        return (uint32_t)(unsigned char)hazard[depth - 1U];
    }
    if (sample->tube) {
        static const char tube[] = "#=:";

        return (uint32_t)(unsigned char)tube[depth - 1U];
    }
    if (sample->pocket) {
        static const char pocket[] = "#:.";

        return (uint32_t)(unsigned char)pocket[depth - 1U];
    }
    if (sample->lip) {
        static const char lip[] = "|:.";

        return (uint32_t)(unsigned char)lip[depth - 1U];
    }
    if (sample->foam) {
        static const char foam[] = "o:.";

        return (uint32_t)(unsigned char)foam[depth - 1U];
    }
    if (flow_position % flow_period >= 3U) {
        return (uint32_t)' ';
    }
    if (sample->slope_q16 >= SURF_MAN_Q16_ONE / 8) {
        return (uint32_t)'/';
    }
    if (sample->slope_q16 <= -SURF_MAN_Q16_ONE / 8) {
        return (uint32_t)'\\';
    }
    return (uint32_t)'-';
}

static SurfManTone wave_depth_tone(const SurfManWaveSample *sample) {
    if (sample->hazard) {
        return SURF_MAN_TONE_SIGNAL;
    }
    if (sample->tube || sample->pocket) {
        return SURF_MAN_TONE_INK;
    }
    if (sample->lip) {
        return SURF_MAN_TONE_SIGNAL;
    }
    return SURF_MAN_TONE_FRAMEWORK;
}

static bool wave_texture_visible(int32_t distance,
                                 int32_t column,
                                 size_t depth) {
    uint32_t stride;

    if (distance <= WAVE_FOCAL_CORE_RADIUS) {
        return false;
    }
    if (distance >= WAVE_FOCAL_FADE_RADIUS) {
        return true;
    }
    /* Restore face density in widening bands outside the rider's clear core. */
    stride = (uint32_t)(WAVE_FOCAL_FADE_RADIUS + 1 - distance);
    return ((uint64_t)(uint32_t)column + depth * 2U) % stride == 0U;
}

static uint32_t wave_break_glyph(const SurfManWaveSample *previous_sample,
                                 const SurfManWaveSample *sample,
                                 const SurfManWaveSample *next_sample,
                                 int32_t column,
                                 uint64_t phase) {
    const bool breaking = sample->lip || sample->foam;
    const bool adjoining_break = previous_sample->lip ||
                                 previous_sample->foam || next_sample->lip ||
                                 next_sample->foam;

    if (!breaking) {
        return adjoining_break ? (uint32_t)'.' : (uint32_t)' ';
    }
    if (sample->foam) {
        static const char foam[] = "o*o";
        const size_t index = ((size_t)column +
                              (size_t)(phase % (sizeof(foam) - 1U))) %
                             (sizeof(foam) - 1U);

        return (uint32_t)(unsigned char)foam[index];
    }
    return (uint32_t)'.';
}

static AFORC_Status draw_wave_column(SurfManApp *app,
                                     const SurfManLayout *layout,
                                     const SurfManWaveSample *previous_sample,
                                     const SurfManWaveSample *sample,
                                     const SurfManWaveSample *next_sample,
                                     int32_t column,
                                     int32_t previous_row,
                                     int32_t row,
                                     int32_t next_row,
                                     int32_t rider_column,
                                     uint64_t texture_phase,
                                     uint64_t wake_phase) {
    const int32_t speed_units = clamp_i32(
        app->simulation.speed_q16 / SURF_MAN_Q16_ONE, 0, 8);
    const int32_t wake_length = 6 + speed_units;
    const int32_t wake_distance = rider_column - column;
    const int32_t focal_offset = column - rider_column;
    const int32_t focal_distance =
        focal_offset < 0 ? -focal_offset : focal_offset;
    const int32_t crest_row = row - 1;
    AFORC_Status status = AFORC_OK;

    for (size_t depth = 1U; status == AFORC_OK && depth <= 3U; ++depth) {
        const int32_t texture_row = row + (int32_t)depth;
        uint32_t glyph;
        AFORC_CellStyle style = AFORC_STYLE_DIM;

        if (!wave_texture_visible(focal_distance, column, depth)) {
            continue;
        }
        glyph = wave_depth_glyph(sample, depth, column, texture_phase);
        if (texture_row >= layout->separator_y || glyph == (uint32_t)' ') {
            continue;
        }
        if (sample->hazard || (sample->tube && depth < 3U)) {
            style = AFORC_STYLE_BOLD;
        }
        status = surf_man_draw_char(
            app,
            (AFORC_Point){layout->play.x + column, texture_row},
            glyph,
            wave_depth_tone(sample),
            style);
    }
    if (status == AFORC_OK &&
        wake_distance > WAVE_FOCAL_CORE_RADIUS &&
        wake_distance <= wake_length && crest_row > layout->play.y + 2) {
        static const char wake[] = ".o. ";
        const size_t index = ((size_t)wake_distance +
                              (size_t)(wake_phase % (sizeof(wake) - 1U))) %
                             (sizeof(wake) - 1U);
        const unsigned char glyph = (unsigned char)wake[index];

        if (glyph != (unsigned char)' ') {
            status = surf_man_draw_char(
                app,
                (AFORC_Point){layout->play.x + column, crest_row},
                glyph,
                SURF_MAN_TONE_FRAMEWORK,
                AFORC_STYLE_DIM);
        }
    }
    if (status == AFORC_OK &&
        focal_distance > WAVE_FOCAL_CORE_RADIUS &&
        crest_row > layout->play.y + 2) {
        const uint32_t break_glyph = wave_break_glyph(previous_sample,
                                                      sample,
                                                      next_sample,
                                                      column,
                                                      texture_phase);

        if (break_glyph != (uint32_t)' ') {
            status = surf_man_draw_char(
                app,
                (AFORC_Point){layout->play.x + column, crest_row},
                break_glyph,
                sample->lip ? SURF_MAN_TONE_SIGNAL
                            : SURF_MAN_TONE_FRAMEWORK,
                AFORC_STYLE_DIM);
        }
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(
            app,
            (AFORC_Point){layout->play.x + column, row},
            wave_glyph(sample, previous_row, row, next_row),
            wave_tone(sample),
            sample->hazard || sample->lip || sample->tube
                ? AFORC_STYLE_BOLD
                : AFORC_STYLE_NONE);
    }
    return status;
}

AFORC_Status surf_man_render_shack_art(SurfManApp *app,
                                       const SurfManLayout *layout) {
    static const char *const shack[] = {
        "        /\\        ",
        "   ____/  \\____   ",
        "  /  SURF SHACK  \\  ",
        " +---------------+ ",
        " | BOARDS  /==== | ",
        " | WAX     [*]   | ",
        " +---------------+ "};
    static const char horizon[] = "--  .   --   .  ";
    static const char rear_swell[] = "_____/~~\\____________";
    static const char shore_break[] = "~=-~~--=~-";
    const uint64_t motion_phase = decorative_phase(app, 6U);
    const uint64_t horizon_phase = decorative_phase(app, 18U);
    const uint64_t rear_phase = decorative_phase(app, 9U);
    const uint64_t shore_phase = decorative_phase(app, 4U);
    const int32_t art_x = layout->play.x + 2;
    const int32_t art_y = layout->play.y + 1;
    const int32_t water_y = layout->play.y + layout->play.height - 2;
    AFORC_Status status = AFORC_OK;

    for (size_t row = 0U;
         status == AFORC_OK && row < sizeof(shack) / sizeof(shack[0]);
         ++row) {
        status = surf_man_draw_text(
            app,
            (AFORC_Point){art_x, art_y + (int32_t)row},
            shack[row],
            row == 2U ? SURF_MAN_TONE_INK : SURF_MAN_TONE_FRAMEWORK,
            row == 2U ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(
            app,
            (AFORC_Point){art_x + 18, art_y},
            motion_phase % 2U == 0U ? (uint32_t)'>' : (uint32_t)'-',
            SURF_MAN_TONE_FRAMEWORK,
            AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = surf_man_draw_char(
            app,
            (AFORC_Point){layout->play.x + layout->play.width / 2,
                          layout->play.y + 1},
            (uint32_t)'o',
            SURF_MAN_TONE_INK,
            AFORC_STYLE_BOLD);
    }
    if (status == AFORC_OK) {
        status = draw_pattern_row(app,
                                  layout,
                                  water_y - 2,
                                  horizon,
                                  horizon_phase,
                                  SURF_MAN_TONE_FRAMEWORK,
                                  AFORC_STYLE_DIM);
    }
    if (status == AFORC_OK) {
        status = draw_pattern_row(app,
                                  layout,
                                  water_y - 1,
                                  rear_swell,
                                  rear_phase,
                                  SURF_MAN_TONE_FRAMEWORK,
                                  AFORC_STYLE_NONE);
    }
    if (status == AFORC_OK) {
        status = draw_pattern_row(app,
                                  layout,
                                  water_y,
                                  shore_break,
                                  shore_phase,
                                  SURF_MAN_TONE_INK,
                                  AFORC_STYLE_NONE);
    }
    return status;
}

AFORC_Status surf_man_render_wave_art(SurfManApp *app,
                                       const SurfManLayout *layout) {
    static const char horizon[] = "---   .   --    . ";
    static const char rear_swell[] = "_____/~~~\\_____________";
    const int32_t wave_anchor_column = layout->play.width / 3;
    const int32_t rider_column =
        surf_man_rider_center_x(&app->simulation, layout->play) -
        layout->play.x;
    const uint64_t horizon_phase = wave_motion_phase(app, 24U);
    const uint64_t rear_phase = wave_motion_phase(app, 16U);
    const uint64_t texture_phase = wave_motion_phase(app, 12U);
    const uint64_t wake_phase = wave_motion_phase(app, 10U);
    SurfManWaveSample previous_sample;
    SurfManWaveSample sample;
    SurfManWaveSample next_sample;
    SurfManWaveSample rider_sample;
    bool rider_sample_ready = false;
    AFORC_Status status;

    status = surf_man_draw_text(
        app,
        (AFORC_Point){layout->play.x + 1, layout->play.y},
        "W/S FACE   A/D LINE   SPACE ACTION   ? HELP",
        SURF_MAN_TONE_FRAMEWORK,
        AFORC_STYLE_DIM);
    if (status == AFORC_OK) {
        status = draw_pattern_row(app,
                                  layout,
                                  layout->play.y + 1,
                                  horizon,
                                  horizon_phase,
                                  SURF_MAN_TONE_FRAMEWORK,
                                  AFORC_STYLE_DIM);
    }
    if (status == AFORC_OK) {
        status = draw_pattern_row(app,
                                  layout,
                                  layout->play.y + 2,
                                  rear_swell,
                                  rear_phase,
                                  SURF_MAN_TONE_FRAMEWORK,
                                  AFORC_STYLE_DIM);
    }
    if (status == AFORC_OK) {
        status = surf_man_wave_sample(
            &app->simulation,
            offset_q16(-wave_anchor_column - 1),
            &previous_sample);
    }
    if (status == AFORC_OK) {
        status = surf_man_wave_sample(
            &app->simulation, offset_q16(-wave_anchor_column), &sample);
    }
    for (int32_t column = 0;
         status == AFORC_OK && column < layout->play.width;
         ++column) {
        const int32_t relative_column = column - wave_anchor_column;
        int32_t previous_row;
        int32_t row;
        int32_t next_row;

        status = surf_man_wave_sample(&app->simulation,
                                      offset_q16(relative_column + 1),
                                      &next_sample);
        if (status != AFORC_OK) {
            break;
        }
        previous_row =
            surf_man_wave_surface_row(&previous_sample, layout->play);
        row = surf_man_wave_surface_row(&sample, layout->play);
        next_row = surf_man_wave_surface_row(&next_sample, layout->play);
        status = draw_wave_column(app,
                                  layout,
                                  &previous_sample,
                                  &sample,
                                  &next_sample,
                                  column,
                                  previous_row,
                                  row,
                                  next_row,
                                   rider_column,
                                   texture_phase,
                                   wake_phase);
        if (column == rider_column) {
            rider_sample = sample;
            rider_sample_ready = true;
        }
        previous_sample = sample;
        sample = next_sample;
    }
    if (status == AFORC_OK && !rider_sample_ready) {
        status = AFORC_ERROR_STATE;
    }
    if (status == AFORC_OK) {
        AFORC_ParticleDrawOptions options =
            aforc_particle_draw_options_default();

        options.clip = layout->play;
        options.clip_enabled = true;
        status = aforc_particle_pool_draw(&app->visuals.particle_pool,
                                           &options,
                                           surf_man_plot_particle_cell,
                                           app);
    }
    if (status == AFORC_OK) {
        status = surf_man_render_rider(
            app,
            layout,
            surf_man_wave_surface_row(&rider_sample, layout->play));
    }
    return status;
}
