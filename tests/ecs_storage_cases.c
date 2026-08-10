/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_optimization_support.h"

#include <stdlib.h>

typedef struct EcsFailAllocator
{
    size_t calls;
    size_t fail_at;
    size_t outstanding;
} EcsFailAllocator;

static bool allocator_should_fail(EcsFailAllocator *allocator)
{
    ++allocator->calls;
    return allocator->fail_at != 0U && allocator->calls == allocator->fail_at;
}

static void *fail_allocate(void *context, size_t size)
{
    EcsFailAllocator *allocator = (EcsFailAllocator *)context;
    void *memory;

    if (allocator_should_fail(allocator))
    {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL)
    {
        ++allocator->outstanding;
    }
    return memory;
}

static void *fail_reallocate(void *context, void *memory, size_t size)
{
    EcsFailAllocator *allocator = (EcsFailAllocator *)context;

    if (allocator_should_fail(allocator))
    {
        return NULL;
    }
    return realloc(memory, size);
}

static void fail_deallocate(void *context, void *memory)
{
    EcsFailAllocator *allocator = (EcsFailAllocator *)context;

    if (memory != NULL)
    {
        --allocator->outstanding;
        free(memory);
    }
}

static bool test_aliasing_growth(void)
{
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity first = AFORC_ENTITY_INVALID;
    AFORC_Entity second = AFORC_ENTITY_INVALID;
    AFORC_Ecs *ecs = NULL;
    EcsTestComponent value = {17U, 29U};
    EcsTestComponent *first_component = NULL;
    EcsTestComponent *second_component = NULL;
    const void *first_after_growth = NULL;
    size_t count = 0U;
    bool passed;

    passed =
        ecs_test_create(4U, 1U, 1U, &ecs) &&
        ecs_test_register_types(ecs, 1U, 1U, &type) &&
        ecs_test_create_entity(ecs, &first) &&
        ecs_test_create_entity(ecs, &second) &&
        aforc_ecs_add(ecs, first, type, &value, (void **)&first_component) ==
            AFORC_OK &&
        aforc_ecs_add(
            ecs, second, type, first_component, (void **)&second_component) ==
            AFORC_OK &&
        aforc_ecs_get_const(ecs, first, type, &first_after_growth) ==
            AFORC_OK &&
        aforc_ecs_component_instance_count(ecs, type, &count) == AFORC_OK &&
        count == 2U && second_component != NULL && first_after_growth != NULL &&
        second_component->entity_index == value.entity_index &&
        second_component->type_id == value.type_id &&
        ((const EcsTestComponent *)first_after_growth)->entity_index ==
            value.entity_index &&
        ((const EcsTestComponent *)first_after_growth)->type_id ==
            value.type_id;
    aforc_ecs_destroy(ecs);
    return passed;
}

static bool run_add_failure(size_t failure_offset)
{
    EcsFailAllocator allocator = {0};
    AFORC_EcsConfig config = aforc_ecs_config_default();
    AFORC_EcsComponentDesc desc = {0};
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    EcsTestComponent value = {1U, 2U};
    AFORC_Ecs *ecs = NULL;
    void *component = &value;
    size_t count = SIZE_MAX;
    bool has_component = true;
    bool passed;

    config.allocator = (AFORC_Allocator){
        &allocator, fail_allocate, fail_reallocate, fail_deallocate};
    config.initial_entity_capacity = 0U;
    config.max_entities = 2U;
    config.initial_component_capacity = 0U;
    config.initial_component_type_capacity = 0U;
    config.max_component_types = 1U;
    desc.size = sizeof(EcsTestComponent);
    desc.alignment = _Alignof(EcsTestComponent);
    passed = aforc_ecs_create(&config, &ecs) == AFORC_OK &&
             aforc_ecs_register_component(ecs, &desc, &type) == AFORC_OK &&
             ecs_test_create_entity(ecs, &entity);
    allocator.fail_at = allocator.calls + failure_offset;
    passed =
        passed &&
        aforc_ecs_add(ecs, entity, type, &value, &component) ==
            AFORC_ERROR_OUT_OF_MEMORY &&
        component == NULL &&
        aforc_ecs_component_instance_count(ecs, type, &count) == AFORC_OK &&
        count == 0U &&
        aforc_ecs_has(ecs, entity, type, &has_component) == AFORC_OK &&
        !has_component;
    allocator.fail_at = 0U;
    passed = passed &&
             aforc_ecs_add(ecs, entity, type, &value, &component) == AFORC_OK &&
             component != NULL;
    aforc_ecs_destroy(ecs);
    return passed && allocator.outstanding == 0U;
}

static bool test_failure_atomicity(void)
{
    size_t failure_offset;

    for (failure_offset = 1U; failure_offset <= 4U; ++failure_offset)
    {
        if (!run_add_failure(failure_offset))
        {
            return false;
        }
    }
    return true;
}

