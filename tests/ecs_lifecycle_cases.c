/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_optimization_support.h"

typedef struct EcsLifecycleCleanupProbe {
    AFORC_ComponentType type;
    AFORC_EcsView *view;
    void *expected_component;
    size_t invocation_count;
    bool component_visible;
    bool mutation_blocked;
    bool view_blocked;
} EcsLifecycleCleanupProbe;

static void lifecycle_cleanup(AFORC_Ecs *ecs,
                              AFORC_Entity entity,
                              void *component,
                              void *user_data)
{
    EcsLifecycleCleanupProbe *probe =
        (EcsLifecycleCleanupProbe *)user_data;
    const AFORC_EcsComponentDesc desc = {
        sizeof(EcsTestComponent),
        _Alignof(EcsTestComponent),
        0U,
        NULL,
        NULL,
    };
    AFORC_ComponentType extra_type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity extra_entity = AFORC_ENTITY_INVALID;
    AFORC_Entity view_entity = entity;
    void *queried = NULL;
    void *view_component = component;
    bool view_has_value = true;

    ++probe->invocation_count;
    probe->component_visible =
        aforc_ecs_get(ecs, entity, probe->type, &queried) == AFORC_OK &&
        queried == component && component == probe->expected_component;
    probe->mutation_blocked =
        aforc_ecs_clear(ecs) == AFORC_ERROR_STATE &&
        aforc_ecs_reserve_entities(ecs, 4U) == AFORC_ERROR_STATE &&
        aforc_ecs_create_entity(ecs, &extra_entity) == AFORC_ERROR_STATE &&
        aforc_ecs_destroy_entity(ecs, entity) == AFORC_ERROR_STATE &&
        aforc_ecs_register_component(ecs, &desc, &extra_type) ==
            AFORC_ERROR_STATE &&
        aforc_ecs_reserve_component(ecs, probe->type, 4U) ==
            AFORC_ERROR_STATE &&
        aforc_ecs_add(ecs, entity, probe->type, NULL, NULL) ==
            AFORC_ERROR_STATE &&
        aforc_ecs_remove(ecs, entity, probe->type) == AFORC_ERROR_STATE;
    probe->view_blocked =
        aforc_ecs_view_next(probe->view,
                            &view_entity,
                            &view_component,
                            &view_has_value) == AFORC_ERROR_STATE &&
        !view_has_value &&
        aforc_entity_equal(view_entity, AFORC_ENTITY_INVALID) &&
        view_component == NULL;
}

static bool test_generation_reuse_and_retirement(void)
{
    AFORC_Entity first = AFORC_ENTITY_INVALID;
    AFORC_Entity second = AFORC_ENTITY_INVALID;
    AFORC_Entity replacement = AFORC_ENTITY_INVALID;
    AFORC_Entity retired = AFORC_ENTITY_INVALID;
    AFORC_Ecs *ecs = NULL;
    bool passed;

    passed = ecs_test_create(3U, 1U, 0U, &ecs) &&
             ecs_test_create_entity(ecs, &first) &&
             ecs_test_create_entity(ecs, &second) &&
             aforc_ecs_destroy_entity(ecs, first) == AFORC_OK &&
             !aforc_ecs_entity_alive(ecs, first) &&
             aforc_ecs_destroy_entity(ecs, first) == AFORC_ERROR_STALE_HANDLE &&
             ecs_test_create_entity(ecs, &replacement) &&
             replacement.index == first.index &&
             replacement.generation == first.generation + UINT32_C(1);
    if (passed) {
        ecs->slots[replacement.index].generation = UINT32_MAX - UINT32_C(1);
        replacement.generation = UINT32_MAX - UINT32_C(1);
        passed = aforc_ecs_destroy_entity(ecs, replacement) == AFORC_OK &&
                 ecs->slots[replacement.index].generation == UINT32_MAX &&
                 ecs->slots[replacement.index].next_free ==
                     AFORC_ENTITY_INVALID_INDEX &&
                 ecs_test_create_entity(ecs, &retired) &&
                 retired.index != replacement.index &&
                 aforc_ecs_create_entity(ecs, &first) == AFORC_ERROR_LIMIT;
    }
    aforc_ecs_destroy(ecs);
    return passed;
}

