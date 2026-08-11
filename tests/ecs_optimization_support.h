/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TESTS_ECS_OPTIMIZATION_SUPPORT_H
#define AFORC_TESTS_ECS_OPTIMIZATION_SUPPORT_H

#include "../src/ecs/ecs_internal.h"

enum
{
    ECS_TEST_TYPE_LIMIT = 8,
    ECS_TEST_HIGH_ENTITY_INDEX = 65535
};

typedef struct EcsTestComponent
{
    uint32_t entity_index;
    uint32_t type_id;
} EcsTestComponent;

bool ecs_test_create(size_t max_entities,
                     size_t max_component_types,
                     size_t initial_component_capacity,
                     AFORC_Ecs **out_ecs);
bool ecs_test_register_types(AFORC_Ecs *ecs,
                             size_t type_count,
                             size_t initial_capacity,
                             AFORC_ComponentType *types);
static inline bool ecs_test_create_entity(AFORC_Ecs *ecs, AFORC_Entity *entity)
{
    return aforc_ecs_create_entity(ecs, entity) == AFORC_OK;
}
static inline bool
ecs_test_add(AFORC_Ecs *ecs, AFORC_Entity entity, AFORC_ComponentType type)
{
    const EcsTestComponent value = {entity.index, type.id};

    return aforc_ecs_add(ecs, entity, type, &value, NULL) == AFORC_OK;
}
bool ecs_test_sparse(void);
bool ecs_test_next_match(AFORC_EcsView *view,
                         AFORC_Ecs *ecs,
                         AFORC_Entity expected,
                         const AFORC_ComponentType *required,
                         size_t type_count);
bool ecs_test_exhausted(AFORC_EcsView *view, size_t type_count);
bool ecs_test_lifecycle_cases(void);
bool ecs_test_storage_cases(void);

#endif
