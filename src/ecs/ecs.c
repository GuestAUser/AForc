/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

AFORC_Status aforc_ecs_allocate_array(const AFORC_Allocator *allocator,
                                      size_t count,
                                      size_t element_size,
                                      bool zero_initialize,
                                      void **out_memory)
{
    size_t byte_count = 0U;
    void *memory = NULL;
    AFORC_Status status;

    if (!aforc_allocator_is_valid(allocator) || out_memory == NULL) {
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

size_t aforc_ecs_handle_capacity_limit(void)
{
#if SIZE_MAX < UINT32_MAX
    return SIZE_MAX;
#else
    return (size_t)UINT32_MAX;
#endif
}

AFORC_Status aforc_ecs_choose_capacity(size_t current,
                                       size_t required,
                                       size_t limit,
                                       size_t *out_capacity)
{
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
        capacity = capacity > limit / 2U ? limit : capacity * 2U;
        if (capacity == 0U) {
            return AFORC_ERROR_OVERFLOW;
        }
    }
    *out_capacity = capacity;
    return AFORC_OK;
}

AFORC_Status aforc_ecs_require_mutable(AFORC_Ecs *ecs)
{
    if (ecs == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    return ecs->cleanup_active ? AFORC_ERROR_STATE : AFORC_OK;
}

AFORC_Status aforc_ecs_require_revision(const AFORC_Ecs *ecs)
{
    return ecs->revision == UINT64_MAX ? AFORC_ERROR_LIMIT : AFORC_OK;
}

void aforc_ecs_commit_revision(AFORC_Ecs *ecs)
{
    ecs->revision += UINT64_C(1);
}

static void release_ecs_storage(AFORC_Ecs *ecs)
{
    size_t store_index;

    for (store_index = 0U; store_index < ecs->store_count; ++store_index) {
        aforc_ecs_release_store(&ecs->allocator, &ecs->stores[store_index]);
    }
    aforc_free(&ecs->allocator, ecs->stores);
    aforc_free(&ecs->allocator, ecs->slots);
    ecs->stores = NULL;
    ecs->slots = NULL;
}

AFORC_EcsConfig aforc_ecs_config_default(void)
{
    AFORC_EcsConfig config;

    config.allocator = aforc_allocator_default();
    config.initial_entity_capacity = 64U;
    config.max_entities = 0U;
    config.initial_component_capacity = 16U;
    config.initial_component_type_capacity = 8U;
    config.max_component_types = 0U;
    return config;
}

AFORC_Status aforc_ecs_create(const AFORC_EcsConfig *config,
                              AFORC_Ecs **out_ecs)
{
    AFORC_EcsConfig selected = aforc_ecs_config_default();
    AFORC_Ecs *ecs = NULL;
    const size_t handle_limit = aforc_ecs_handle_capacity_limit();
    AFORC_Status status;

    if (out_ecs == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_ecs = NULL;
    if (config != NULL) {
        selected = *config;
    }
    if (!aforc_allocator_is_valid(&selected.allocator)) {
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
    status = aforc_ecs_allocate_array(&selected.allocator,
                                      1U,
                                      sizeof(*ecs),
                                      true,
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

    status = aforc_ecs_reserve_slots(ecs, selected.initial_entity_capacity);
    if (status == AFORC_OK) {
        status = aforc_ecs_reserve_stores(
            ecs, selected.initial_component_type_capacity);
    }
    if (status != AFORC_OK) {
        release_ecs_storage(ecs);
        aforc_free(&selected.allocator, ecs);
        return status;
    }
    *out_ecs = ecs;
    return AFORC_OK;
}

void aforc_ecs_destroy(AFORC_Ecs *ecs)
{
    AFORC_Allocator allocator;

    if (ecs == NULL || ecs->cleanup_active) {
        return;
    }
    aforc_ecs_clear_component_stores(ecs);
    allocator = ecs->allocator;
    release_ecs_storage(ecs);
    aforc_free(&allocator, ecs);
}

AFORC_Status aforc_ecs_clear(AFORC_Ecs *ecs)
{
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
    aforc_ecs_clear_component_stores(ecs);
    ecs->free_head = AFORC_ENTITY_INVALID_INDEX;
    slot_cursor = ecs->slot_count;
    while (slot_cursor != 0U) {
        const uint32_t index = (uint32_t)(slot_cursor - 1U);
        --slot_cursor;
        aforc_ecs_recycle_or_retire_slot(ecs, index);
    }
    ecs->active_count = 0U;
    aforc_ecs_commit_revision(ecs);
    return AFORC_OK;
}