static bool test_cleanup_lifecycle(void)
{
    AFORC_EcsComponentDesc desc = {0};
    EcsLifecycleCleanupProbe probe = {0};
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    EcsTestComponent value = {0};
    AFORC_Ecs *ecs = NULL;
    bool has_component = true;
    bool passed;

    passed = ecs_test_create(4U, 2U, 0U, &ecs);
    desc.size = sizeof(EcsTestComponent);
    desc.alignment = _Alignof(EcsTestComponent);
    desc.cleanup = lifecycle_cleanup;
    desc.cleanup_user_data = &probe;
    passed = passed &&
             aforc_ecs_register_component(ecs, &desc, &probe.type) ==
                 AFORC_OK &&
             ecs_test_create_entity(ecs, &entity);
    value = (EcsTestComponent){entity.index, probe.type.id};
    passed = passed &&
             aforc_ecs_add(ecs,
                           entity,
                           probe.type,
                           &value,
                           &probe.expected_component) == AFORC_OK &&
             aforc_ecs_view_create(ecs, &probe.type, 1U, &probe.view) ==
                 AFORC_OK &&
             aforc_ecs_remove(ecs, entity, probe.type) == AFORC_OK &&
             probe.invocation_count == 1U && probe.component_visible &&
             probe.mutation_blocked && probe.view_blocked &&
             aforc_ecs_has(ecs, entity, probe.type, &has_component) ==
                 AFORC_OK &&
             !has_component;
    aforc_ecs_view_destroy(probe.view);
    aforc_ecs_destroy(ecs);
    return passed;
}

static void count_cleanup(AFORC_Ecs *ecs,
                          AFORC_Entity entity,
                          void *component,
                          void *user_data)
{
    size_t *count = (size_t *)user_data;

    (void)ecs;
    (void)entity;
    (void)component;
    ++*count;
}

static bool test_cleanup_paths(void)
{
    AFORC_EcsComponentDesc desc = {0};
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entities[4];
    AFORC_Ecs *ecs = NULL;
    size_t cleanup_count = 0U;
    size_t index;
    bool passed;

    passed = ecs_test_create(8U, 1U, 0U, &ecs);
    desc.size = sizeof(EcsTestComponent);
    desc.alignment = _Alignof(EcsTestComponent);
    desc.cleanup = count_cleanup;
    desc.cleanup_user_data = &cleanup_count;
    passed = passed &&
             aforc_ecs_register_component(ecs, &desc, &type) == AFORC_OK;
    for (index = 0U; passed && index < 4U; ++index) {
        passed = ecs_test_create_entity(ecs, &entities[index]) &&
                 ecs_test_add(ecs, entities[index], type);
    }
    passed = passed &&
             aforc_ecs_destroy_entity(ecs, entities[0]) == AFORC_OK &&
             cleanup_count == 1U &&
             aforc_ecs_remove(ecs, entities[1], type) == AFORC_OK &&
             cleanup_count == 2U && aforc_ecs_clear(ecs) == AFORC_OK &&
             cleanup_count == 4U && aforc_ecs_entity_count(ecs) == 0U &&
             ecs_test_create_entity(ecs, &entities[0]) &&
             ecs_test_add(ecs, entities[0], type);
    if (passed) {
        aforc_ecs_destroy(ecs);
        ecs = NULL;
        passed = cleanup_count == 5U;
    }
    aforc_ecs_destroy(ecs);
    return passed;
}

bool ecs_test_lifecycle_cases(void)
{
    return test_generation_reuse_and_retirement() && test_cleanup_lifecycle() &&
           test_cleanup_paths();
}
