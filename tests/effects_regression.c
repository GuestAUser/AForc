/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/effects.h"

#include <stdio.h>

typedef struct PlotCapture {
    AFORC_Point positions[8];
    AFORC_Cell cells[8];
    size_t count;
} PlotCapture;

static AFORC_Status capture_plot(void *context,
                                 AFORC_Point position,
                                 AFORC_Cell cell)
{
    PlotCapture *capture = context;

    if (capture->count == sizeof(capture->positions) /
                              sizeof(capture->positions[0])) {
        return AFORC_ERROR_LIMIT;
    }
    capture->positions[capture->count] = position;
    capture->cells[capture->count] = cell;
    ++capture->count;
    return AFORC_OK;
}

static AFORC_Cell test_cell(uint32_t codepoint)
{
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    return cell;
}

static bool sprite_animation_and_tween_are_deterministic(void)
{
    const AFORC_Cell cells[] = {test_cell((uint32_t)'A'),
                                test_cell((uint32_t)'B')};
    AFORC_Sprite sprite;
    AFORC_SpriteDrawOptions options = aforc_sprite_draw_options_default();
    AFORC_AnimationFrame frames[2];
    AFORC_Animation animation;
    AFORC_Tween tween;
    PlotCapture capture = {0};
    double sample = 0.0;

    if (aforc_sprite_init(&sprite, cells, (AFORC_Size){2, 1}, 2U) !=
        AFORC_OK) {
        return false;
    }
    options.transform.position = (AFORC_Point){3, 4};
    options.transform.rotation = AFORC_SPRITE_ROTATION_90;
    if (aforc_sprite_draw(&sprite, &options, capture_plot, &capture) !=
            AFORC_OK ||
        capture.count != 2U || capture.positions[0].x != 3 ||
        capture.positions[0].y != 4 ||
        capture.cells[0].codepoint != (uint32_t)'A' ||
        capture.positions[1].x != 3 || capture.positions[1].y != 5 ||
        capture.cells[1].codepoint != (uint32_t)'B') {
        return false;
    }

    frames[0] = (AFORC_AnimationFrame){&sprite, 100U};
    frames[1] = (AFORC_AnimationFrame){&sprite, 200U};
    if (aforc_animation_init(&animation, frames, 2U,
                             AFORC_ANIMATION_LOOP) != AFORC_OK ||
        aforc_animation_update(&animation, 350U) != AFORC_OK ||
        animation.frame_index != 0U || animation.frame_elapsed_ms != 50U ||
        animation.finished) {
        return false;
    }

    if (aforc_tween_init(&tween, 0.0, 10.0, 1000U,
                         AFORC_EASING_QUADRATIC_IN) != AFORC_OK ||
        aforc_tween_update(&tween, 500U) != AFORC_OK ||
        aforc_tween_sample(&tween, &sample) != AFORC_OK || sample != 2.5) {
        return false;
    }
    aforc_tween_dispose(&tween);
    aforc_animation_dispose(&animation);
    aforc_sprite_dispose(&sprite);
    return true;
}

static bool particle_emission_preserves_order(void)
{
    AFORC_Particle storage[5];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 1000U, test_cell((uint32_t)'x')};
    const AFORC_Cell emitted_cells[] = {test_cell((uint32_t)'*')};
    const AFORC_ParticleEmitter emitter = {
        {0, 0}, {0, 0}, {0, 0}, {0, 0},
        {0, 0}, {0, 0}, {250U, 250U}, emitted_cells, 1U};
    size_t spawned = 0U;

    if (aforc_particle_pool_init(&pool, storage, 5U, UINT32_C(1)) !=
        AFORC_OK) {
        return false;
    }
    for (size_t index = 0U; index < 3U; ++index) {
        description.cell.codepoint = (uint32_t)'a' + (uint32_t)index;
        if (aforc_particle_pool_spawn(&pool, &description, NULL) != AFORC_OK) {
            return false;
        }
    }
    if (aforc_particle_pool_kill(&pool, 1U) != AFORC_OK ||
        aforc_particle_pool_emit(&pool, &emitter, 4U, &spawned) !=
            AFORC_ERROR_LIMIT ||
        spawned != 3U || pool.active_count != 5U) {
        return false;
    }
    if (!storage[0].active || storage[0].cell.codepoint != (uint32_t)'a' ||
        !storage[1].active || storage[1].cell.codepoint != (uint32_t)'*' ||
        !storage[2].active || storage[2].cell.codepoint != (uint32_t)'c' ||
        !storage[3].active || storage[3].cell.codepoint != (uint32_t)'*' ||
        !storage[4].active || storage[4].cell.codepoint != (uint32_t)'*') {
        return false;
    }
    return true;
}

static bool particle_update_uses_semi_implicit_euler(void)
{
    AFORC_Particle storage[1];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0},
        {AFORC_EFFECT_FIXED_ONE, 0},
        {AFORC_EFFECT_FIXED_ONE, 0},
        1000U,
        test_cell((uint32_t)'*')};
    AFORC_ParticleDrawOptions options = aforc_particle_draw_options_default();
    PlotCapture capture = {0};

    if (aforc_particle_pool_init(&pool, storage, 1U, UINT32_C(7)) !=
            AFORC_OK ||
        aforc_particle_pool_spawn(&pool, &description, NULL) != AFORC_OK ||
        aforc_particle_pool_update(&pool, 500U) != AFORC_OK ||
        storage[0].velocity.x != AFORC_EFFECT_FIXED_ONE +
                                     AFORC_EFFECT_FIXED_ONE / 2 ||
        storage[0].position.x != AFORC_EFFECT_FIXED_ONE * 3 / 4 ||
        storage[0].age_ms != 500U) {
        return false;
    }
    options.offset = (AFORC_Point){2, 3};
    if (aforc_particle_pool_draw(&pool, &options, capture_plot, &capture) !=
            AFORC_OK ||
        capture.count != 1U || capture.positions[0].x != 2 ||
        capture.positions[0].y != 3) {
        return false;
    }
    if (aforc_particle_pool_update(&pool, 500U) != AFORC_OK ||
        storage[0].active || pool.active_count != 0U ||
        storage[0].age_ms != storage[0].lifetime_ms) {
        return false;
    }
    return true;
}

int main(void)
{
    if (!sprite_animation_and_tween_are_deterministic()) {
        (void)fputs("sprite/animation/tween regression failed\n", stderr);
        return 1;
    }
    if (!particle_emission_preserves_order()) {
        (void)fputs("particle emission regression failed\n", stderr);
        return 2;
    }
    if (!particle_update_uses_semi_implicit_euler()) {
        (void)fputs("particle update regression failed\n", stderr);
        return 3;
    }
    (void)puts("effects regression: ok");
    return 0;
}
