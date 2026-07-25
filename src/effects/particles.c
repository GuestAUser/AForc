/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

#include "effects_internal.h"

#include <limits.h>

#define AFORC_PARTICLE_DEFAULT_SEED UINT32_C(0x6d2b79f5)

/*
 * Fixed-capacity deterministic particle simulation.
 *
 * The pool borrows caller-owned storage and never allocates. Position,
 * velocity, and acceleration use fixed-point arithmetic so identical seeds
 * and time steps produce identical trajectories across supported platforms.
 */

static bool i32_range_valid(AFORC_ParticleI32Range range) {
    return range.minimum <= range.maximum;
}

static bool u32_range_valid(AFORC_ParticleU32Range range) {
    return range.minimum <= range.maximum;
}

static bool particle_pool_ready(const AFORC_ParticlePool *pool) {
    return pool != NULL && pool->initialized && pool->particles != NULL &&
           pool->capacity > 0U && pool->active_count <= pool->capacity &&
           pool->random_state != 0U;
}

static AFORC_Status emitter_validate(const AFORC_ParticleEmitter *emitter) {
    if (emitter == NULL || !i32_range_valid(emitter->x) ||
        !i32_range_valid(emitter->y) ||
        !i32_range_valid(emitter->velocity_x) ||
        !i32_range_valid(emitter->velocity_y) ||
        !i32_range_valid(emitter->acceleration_x) ||
        !i32_range_valid(emitter->acceleration_y) ||
        !u32_range_valid(emitter->lifetime_ms) ||
        emitter->lifetime_ms.minimum == 0U || emitter->cells == NULL ||
        emitter->cell_count == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return AFORC_OK;
}

static uint32_t random_next(AFORC_ParticlePool *pool) {
    uint32_t value = pool->random_state;

    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    pool->random_state = value;
    return value;
}

static uint32_t random_bounded(AFORC_ParticlePool *pool, uint32_t bound) {
    /* Reject the short prefix that would bias a modulo reduction. */
    const uint32_t threshold = (uint32_t)(0U - bound) % bound;
    uint32_t value;

    do {
        value = random_next(pool);
    } while (value < threshold);
    return value % bound;
}

static uint64_t random_offset(AFORC_ParticlePool *pool, uint64_t span) {
    if (span == UINT64_C(0x100000000)) {
        return random_next(pool);
    }
    return random_bounded(pool, (uint32_t)span);
}

static int32_t random_i32(AFORC_ParticlePool *pool,
                          AFORC_ParticleI32Range range) {
    const uint64_t span =
        (uint64_t)((int64_t)range.maximum - range.minimum) + 1U;
    const uint64_t offset = random_offset(pool, span);

    return (int32_t)((int64_t)range.minimum + (int64_t)offset);
}

static uint32_t random_u32(AFORC_ParticlePool *pool,
                           AFORC_ParticleU32Range range) {
    const uint64_t span = (uint64_t)range.maximum - range.minimum + 1U;
    const uint64_t offset = random_offset(pool, span);

    return (uint32_t)((uint64_t)range.minimum + offset);
}

static size_t random_index(AFORC_ParticlePool *pool, size_t count) {
    if (count <= UINT32_MAX) {
        return (size_t)random_bounded(pool, (uint32_t)count);
    }
#if SIZE_MAX > UINT32_MAX
    {
        const uint64_t value = ((uint64_t)random_next(pool) << 32U) |
                               random_next(pool);
        return (size_t)(value % (uint64_t)count);
    }
#else
    return 0U;
#endif
}

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

    /* Per-step integer truncation is part of deterministic fixed-point
       integration; saturation prevents signed overflow at the boundary. */
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
    if (particle_pool_ready(pool)) {
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
    if (!particle_pool_ready(pool)) {
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
    if (!particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    pool->random_state = seed == 0U ? AFORC_PARTICLE_DEFAULT_SEED : seed;
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_spawn(AFORC_ParticlePool *pool,
                                   const AFORC_ParticleDesc *description,
                                   size_t *out_particle_index) {
    size_t index;
    AFORC_Particle *particle;

    if (pool == NULL || description == NULL ||
        description->lifetime_ms == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!particle_pool_ready(pool)) {
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

    particle = &pool->particles[index];
    particle->position = description->position;
    particle->velocity = description->velocity;
    particle->acceleration = description->acceleration;
    particle->age_ms = 0U;
    particle->lifetime_ms = description->lifetime_ms;
    particle->cell = description->cell;
    particle->active = true;
    ++pool->active_count;
    if (out_particle_index != NULL) {
        *out_particle_index = index;
    }
    return AFORC_OK;
}

AFORC_Status aforc_particle_pool_emit(AFORC_ParticlePool *pool,
                                  const AFORC_ParticleEmitter *emitter,
                                  size_t requested_count,
                                  size_t *out_spawned_count) {
    size_t emit_count;
    AFORC_Status status;

    if (pool == NULL || out_spawned_count == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_spawned_count = 0U;
    if (!particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    status = emitter_validate(emitter);
    if (status != AFORC_OK) {
        return status;
    }
    emit_count = requested_count;
    if (emit_count > pool->capacity - pool->active_count) {
        emit_count = pool->capacity - pool->active_count;
    }

    /* Capacity limits are a partial-success contract reported by count. */
    for (size_t count = 0U; count < emit_count; ++count) {
        AFORC_ParticleDesc description;

        description.position.x = random_i32(pool, emitter->x);
        description.position.y = random_i32(pool, emitter->y);
        description.velocity.x = random_i32(pool, emitter->velocity_x);
        description.velocity.y = random_i32(pool, emitter->velocity_y);
        description.acceleration.x = random_i32(pool, emitter->acceleration_x);
        description.acceleration.y = random_i32(pool, emitter->acceleration_y);
        description.lifetime_ms = random_u32(pool, emitter->lifetime_ms);
        description.cell = emitter->cells[random_index(pool,
                                                       emitter->cell_count)];
        status = aforc_particle_pool_spawn(pool, &description, NULL);
        if (status != AFORC_OK) {
            *out_spawned_count = count;
            return status;
        }
    }
    *out_spawned_count = emit_count;
    return emit_count == requested_count ? AFORC_OK : AFORC_ERROR_LIMIT;
}

AFORC_Status aforc_particle_pool_kill(AFORC_ParticlePool *pool,
                                  size_t particle_index) {
    if (pool == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!particle_pool_ready(pool)) {
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
    if (!particle_pool_ready(pool)) {
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
        /* Semi-implicit Euler advances velocity first; reversing this order
           changes otherwise deterministic particle trajectories. */
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
        (options->clip_enabled &&
         !aforc_effect_clip_valid(options->clip))) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!particle_pool_ready(pool)) {
        return AFORC_ERROR_STATE;
    }
    for (size_t index = 0U; index < pool->capacity; ++index) {
        const AFORC_Particle *particle = &pool->particles[index];
        const int64_t x = (int64_t)fixed_floor(particle->position.x) +
                          options->offset.x;
        const int64_t y = (int64_t)fixed_floor(particle->position.y) +
                          options->offset.y;
        AFORC_Point position;
        AFORC_Status status;

        if (!particle->active || x < INT32_MIN || x > INT32_MAX ||
            y < INT32_MIN || y > INT32_MAX) {
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
