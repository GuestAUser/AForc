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
 * ECS ownership and generational entity lifecycle.
 *
 * The registry owns entity slots and every registered component store through
 * its copied allocator. Destroyed indices enter a free list with an advanced
 * generation; exhausted generations retire permanently to prevent aliasing.
 */

static AFORC_Status reserve_slots(AFORC_Ecs *ecs, size_t required) {
    AFORC_EcsEntitySlot *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status = AFORC_OK;

    if (required <= ecs->slot_capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(ecs->slot_capacity, required,
                                     ecs->max_entities, &capacity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, capacity,
                                    sizeof(*replacement), false,
                                    (void **)&replacement);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_multiply(ecs->slot_count, sizeof(*replacement),
                           &copy_bytes)) {
        aforc_ecs_deallocate(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U) {
        (void)memcpy(replacement, ecs->slots, copy_bytes);
    }
    aforc_ecs_deallocate(&ecs->allocator, ecs->slots);
    ecs->slots = replacement;
    ecs->slot_capacity = capacity;
    return AFORC_OK;
}

static void release_ecs_storage(AFORC_Ecs *ecs) {
    size_t store_index = 0U;

    for (store_index = 0U; store_index < ecs->store_count; ++store_index) {
        aforc_ecs_release_store(&ecs->allocator, &ecs->stores[store_index]);
    }
    aforc_ecs_deallocate(&ecs->allocator, ecs->stores);
    aforc_ecs_deallocate(&ecs->allocator, ecs->slots);
    ecs->stores = NULL;
    ecs->slots = NULL;
}

static void recycle_or_retire_slot(AFORC_Ecs *ecs, uint32_t index) {
    AFORC_EcsEntitySlot *slot = &ecs->slots[index];

    if (slot->alive && slot->generation != UINT32_MAX) {
        ++slot->generation;
    }
    slot->alive = false;
    slot->next_free = AFORC_ENTITY_INVALID_INDEX;
    /* UINT32_MAX retires the slot rather than rolling over to generation 0. */
    if (slot->generation != UINT32_MAX) {
        slot->next_free = ecs->free_head;
        ecs->free_head = index;
    }
}

AFORC_EcsConfig aforc_ecs_config_default(void) {
    AFORC_EcsConfig config;

    config.allocator = aforc_allocator_default();
    config.initial_entity_capacity = 64U;
    config.max_entities = 0U;
    config.initial_component_capacity = 16U;
    config.initial_component_type_capacity = 8U;
    config.max_component_types = 0U;
    return config;
}

AFORC_Status aforc_ecs_create(const AFORC_EcsConfig *config, AFORC_Ecs **out_ecs) {
    AFORC_EcsConfig selected = aforc_ecs_config_default();
    AFORC_Ecs *ecs = NULL;
    const size_t handle_limit = aforc_ecs_handle_capacity_limit();
    AFORC_Status status = AFORC_OK;

    if (out_ecs == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_ecs = NULL;
    if (config != NULL) {
        selected = *config;
    }
    if (!aforc_ecs_allocator_is_valid(&selected.allocator)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (selected.max_entities == 0U) {
        selected.max_entities = handle_limit;
    }
    if (selected.max_component_types == 0U) {
        selected.max_component_types = handle_limit;
    }
    if (selected.max_entities > handle_limit ||
        selected.max_component_types > handle_limit) {
        return AFORC_ERROR_LIMIT;
    }
    if (selected.initial_entity_capacity > selected.max_entities ||
        selected.initial_component_capacity > selected.max_entities ||
        selected.initial_component_type_capacity >
            selected.max_component_types) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_ecs_allocate_array(&selected.allocator, 1U, sizeof(*ecs), true,
                                    (void **)&ecs);
    if (status != AFORC_OK) {
        return status;
    }
    ecs->allocator = selected.allocator;
    ecs->max_entities = selected.max_entities;
    ecs->max_component_types = selected.max_component_types;
    ecs->initial_component_capacity = selected.initial_component_capacity;
    ecs->free_head = AFORC_ENTITY_INVALID_INDEX;
    ecs->revision = UINT64_C(1);

    status = reserve_slots(ecs, selected.initial_entity_capacity);
    if (status == AFORC_OK) {
        status = aforc_ecs_reserve_stores(
            ecs, selected.initial_component_type_capacity);
    }
    if (status != AFORC_OK) {
        release_ecs_storage(ecs);
        aforc_ecs_deallocate(&selected.allocator, ecs);
        return status;
    }
    *out_ecs = ecs;
    return AFORC_OK;
}

void aforc_ecs_destroy(AFORC_Ecs *ecs) {
    size_t store_index = 0U;
    AFORC_Allocator allocator;

    if (ecs == NULL || ecs->cleanup_active) {
        return;
    }
    for (store_index = 0U; store_index < ecs->store_count; ++store_index) {
        AFORC_EcsComponentStore *store = &ecs->stores[store_index];
        while (store->count != 0U) {
            aforc_ecs_remove_component_at(ecs, store, store->count - 1U);
        }
    }
    allocator = ecs->allocator;
    release_ecs_storage(ecs);
    aforc_ecs_deallocate(&allocator, ecs);
}

AFORC_Status aforc_ecs_clear(AFORC_Ecs *ecs) {
    size_t store_index = 0U;
    size_t slot_cursor = 0U;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK) {
        return status;
    }
    if (ecs->active_count == 0U) {
        return AFORC_OK;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    for (store_index = 0U; store_index < ecs->store_count; ++store_index) {
        AFORC_EcsComponentStore *store = &ecs->stores[store_index];
        while (store->count != 0U) {
            aforc_ecs_remove_component_at(ecs, store, store->count - 1U);
        }
    }
    ecs->free_head = AFORC_ENTITY_INVALID_INDEX;
    slot_cursor = ecs->slot_count;
    while (slot_cursor != 0U) {
        const uint32_t index = (uint32_t)(slot_cursor - 1U);
        --slot_cursor;
        recycle_or_retire_slot(ecs, index);
    }
    ecs->active_count = 0U;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}

size_t aforc_ecs_entity_count(const AFORC_Ecs *ecs) {
    return ecs == NULL ? 0U : ecs->active_count;
}

size_t aforc_ecs_entity_capacity(const AFORC_Ecs *ecs) {
    return ecs == NULL ? 0U : ecs->slot_capacity;
}

bool aforc_ecs_entity_alive(const AFORC_Ecs *ecs, AFORC_Entity entity) {
    return aforc_ecs_validate_entity(ecs, entity) == AFORC_OK;
}

bool aforc_entity_equal(AFORC_Entity left, AFORC_Entity right) {
    return left.index == right.index && left.generation == right.generation;
}

AFORC_Status aforc_ecs_reserve_entities(AFORC_Ecs *ecs, size_t capacity) {
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK) {
        return status;
    }
    if (capacity > ecs->max_entities) {
        return AFORC_ERROR_LIMIT;
    }
    if (capacity <= ecs->slot_capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    status = reserve_slots(ecs, capacity);
    if (status == AFORC_OK) {
        aforc_ecs_commit_revision(ecs);
    }
    return status;
}

AFORC_Status aforc_ecs_create_entity(AFORC_Ecs *ecs, AFORC_Entity *out_entity) {
    AFORC_EcsEntitySlot *slot = NULL;
    uint32_t index = AFORC_ENTITY_INVALID_INDEX;
    size_t required = 0U;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (out_entity == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = AFORC_ENTITY_INVALID;
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    if (ecs->free_head != AFORC_ENTITY_INVALID_INDEX) {
        index = ecs->free_head;
        slot = &ecs->slots[index];
        ecs->free_head = slot->next_free;
    } else {
        if (ecs->slot_count >= ecs->max_entities ||
            !aforc_size_add(ecs->slot_count, 1U, &required)) {
            return AFORC_ERROR_LIMIT;
        }
        status = reserve_slots(ecs, required);
        if (status != AFORC_OK) {
            return status;
        }
        index = (uint32_t)ecs->slot_count;
        slot = &ecs->slots[index];
        slot->generation = 1U;
        ++ecs->slot_count;
    }
    slot->alive = true;
    slot->next_free = AFORC_ENTITY_INVALID_INDEX;
    ++ecs->active_count;
    out_entity->index = index;
    out_entity->generation = slot->generation;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}

AFORC_Status aforc_ecs_destroy_entity(AFORC_Ecs *ecs, AFORC_Entity entity) {
    size_t store_index = 0U;
    AFORC_Status status = aforc_ecs_require_mutable(ecs);

    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_validate_entity(ecs, entity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_require_revision(ecs);
    if (status != AFORC_OK) {
        return status;
    }
    for (store_index = 0U; store_index < ecs->store_count; ++store_index) {
        AFORC_EcsComponentStore *store = &ecs->stores[store_index];
        const size_t dense_index = aforc_ecs_find_component(store, entity);
        if (dense_index != SIZE_MAX) {
            aforc_ecs_remove_component_at(ecs, store, dense_index);
        }
    }
    recycle_or_retire_slot(ecs, entity.index);
    --ecs->active_count;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}
