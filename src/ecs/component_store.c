/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "component_store_internal.h"

#include <string.h>

bool aforc_ecs_encode_sparse_index(size_t dense_index,
                                   uint32_t *out_encoded_index)
{
    if (out_encoded_index == NULL ||
        dense_index >= aforc_ecs_handle_capacity_limit())
    {
        return false;
    }
    *out_encoded_index = (uint32_t)dense_index + UINT32_C(1);
    return true;
}

AFORC_Status aforc_ecs_reserve_stores(AFORC_Ecs *ecs, size_t required)
{
    AFORC_EcsComponentStore *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status;

    if (required <= ecs->store_capacity)
    {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(
        ecs->store_capacity, required, ecs->max_component_types, &capacity);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_alloc_array(
        &ecs->allocator, capacity, sizeof(*replacement), (void **)&replacement);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(replacement, 0, capacity * sizeof(*replacement));
    if (!aforc_size_multiply(
            ecs->store_count, sizeof(*replacement), &copy_bytes))
    {
        aforc_free(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U)
    {
        (void)memcpy(replacement, ecs->stores, copy_bytes);
    }
    aforc_free(&ecs->allocator, ecs->stores);
    ecs->stores = replacement;
    ecs->store_capacity = capacity;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_reserve_sparse(AFORC_Ecs *ecs,
                                      AFORC_EcsComponentStore *store,
                                      size_t required)
{
    uint32_t *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status;

    if (required <= store->sparse_capacity)
    {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(
        store->sparse_capacity, required, ecs->max_entities, &capacity);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_alloc_array(
        &ecs->allocator, capacity, sizeof(*replacement), (void **)&replacement);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(replacement, 0, capacity * sizeof(*replacement));
    if (!aforc_size_multiply(
            store->sparse_capacity, sizeof(*replacement), &copy_bytes))
    {
        aforc_free(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U)
    {
        (void)memcpy(replacement, store->sparse, copy_bytes);
    }
    aforc_free(&ecs->allocator, store->sparse);
    store->sparse = replacement;
    store->sparse_capacity = capacity;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_reserve_dense(AFORC_Ecs *ecs,
                                     AFORC_EcsComponentStore *store,
                                     size_t required)
{
    AFORC_Entity *replacement_entities = NULL;
    unsigned char *replacement_data = NULL;
    size_t capacity = 0U;
    size_t entity_copy_bytes = 0U;
    size_t data_copy_bytes = 0U;
    AFORC_Status status;

    if (required <= store->capacity)
    {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(
        store->capacity, required, ecs->max_entities, &capacity);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_alloc_array(&ecs->allocator,
                               capacity,
                               sizeof(*replacement_entities),
                               (void **)&replacement_entities);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_alloc_array(
        &ecs->allocator, capacity, store->stride, (void **)&replacement_data);
    if (status != AFORC_OK)
    {
        aforc_free(&ecs->allocator, replacement_entities);
        return status;
    }
    if (!aforc_size_multiply(
            store->count, sizeof(*replacement_entities), &entity_copy_bytes) ||
        !aforc_size_multiply(store->count, store->stride, &data_copy_bytes))
    {
        aforc_free(&ecs->allocator, replacement_data);
        aforc_free(&ecs->allocator, replacement_entities);
        return AFORC_ERROR_OVERFLOW;
    }
    if (entity_copy_bytes != 0U)
    {
        (void)memcpy(
            replacement_entities, store->dense_entities, entity_copy_bytes);
        (void)memcpy(replacement_data, store->dense_data, data_copy_bytes);
    }
    aforc_free(&ecs->allocator, store->dense_data);
    aforc_free(&ecs->allocator, store->dense_entities);
    store->dense_entities = replacement_entities;
    store->dense_data = replacement_data;
    store->capacity = capacity;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_get_store(const AFORC_Ecs *ecs,
                                 AFORC_ComponentType type,
                                 AFORC_EcsComponentStore **out_store)
{
    if (ecs == NULL || out_store == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    if ((size_t)type.id >= ecs->store_count)
    {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_store = &ecs->stores[type.id];
    return AFORC_OK;
}

AFORC_Status
aforc_ecs_get_store_const(const AFORC_Ecs *ecs,
                          AFORC_ComponentType type,
                          const AFORC_EcsComponentStore **out_store)
{
    if (ecs == NULL || out_store == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    if ((size_t)type.id >= ecs->store_count)
    {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_store = &ecs->stores[type.id];
    return AFORC_OK;
}

static void invoke_cleanup(AFORC_Ecs *ecs,
                           AFORC_EcsComponentStore *store,
                           size_t dense_index)
{
    if (store->cleanup == NULL)
    {
        return;
    }
    ecs->cleanup_active = true;
    store->cleanup(ecs,
                   store->dense_entities[dense_index],
                   aforc_ecs_component_at(store, dense_index),
                   store->cleanup_user_data);
    ecs->cleanup_active = false;
}

static void erase_component(AFORC_EcsComponentStore *store, size_t dense_index)
{
    const size_t last_index = store->count - 1U;
    const AFORC_Entity removed_entity = store->dense_entities[dense_index];
    const uint32_t encoded_dense_index = store->sparse[removed_entity.index];

    store->sparse[removed_entity.index] = 0U;
    if (dense_index != last_index)
    {
        const AFORC_Entity moved_entity = store->dense_entities[last_index];

        store->dense_entities[dense_index] = moved_entity;
        (void)memcpy(aforc_ecs_component_at(store, dense_index),
                     aforc_ecs_component_at_const(store, last_index),
                     store->stride);
        store->sparse[moved_entity.index] = encoded_dense_index;
    }
    store->count = last_index;
}

void aforc_ecs_remove_component_at(AFORC_Ecs *ecs,
                                   AFORC_EcsComponentStore *store,
                                   size_t dense_index)
{
    invoke_cleanup(ecs, store, dense_index);
    erase_component(store, dense_index);
}

void aforc_ecs_clear_component_stores(AFORC_Ecs *ecs)
{
    size_t store_index;

    for (store_index = 0U; store_index < ecs->store_count; ++store_index)
    {
        AFORC_EcsComponentStore *store = &ecs->stores[store_index];

        while (store->count != 0U)
        {
            aforc_ecs_remove_component_at(ecs, store, store->count - 1U);
        }
    }
}

void aforc_ecs_release_store(const AFORC_Allocator *allocator,
                             AFORC_EcsComponentStore *store)
{
    aforc_free(allocator, store->sparse);
    aforc_free(allocator, store->dense_data);
    aforc_free(allocator, store->dense_entities);
    (void)memset(store, 0, sizeof(*store));
}
