/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "presentation_internal.h"

#include "surf_man/app.h"

#include <limits.h>

enum { SURF_MAN_EVENT_PARTICLE_LIMIT = 24 };

static uint32_t surf_man_visual_step_milliseconds(uint64_t visual_tick) {
    const uint32_t base = 1000U / SURF_MAN_VISUAL_HZ;
    const uint32_t remainder = 1000U % SURF_MAN_VISUAL_HZ;
    const uint32_t phase =
        (uint32_t)(visual_tick % SURF_MAN_VISUAL_HZ) * remainder;

    /*
     * Distribute the fractional millisecond remainder across fixed visual
     * ticks. The sequence sums to exactly one second without accumulating
     * floating-point drift in tweens or particle lifetimes.
     */
    return base + (phase + remainder >= SURF_MAN_VISUAL_HZ ? 1U : 0U);
}

static int32_t fixed_from_cell(int32_t cell) {
    const int64_t fixed = (int64_t)cell * AFORC_EFFECT_FIXED_ONE;

    if (fixed > INT32_MAX) {
        return INT32_MAX;
    }
    if (fixed < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)fixed;
}

static AFORC_ParticleI32Range fixed_velocity_range(int32_t minimum,
                                                   int32_t maximum) {
    return (AFORC_ParticleI32Range){fixed_from_cell(minimum),
                                    fixed_from_cell(maximum)};
}

static int32_t spray_surface_row(const SurfManSimulation *simulation,
                                 AFORC_Rect play) {
    int64_t face = simulation->face_q16;
    const int32_t maximum_rise = play.height > 7 ? play.height - 6 : 1;
    const int32_t minimum_row = play.y + 3;
    const int32_t maximum_row = play.y + play.height - 2;
    int32_t row;
    int32_t rise;

    if (face < 0) {
        face = -face;
    }
    if (face > (int64_t)SURF_MAN_Q16_ONE * SURF_MAN_VISUAL_FACE_UNITS) {
        face = (int64_t)SURF_MAN_Q16_ONE * SURF_MAN_VISUAL_FACE_UNITS;
    }
    rise = 1 + (int32_t)(face * (maximum_rise - 1) /
                          ((int64_t)SURF_MAN_Q16_ONE *
                           SURF_MAN_VISUAL_FACE_UNITS));
    row = play.y + play.height - 2 - rise;
    if (row < minimum_row) {
        return minimum_row;
    }
    if (row > maximum_row) {
        return maximum_row;
    }
    return row;
}

static int32_t spray_direction(const SurfManSimulation *simulation) {
    if (simulation->last_turn < 0) {
        return 1;
    }
    if (simulation->last_turn > 0) {
        return -1;
    }
    return simulation->face_velocity_q16 < 0 ? -1 : 1;
}

