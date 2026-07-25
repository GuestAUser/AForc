/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Sparse-set component registration, storage, and mutation.
 *
 * Each type maps entity indices through a sparse back-link into packed dense
 * arrays, giving O(1) lookup and swap-remove. Structural changes advance the
 * registry revision; cleanup callbacks run behind a mutation guard.
 */

static AFORC_Status reserve_sparse(AFORC_Ecs *ecs,
                                 AFORC_EcsComponentStore *store,
                                 size_t required) {
    uint32_t *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status = AFORC_OK;

    if (required <= store->sparse_capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(store->sparse_capacity, required,
                                     ecs->max_entities, &capacity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, capacity,
                                    sizeof(*replacement), true,
                                    (void **)&replacement);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_multiply(store->sparse_capacity, sizeof(*replacement),
                           &copy_bytes)) {
        aforc_ecs_deallocate(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U) {
        (void)memcpy(replacement, store->sparse, copy_bytes);
    }
    aforc_ecs_deallocate(&ecs->allocator, store->sparse);
    store->sparse = replacement;
    store->sparse_capacity = capacity;
    return AFORC_OK;
}

static AFORC_Status reserve_dense(AFORC_Ecs *ecs,
                                AFORC_EcsComponentStore *store,
                                size_t required) {
    AFORC_Entity *replacement_entities = NULL;
    unsigned char *replacement_data = NULL;
    size_t capacity = 0U;
    size_t entity_copy_bytes = 0U;
    size_t data_copy_bytes = 0U;
    AFORC_Status status = AFORC_OK;

    if (required <= store->capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(store->capacity, required,
                                     ecs->max_entities, &capacity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, capacity,
                                    sizeof(*replacement_entities), false,
                                    (void **)&replacement_entities);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, capacity, store->stride,
                                    false, (void **)&replacement_data);
    if (status != AFORC_OK) {
        aforc_ecs_deallocate(&ecs->allocator, replacement_entities);
        return status;
    }
    if (!aforc_size_multiply(store->count, sizeof(*replacement_entities),
                           &entity_copy_bytes) ||
        !aforc_size_multiply(store->count, store->stride, &data_copy_bytes)) {
        aforc_ecs_deallocate(&ecs->allocator, replacement_data);
        aforc_ecs_deallocate(&ecs->allocator, replacement_entities);
        return AFORC_ERROR_OVERFLOW;
    }
    if (entity_copy_bytes != 0U) {
        (void)memcpy(replacement_entities, store->dense_entities,
                     entity_copy_bytes);
        (void)memcpy(replacement_data, store->dense_data, data_copy_bytes);
    }
    aforc_ecs_deallocate(&ecs->allocator, store->dense_data);
    aforc_ecs_deallocate(&ecs->allocator, store->dense_entities);
    store->dense_entities = replacement_entities;
    store->dense_data = replacement_data;
    store->capacity = capacity;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_register_component(AFORC_Ecs *ecs,
                                      const AFORC_EcsComponentDesc *desc,
                                      AFORC_ComponentType *out_type) {
    AFORC_EcsComponentStore store;
    const size_t maximum_alignment = _Alignof(max_align_t);
    size_t alignment = 0U;
    size_t padding = 0U;
    size_t initial_capacity = 0U;
    size_t required_store_count = 0U;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (out_type == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_type = AFORC_COMPONENT_TYPE_INVALID;
    if (desc == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (status != AFORC_OK) {
        return status;
    }
    if (desc->size == 0U) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    alignment = desc->alignment == 0U ? maximum_alignment : desc->alignment;
    if (alignment > maximum_alignment || maximum_alignment % alignment != 0U) {
        return AFORC_ERROR_UNSUPPORTED;
    }
    initial_capacity = desc->initial_capacity == 0U
                           ? ecs->initial_component_capacity
                           : desc->initial_capacity;
    if (initial_capacity > ecs->max_entities) {
        return AFORC_ERROR_LIMIT;
    }
    if (ecs->store_count >= ecs->max_component_types ||
        !aforc_size_add(ecs->store_count, 1U, &required_store_count)) {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    padding = desc->size % alignment;
    padding = padding == 0U ? 0U : alignment - padding;
    (void)memset(&store, 0, sizeof(store));
    if (!aforc_size_add(desc->size, padding, &store.stride)) {
        return AFORC_ERROR_OVERFLOW;
    }
    store.component_size = desc->size;
    store.cleanup = desc->cleanup;
    store.cleanup_user_data = desc->cleanup_user_data;

    status = aforc_ecs_reserve_stores(ecs, required_store_count);
    if (status == AFORC_OK && initial_capacity != 0U) {
        status = reserve_dense(ecs, &store, initial_capacity);
    }
    if (status != AFORC_OK) {
        aforc_ecs_release_store(&ecs->allocator, &store);
        return status;
    }
    ecs->stores[ecs->store_count] = store;
    out_type->id = (uint32_t)ecs->store_count;
    ++ecs->store_count;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}

size_t aforc_ecs_registered_component_count(const AFORC_Ecs *ecs) {
    return ecs == NULL ? 0U : ecs->store_count;
}

AFORC_Status aforc_ecs_component_size(const AFORC_Ecs *ecs,
                                  AFORC_ComponentType type,
                                  size_t *out_size) {
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = AFORC_OK;

    if (out_size == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_size = 0U;
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    *out_size = store->component_size;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_component_instance_count(const AFORC_Ecs *ecs,
                                            AFORC_ComponentType type,
                                            size_t *out_count) {
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = AFORC_OK;

    if (out_count == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_count = 0U;
    status = aforc_ecs_get_store_const(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    *out_count = store->count;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_reserve_component(AFORC_Ecs *ecs,
                                     AFORC_ComponentType type,
                                     size_t capacity) {
    AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_get_store(ecs, type, &store);
    if (status != AFORC_OK) {
        return status;
    }
    if (capacity > ecs->max_entities) {
        return AFORC_ERROR_LIMIT;
    }
    if (capacity <= store->capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    status = reserve_dense(ecs, store, capacity);
    if (status == AFORC_OK) {
        aforc_ecs_commit_revision(ecs);
    }
    return status;
}

AFORC_Status aforc_ecs_add(AFORC_Ecs *ecs,
                       AFORC_Entity entity,
                       AFORC_ComponentType type,
                       const void *initial_value,
                       void **out_component) {
    AFORC_EcsComponentStore *store = NULL;
    unsigned char *staged_value = NULL;
    const void *source = initial_value;
    void *destination = NULL;
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
    status = reserve_sparse(ecs, store, required_sparse);
    if (status != AFORC_OK) {
        return status;
    }
    if (initial_value != NULL && required_count > store->capacity) {
        /* Growth may invalidate an initial value that aliases dense storage. */
        status = aforc_ecs_allocate_array(&ecs->allocator, 1U,
                                        store->component_size, false,
                                        (void **)&staged_value);
        if (status != AFORC_OK) {
            return status;
        }
        (void)memcpy(staged_value, initial_value, store->component_size);
        source = staged_value;
    }
    status = reserve_dense(ecs, store, required_count);
    if (status != AFORC_OK) {
        aforc_ecs_deallocate(&ecs->allocator, staged_value);
        return status;
    }
    destination = aforc_ecs_component_at(store, store->count);
    (void)memset(destination, 0, store->stride);
    if (source != NULL) {
        (void)memcpy(destination, source, store->component_size);
    }
    aforc_ecs_deallocate(&ecs->allocator, staged_value);
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
                       void **out_component) {
    AFORC_EcsComponentStore *store = NULL;
    size_t dense_index = 0U;
    AFORC_Status status = AFORC_OK;

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
                             const void **out_component) {
    const AFORC_EcsComponentStore *store = NULL;
    size_t dense_index = 0U;
    AFORC_Status status = AFORC_OK;

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
                       bool *out_has) {
    const AFORC_EcsComponentStore *store = NULL;
    AFORC_Status status = AFORC_OK;

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
                          AFORC_ComponentType type) {
    AFORC_EcsComponentStore *store = NULL;
    size_t dense_index = 0U;
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
