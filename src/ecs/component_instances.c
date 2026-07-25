/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "component_store_internal.h"

#include <string.h>

AFORC_Status aforc_ecs_add(AFORC_Ecs *ecs,
                           AFORC_Entity entity,
                           AFORC_ComponentType type,
                           const void *initial_value,
                           void **out_component)
{
    AFORC_EcsComponentStore *store = NULL;
    unsigned char *staged_value = NULL;
    const void *source = initial_value;
    void *destination;
    size_t required_count = 0U;
    size_t required_sparse = 0U;
    uint32_t encoded_dense_index = UINT32_C(0);
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (out_component != NULL) {
        *out_component = NULL;
    }
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    if (aforc_ecs_find_component(store, entity) != SIZE_MAX) {
        return AFORC_ERROR_EXISTS;
    }
    if (!aforc_ecs_encode_sparse_index(store->count,
                                       &encoded_dense_index)) {
        return AFORC_ERROR_LIMIT;
    }
    if (!aforc_size_add(store->count, 1U, &required_count) ||
        !aforc_size_add((size_t)entity.index, 1U, &required_sparse)) {
        return AFORC_ERROR_OVERFLOW;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_reserve_sparse(ecs, store, required_sparse);
    if (status != AFORC_OK) {
        return status;
    }
    if (initial_value != NULL && required_count > store->capacity) {
        status = aforc_ecs_allocate_array(&ecs->allocator,
                                          1U,
                                          store->component_size,
                                          false,
                                          (void **)&staged_value);
        if (status != AFORC_OK) {
            return status;
        }
        (void)memcpy(staged_value, initial_value, store->component_size);
        source = staged_value;
    }
    status = aforc_ecs_reserve_dense(ecs, store, required_count);
    if (status != AFORC_OK) {
        aforc_free(&ecs->allocator, staged_value);
        return status;
    }
    destination = aforc_ecs_component_at(store, store->count);
    (void)memset(destination, 0, store->stride);
    if (source != NULL) {
        (void)memcpy(destination, source, store->component_size);
    }
    aforc_free(&ecs->allocator, staged_value);
    store->dense_entities[store->count] = entity;
    store->sparse[entity.index] = encoded_dense_index;
    ++store->count;
    aforc_ecs_commit_revision(ecs);
    if (out_component != NULL) {
        *out_component = destination;
    }
    return AFORC_OK;
}

AFORC_Status aforc_ecs_get(AFORC_Ecs *ecs,
                           AFORC_Entity entity,
                           AFORC_ComponentType type,
                           void **out_component)
{
    AFORC_EcsComponentStore *store = NULL;
    size_t dense_index;
    AFORC_Status status;

    if (out_component == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_component = NULL;
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    dense_index = aforc_ecs_find_component(store, entity);
    if (dense_index == SIZE_MAX) {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_component = aforc_ecs_component_at(store, dense_index);
    return AFORC_OK;
}

AFORC_Status aforc_ecs_get_const(const AFORC_Ecs *ecs,
                                 AFORC_Entity entity,
                                 AFORC_ComponentType type,
                                 const void **out_component)
{
    const AFORC_EcsComponentStore *store = NULL;
    size_t dense_index;
    AFORC_Status status;

    if (out_component == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_component = NULL;
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    dense_index = aforc_ecs_find_component(store, entity);
    if (dense_index == SIZE_MAX) {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_component = aforc_ecs_component_at_const(store, dense_index);
    return AFORC_OK;
}

AFORC_Status aforc_ecs_has(const AFORC_Ecs *ecs,
                           AFORC_Entity entity,
                           AFORC_ComponentType type,
                           bool *out_has)
{
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status;

    if (out_has == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_has = false;
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    *out_has = aforc_ecs_find_component(store, entity) != SIZE_MAX;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_remove(AFORC_Ecs *ecs,
                              AFORC_Entity entity,
                              AFORC_ComponentType type)
{
    AFORC_EcsComponentStore *store = NULL;
    size_t dense_index;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    dense_index = aforc_ecs_find_component(store, entity);
    if (dense_index == SIZE_MAX) {
        return AFORC_ERROR_NOT_FOUND;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    aforc_ecs_remove_component_at(ecs, store, dense_index);
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}
