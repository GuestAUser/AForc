/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EFFECTS_INTERNAL_H
#define AFORC_EFFECTS_INTERNAL_H

#include "aforc/effects.h"

static inline bool aforc_effect_clip_valid(AFORC_Rect clip) {
    return clip.width >= 0 && clip.height >= 0;
}

static inline bool aforc_particle_pool_ready(const AFORC_ParticlePool *pool) {
    return pool != NULL && pool->initialized && pool->particles != NULL &&
           pool->capacity > 0U && pool->active_count <= pool->capacity &&
           pool->random_state != 0U;
}

static inline bool aforc_particle_pool_size_output_aliases_state(
    const AFORC_ParticlePool *pool,
    const size_t *output) {
    return output == &pool->capacity || output == &pool->active_count;
}

static inline void aforc_particle_assign(
    AFORC_Particle *particle,
    const AFORC_ParticleDesc *description) {
    particle->position = description->position;
    particle->velocity = description->velocity;
    particle->acceleration = description->acceleration;
    particle->age_ms = 0U;
    particle->lifetime_ms = description->lifetime_ms;
    particle->cell = description->cell;
    particle->active = true;
}

#endif
