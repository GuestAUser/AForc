/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/effects.h"

#include <stdio.h>
#include <string.h>

typedef struct PlotCapture
{
    AFORC_Point positions[8];
    AFORC_Cell cells[8];
    size_t count;
} PlotCapture;

static AFORC_Status
capture_plot(void *context, AFORC_Point position, AFORC_Cell cell)
{
    PlotCapture *capture = context;

    if (capture->count ==
        sizeof(capture->positions) / sizeof(capture->positions[0]))
    {
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

    if (aforc_sprite_init(&sprite, cells, (AFORC_Size){2, 1}, 2U) != AFORC_OK)
    {
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
        capture.cells[1].codepoint != (uint32_t)'B')
    {
        return false;
    }

    frames[0] = (AFORC_AnimationFrame){&sprite, 100U};
    frames[1] = (AFORC_AnimationFrame){&sprite, 200U};
    if (aforc_animation_init(&animation, frames, 2U, AFORC_ANIMATION_LOOP) !=
            AFORC_OK ||
        aforc_animation_update(&animation, 350U) != AFORC_OK ||
        animation.frame_index != 0U || animation.frame_elapsed_ms != 50U ||
        animation.finished)
    {
        return false;
    }

    if (aforc_tween_init(&tween, 0.0, 10.0, 1000U, AFORC_EASING_QUADRATIC_IN) !=
            AFORC_OK ||
        aforc_tween_update(&tween, 500U) != AFORC_OK ||
        aforc_tween_sample(&tween, &sample) != AFORC_OK || sample != 2.5)
    {
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
    const AFORC_ParticleEmitter emitter = {{0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {250U, 250U},
                                           emitted_cells,
                                           1U};
    size_t spawned = 0U;

    if (aforc_particle_pool_init(&pool, storage, 5U, UINT32_C(1)) != AFORC_OK)
    {
        return false;
    }
    for (size_t index = 0U; index < 3U; ++index)
    {
        description.cell.codepoint = (uint32_t)'a' + (uint32_t)index;
        if (aforc_particle_pool_spawn(&pool, &description, NULL) != AFORC_OK)
        {
            return false;
        }
    }
    if (aforc_particle_pool_kill(&pool, 1U) != AFORC_OK ||
        aforc_particle_pool_emit(&pool, &emitter, 4U, &spawned) !=
            AFORC_ERROR_LIMIT ||
        spawned != 3U || pool.active_count != 5U)
    {
        return false;
    }
    if (!storage[0].active || storage[0].cell.codepoint != (uint32_t)'a' ||
        !storage[1].active || storage[1].cell.codepoint != (uint32_t)'*' ||
        !storage[2].active || storage[2].cell.codepoint != (uint32_t)'c' ||
        !storage[3].active || storage[3].cell.codepoint != (uint32_t)'*' ||
        !storage[4].active || storage[4].cell.codepoint != (uint32_t)'*')
    {
        return false;
    }
    return true;
}

static bool particle_state_outputs_reject_aliases(void)
{
    AFORC_Particle storage[2];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 1000U, test_cell((uint32_t)'x')};
    const AFORC_Cell emitted_cells[] = {test_cell((uint32_t)'*')};
    const AFORC_ParticleEmitter emitter = {{0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {250U, 250U},
                                           emitted_cells,
                                           1U};
    size_t spawned = 0U;

    if (aforc_particle_pool_init(&pool, storage, 2U, UINT32_C(1)) != AFORC_OK ||
        aforc_particle_pool_spawn(&pool, &description, &pool.active_count) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        pool.active_count != 0U || pool.capacity != 2U || storage[0].active ||
        aforc_particle_pool_emit(&pool, &emitter, 1U, &pool.capacity) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        pool.active_count != 0U || pool.capacity != 2U || storage[0].active ||
        aforc_particle_pool_spawn(&pool, &description, &pool.free_head) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        pool.active_count != 0U || pool.free_head != 0U ||
        aforc_particle_pool_emit(&pool, &emitter, 1U, &pool.used_high_water) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        pool.active_count != 0U || pool.used_high_water != 0U ||
        aforc_particle_pool_emit(&pool, &emitter, 1U, &spawned) != AFORC_OK ||
        spawned != 1U || pool.active_count != 1U || !storage[0].active)
    {
        return false;
    }
    return true;
}

static bool particle_bitmap_outputs_reject_aliases(void)
{
    enum
    {
        BITMAP_CAPACITY = 67
    };
    AFORC_Particle storage[BITMAP_CAPACITY];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 1000U, test_cell((uint32_t)'x')};
    const AFORC_Cell emitted_cells[] = {test_cell((uint32_t)'*')};
    const AFORC_ParticleEmitter emitter = {{0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {0, 0},
                                           {250U, 250U},
                                           emitted_cells,
                                           1U};
    size_t first_word;
    size_t second_word;
    size_t particle_index = SIZE_MAX;
    size_t spawned = 0U;

    if (aforc_particle_pool_init(
            &pool, storage, BITMAP_CAPACITY, UINT32_C(1)) != AFORC_OK)
    {
        return false;
    }
    first_word = storage[0].free_bits;
    second_word = storage[1].free_bits;
    if (aforc_particle_pool_spawn(&pool, &description, &storage[0].free_bits) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        aforc_particle_pool_emit(&pool, &emitter, 1U, &storage[1].free_bits) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        pool.active_count != 0U || pool.free_head != 0U ||
        pool.used_high_water != 0U || storage[0].active || storage[1].active ||
        storage[0].free_bits != first_word ||
        storage[1].free_bits != second_word ||
        aforc_particle_pool_spawn(&pool, &description, &particle_index) !=
            AFORC_OK ||
        particle_index != 0U ||
        aforc_particle_pool_emit(&pool, &emitter, 1U, &spawned) != AFORC_OK ||
        spawned != 1U || pool.active_count != 2U)
    {
        return false;
    }
    return true;
}

static bool particle_bookkeeping_preserves_stable_indices(void)
{
    AFORC_Particle storage[5];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 1000U, test_cell((uint32_t)'a')};
    size_t indices[4] = {SIZE_MAX, SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t reused = SIZE_MAX;
    size_t free_head;
    size_t used_high_water;

    if (aforc_particle_pool_init(&pool, storage, 5U, UINT32_C(7)) != AFORC_OK ||
        pool.free_head != 0U || pool.used_high_water != 0U)
    {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index)
    {
        description.cell.codepoint = (uint32_t)'a' + (uint32_t)index;
        if (aforc_particle_pool_spawn(&pool, &description, &indices[index]) !=
                AFORC_OK ||
            indices[index] != index)
        {
            return false;
        }
    }
    if (pool.used_high_water != 4U ||
        aforc_particle_pool_kill(&pool, 3U) != AFORC_OK ||
        aforc_particle_pool_kill(&pool, 1U) != AFORC_OK ||
        aforc_particle_pool_kill(&pool, 1U) != AFORC_OK ||
        pool.active_count != 2U || !storage[0].active || storage[1].active ||
        !storage[2].active || storage[3].active || pool.free_head != 1U ||
        pool.used_high_water != 3U)
    {
        return false;
    }
    description.cell.codepoint = (uint32_t)'x';
    if (aforc_particle_pool_spawn(&pool, &description, &reused) != AFORC_OK ||
        reused != 1U || !storage[0].active ||
        storage[0].cell.codepoint != (uint32_t)'a' || !storage[2].active ||
        storage[2].cell.codepoint != (uint32_t)'c' ||
        aforc_particle_pool_spawn(&pool, &description, &reused) != AFORC_OK ||
        reused != 3U || pool.active_count != 4U || pool.used_high_water != 4U)
    {
        return false;
    }
    free_head = pool.free_head;
    used_high_water = pool.used_high_water;
    if (aforc_particle_pool_reseed(&pool, UINT32_C(99)) != AFORC_OK ||
        pool.free_head != free_head ||
        pool.used_high_water != used_high_water ||
        pool.random_state != UINT32_C(99))
    {
        return false;
    }
    if (aforc_particle_pool_clear(&pool) != AFORC_OK ||
        pool.active_count != 0U || pool.free_head != 0U ||
        pool.used_high_water != 0U || storage[0].active || storage[1].active ||
        storage[2].active || storage[3].active || storage[4].active ||
        aforc_particle_pool_spawn(&pool, &description, &reused) != AFORC_OK ||
        reused != 0U || pool.used_high_water != 1U)
    {
        return false;
    }
    return true;
}

static bool particle_update_uses_semi_implicit_euler(void)
{
    AFORC_Particle storage[1];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {{0, 0},
                                      {AFORC_EFFECT_FIXED_ONE, 0},
                                      {AFORC_EFFECT_FIXED_ONE, 0},
                                      1000U,
                                      test_cell((uint32_t)'*')};
    AFORC_ParticleDrawOptions options = aforc_particle_draw_options_default();
    PlotCapture capture = {0};

    if (aforc_particle_pool_init(&pool, storage, 1U, UINT32_C(7)) != AFORC_OK ||
        aforc_particle_pool_spawn(&pool, &description, NULL) != AFORC_OK ||
        aforc_particle_pool_update(&pool, 500U) != AFORC_OK ||
        storage[0].velocity.x !=
            AFORC_EFFECT_FIXED_ONE + AFORC_EFFECT_FIXED_ONE / 2 ||
        storage[0].position.x != AFORC_EFFECT_FIXED_ONE * 3 / 4 ||
        storage[0].age_ms != 500U)
    {
        return false;
    }
    options.offset = (AFORC_Point){2, 3};
    if (aforc_particle_pool_draw(&pool, &options, capture_plot, &capture) !=
            AFORC_OK ||
        capture.count != 1U || capture.positions[0].x != 2 ||
        capture.positions[0].y != 3)
    {
        return false;
    }
    if (aforc_particle_pool_update(&pool, 500U) != AFORC_OK ||
        storage[0].active || pool.active_count != 0U ||
        storage[0].age_ms != storage[0].lifetime_ms)
    {
        return false;
    }
    return true;
}

static bool particle_expiry_reuses_lowest_indices(void)
{
    AFORC_Particle storage[4];
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 100U, test_cell((uint32_t)'x')};
    size_t particle_index = SIZE_MAX;

    if (aforc_particle_pool_init(&pool, storage, 4U, UINT32_C(5)) != AFORC_OK)
    {
        return false;
    }
    for (size_t index = 0U; index < 4U; ++index)
    {
        description.lifetime_ms = (index & 1U) == 0U ? 100U : 1U;
        if (aforc_particle_pool_spawn(&pool, &description, &particle_index) !=
                AFORC_OK ||
            particle_index != index)
        {
            return false;
        }
    }
    description.lifetime_ms = 100U;
    if (aforc_particle_pool_update(&pool, 1U) != AFORC_OK ||
        !storage[0].active || storage[1].active || !storage[2].active ||
        storage[3].active || pool.active_count != 2U || pool.free_head != 1U ||
        pool.used_high_water != 3U ||
        aforc_particle_pool_spawn(&pool, &description, &particle_index) !=
            AFORC_OK ||
        particle_index != 1U ||
        aforc_particle_pool_spawn(&pool, &description, &particle_index) !=
            AFORC_OK ||
        particle_index != 3U || pool.active_count != 4U ||
        pool.used_high_water != 4U)
    {
        return false;
    }
    return true;
}

static bool particle_free_list_matches_slot_zero_model(void)
{
    enum
    {
        MODEL_CAPACITY = 67,
        MODEL_STEPS = 2000
    };
    AFORC_Particle storage[MODEL_CAPACITY];
    bool active[MODEL_CAPACITY] = {false};
    AFORC_ParticlePool pool;
    AFORC_ParticleDesc description = {
        {0, 0}, {0, 0}, {0, 0}, 1000U, test_cell((uint32_t)'x')};
    uint32_t state = UINT32_C(0x4d595df4);

    if (aforc_particle_pool_init(&pool, storage, MODEL_CAPACITY, UINT32_C(1)) !=
        AFORC_OK)
    {
        return false;
    }
    for (size_t step = 0U; step < MODEL_STEPS; ++step)
    {
        size_t active_count = 0U;
        size_t expected_free = MODEL_CAPACITY;
        size_t expected_high_water = 0U;

        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        if (state % 7U < 3U)
        {
            size_t actual_index = SIZE_MAX;
            size_t expected_index = 0U;

            while (expected_index < MODEL_CAPACITY && active[expected_index])
            {
                ++expected_index;
            }
            if (expected_index == MODEL_CAPACITY)
            {
                if (aforc_particle_pool_spawn(
                        &pool, &description, &actual_index) !=
                    AFORC_ERROR_LIMIT)
                {
                    return false;
                }
            }
            else
            {
                if (aforc_particle_pool_spawn(
                        &pool, &description, &actual_index) != AFORC_OK ||
                    actual_index != expected_index)
                {
                    return false;
                }
                active[actual_index] = true;
            }
        }
        else if (state % 7U < 6U)
        {
            const size_t index = (size_t)(state % MODEL_CAPACITY);

            if (aforc_particle_pool_kill(&pool, index) != AFORC_OK)
            {
                return false;
            }
            active[index] = false;
        }
        else if ((state & UINT32_C(1)) == 0U)
        {
            if (aforc_particle_pool_clear(&pool) != AFORC_OK)
            {
                return false;
            }
            (void)memset(active, 0, sizeof(active));
        }
        else if (aforc_particle_pool_reseed(&pool, state) != AFORC_OK)
        {
            return false;
        }

        for (size_t index = 0U; index < MODEL_CAPACITY; ++index)
        {
            if (storage[index].active != active[index])
            {
                return false;
            }
            if (active[index])
            {
                ++active_count;
                expected_high_water = index + 1U;
            }
            else if (expected_free == MODEL_CAPACITY)
            {
                expected_free = index;
            }
        }
        if (pool.active_count != active_count ||
            pool.used_high_water != expected_high_water ||
            pool.free_head !=
                (expected_free == MODEL_CAPACITY ? SIZE_MAX : expected_free))
        {
            return false;
        }
    }
    return true;
}

int main(void)
{
    if (!sprite_animation_and_tween_are_deterministic())
    {
        (void)fputs("sprite/animation/tween regression failed\n", stderr);
        return 1;
    }
    if (!particle_emission_preserves_order())
    {
        (void)fputs("particle emission regression failed\n", stderr);
        return 2;
    }
    if (!particle_state_outputs_reject_aliases())
    {
        (void)fputs("particle state output alias regression failed\n", stderr);
        return 3;
    }
    if (!particle_bitmap_outputs_reject_aliases())
    {
        (void)fputs("particle bitmap output alias regression failed\n", stderr);
        return 4;
    }
    if (!particle_update_uses_semi_implicit_euler())
    {
        (void)fputs("particle update regression failed\n", stderr);
        return 5;
    }
    if (!particle_bookkeeping_preserves_stable_indices())
    {
        (void)fputs("particle bookkeeping regression failed\n", stderr);
        return 6;
    }
    if (!particle_free_list_matches_slot_zero_model())
    {
        (void)fputs("particle free-list model regression failed\n", stderr);
        return 7;
    }
    if (!particle_expiry_reuses_lowest_indices())
    {
        (void)fputs("particle expiry free-list regression failed\n", stderr);
        return 8;
    }
    (void)puts("effects regression: ok");
    return 0;
}