AFORC_Status surf_man_visuals_init(SurfManVisuals *visuals, uint32_t seed) {
    AFORC_Status status;

    if (visuals == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *visuals = (SurfManVisuals){0};
    status = aforc_particle_pool_init(&visuals->particle_pool,
                                      visuals->particles,
                                      SURF_MAN_PARTICLE_CAPACITY,
                                      seed);
    if (status == AFORC_OK) {
        status = aforc_tween_init(&visuals->title_tween,
                                  0.0,
                                  1.0,
                                  600U,
                                  AFORC_EASING_CUBIC_OUT);
    }
    if (status == AFORC_OK) {
        status = aforc_tween_init(&visuals->score_tween,
                                  0.0,
                                  1.0,
                                  300U,
                                  AFORC_EASING_QUADRATIC_OUT);
    }
    if (status != AFORC_OK) {
        surf_man_visuals_dispose(visuals);
        return status;
    }
    visuals->dirty = true;
    visuals->initialized = true;
    return AFORC_OK;
}

void surf_man_visuals_dispose(SurfManVisuals *visuals) {
    if (visuals == NULL) {
        return;
    }
    aforc_tween_dispose(&visuals->score_tween);
    aforc_tween_dispose(&visuals->title_tween);
    aforc_particle_pool_dispose(&visuals->particle_pool);
    *visuals = (SurfManVisuals){0};
}

AFORC_Status surf_man_visuals_step(SurfManApp *app) {
    SurfManVisuals *visuals;
    uint32_t milliseconds;
    AFORC_Status status;

    if (app == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    visuals = &app->visuals;
    if (!visuals->initialized) {
        return AFORC_ERROR_STATE;
    }
    milliseconds = surf_man_visual_step_milliseconds(visuals->visual_tick);
    if (app->settings.reduced_motion && visuals->particle_pool.active_count > 0U) {
        status = aforc_particle_pool_clear(&visuals->particle_pool);
    } else {
        status = aforc_particle_pool_update(&visuals->particle_pool,
                                            milliseconds);
    }
    if (status == AFORC_OK && !visuals->title_tween.finished) {
        status = aforc_tween_update(&visuals->title_tween, milliseconds);
    }
    if (status == AFORC_OK && !visuals->score_tween.finished) {
        status = aforc_tween_update(&visuals->score_tween, milliseconds);
    }
    if (status != AFORC_OK) {
        return status;
    }
    ++visuals->visual_tick;
    visuals->dirty = true;
    return AFORC_OK;
}

void surf_man_visuals_mark_dirty(SurfManVisuals *visuals) {
    if (visuals != NULL) {
        visuals->dirty = true;
    }
}

AFORC_Status surf_man_emit_spray(SurfManApp *app, bool wipeout) {
    AFORC_Size size;
    SurfManLayout layout;
    AFORC_Cell cells[3];
    AFORC_ParticleEmitter emitter;
    size_t available;
    size_t requested;
    size_t spawned = 0U;
    uint32_t primary_glyph;
    uint32_t secondary_glyph;
    uint32_t detail_glyph;
    int32_t direction;
    int32_t origin_x;
    int32_t origin_y;
    int32_t speed_cells;
    AFORC_Status status;

    if (app == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!app->visuals.initialized) {
        return AFORC_ERROR_STATE;
    }
    if (app->settings.reduced_motion) {
        if (app->visuals.particle_pool.active_count == 0U) {
            return AFORC_OK;
        }
        status = aforc_particle_pool_clear(&app->visuals.particle_pool);
        if (status == AFORC_OK) {
            app->visuals.dirty = true;
        }
        return status;
    }
    size = aforc_renderer_size(app->renderer);
    origin_x = size.width / 3;
    origin_y = size.height / 2;
    if (size.width >= SURF_MAN_MIN_COLUMNS && size.height >= SURF_MAN_MIN_ROWS) {
        layout = surf_man_layout_for_size(size);
        origin_x = layout.play.x + layout.play.width / 3;
        origin_y = spray_surface_row(&app->simulation, layout.play);
    }
    direction = spray_direction(&app->simulation);
    speed_cells = app->simulation.speed_q16 / SURF_MAN_Q16_ONE;
    if (speed_cells < 2) {
        speed_cells = 2;
    } else if (speed_cells > 8) {
        speed_cells = 8;
    }

    emitter = (AFORC_ParticleEmitter){0};
    emitter.x = direction < 0
                    ? (AFORC_ParticleI32Range){fixed_from_cell(origin_x - 2),
                                               fixed_from_cell(origin_x)}
                    : (AFORC_ParticleI32Range){fixed_from_cell(origin_x),
                                               fixed_from_cell(origin_x + 2)};
    emitter.y = (AFORC_ParticleI32Range){fixed_from_cell(origin_y - 1),
                                         fixed_from_cell(origin_y)};
    emitter.velocity_x =
        direction < 0
            ? fixed_velocity_range(-(speed_cells + 6), -(speed_cells + 2))
            : fixed_velocity_range(speed_cells + 2, speed_cells + 6);
    emitter.velocity_y = fixed_velocity_range(-6, -2);
    emitter.acceleration_x = (AFORC_ParticleI32Range){0, 0};
    emitter.acceleration_y = fixed_velocity_range(7, 10);
    emitter.lifetime_ms = (AFORC_ParticleU32Range){180U, 440U};
    primary_glyph = direction < 0 ? (uint32_t)'\\' : (uint32_t)'/';
    secondary_glyph = (uint32_t)'*';
    detail_glyph = (uint32_t)'.';
    requested = 10U;

    if (wipeout) {
        emitter.x = (AFORC_ParticleI32Range){fixed_from_cell(origin_x - 2),
                                             fixed_from_cell(origin_x + 2)};
        emitter.velocity_x = fixed_velocity_range(-12, 12);
        emitter.velocity_y = fixed_velocity_range(-10, -2);
        emitter.acceleration_x = fixed_velocity_range(-2, 2);
        emitter.acceleration_y = fixed_velocity_range(10, 15);
        emitter.lifetime_ms = (AFORC_ParticleU32Range){360U, 760U};
        primary_glyph = (uint32_t)'x';
        secondary_glyph = (uint32_t)'+';
        detail_glyph = (uint32_t)'*';
        requested = 24U;
    } else if (app->simulation.last_maneuver == SURF_MAN_MANEUVER_LIP_SNAP) {
        emitter.x = (AFORC_ParticleI32Range){fixed_from_cell(origin_x - 1),
                                             fixed_from_cell(origin_x + 1)};
        emitter.velocity_x = direction < 0
                                 ? fixed_velocity_range(-9, -3)
                                 : fixed_velocity_range(3, 9);
        emitter.velocity_y = fixed_velocity_range(-11, -6);
        emitter.acceleration_y = fixed_velocity_range(10, 13);
        emitter.lifetime_ms = (AFORC_ParticleU32Range){280U, 600U};
        primary_glyph = (uint32_t)'^';
        requested = 14U;
    } else if (app->simulation.last_maneuver == SURF_MAN_MANEUVER_AIR) {
        emitter.x = (AFORC_ParticleI32Range){fixed_from_cell(origin_x - 2),
                                             fixed_from_cell(origin_x + 2)};
        emitter.velocity_x = fixed_velocity_range(-8, 8);
        emitter.velocity_y = fixed_velocity_range(-7, -3);
        emitter.acceleration_y = fixed_velocity_range(10, 14);
        emitter.lifetime_ms = (AFORC_ParticleU32Range){260U, 620U};
        primary_glyph = (uint32_t)'!';
        requested = 16U;
    } else if (app->simulation.last_maneuver == SURF_MAN_MANEUVER_TUBE) {
        emitter.x = (AFORC_ParticleI32Range){fixed_from_cell(origin_x - 3),
                                             fixed_from_cell(origin_x - 1)};
        emitter.velocity_x = fixed_velocity_range(-(speed_cells + 5),
                                                   -(speed_cells + 1));
        emitter.velocity_y = fixed_velocity_range(-2, 1);
        emitter.acceleration_y = fixed_velocity_range(2, 4);
        emitter.lifetime_ms = (AFORC_ParticleU32Range){320U, 700U};
        primary_glyph = (uint32_t)'=';
        secondary_glyph = (uint32_t)'~';
        requested = 12U;
    }

    cells[0] = surf_man_tone_cell(app,
                                  primary_glyph,
                                  SURF_MAN_TONE_SIGNAL,
                                  AFORC_STYLE_BOLD);
    cells[1] = surf_man_tone_cell(app,
                                  secondary_glyph,
                                  SURF_MAN_TONE_INK,
                                  AFORC_STYLE_BOLD);
    cells[2] = surf_man_tone_cell(app,
                                  detail_glyph,
                                  SURF_MAN_TONE_FRAMEWORK,
                                  AFORC_STYLE_DIM);
    emitter.cells = cells;
    emitter.cell_count = sizeof(cells) / sizeof(cells[0]);
    if (requested > SURF_MAN_EVENT_PARTICLE_LIMIT) {
        requested = SURF_MAN_EVENT_PARTICLE_LIMIT;
    }
    if (app->visuals.particle_pool.active_count >
        app->visuals.particle_pool.capacity) {
        return AFORC_ERROR_STATE;
    }
    available = app->visuals.particle_pool.capacity -
                app->visuals.particle_pool.active_count;
    if (requested > available) {
        requested = available;
    }
    if (requested == 0U) {
        return AFORC_OK;
    }
    status = aforc_particle_pool_emit(&app->visuals.particle_pool,
                                      &emitter,
                                      requested,
                                      &spawned);
    if (status == AFORC_OK && spawned > 0U) {
        app->visuals.dirty = true;
    }
    return status;
}
