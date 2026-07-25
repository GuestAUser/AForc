/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "component_store_internal.h"

#include <string.h>

AFORC_Status aforc_ecs_reserve_slots(AFORC_Ecs *ecs, size_t required)
{
    AFORC_EcsEntitySlot *replacement = NULL;
    size_t capacity = 0U;
    size_t copy_bytes = 0U;
    AFORC_Status status;

    if (required <= ecs->slot_capacity) {
        return AFORC_OK;
    }
    status = aforc_ecs_choose_capacity(ecs->slot_capacity,
                                       required,
                                       ecs->max_entities,
                                       &capacity);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_ecs_allocate_array(&ecs->allocator,
                                      capacity,
                                      sizeof(*replacement),
                                      false,
                                      (void **)&replacement);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_multiply(ecs->slot_count,
                             sizeof(*replacement),
                             &copy_bytes)) {
        aforc_free(&ecs->allocator, replacement);
        return AFORC_ERROR_OVERFLOW;
    }
    if (copy_bytes != 0U) {
        (void)memcpy(replacement, ecs->slots, copy_bytes);
    }
    aforc_free(&ecs->allocator, ecs->slots);
    ecs->slots = replacement;
    ecs->slot_capacity = capacity;
    return AFORC_OK;
}

void aforc_ecs_recycle_or_retire_slot(AFORC_Ecs *ecs, uint32_t index)
{
    AFORC_EcsEntitySlot *slot = &ecs->slots[index];

    if (slot->alive && slot->generation != UINT32_MAX) {
        ++slot->generation;
    }
    slot->alive = false;
    slot->next_free = AFORC_ENTITY_INVALID_INDEX;
    if (slot->generation != UINT32_MAX) {
        slot->next_free = ecs->free_head;
        ecs->free_head = index;
    }
}

AFORC_Status aforc_ecs_validate_entity(const AFORC_Ecs *ecs,
                                       AFORC_Entity entity)
{
    const AFORC_EcsEntitySlot *slot;

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

size_t aforc_ecs_entity_count(const AFORC_Ecs *ecs)
{
    return ecs == NULL ? 0U : ecs->active_count;
}

size_t aforc_ecs_entity_capacity(const AFORC_Ecs *ecs)
{
    return ecs == NULL ? 0U : ecs->slot_capacity;
}

bool aforc_ecs_entity_alive(const AFORC_Ecs *ecs, AFORC_Entity entity)
{
    return aforc_ecs_validate_entity(ecs, entity) == AFORC_OK;
}

bool aforc_entity_equal(AFORC_Entity left, AFORC_Entity right)
{
    return left.index == right.index && left.generation == right.generation;
}

AFORC_Status aforc_ecs_reserve_entities(AFORC_Ecs *ecs, size_t capacity)
{
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
    status = aforc_ecs_reserve_slots(ecs, capacity);
    if (status == AFORC_OK) {
        aforc_ecs_commit_revision(ecs);
    }
    return status;
}

AFORC_Status aforc_ecs_create_entity(AFORC_Ecs *ecs,
                                     AFORC_Entity *out_entity)
{
    AFORC_EcsEntitySlot *slot;
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
        status = aforc_ecs_reserve_slots(ecs, required);
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

AFORC_Status aforc_ecs_destroy_entity(AFORC_Ecs *ecs, AFORC_Entity entity)
{
    size_t store_index;
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
    aforc_ecs_recycle_or_retire_slot(ecs, entity.index);
    --ecs->active_count;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}
