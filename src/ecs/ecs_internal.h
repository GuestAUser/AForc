/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ECS_INTERNAL_H
#define AFORC_ECS_INTERNAL_H

#include "aforc/ecs.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct AFORC_EcsEntitySlot {
    uint32_t generation;
    uint32_t next_free;
    bool alive;
} AFORC_EcsEntitySlot;

typedef struct AFORC_EcsComponentStore {
    size_t component_size;
    size_t stride;
    size_t count;
    size_t capacity;
    size_t sparse_capacity;
    AFORC_Entity *dense_entities;
    unsigned char *dense_data;
    uint32_t *sparse;
    AFORC_EcsComponentCleanup cleanup;
    void *cleanup_user_data;
} AFORC_EcsComponentStore;

struct AFORC_Ecs {
    AFORC_Allocator allocator;
    AFORC_EcsEntitySlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    size_t active_count;
    size_t max_entities;
    uint32_t free_head;
    AFORC_EcsComponentStore *stores;
    size_t store_count;
    size_t store_capacity;
    size_t max_component_types;
    size_t initial_component_capacity;
    uint64_t revision;
    bool cleanup_active;
};

struct AFORC_EcsView {
    AFORC_Allocator allocator;
    AFORC_Ecs *ecs;
    AFORC_ComponentType *required_types;
    size_t required_type_count;
    size_t cursor;
    uint32_t driver_type_id;
    uint64_t revision;
};

static inline bool aforc_ecs_allocator_is_valid(
    const AFORC_Allocator *allocator) {
    return allocator != NULL && allocator->allocate != NULL &&
           allocator->reallocate != NULL && allocator->deallocate != NULL;
}

