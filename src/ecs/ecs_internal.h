/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ECS_INTERNAL_H
#define AFORC_ECS_INTERNAL_H

#include "../core/common_internal.h"
#include "aforc/ecs.h"

#include <stddef.h>
#include <stdint.h>

typedef struct AFORC_EcsEntitySlot
{
    uint32_t generation;
    uint32_t next_free;
    bool alive;
} AFORC_EcsEntitySlot;

typedef struct AFORC_EcsComponentStore
{
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

struct AFORC_Ecs
{
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

struct AFORC_EcsView
{
    AFORC_Allocator allocator;
    AFORC_Ecs *ecs;
    AFORC_ComponentType *required_types;
    size_t required_type_count;
    size_t cursor;
    uint32_t driver_type_id;
    uint64_t revision;
};

AFORC_INTERNAL AFORC_Status
aforc_ecs_allocate_array(const AFORC_Allocator *allocator,
                         size_t count,
                         size_t element_size,
                         bool zero_initialize,
                         void **out_memory);
AFORC_INTERNAL size_t aforc_ecs_handle_capacity_limit(void);
AFORC_INTERNAL bool aforc_ecs_encode_sparse_index(size_t dense_index,
                                                  uint32_t *out_encoded_index);
AFORC_INTERNAL AFORC_Status aforc_ecs_choose_capacity(size_t current,
                                                      size_t required,
                                                      size_t limit,
                                                      size_t *out_capacity);
AFORC_INTERNAL AFORC_Status aforc_ecs_require_mutable(AFORC_Ecs *ecs);
AFORC_INTERNAL AFORC_Status aforc_ecs_require_revision(const AFORC_Ecs *ecs);
AFORC_INTERNAL void aforc_ecs_commit_revision(AFORC_Ecs *ecs);

AFORC_INTERNAL AFORC_Status aforc_ecs_reserve_slots(AFORC_Ecs *ecs,
                                                    size_t required);
AFORC_INTERNAL void aforc_ecs_recycle_or_retire_slot(AFORC_Ecs *ecs,
                                                     uint32_t index);
AFORC_INTERNAL AFORC_Status aforc_ecs_validate_entity(const AFORC_Ecs *ecs,
                                                      AFORC_Entity entity);

AFORC_INTERNAL AFORC_Status aforc_ecs_reserve_stores(AFORC_Ecs *ecs,
                                                     size_t required);
AFORC_INTERNAL AFORC_Status aforc_ecs_reserve_sparse(
    AFORC_Ecs *ecs, AFORC_EcsComponentStore *store, size_t required);
AFORC_INTERNAL AFORC_Status aforc_ecs_reserve_dense(
    AFORC_Ecs *ecs, AFORC_EcsComponentStore *store, size_t required);
AFORC_INTERNAL AFORC_Status
aforc_ecs_get_store(const AFORC_Ecs *ecs,
                    AFORC_ComponentType type,
                    AFORC_EcsComponentStore **out_store);
AFORC_INTERNAL AFORC_Status
aforc_ecs_get_store_const(const AFORC_Ecs *ecs,
                          AFORC_ComponentType type,
                          const AFORC_EcsComponentStore **out_store);
AFORC_INTERNAL void aforc_ecs_remove_component_at(
    AFORC_Ecs *ecs, AFORC_EcsComponentStore *store, size_t dense_index);
AFORC_INTERNAL void aforc_ecs_clear_component_stores(AFORC_Ecs *ecs);
AFORC_INTERNAL void aforc_ecs_release_store(const AFORC_Allocator *allocator,
                                            AFORC_EcsComponentStore *store);

#endif
