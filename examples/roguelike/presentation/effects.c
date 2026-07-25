/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

AFORC_Status game_emit_burst(Game *game, AFORC_Point point, bool strong) {
    AFORC_Cell cells[3];
    AFORC_ParticleEmitter emitter;
    size_t spawned = 0U;
    const size_t burst_count = strong ? 12U : 7U;
    int32_t x = point.x * AFORC_EFFECT_FIXED_ONE;
    int32_t y = point.y * AFORC_EFFECT_FIXED_ONE;
    AFORC_Status status;

    cells[0] = game_cell((uint32_t)'*',
                         aforc_color_indexed(strong ? 196U : 214U),
                         AFORC_STYLE_BOLD);
    cells[1] = game_cell((uint32_t)'+',
                         aforc_color_indexed(strong ? 203U : 220U),
                         AFORC_STYLE_BOLD);
    cells[2] = game_cell((uint32_t)'.',
                         aforc_color_indexed(215U),
                         AFORC_STYLE_DIM);
    emitter.x = (AFORC_ParticleI32Range){x, x};
    emitter.y = (AFORC_ParticleI32Range){y, y};
    emitter.velocity_x =
        (AFORC_ParticleI32Range){-3 * AFORC_EFFECT_FIXED_ONE,
                                 3 * AFORC_EFFECT_FIXED_ONE};
    emitter.velocity_y =
        (AFORC_ParticleI32Range){-2 * AFORC_EFFECT_FIXED_ONE,
                                 2 * AFORC_EFFECT_FIXED_ONE};
    emitter.acceleration_x = (AFORC_ParticleI32Range){0, 0};
    emitter.acceleration_y = (AFORC_ParticleI32Range){0,
                                                       AFORC_EFFECT_FIXED_ONE};
    emitter.lifetime_ms = (AFORC_ParticleU32Range){180U, 520U};
    emitter.cells = cells;
    emitter.cell_count = sizeof(cells) / sizeof(cells[0]);
    status = aforc_particle_pool_emit(&game->particle_pool,
                                      &emitter,
                                      burst_count,
                                      &spawned);
    if (status == AFORC_ERROR_LIMIT) {
        return AFORC_OK;
    }
    return status;
}

AFORC_Status game_scene_fixed_update(AFORC_Scene *scene,
                                     AFORC_Engine *engine,
                                     double seconds,
                                     AFORC_Error *error) {
    Game *game = scene->user_data;
    uint32_t milliseconds = (uint32_t)(seconds * 1000.0 + 0.5);
    AFORC_Status status;

    (void)engine;
    if (milliseconds == 0U) {
        milliseconds = 1U;
    }
    status = aforc_particle_pool_update(&game->particle_pool, milliseconds);
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "effects",
                          "particle update failed");
    }
    return AFORC_OK;
}

AFORC_Status game_scene_update(AFORC_Scene *scene,
                               AFORC_Engine *engine,
                               double seconds,
                               AFORC_Error *error) {
    Game *game = scene->user_data;
    uint64_t milliseconds = (uint64_t)(seconds * 1000.0 + 0.5);
    AFORC_Status status;

    (void)engine;
    status = aforc_tween_update(&game->exit_tween, milliseconds);
    if (status == AFORC_OK && game->exit_tween.finished) {
        status = aforc_tween_restart(&game->exit_tween);
    }
    if (status != AFORC_OK) {
        return game_error(error,
                          status,
                          "effects",
                          "exit tween update failed");
    }
    return AFORC_OK;
}
