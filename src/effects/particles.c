/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

#include "effects_internal.h"

#include <limits.h>

#define AFORC_PARTICLE_DEFAULT_SEED UINT32_C(0x6d2b79f5)

static int32_t saturate_i64(int64_t value)
{
    if (value > INT32_MAX)
    {
        return INT32_MAX;
    }
    if (value < INT32_MIN)
    {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static int32_t integrate_fixed(int32_t value, int32_t rate, uint32_t delta_ms)
{
    const int64_t change = ((int64_t)rate * delta_ms) / 1000;

    return saturate_i64((int64_t)value + change);
}

static int32_t fixed_floor(int32_t value)
{
    int32_t whole = value / AFORC_EFFECT_FIXED_ONE;

    if (value % AFORC_EFFECT_FIXED_ONE < 0)
    {
        --whole;
    }
    return whole;
}

static void particle_pool_reset_storage(AFORC_ParticlePool *pool)
{
    const size_t word_count = aforc_particle_free_word_count(pool);

    for (size_t index = 0U; index < pool->capacity; ++index)
    {
        pool->particles[index] = (AFORC_Particle){0};
    }
    for (size_t word_index = 0U; word_index < word_count; ++word_index)
    {
        pool->particles[word_index].free_bits = SIZE_MAX;
    }
    if (pool->capacity % AFORC_PARTICLE_INDEX_BITS != 0U)
    {
        const size_t valid_bits = pool->capacity % AFORC_PARTICLE_INDEX_BITS;

        pool->particles[word_count - 1U].free_bits =
            ((size_t)1U << valid_bits) - 1U;
    }
    pool->active_count = 0U;
    pool->free_head = 0U;
    pool->used_high_water = 0U;
}

static size_t highest_set_bit(size_t word)
{
    size_t bit_index = 0U;

    while (word > 1U)
    {
        word >>= 1U;
        ++bit_index;
    }
    return bit_index;
}

static size_t particle_pool_active_high_water(const AFORC_ParticlePool *pool,
                                              size_t limit)
{
    size_t word_index;
    size_t valid_bits;
    size_t valid_mask;

    if (pool->active_count == 0U || limit == 0U)
    {
        return 0U;
    }
    word_index = (limit - 1U) / AFORC_PARTICLE_INDEX_BITS;
    valid_bits = (limit - 1U) % AFORC_PARTICLE_INDEX_BITS + 1U;
    valid_mask = valid_bits == AFORC_PARTICLE_INDEX_BITS
                     ? SIZE_MAX
                     : ((size_t)1U << valid_bits) - 1U;
    for (;;)
    {
        const size_t active_bits =
            ~pool->particles[word_index].free_bits & valid_mask;

        if (active_bits != 0U)
        {
            return word_index * AFORC_PARTICLE_INDEX_BITS +
                   highest_set_bit(active_bits) + 1U;
        }
        if (word_index == 0U)
        {
            return 0U;
        }
        --word_index;
        valid_mask = SIZE_MAX;
    }
}

static AFORC_Status particle_pool_release(AFORC_ParticlePool *pool,
                                          size_t particle_index)
{
    AFORC_Particle *particle = &pool->particles[particle_index];

    if (!particle->active)
    {
        return aforc_particle_slot_is_free(pool, particle_index)
                   ? AFORC_OK
                   : AFORC_ERROR_STATE;
    }
    if (aforc_particle_slot_is_free(pool, particle_index))
    {
        return AFORC_ERROR_STATE;
    }
    particle->active = false;
    aforc_particle_mark_free(pool, particle_index);
    if (pool->free_head == AFORC_PARTICLE_FREE_NONE ||
        particle_index < pool->free_head)
    {
        pool->free_head = particle_index;
    }
    --pool->active_count;
    if (particle_index + 1U == pool->used_high_water)
    {
        pool->used_high_water =
            particle_pool_active_high_water(pool, pool->used_high_water);
    }
    return AFORC_OK;
}

AFORC_ParticleDrawOptions aforc_particle_draw_options_default(void)
{
    AFORC_ParticleDrawOptions options;

    options.offset = (AFORC_Point){0, 0};
    options.clip = (AFORC_Rect){0, 0, 0, 0};
    options.clip_enabled = false;
    return options;
}

AFORC_Status aforc_particle_pool_init(AFORC_ParticlePool *pool,
                                      AFORC_Particle *storage,
                                      size_t capacity,
                                      uint32_t seed)
{
    if (pool == NULL || storage == NULL || capacity == 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (capacity > SIZE_MAX / sizeof(*storage))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    pool->particles = storage;
    pool->capacity = capacity;
    pool->random_state = seed == 0U ? AFORC_PARTICLE_DEFAULT_SEED : seed;
    pool->initialized = true;
    particle_pool_reset_storage(pool);
    return AFORC_OK;
}

void aforc_particle_pool_dispose(AFORC_ParticlePool *pool)
{
    if (pool == NULL)
    {
        return;
    }
    if (aforc_particle_pool_ready(pool))
    {
        for (size_t index = 0U; index < pool->capacity; ++index)
        {
            pool->particles[index] = (AFORC_Particle){0};
        }
    }
    pool->particles = NULL;
    pool->capacity = 0U;
    pool->active_count = 0U;
    pool->free_head = 0U;
    pool->used_high_water = 0U;
    pool->random_state = 0U;
    pool->initialized = false;
}

AFORC_Status aforc_particle_pool_clear(AFORC_ParticlePool *pool)
{
    if (pool == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    particle_pool_reset_storage(pool);
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_reseed(AFORC_ParticlePool *pool, uint32_t seed)
{
    if (pool == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    pool->random_state = seed == 0U ? AFORC_PARTICLE_DEFAULT_SEED : seed;
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_spawn(AFORC_ParticlePool *pool,
                                       const AFORC_ParticleDesc *description,
                                       size_t *out_particle_index)
{
    size_t index = 0U;
    AFORC_Status status;

    if (pool == NULL || description == NULL || description->lifetime_ms == 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (out_particle_index != NULL &&
        aforc_particle_pool_size_output_aliases_state(pool, out_particle_index))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    if (pool->active_count == pool->capacity)
    {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_particle_pool_activate_free(pool, description, &index);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (out_particle_index != NULL)
    {
        *out_particle_index = index;
    }
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_kill(AFORC_ParticlePool *pool,
                                      size_t particle_index)
{
    if (pool == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    if (particle_index >= pool->capacity)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return particle_pool_release(pool, particle_index);
}

AFORC_Status aforc_particle_pool_update(AFORC_ParticlePool *pool,
                                        uint32_t delta_ms)
{
    if (pool == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    if (delta_ms == 0U)
    {
        return AFORC_OK;
    }
    size_t first_released = AFORC_PARTICLE_FREE_NONE;

    for (size_t index = 0U; index < pool->used_high_water; ++index)
    {
        AFORC_Particle *particle = &pool->particles[index];
        uint32_t remaining;

        if (!particle->active)
        {
            if (!aforc_particle_slot_is_free(pool, index))
            {
                return AFORC_ERROR_STATE;
            }
            continue;
        }
        if (aforc_particle_slot_is_free(pool, index))
        {
            return AFORC_ERROR_STATE;
        }
        if (particle->lifetime_ms == 0U ||
            particle->age_ms >= particle->lifetime_ms)
        {
            return AFORC_ERROR_STATE;
        }
        remaining = particle->lifetime_ms - particle->age_ms;
        if (delta_ms >= remaining)
        {
            particle->age_ms = particle->lifetime_ms;
            particle->active = false;
            aforc_particle_mark_free(pool, index);
            if (first_released == AFORC_PARTICLE_FREE_NONE)
            {
                first_released = index;
            }
            --pool->active_count;
            continue;
        }
        particle->velocity.x = integrate_fixed(
            particle->velocity.x, particle->acceleration.x, delta_ms);
        particle->velocity.y = integrate_fixed(
            particle->velocity.y, particle->acceleration.y, delta_ms);
        particle->position.x = integrate_fixed(
            particle->position.x, particle->velocity.x, delta_ms);
        particle->position.y = integrate_fixed(
            particle->position.y, particle->velocity.y, delta_ms);
        particle->age_ms += delta_ms;
    }
    if (first_released != AFORC_PARTICLE_FREE_NONE)
    {
        if (pool->free_head == AFORC_PARTICLE_FREE_NONE ||
            first_released < pool->free_head)
        {
            pool->free_head = first_released;
        }
        if (pool->used_high_water > 0U &&
            !pool->particles[pool->used_high_water - 1U].active)
        {
            pool->used_high_water =
                particle_pool_active_high_water(pool, pool->used_high_water);
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_draw(const AFORC_ParticlePool *pool,
                                      const AFORC_ParticleDrawOptions *options,
                                      AFORC_EffectPlotFn plot,
                                      void *context)
{
    if (pool == NULL || options == NULL || plot == NULL ||
        (options->clip_enabled && !aforc_effect_clip_valid(options->clip)))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool))
    {
        return AFORC_ERROR_STATE;
    }
    for (size_t index = 0U; index < pool->used_high_water; ++index)
    {
        const AFORC_Particle *particle = &pool->particles[index];
        AFORC_Point position;
        AFORC_Status status;
        int64_t x;
        int64_t y;

        if (!particle->active)
        {
            continue;
        }
        x = (int64_t)fixed_floor(particle->position.x) + options->offset.x;
        y = (int64_t)fixed_floor(particle->position.y) + options->offset.y;
        if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX)
        {
            continue;
        }
        position = (AFORC_Point){(int32_t)x, (int32_t)y};
        if (options->clip_enabled &&
            !aforc_rect_contains(options->clip, position))
        {
            continue;
        }
        status = plot(context, position, particle->cell);
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return AFORC_OK;
}
