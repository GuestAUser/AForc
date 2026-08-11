/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_optimization_support.h"

#include <stdio.h>

static bool test_view_case(size_t type_count)
{
    AFORC_ComponentType types[ECS_TEST_TYPE_LIMIT];
    AFORC_ComponentType required[ECS_TEST_TYPE_LIMIT];
    AFORC_Entity drivers[4];
    AFORC_Entity fillers[3U * (ECS_TEST_TYPE_LIMIT - 1U)];
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_EcsView *view = NULL;
    AFORC_Ecs *ecs = NULL;
    bool has_value = false;
    size_t type_index;
    size_t index;
    size_t visited = 0U;
    bool passed;

    passed = ecs_test_create(32U, type_count, 0U, &ecs) &&
             ecs_test_register_types(ecs, type_count, 0U, types);
    for (index = 0U; passed && index < 4U; ++index)
    {
        passed = ecs_test_create_entity(ecs, &drivers[index]);
    }
    for (index = 0U; passed && index < 3U * (type_count - 1U); ++index)
    {
        passed = ecs_test_create_entity(ecs, &fillers[index]);
    }
    for (index = 0U; passed && index < 4U; ++index)
    {
        passed = ecs_test_add(ecs, drivers[index], types[type_count - 1U]);
    }
    for (type_index = 0U; passed && type_index + 1U < type_count; ++type_index)
    {
        passed =
            ecs_test_add(ecs, drivers[0], types[type_index]) &&
            ecs_test_add(ecs, drivers[2], types[type_index]) &&
            ecs_test_add(ecs, fillers[3U * type_index], types[type_index]) &&
            ecs_test_add(
                ecs, fillers[3U * type_index + 1U], types[type_index]) &&
            ecs_test_add(ecs, fillers[3U * type_index + 2U], types[type_index]);
    }
    for (type_index = 0U; type_index + 1U < type_count; ++type_index)
    {
        required[type_index] = types[(type_index + 1U) % (type_count - 1U)];
    }
    required[type_count - 1U] = types[type_count - 1U];
    for (type_index = 0U; passed && type_index + 2U < type_count; ++type_index)
    {
        passed = ecs_test_add(ecs, drivers[1], required[type_index]) &&
                 ecs_test_add(ecs, drivers[3], required[type_index]);
    }
    passed =
        passed &&
        aforc_ecs_view_create(ecs, required, type_count, &view) == AFORC_OK &&
        ecs_test_next_match(view, ecs, drivers[0], required, type_count) &&
        ecs_test_next_match(view, ecs, drivers[2], required, type_count) &&
        ecs_test_exhausted(view, type_count);
    aforc_ecs_view_destroy(view);
    view = NULL;
    passed = passed && aforc_ecs_view_create(
                           ecs, required, type_count, &view) == AFORC_OK;
    while (passed)
    {
        passed =
            aforc_ecs_view_next(view, &entity, NULL, &has_value) == AFORC_OK;
        if (!passed || !has_value)
        {
            break;
        }
        ++visited;
    }
    passed = passed && visited == 2U;
    aforc_ecs_view_destroy(view);
    aforc_ecs_destroy(ecs);
    return passed;
}

static bool test_zero_and_revision(void)
{
    AFORC_ComponentType type = AFORC_COMPONENT_TYPE_INVALID;
    AFORC_Entity entities[4];
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_EcsView *view = NULL;
    AFORC_Ecs *ecs = NULL;
    void *component = NULL;
    bool has_value = false;
    size_t index;
    size_t visited = 0U;
    bool passed;

    passed = ecs_test_create(8U, 1U, 0U, &ecs) &&
             ecs_test_register_types(ecs, 1U, 0U, &type);
    for (index = 0U; passed && index < 4U; ++index)
    {
        passed = ecs_test_create_entity(ecs, &entities[index]);
    }
    passed = passed && aforc_ecs_destroy_entity(ecs, entities[1]) == AFORC_OK &&
             aforc_ecs_view_create(ecs, NULL, 0U, &view) == AFORC_OK;
    while (passed)
    {
        passed =
            aforc_ecs_view_next(view, &entity, NULL, &has_value) == AFORC_OK;
        if (!passed || !has_value)
        {
            break;
        }
        ++visited;
    }
    passed = passed && visited == 3U;
    aforc_ecs_view_destroy(view);
    view = NULL;
    passed = passed &&
             aforc_ecs_view_create(ecs, &type, 1U, &view) == AFORC_OK &&
             ecs_test_add(ecs, entities[0], type);
    if (passed)
    {
        component = &entity;
        passed = aforc_ecs_view_next(view, &entity, &component, &has_value) ==
                     AFORC_ERROR_STATE &&
                 !has_value &&
                 aforc_entity_equal(entity, AFORC_ENTITY_INVALID) &&
                 component == NULL;
    }
    if (passed)
    {
        passed = aforc_ecs_view_reset(view) == AFORC_OK &&
                 aforc_ecs_view_next(view, &entity, &component, &has_value) ==
                     AFORC_OK &&
                 has_value && aforc_entity_equal(entity, entities[0]) &&
                 component != NULL;
    }
    aforc_ecs_view_destroy(view);
    aforc_ecs_destroy(ecs);
    return passed;
}

int main(void)
{
    if (!ecs_test_sparse())
    {
        (void)fprintf(stderr, "sparse width/swap regression failed\n");
        return 3;
    }
    if (!test_view_case(2U) || !test_view_case(4U) || !test_view_case(8U))
    {
        (void)fprintf(stderr, "2/4/8-type view pointer parity failed\n");
        return 4;
    }
    if (!test_zero_and_revision())
    {
        (void)fprintf(stderr, "zero-type/revision view regression failed\n");
        return 5;
    }
    if (!ecs_test_lifecycle_cases())
    {
        (void)fprintf(stderr, "entity lifecycle/cleanup regression failed\n");
        return 6;
    }
    if (!ecs_test_storage_cases())
    {
        (void)fprintf(stderr, "component storage boundary regression failed\n");
        return 7;
    }
    (void)puts("ecs optimization: ok");
    return 0;
}
