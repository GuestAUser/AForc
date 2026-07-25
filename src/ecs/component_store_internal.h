/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ECS_COMPONENT_STORE_INTERNAL_H
#define AFORC_ECS_COMPONENT_STORE_INTERNAL_H

#include "ecs_internal.h"

static inline size_t aforc_ecs_find_component(
    const AFORC_EcsComponentStore *store,
    AFORC_Entity entity)
{
    uint32_t encoded_index;
    size_t dense_index;

    if ((size_t)entity.index >= store->sparse_capacity) {
        return SIZE_MAX;
    }
    encoded_index = store->sparse[entity.index];
    if (encoded_index == 0U) {
        return SIZE_MAX;
    }
    dense_index = (size_t)(encoded_index - UINT32_C(1));
    if (dense_index >= store->count ||
        !aforc_entity_equal(store->dense_entities[dense_index], entity)) {
        return SIZE_MAX;
    }
    return dense_index;
}

static inline void *aforc_ecs_component_at(AFORC_EcsComponentStore *store,
                                            size_t dense_index)
{
    return store->dense_data + dense_index * store->stride;
}

static inline const void *aforc_ecs_component_at_const(
    const AFORC_EcsComponentStore *store,
    size_t dense_index)
{
    return store->dense_data + dense_index * store->stride;
}

#endif
