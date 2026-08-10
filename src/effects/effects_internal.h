/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EFFECTS_INTERNAL_H
#define AFORC_EFFECTS_INTERNAL_H

#include "aforc/effects.h"

#include <limits.h>

#define AFORC_PARTICLE_FREE_NONE SIZE_MAX
#define AFORC_PARTICLE_INDEX_BITS (sizeof(size_t) * CHAR_BIT)

static inline size_t
aforc_particle_free_word_count(const AFORC_ParticlePool *pool)
{
    return pool->capacity / AFORC_PARTICLE_INDEX_BITS +
           (pool->capacity % AFORC_PARTICLE_INDEX_BITS != 0U ? 1U : 0U);
}

static inline bool aforc_particle_slot_is_free(const AFORC_ParticlePool *pool,
                                               size_t index)
{
    const size_t word_index = index / AFORC_PARTICLE_INDEX_BITS;
    const size_t bit_index = index % AFORC_PARTICLE_INDEX_BITS;

    return (pool->particles[word_index].free_bits &
            ((size_t)1U << bit_index)) != 0U;
}

static inline void aforc_particle_mark_free(AFORC_ParticlePool *pool,
                                            size_t index)
{
    const size_t word_index = index / AFORC_PARTICLE_INDEX_BITS;
    const size_t bit_index = index % AFORC_PARTICLE_INDEX_BITS;

    pool->particles[word_index].free_bits |= (size_t)1U << bit_index;
}

static inline void aforc_particle_mark_occupied(AFORC_ParticlePool *pool,
                                                size_t index)
{
    const size_t word_index = index / AFORC_PARTICLE_INDEX_BITS;
    const size_t bit_index = index % AFORC_PARTICLE_INDEX_BITS;

    pool->particles[word_index].free_bits &= ~((size_t)1U << bit_index);
}

static inline size_t aforc_particle_lowest_set_bit(size_t word)
{
    size_t bit_index = 0U;

    while ((word & (size_t)1U) == 0U)
    {
        word >>= 1U;
        ++bit_index;
    }
    return bit_index;
}

static inline size_t
aforc_particle_find_free_from(const AFORC_ParticlePool *pool, size_t start)
{
    const size_t word_count = aforc_particle_free_word_count(pool);
    size_t word_index;
    size_t word;

    if (start >= pool->capacity)
    {
        return AFORC_PARTICLE_FREE_NONE;
    }
    word_index = start / AFORC_PARTICLE_INDEX_BITS;
    word = pool->particles[word_index].free_bits;
    if (start % AFORC_PARTICLE_INDEX_BITS != 0U)
    {
        word &= SIZE_MAX << (start % AFORC_PARTICLE_INDEX_BITS);
    }
    for (;;)
    {
        if (word != 0U)
        {
            const size_t index = word_index * AFORC_PARTICLE_INDEX_BITS +
                                 aforc_particle_lowest_set_bit(word);

            return index < pool->capacity ? index : AFORC_PARTICLE_FREE_NONE;
        }
        ++word_index;
        if (word_index >= word_count)
        {
            return AFORC_PARTICLE_FREE_NONE;
        }
        word = pool->particles[word_index].free_bits;
    }
}

static inline bool aforc_effect_clip_valid(AFORC_Rect clip)
{
    return clip.width >= 0 && clip.height >= 0;
}

static inline bool aforc_particle_pool_ready(const AFORC_ParticlePool *pool)
{
    if (pool == NULL || !pool->initialized || pool->particles == NULL ||
        pool->capacity == 0U || pool->active_count > pool->capacity ||
        pool->active_count > pool->used_high_water ||
        pool->used_high_water > pool->capacity || pool->random_state == 0U)
    {
        return false;
    }
    if (pool->active_count == 0U)
    {
        return pool->free_head == 0U && pool->used_high_water == 0U &&
               aforc_particle_slot_is_free(pool, 0U) &&
               !pool->particles[0].active;
    }
    if (pool->used_high_water == 0U ||
        !pool->particles[pool->used_high_water - 1U].active ||
        aforc_particle_slot_is_free(pool, pool->used_high_water - 1U))
    {
        return false;
    }
    if (pool->active_count == pool->capacity)
    {
        return pool->free_head == AFORC_PARTICLE_FREE_NONE;
    }
    return pool->free_head < pool->capacity &&
           aforc_particle_slot_is_free(pool, pool->free_head) &&
           !pool->particles[pool->free_head].active;
}

static inline bool
aforc_particle_pool_size_output_aliases_state(const AFORC_ParticlePool *pool,
                                              const size_t *output)
{
    size_t word_count;
    uintptr_t output_address;
    uintptr_t first_word_address;
    uintptr_t last_word_address;

    if (output == &pool->capacity || output == &pool->active_count ||
        output == &pool->free_head || output == &pool->used_high_water)
    {
        return true;
    }
    if (pool->particles == NULL || pool->capacity == 0U)
    {
        return false;
    }
    word_count = aforc_particle_free_word_count(pool);
    output_address = (uintptr_t)(const void *)output;
    first_word_address = (uintptr_t)(const void *)&pool->particles[0].free_bits;
    last_word_address =
        (uintptr_t)(const void *)&pool->particles[word_count - 1U].free_bits;
    return output_address >= first_word_address &&
           output_address <= last_word_address &&
           (output_address - first_word_address) % sizeof(AFORC_Particle) == 0U;
}

static inline void aforc_particle_assign(AFORC_Particle *particle,
                                         const AFORC_ParticleDesc *description)
{
    particle->position = description->position;
    particle->velocity = description->velocity;
    particle->acceleration = description->acceleration;
    particle->age_ms = 0U;
    particle->lifetime_ms = description->lifetime_ms;
    particle->cell = description->cell;
    particle->active = true;
}

static inline AFORC_Status
aforc_particle_pool_activate_free(AFORC_ParticlePool *pool,
                                  const AFORC_ParticleDesc *description,
                                  size_t *out_index)
{
    const size_t index = pool->free_head;
    AFORC_Particle *particle;
    size_t next_free;

    if (index >= pool->capacity)
    {
        return AFORC_ERROR_STATE;
    }
    particle = &pool->particles[index];
    if (particle->active || !aforc_particle_slot_is_free(pool, index))
    {
        return AFORC_ERROR_STATE;
    }
    next_free = aforc_particle_find_free_from(pool, index + 1U);
    if ((pool->active_count + 1U < pool->capacity &&
         next_free == AFORC_PARTICLE_FREE_NONE) ||
        (pool->active_count + 1U == pool->capacity &&
         next_free != AFORC_PARTICLE_FREE_NONE))
    {
        return AFORC_ERROR_STATE;
    }
    aforc_particle_mark_occupied(pool, index);
    aforc_particle_assign(particle, description);
    ++pool->active_count;
    pool->free_head = next_free;
    if (pool->used_high_water <= index)
    {
        pool->used_high_water = index + 1U;
    }
    *out_index = index;
    return AFORC_OK;
}

#endif