static bool test_view_and_registration_boundaries(void)
{
    AFORC_EcsComponentDesc overflow_desc = {0};
    AFORC_ComponentType types[2];
    AFORC_ComponentType duplicate[2];
    AFORC_ComponentType invalid = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_EcsView *view = NULL;
    AFORC_Ecs *ecs = NULL;
    bool has_value = true;
    bool passed;

    passed = ecs_test_create(4U, 2U, 0U, &ecs);
    overflow_desc.size = SIZE_MAX;
    overflow_desc.alignment = 2U;
    passed = passed &&
             aforc_ecs_register_component(ecs, &overflow_desc, &invalid) ==
                 AFORC_ERROR_OVERFLOW &&
             invalid.id == AFORC_COMPONENT_TYPE_INVALID_ID &&
             ecs_test_register_types(ecs, 2U, 0U, types) &&
             ecs_test_create_entity(ecs, &entity) &&
             ecs_test_add(ecs, entity, types[0]);
    duplicate[0] = types[0];
    duplicate[1] = types[0];
    passed = passed &&
             aforc_ecs_view_create(ecs, duplicate, 2U, &view) ==
                 AFORC_ERROR_INVALID_ARGUMENT &&
             view == NULL &&
             aforc_ecs_view_create(ecs, &invalid, 1U, &view) ==
                 AFORC_ERROR_NOT_FOUND &&
             view == NULL &&
             aforc_ecs_view_create(ecs, types, 2U, &view) == AFORC_OK &&
             aforc_ecs_view_next(view, &entity, NULL, &has_value) == AFORC_OK &&
             !has_value;
    aforc_ecs_view_destroy(view);
    aforc_ecs_destroy(ecs);
    return passed;
}

static bool test_reserve_invalidation(void)
{
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_EcsView *view = NULL;
    AFORC_Ecs *ecs = NULL;
    bool has_value = true;
    bool passed;

    passed = ecs_test_create(8U, 1U, 0U, &ecs) &&
             ecs_test_register_types(ecs, 1U, 0U, &type) &&
             ecs_test_create_entity(ecs, &entity) &&
             ecs_test_add(ecs, entity, type) &&
             aforc_ecs_view_create(ecs, &type, 1U, &view) == AFORC_OK &&
             aforc_ecs_reserve_component(ecs, type, 4U) == AFORC_OK &&
             aforc_ecs_view_next(view, &entity, NULL, &has_value) ==
                 AFORC_ERROR_STATE &&
             !has_value;
    aforc_ecs_view_destroy(view);
    view = NULL;
    passed = passed &&
             aforc_ecs_view_create(ecs, NULL, 0U, &view) == AFORC_OK &&
             aforc_ecs_reserve_entities(ecs, 8U) == AFORC_OK &&
             aforc_ecs_view_next(view, &entity, NULL, &has_value) ==
                 AFORC_ERROR_STATE &&
             !has_value;
    aforc_ecs_view_destroy(view);
    aforc_ecs_destroy(ecs);
    return passed;
}

static bool test_view_reset_reuses_storage(void)
{
    EcsFailAllocator allocator = {0};
    AFORC_EcsConfig config = aforc_ecs_config_default();
    const AFORC_EcsComponentDesc desc = {
        sizeof(EcsTestComponent),
        _Alignof(EcsTestComponent),
        2U,
        NULL,
        NULL,
    };
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_EcsView *view = NULL;
    AFORC_Ecs *ecs = NULL;
    void *component = NULL;
    bool has_value = false;
    size_t calls_before_reset;
    bool passed;

    config.allocator = (AFORC_Allocator){
        &allocator, fail_allocate, fail_reallocate, fail_deallocate};
    config.initial_entity_capacity = 2U;
    config.max_entities = 2U;
    config.initial_component_capacity = 2U;
    config.initial_component_type_capacity = 1U;
    config.max_component_types = 1U;
    passed = aforc_ecs_create(&config, &ecs) == AFORC_OK &&
             aforc_ecs_register_component(ecs, &desc, &type) == AFORC_OK &&
             ecs_test_create_entity(ecs, &entity) &&
             ecs_test_add(ecs, entity, type) &&
             aforc_ecs_view_create(ecs, &type, 1U, &view) == AFORC_OK;
    calls_before_reset = allocator.calls;
    allocator.fail_at = calls_before_reset + 1U;
    passed = passed && aforc_ecs_view_reset(view) == AFORC_OK &&
             allocator.calls == calls_before_reset &&
             aforc_ecs_view_next(view, &entity, &component, &has_value) ==
                 AFORC_OK &&
             has_value && component != NULL;
    aforc_ecs_view_destroy(view);
    aforc_ecs_destroy(ecs);
    return passed && allocator.outstanding == 0U;
}

bool ecs_test_storage_cases(void)
{
    return test_aliasing_growth() && test_failure_atomicity() &&
           test_view_and_registration_boundaries() &&
           test_reserve_invalidation() && test_view_reset_reuses_storage();
}
