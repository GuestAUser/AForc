/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

AFORC_Status aforc_ecs_register_component(AFORC_Ecs *ecs,
                                          const AFORC_EcsComponentDesc *desc,
                                          AFORC_ComponentType *out_type)
{
    AFORC_EcsComponentStore store;
    const size_t maximum_alignment = _Alignof(max_align_t);
    size_t alignment = 0U;
    size_t padding = 0U;
    size_t initial_capacity = 0U;
    size_t required_store_count = 0U;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (out_type == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_type = AFORC_COMPONENT_TYPE_INVALID;
    if (desc == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    if (desc->size == 0U)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    alignment = desc->alignment == 0U ? maximum_alignment : desc->alignment;
    if (alignment > maximum_alignment || maximum_alignment % alignment != 0U)
    {
        return AFORC_ERROR_UNSUPPORTED;
    }
    initial_capacity = desc->initial_capacity == 0U
                           ? ecs->initial_component_capacity
                           : desc->initial_capacity;
    if (initial_capacity > ecs->max_entities)
    {
        return AFORC_ERROR_LIMIT;
    }
    if (ecs->store_count >= ecs->max_component_types ||
        !aforc_size_add(ecs->store_count, 1U, &required_store_count))
    {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK)
    {
        return status;
    }
    padding = desc->size % alignment;
    padding = padding == 0U ? 0U : alignment - padding;
    (void)memset(&store, 0, sizeof(store));
    if (!aforc_size_add(desc->size, padding, &store.stride))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    store.component_size = desc->size;
    store.cleanup = desc->cleanup;
    store.cleanup_user_data = desc->cleanup_user_data;

    status = aforc_ecs_reserve_stores(ecs, required_store_count);
    if (status == AFORC_OK && initial_capacity != 0U)
    {
        status = aforc_ecs_reserve_dense(ecs, &store, initial_capacity);
    }
    if (status != AFORC_OK)
    {
        aforc_ecs_release_store(&ecs->allocator, &store);
        return status;
    }
    ecs->stores[ecs->store_count] = store;
    out_type->id = (uint32_t)ecs->store_count;
    ++ecs->store_count;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}

size_t aforc_ecs_registered_component_count(const AFORC_Ecs *ecs)
{
    return ecs == NULL ? 0U : ecs->store_count;
}

AFORC_Status aforc_ecs_component_size(const AFORC_Ecs *ecs,
                                      AFORC_ComponentType type,
                                      size_t *out_size)
{
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = AFORC_OK;

    if (out_size == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_size = 0U;
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK)
    {
        return status;
    }
    *out_size = store->component_size;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_component_instance_count(const AFORC_Ecs *ecs,
                                                AFORC_ComponentType type,
                                                size_t *out_count)
{
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = AFORC_OK;

    if (out_count == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0U;
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK)
    {
        return status;
    }
    *out_count = store->count;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_reserve_component(AFORC_Ecs *ecs,
                                         AFORC_ComponentType type,
                                         size_t capacity)
{
    AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_ecs_get_store(ecs, type, &store);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (capacity > ecs->max_entities)
    {
        return AFORC_ERROR_LIMIT;
    }
    if (capacity <= store->capacity)
    {
        return AFORC_OK;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_ecs_reserve_dense(ecs, store, capacity);
    if (status == AFORC_OK)
    {
        aforc_ecs_commit_revision(ecs);
    }
    return status;
}
