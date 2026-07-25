/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "effects_internal.h"

static bool i32_range_valid(AFORC_ParticleI32Range range) {
    return range.minimum <= range.maximum;
}

static bool u32_range_valid(AFORC_ParticleU32Range range) {
    return range.minimum <= range.maximum;
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

AFORC_Status aforc_particle_pool_emit(AFORC_ParticlePool *pool,
                                      const AFORC_ParticleEmitter *emitter,
                                      size_t requested_count,
                                      size_t *out_spawned_count) {
    size_t emit_count;
    size_t search_index = 0U;
    AFORC_Status status;

    if (pool == NULL || out_spawned_count == NULL ||
        aforc_particle_pool_size_output_aliases_state(
            pool,
            out_spawned_count)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_spawned_count = 0U;
    if (!aforc_particle_pool_ready(pool)) {
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
        while (search_index < pool->capacity &&
               pool->particles[search_index].active) {
            ++search_index;
        }
        if (search_index == pool->capacity) {
            *out_spawned_count = count;
            return AFORC_ERROR_STATE;
        }
        aforc_particle_assign(&pool->particles[search_index], &description);
        ++pool->active_count;
        ++search_index;
    }
    *out_spawned_count = emit_count;
    return emit_count == requested_count ? AFORC_OK : AFORC_ERROR_LIMIT;
}