static inline AFORC_Status aforc_ecs_allocate_array(
    const AFORC_Allocator *allocator,
    size_t count,
    size_t element_size,
    bool zero_initialize,
    void **out_memory) {
    size_t byte_count = 0U;
    void *memory = NULL;
    AFORC_Status status = AFORC_OK;

    if (!aforc_ecs_allocator_is_valid(allocator) || out_memory == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    if (!aforc_size_multiply(count, element_size, &byte_count)) {
        return AFORC_ERROR_OVERFLOW;
    }
    status = aforc_alloc_array(allocator, count, element_size, &memory);
    if (status != AFORC_OK) {
        return status;
    }
    if (zero_initialize && byte_count != 0U) {
        (void)memset(memory, 0, byte_count);
    }
    *out_memory = memory;
    return AFORC_OK;
}

static inline void aforc_ecs_deallocate(const AFORC_Allocator *allocator,
                                      void *memory) {
    aforc_free(allocator, memory);
}

static inline size_t aforc_ecs_handle_capacity_limit(void) {
#if SIZE_MAX < UINT32_MAX
    return SIZE_MAX;
#else
    return (size_t)UINT32_MAX;
#endif
}

static inline bool aforc_ecs_encode_sparse_index(
    size_t dense_index,
    uint32_t *out_encoded_index) {
    if (out_encoded_index == NULL ||
        dense_index >= aforc_ecs_handle_capacity_limit()) {
        return false;
    }
    *out_encoded_index = (uint32_t)dense_index + UINT32_C(1);
    return true;
}

static inline AFORC_Status aforc_ecs_choose_capacity(size_t current,
                                                 size_t required,
                                                 size_t limit,
                                                 size_t *out_capacity) {
    size_t capacity = current;

    if (out_capacity == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (required > limit) {
        return AFORC_ERROR_LIMIT;
    }
    if (required <= current) {
        *out_capacity = current;
        return AFORC_OK;
    }
    if (capacity == 0U) {
        capacity = 1U;
    }
    while (capacity < required) {
        /* Clamp before doubling so growth cannot overflow the handle limit. */
        if (capacity > limit / 2U) {
            capacity = limit;
        } else {
            capacity *= 2U;
        }
        if (capacity == 0U) {
            return AFORC_ERROR_OVERFLOW;
        }
    }
    *out_capacity = capacity;
    return AFORC_OK;
}

static inline AFORC_Status aforc_ecs_require_mutable(AFORC_Ecs *ecs) {
    if (ecs == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (ecs->cleanup_active) {
        return AFORC_ERROR_STATE;
    }
    return AFORC_OK;
}

static inline AFORC_Status aforc_ecs_require_revision(const AFORC_Ecs *ecs) {
    return ecs->revision == UINT64_MAX ? AFORC_ERROR_LIMIT : AFORC_OK;
}

static inline void aforc_ecs_commit_revision(AFORC_Ecs *ecs) {
    /* Every structural mutation advances once, invalidating all older views. */
    ecs->revision += UINT64_C(1);
}

static inline AFORC_Status aforc_ecs_reserve_stores(AFORC_Ecs *ecs,
                                                size_t required) {
    AFORC_EcsComponentStore *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status = AFORC_OK;

    if (required <= ecs->store_capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(ecs->store_capacity, required,
                                     ecs->max_component_types, &capacity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, capacity,
                                    sizeof(*replacement), true,
                                    (void **)&replacement);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_multiply(ecs->store_count, sizeof(*replacement),
                           &copy_bytes)) {
        aforc_ecs_deallocate(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U) {
        (void)memcpy(replacement, ecs->stores, copy_bytes);
    }
    aforc_ecs_deallocate(&ecs->allocator, ecs->stores);
    ecs->stores = replacement;
    ecs->store_capacity = capacity;
    return AFORC_OK;
}

static inline AFORC_Status aforc_ecs_get_store(
    const AFORC_Ecs *ecs,
    AFORC_ComponentType type,
    AFORC_EcsComponentStore **out_store) {
    if (ecs == NULL || out_store == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    if ((size_t)type.id >= ecs->store_count) {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_store = &ecs->stores[type.id];
    return AFORC_OK;
}

static inline AFORC_Status aforc_ecs_get_store_const(
    const AFORC_Ecs *ecs,
    AFORC_ComponentType type,
    const AFORC_EcsComponentStore **out_store) {
    if (ecs == NULL || out_store == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_store = NULL;
    if ((size_t)type.id >= ecs->store_count) {
        return AFORC_ERROR_NOT_FOUND;
    }
    *out_store = &ecs->stores[type.id];
    return AFORC_OK;
}

static inline AFORC_Status aforc_ecs_validate_entity(const AFORC_Ecs *ecs,
                                                 AFORC_Entity entity) {
    const AFORC_EcsEntitySlot *slot = NULL;

    if (ecs == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (entity.index == AFORC_ENTITY_INVALID_INDEX || entity.generation == 0U ||
        (size_t)entity.index >= ecs->slot_count) {
        return AFORC_ERROR_STALE_HANDLE;
    }
    slot = &ecs->slots[entity.index];
    if (!slot->alive || slot->generation != entity.generation) {
        return AFORC_ERROR_STALE_HANDLE;
    }
    return AFORC_OK;
}

static inline size_t aforc_ecs_find_component(
    const AFORC_EcsComponentStore *store,
    AFORC_Entity entity) {
    uint32_t encoded_index = UINT32_C(0);
    size_t dense_index = 0U;

    if ((size_t)entity.index >= store->sparse_capacity) {
        return SIZE_MAX;
    }
    encoded_index = store->sparse[entity.index];
    if (encoded_index == 0U) {
        return SIZE_MAX;
    }
    dense_index = (size_t)(encoded_index - UINT32_C(1));
    /* Sparse stores dense_index + 1; generation rejects recycled-index aliasing. */
    if (dense_index >= store->count ||
        !aforc_entity_equal(store->dense_entities[dense_index], entity)) {
        return SIZE_MAX;
    }
    return dense_index;
}

static inline void *aforc_ecs_component_at(AFORC_EcsComponentStore *store,
                                         size_t dense_index) {
    return store->dense_data + dense_index * store->stride;
}

static inline const void *aforc_ecs_component_at_const(
    const AFORC_EcsComponentStore *store,
    size_t dense_index) {
    return store->dense_data + dense_index * store->stride;
}

static inline void aforc_ecs_invoke_cleanup(AFORC_Ecs *ecs,
                                          AFORC_EcsComponentStore *store,
                                          size_t dense_index) {
    if (store->cleanup != NULL) {
        ecs->cleanup_active = true;
        store->cleanup(ecs, store->dense_entities[dense_index],
                       aforc_ecs_component_at(store, dense_index),
                       store->cleanup_user_data);
        ecs->cleanup_active = false;
    }
}

static inline void aforc_ecs_erase_component_at(
    AFORC_EcsComponentStore *store,
    size_t dense_index) {
    const size_t last_index = store->count - 1U;
    const AFORC_Entity removed_entity = store->dense_entities[dense_index];
    const uint32_t encoded_dense_index = store->sparse[removed_entity.index];

    store->sparse[removed_entity.index] = 0U;
    if (dense_index != last_index) {
        const AFORC_Entity moved_entity = store->dense_entities[last_index];
        /* Swap-remove keeps deletion O(1); repair the moved entity's back-link. */
        store->dense_entities[dense_index] = moved_entity;
        (void)memcpy(aforc_ecs_component_at(store, dense_index),
                     aforc_ecs_component_at_const(store, last_index),
                     store->stride);
        store->sparse[moved_entity.index] = encoded_dense_index;
    }
    store->count = last_index;
}

static inline void aforc_ecs_remove_component_at(
    AFORC_Ecs *ecs,
    AFORC_EcsComponentStore *store,
    size_t dense_index) {
    aforc_ecs_invoke_cleanup(ecs, store, dense_index);
    aforc_ecs_erase_component_at(store, dense_index);
}

static inline void aforc_ecs_release_store(const AFORC_Allocator *allocator,
                                         AFORC_EcsComponentStore *store) {
    aforc_ecs_deallocate(allocator, store->sparse);
    aforc_ecs_deallocate(allocator, store->dense_data);
    aforc_ecs_deallocate(allocator, store->dense_entities);
    (void)memset(store, 0, sizeof(*store));
}

#endif
