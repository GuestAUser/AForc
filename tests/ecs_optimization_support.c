/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_optimization_support.h"

bool ecs_test_create(size_t max_entities,
                     size_t max_component_types,
                     size_t initial_component_capacity,
                     AFORC_Ecs **out_ecs)
{
    AFORC_EcsConfig config = aforc_ecs_config_default();
    config.initial_entity_capacity = 0U;
    config.max_entities = max_entities;
    config.initial_component_capacity = initial_component_capacity;
    config.initial_component_type_capacity = max_component_types;
    config.max_component_types = max_component_types;
    return aforc_ecs_create(&config, out_ecs) == AFORC_OK;
}

bool ecs_test_register_types(AFORC_Ecs *ecs,
                             size_t type_count,
                             size_t initial_capacity,
                             AFORC_ComponentType *types)
{
    const AFORC_EcsComponentDesc desc = {
        sizeof(EcsTestComponent),
        _Alignof(EcsTestComponent),
        initial_capacity,
        NULL,
        NULL,
    };
    size_t index;
    for (index = 0U; index < type_count; ++index)
    {
        if (aforc_ecs_register_component(ecs, &desc, &types[index]) != AFORC_OK)
        {
            return false;
        }
    }
    return true;
}

bool ecs_test_sparse(void)
{
    const size_t limit = aforc_ecs_handle_capacity_limit();
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity selected[3];
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    EcsTestComponent values[3];
    AFORC_EcsComponentStore *store;
    AFORC_Ecs *ecs = NULL;
    void *component = NULL;
    uint32_t encoded = UINT32_C(0);
    size_t index;
    bool passed;
    passed = limit != 0U && aforc_ecs_encode_sparse_index(0U, &encoded) &&
             encoded == UINT32_C(1) &&
             aforc_ecs_encode_sparse_index(limit - 1U, &encoded) &&
             encoded == (uint32_t)limit &&
             !aforc_ecs_encode_sparse_index(limit, &encoded) &&
             ecs_test_create(
                 (size_t)ECS_TEST_HIGH_ENTITY_INDEX + 1U, 1U, 0U, &ecs) &&
             ecs_test_register_types(ecs, 1U, 0U, &type);
    for (index = 0U; passed && index <= (size_t)ECS_TEST_HIGH_ENTITY_INDEX;
         ++index)
    {
        passed = ecs_test_create_entity(ecs, &entity);
        if (index + 3U > (size_t)ECS_TEST_HIGH_ENTITY_INDEX)
        {
            selected[index + 2U - (size_t)ECS_TEST_HIGH_ENTITY_INDEX] = entity;
        }
    }
    for (index = 0U; passed && index < 3U; ++index)
    {
        values[index] = (EcsTestComponent){selected[index].index,
                                           (uint32_t)(11U * (index + 1U))};
        passed =
            aforc_ecs_add(ecs, selected[index], type, &values[index], NULL) ==
            AFORC_OK;
    }
    if (!passed)
    {
        aforc_ecs_destroy(ecs);
        return false;
    }
    store = &ecs->stores[type.id];
    passed =
        sizeof(*store->sparse) == sizeof(uint32_t) &&
        store->sparse_capacity == (size_t)ECS_TEST_HIGH_ENTITY_INDEX + 1U &&
        store->sparse[selected[0].index] == UINT32_C(1) &&
        store->sparse[selected[1].index] == UINT32_C(2) &&
        store->sparse[selected[2].index] == UINT32_C(3) &&
        aforc_ecs_remove(ecs, selected[1], type) == AFORC_OK &&
        store->sparse[selected[1].index] == UINT32_C(0) &&
        store->sparse[selected[2].index] == UINT32_C(2) &&
        aforc_ecs_get(ecs, selected[2], type, &component) == AFORC_OK &&
        ((EcsTestComponent *)component)->type_id == values[2].type_id &&
        aforc_ecs_remove(ecs, selected[0], type) == AFORC_OK &&
        store->sparse[selected[0].index] == UINT32_C(0) &&
        store->sparse[selected[2].index] == UINT32_C(1) &&
        aforc_ecs_get(ecs, selected[2], type, &component) == AFORC_OK &&
        ((EcsTestComponent *)component)->type_id == values[2].type_id &&
        aforc_ecs_add(ecs, selected[1], type, &values[1], NULL) == AFORC_OK &&
        store->sparse[selected[1].index] == UINT32_C(2) &&
        aforc_ecs_remove(ecs, selected[1], type) == AFORC_OK &&
        store->sparse[selected[1].index] == UINT32_C(0);
    aforc_ecs_destroy(ecs);
    return passed;
}

bool ecs_test_next_match(AFORC_EcsView *view,
                         AFORC_Ecs *ecs,
                         AFORC_Entity expected,
                         const AFORC_ComponentType *required,
                         size_t type_count)
{
    void *components[ECS_TEST_TYPE_LIMIT];
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    int sentinel = 0;
    bool has_value = false;
    size_t index;
    for (index = 0U; index < type_count; ++index)
    {
        components[index] = &sentinel;
    }
    if (aforc_ecs_view_next(view, &entity, components, &has_value) !=
            AFORC_OK ||
        !has_value || !aforc_entity_equal(entity, expected))
    {
        return false;
    }
    for (index = 0U; index < type_count; ++index)
    {
        const EcsTestComponent *component =
            (const EcsTestComponent *)components[index];
        void *expected_component = NULL;
        if (aforc_ecs_get(ecs, entity, required[index], &expected_component) !=
                AFORC_OK ||
            components[index] != expected_component || component == NULL ||
            component->entity_index != entity.index ||
            component->type_id != required[index].id)
        {
            return false;
        }
    }
    return true;
}

bool ecs_test_exhausted(AFORC_EcsView *view, size_t type_count)
{
    void *components[ECS_TEST_TYPE_LIMIT];
    AFORC_Entity entity = {0U, 1U};
    bool has_value = true;
    size_t index;
    for (index = 0U; index < type_count; ++index)
    {
        components[index] = &entity;
    }
    if (aforc_ecs_view_next(view, &entity, components, &has_value) !=
            AFORC_OK ||
        has_value || !aforc_entity_equal(entity, AFORC_ENTITY_INVALID))
    {
        return false;
    }
    for (index = 0U; index < type_count; ++index)
    {
        if (components[index] != NULL)
        {
            return false;
        }
    }
    return true;
}
