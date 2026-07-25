/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

#include "effects_internal.h"

#include <limits.h>

#define AFORC_PARTICLE_DEFAULT_SEED UINT32_C(0x6d2b79f5)

static int32_t saturate_i64(int64_t value) {
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

static int32_t integrate_fixed(int32_t value,
                               int32_t rate,
                               uint32_t delta_ms) {
    const int64_t change = ((int64_t)rate * delta_ms) / 1000;

    return saturate_i64((int64_t)value + change);
}

static int32_t fixed_floor(int32_t value) {
    int32_t whole = value / AFORC_EFFECT_FIXED_ONE;

    if (value % AFORC_EFFECT_FIXED_ONE < 0) {
        --whole;
    }
    return whole;
}

AFORC_ParticleDrawOptions aforc_particle_draw_options_default(void) {
    AFORC_ParticleDrawOptions options;

    options.offset = (AFORC_Point){0, 0};
    options.clip = (AFORC_Rect){0, 0, 0, 0};
    options.clip_enabled = false;
    return options;
}

AFORC_Status aforc_particle_pool_init(AFORC_ParticlePool *pool,
                                      AFORC_Particle *storage,
                                      size_t capacity,
                                      uint32_t seed) {
    if (pool == NULL || storage == NULL || capacity == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (capacity > SIZE_MAX / sizeof(*storage)) {
        return AFORC_ERROR_OVERFLOW;
    }
    for (size_t index = 0U; index < capacity; ++index) {
        storage[index] = (AFORC_Particle){0};
    }
    pool->particles = storage;
    pool->capacity = capacity;
    pool->active_count = 0U;
    pool->random_state = seed == 0U ? AFORC_PARTICLE_DEFAULT_SEED : seed;
    pool->initialized = true;
    return AFORC_OK;
}

void aforc_particle_pool_dispose(AFORC_ParticlePool *pool) {
    if (pool == NULL) {
        return;
    }
    if (aforc_particle_pool_ready(pool)) {
        for (size_t index = 0U; index < pool->capacity; ++index) {
            pool->particles[index] = (AFORC_Particle){0};
        }
    }
    pool->particles = NULL;
    pool->capacity = 0U;
    pool->active_count = 0U;
    pool->random_state = 0U;
    pool->initialized = false;
}

AFORC_Status aforc_particle_pool_clear(AFORC_ParticlePool *pool) {
    if (pool == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    for (size_t index = 0U; index < pool->capacity; ++index) {
        pool->particles[index] = (AFORC_Particle){0};
    }
    pool->active_count = 0U;
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_reseed(AFORC_ParticlePool *pool, uint32_t seed) {
    if (pool == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    pool->random_state = seed == 0U ? AFORC_PARTICLE_DEFAULT_SEED : seed;
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_spawn(AFORC_ParticlePool *pool,
                                       const AFORC_ParticleDesc *description,
                                       size_t *out_particle_index) {
    size_t index;

    if (pool == NULL || description == NULL ||
        description->lifetime_ms == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (out_particle_index != NULL &&
        aforc_particle_pool_size_output_aliases_state(
            pool,
            out_particle_index)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    if (pool->active_count == pool->capacity) {
        return AFORC_ERROR_LIMIT;
    }
    for (index = 0U; index < pool->capacity; ++index) {
        if (!pool->particles[index].active) {
            break;
        }
    }
    if (index == pool->capacity) {
        return AFORC_ERROR_STATE;
    }
    aforc_particle_assign(&pool->particles[index], description);
    ++pool->active_count;
    if (out_particle_index != NULL) {
        *out_particle_index = index;
    }
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_kill(AFORC_ParticlePool *pool,
                                      size_t particle_index) {
    if (pool == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    if (particle_index >= pool->capacity) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (pool->particles[particle_index].active) {
        pool->particles[particle_index].active = false;
        --pool->active_count;
    }
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_update(AFORC_ParticlePool *pool,
                                        uint32_t delta_ms) {
    if (pool == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    for (size_t index = 0U; index < pool->capacity; ++index) {
        AFORC_Particle *particle = &pool->particles[index];
        uint32_t remaining;

        if (!particle->active || delta_ms == 0U) {
            continue;
        }
        if (particle->lifetime_ms == 0U ||
            particle->age_ms >= particle->lifetime_ms) {
            return AFORC_ERROR_STATE;
        }
        remaining = particle->lifetime_ms - particle->age_ms;
        if (delta_ms >= remaining) {
            particle->age_ms = particle->lifetime_ms;
            particle->active = false;
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
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_draw(const AFORC_ParticlePool *pool,
                                      const AFORC_ParticleDrawOptions *options,
                                      AFORC_EffectPlotFn plot,
                                      void *context) {
    if (pool == NULL || options == NULL || plot == NULL ||
        (options->clip_enabled && !aforc_effect_clip_valid(options->clip))) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    for (size_t index = 0U; index < pool->capacity; ++index) {
        const AFORC_Particle *particle = &pool->particles[index];
        AFORC_Point position;
        AFORC_Status status;
        int64_t x;
        int64_t y;

        if (!particle->active) {
            continue;
        }
        x = (int64_t)fixed_floor(particle->position.x) + options->offset.x;
        y = (int64_t)fixed_floor(particle->position.y) + options->offset.y;
        if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN ||
            y > INT32_MAX) {
            continue;
        }
        position = (AFORC_Point){(int32_t)x, (int32_t)y};
        if (options->clip_enabled &&
            !aforc_rect_contains(options->clip, position)) {
            continue;
        }
        status = plot(context, position, particle->cell);
        if (status != AFORC_OK) {
            return status;
        }
    }
    return AFORC_OK;
}
