/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L
#include "ecs_optimization_support.h"
#include <stdio.h>
#include <time.h>
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

bool ecs_test_sparse(size_t *out_sparse_bytes)
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
    *out_sparse_bytes = store->sparse_capacity * sizeof(*store->sparse);
    (void)printf("sparse memory: capacity=%zu element=%zu bytes=%zu\n",
                 store->sparse_capacity,
                 sizeof(*store->sparse),
                 *out_sparse_bytes);
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

static uint64_t monotonic_nanoseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return UINT64_C(0);
    }
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) + (uint64_t)now.tv_nsec;
}

bool ecs_test_benchmark(void)
{
    AFORC_ComponentType types[ECS_TEST_TYPE_LIMIT];
    AFORC_Entity entity = AFORC_ENTITY_INVALID;
    AFORC_Ecs *ecs = NULL;
    uint64_t checksum = UINT64_C(0);
    uint64_t started;
    uint64_t elapsed;
    size_t entity_index;
    size_t round;
    if (!ecs_test_create(ECS_BENCHMARK_ENTITY_COUNT,
                         ECS_TEST_TYPE_LIMIT,
                         ECS_BENCHMARK_ENTITY_COUNT,
                         &ecs) ||
        !ecs_test_register_types(
            ecs, ECS_TEST_TYPE_LIMIT, ECS_BENCHMARK_ENTITY_COUNT, types))
    {
        aforc_ecs_destroy(ecs);
        return false;
    }
    for (entity_index = 0U; entity_index < ECS_BENCHMARK_ENTITY_COUNT;
         ++entity_index)
    {
        size_t type_index;
        if (!ecs_test_create_entity(ecs, &entity))
        {
            aforc_ecs_destroy(ecs);
            return false;
        }
        for (type_index = 0U; type_index < ECS_TEST_TYPE_LIMIT; ++type_index)
        {
            if (!ecs_test_add(ecs, entity, types[type_index]))
            {
                aforc_ecs_destroy(ecs);
                return false;
            }
        }
    }
    (void)printf("benchmark memory: capacity=%zu element=%zu bytes/store=%zu\n",
                 ecs->stores[0].sparse_capacity,
                 sizeof(*ecs->stores[0].sparse),
                 ecs->stores[0].sparse_capacity *
                     sizeof(*ecs->stores[0].sparse));
    started = monotonic_nanoseconds();
    for (round = 0U; round < ECS_BENCHMARK_ROUNDS; ++round)
    {
        AFORC_EcsView *view = NULL;
        void *components[ECS_TEST_TYPE_LIMIT];
        bool has_value = false;
        if (aforc_ecs_view_create(ecs, types, ECS_TEST_TYPE_LIMIT, &view) !=
            AFORC_OK)
        {
            aforc_ecs_destroy(ecs);
            return false;
        }
        for (;;)
        {
            size_t type_index;
            if (aforc_ecs_view_next(view, &entity, components, &has_value) !=
                AFORC_OK)
            {
                aforc_ecs_view_destroy(view);
                aforc_ecs_destroy(ecs);
                return false;
            }
            if (!has_value)
            {
                break;
            }
            for (type_index = 0U; type_index < ECS_TEST_TYPE_LIMIT;
                 ++type_index)
            {
                const EcsTestComponent *component =
                    (const EcsTestComponent *)components[type_index];
                checksum += (uint64_t)component->entity_index +
                            (uint64_t)component->type_id;
            }
        }
        aforc_ecs_view_destroy(view);
    }
    elapsed = monotonic_nanoseconds() - started;
    (void)printf(
        "benchmark iteration: visits=%zu elapsed_ns=%llu ns/visit=%.3f "
        "checksum=%llu\n",
        (size_t)ECS_BENCHMARK_ENTITY_COUNT * (size_t)ECS_BENCHMARK_ROUNDS,
        (unsigned long long)elapsed,
        (double)elapsed /
            ((double)ECS_BENCHMARK_ENTITY_COUNT * (double)ECS_BENCHMARK_ROUNDS),
        (unsigned long long)checksum);
    aforc_ecs_destroy(ecs);
    return true;
}
