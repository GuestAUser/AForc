/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ECS_H
#define AFORC_ECS_H

#include <aforc/common.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define AFORC_ENTITY_INVALID_INDEX UINT32_MAX
#define AFORC_COMPONENT_TYPE_INVALID_ID UINT32_MAX
#ifdef __cplusplus
#define AFORC_ENTITY_INVALID                                                   \
    AFORC_Entity                                                               \
    {                                                                          \
        AFORC_ENTITY_INVALID_INDEX, UINT32_C(0)                                \
    }
#define AFORC_COMPONENT_TYPE_INVALID                                           \
    AFORC_ComponentType                                                        \
    {                                                                          \
        AFORC_COMPONENT_TYPE_INVALID_ID                                        \
    }
#else
#define AFORC_ENTITY_INVALID ((AFORC_Entity){AFORC_ENTITY_INVALID_INDEX, 0u})
#define AFORC_COMPONENT_TYPE_INVALID                                           \
    ((AFORC_ComponentType){AFORC_COMPONENT_TYPE_INVALID_ID})
#endif

typedef struct AFORC_Ecs AFORC_Ecs;
typedef struct AFORC_EcsView AFORC_EcsView;

typedef struct AFORC_Entity
{
    uint32_t index;
    uint32_t generation;
} AFORC_Entity;

typedef struct AFORC_ComponentType
{
    uint32_t id;
} AFORC_ComponentType;

/*
 * Cleanup runs immediately before a component is removed. The component and
 * entity remain queryable for the duration of the callback. The callback
 * may inspect the ECS, but must not mutate or destroy it; mutating calls
 * return an invalid-state status while cleanup is active.
 */
typedef void (*AFORC_EcsComponentCleanup)(AFORC_Ecs *ecs,
                                          AFORC_Entity entity,
                                          void *component,
                                          void *user_data);

typedef struct AFORC_EcsComponentDesc
{
    size_t size;
    size_t alignment;
    size_t initial_capacity;
    AFORC_EcsComponentCleanup cleanup;
    void *cleanup_user_data;
} AFORC_EcsComponentDesc;

typedef struct AFORC_EcsConfig
{
    AFORC_Allocator allocator;
    size_t initial_entity_capacity;
    size_t max_entities;
    size_t initial_component_capacity;
    size_t initial_component_type_capacity;
    size_t max_component_types;
} AFORC_EcsConfig;

/*
 * Zero max_entities or max_component_types selects the handle-format limit.
 * Initial capacities are reservations, not live object counts. A component
 * descriptor with zero alignment uses _Alignof(max_align_t), and zero
 * initial capacity uses the ECS-wide component reservation. The allocator
 * and its context must remain valid until the ECS and all derived views are
 * destroyed.
 */
AFORC_API AFORC_EcsConfig aforc_ecs_config_default(void);

/*
 * The returned ECS is heap-owned by the caller. Destroy invokes every
 * registered cleanup callback, releases all ECS-owned byte storage, and
 * accepts NULL. Resources referenced by component bytes remain caller-owned
 * unless their cleanup callback releases them.
 */
AFORC_API AFORC_Status aforc_ecs_create(const AFORC_EcsConfig *config,
                                        AFORC_Ecs **out_ecs);
AFORC_API void aforc_ecs_destroy(AFORC_Ecs *ecs);

AFORC_API AFORC_Status aforc_ecs_clear(AFORC_Ecs *ecs);

AFORC_API size_t aforc_ecs_entity_count(const AFORC_Ecs *ecs);
AFORC_API size_t aforc_ecs_entity_capacity(const AFORC_Ecs *ecs);
AFORC_API bool aforc_ecs_entity_alive(const AFORC_Ecs *ecs,
                                      AFORC_Entity entity);
AFORC_API bool aforc_entity_equal(AFORC_Entity left, AFORC_Entity right);

/* Dead, malformed, and generation-mismatched handles return STALE_HANDLE.
 */
AFORC_API AFORC_Status aforc_ecs_reserve_entities(AFORC_Ecs *ecs,
                                                  size_t capacity);
AFORC_API AFORC_Status aforc_ecs_create_entity(AFORC_Ecs *ecs,
                                               AFORC_Entity *out_entity);
AFORC_API AFORC_Status aforc_ecs_destroy_entity(AFORC_Ecs *ecs,
                                                AFORC_Entity entity);

/*
 * Registration copies the descriptor. Component type handles remain valid
 * until the ECS is destroyed; types are never recycled or unregistered.
 * alignment must divide _Alignof(max_align_t), so all returned component
 * addresses satisfy it on a conforming C17 allocator.
 */
AFORC_API AFORC_Status
aforc_ecs_register_component(AFORC_Ecs *ecs,
                             const AFORC_EcsComponentDesc *desc,
                             AFORC_ComponentType *out_type);
AFORC_API size_t aforc_ecs_registered_component_count(const AFORC_Ecs *ecs);
AFORC_API AFORC_Status aforc_ecs_component_size(const AFORC_Ecs *ecs,
                                                AFORC_ComponentType type,
                                                size_t *out_size);
AFORC_API AFORC_Status aforc_ecs_component_instance_count(
    const AFORC_Ecs *ecs, AFORC_ComponentType type, size_t *out_count);
AFORC_API AFORC_Status aforc_ecs_reserve_component(AFORC_Ecs *ecs,
                                                   AFORC_ComponentType type,
                                                   size_t capacity);

/*
 * initial_value supplies exactly the registered component size. NULL
 * requests zero-initialization. out_component is optional. Returned
 * pointers are ECS-owned and may be invalidated by any structural ECS
 * mutation; never free or retain them across such a mutation.
 */
AFORC_API AFORC_Status aforc_ecs_add(AFORC_Ecs *ecs,
                                     AFORC_Entity entity,
                                     AFORC_ComponentType type,
                                     const void *initial_value,
                                     void **out_component);
AFORC_API AFORC_Status aforc_ecs_get(AFORC_Ecs *ecs,
                                     AFORC_Entity entity,
                                     AFORC_ComponentType type,
                                     void **out_component);
AFORC_API AFORC_Status aforc_ecs_get_const(const AFORC_Ecs *ecs,
                                           AFORC_Entity entity,
                                           AFORC_ComponentType type,
                                           const void **out_component);
AFORC_API AFORC_Status aforc_ecs_has(const AFORC_Ecs *ecs,
                                     AFORC_Entity entity,
                                     AFORC_ComponentType type,
                                     bool *out_has);
AFORC_API AFORC_Status aforc_ecs_remove(AFORC_Ecs *ecs,
                                        AFORC_Entity entity,
                                        AFORC_ComponentType type);

/*
 * A view owns a copy of its required type list but does not retain the ECS.
 * Destroy it before its ECS. A zero-type view visits every live entity.
 * Structural mutation invalidates the current iteration, after which next
 * returns an invalid-state status until reset captures the new revision.
 * Reset also restarts exhausted views and reselects the smallest component
 * store. out_components, when non-NULL, must provide one slot per required
 * type in the same order supplied at creation.
 */
AFORC_API AFORC_Status
aforc_ecs_view_create(AFORC_Ecs *ecs,
                      const AFORC_ComponentType *required_types,
                      size_t required_type_count,
                      AFORC_EcsView **out_view);
AFORC_API void aforc_ecs_view_destroy(AFORC_EcsView *view);
AFORC_API size_t aforc_ecs_view_component_count(const AFORC_EcsView *view);
AFORC_API AFORC_Status aforc_ecs_view_reset(AFORC_EcsView *view);
AFORC_API AFORC_Status aforc_ecs_view_next(AFORC_EcsView *view,
                                           AFORC_Entity *out_entity,
                                           void **out_components,
                                           bool *out_has_value);

#ifdef __cplusplus
}
#endif

#endif
