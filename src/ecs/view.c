/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "ecs_internal.h"

#include <stddef.h>
#include <string.h>

/*
 * Revision-bound ECS query planning and iteration.
 *
 * A view copies its requested type list, snapshots the registry revision, and
 * drives iteration from the smallest dense store. Any structural mutation
 * invalidates the view before it can expose stale component pointers.
 */

AFORC_Status aforc_ecs_view_create(AFORC_Ecs *ecs,
                               const AFORC_ComponentType *required_types,
                               size_t required_type_count,
                               AFORC_EcsView **out_view) {
    AFORC_EcsView *view = NULL;
    size_t type_index = 0U;
    size_t prior_index = 0U;
    size_t smallest_count = SIZE_MAX;
    AFORC_Status status = AFORC_OK;

    if (out_view == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_view = NULL;
    if (ecs == NULL ||
        (required_type_count != 0U && required_types == NULL)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    for (type_index = 0U; type_index < required_type_count; ++type_index) {
        if ((size_t)required_types[type_index].id >= ecs->store_count) {
            return AFORC_ERROR_NOT_FOUND;
        }
        for (prior_index = 0U; prior_index < type_index; ++prior_index) {
            if (required_types[prior_index].id ==
                required_types[type_index].id) {
                return AFORC_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    status = aforc_ecs_allocate_array(&ecs->allocator, 1U, sizeof(*view), true,
                                    (void **)&view);
    if (status != AFORC_OK) {
        return status;
    }
    view->allocator = ecs->allocator;
    view->ecs = ecs;
    view->driver_type_id = AFORC_COMPONENT_TYPE_INVALID_ID;
    view->revision = ecs->revision;
    if (required_type_count != 0U) {
        status = aforc_ecs_allocate_array(&ecs->allocator, required_type_count,
                                        sizeof(*view->required_types), false,
                                        (void **)&view->required_types);
        if (status != AFORC_OK) {
            aforc_ecs_deallocate(&view->allocator, view);
            return status;
        }
        (void)memcpy(view->required_types, required_types,
                     required_type_count * sizeof(*required_types));
        view->required_type_count = required_type_count;
        for (type_index = 0U; type_index < required_type_count; ++type_index) {
            const AFORC_EcsComponentStore *store =
                &ecs->stores[required_types[type_index].id];
            /* The smallest dense set bounds query work; ties keep caller order. */
            if (store->count < smallest_count) {
                smallest_count = store->count;
                view->driver_type_id = required_types[type_index].id;
            }
        }
    }
    *out_view = view;
    return AFORC_OK;
}

void aforc_ecs_view_destroy(AFORC_EcsView *view) {
    AFORC_Allocator allocator;

    if (view == NULL) {
        return;
    }
    allocator = view->allocator;
    aforc_ecs_deallocate(&allocator, view->required_types);
    aforc_ecs_deallocate(&allocator, view);
}

size_t aforc_ecs_view_component_count(const AFORC_EcsView *view) {
    return view == NULL ? 0U : view->required_type_count;
}

AFORC_Status aforc_ecs_view_next(AFORC_EcsView *view,
                             AFORC_Entity *out_entity,
                             void **out_components,
                             bool *out_has_value) {
    AFORC_Ecs *ecs = NULL;
    size_t type_index = 0U;

    if (out_entity == NULL || out_has_value == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = AFORC_ENTITY_INVALID;
    *out_has_value = false;
    if (view == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    for (type_index = 0U; out_components != NULL &&
                          type_index < view->required_type_count;
         ++type_index) {
        out_components[type_index] = NULL;
    }
    ecs = view->ecs;
    if (ecs->cleanup_active || view->revision != ecs->revision) {
        return AFORC_ERROR_STATE;
    }
    if (view->driver_type_id == AFORC_COMPONENT_TYPE_INVALID_ID) {
        while (view->cursor < ecs->slot_count) {
            const size_t slot_index = view->cursor;
            const AFORC_EcsEntitySlot *slot = &ecs->slots[slot_index];
            ++view->cursor;
            if (slot->alive) {
                out_entity->index = (uint32_t)slot_index;
                out_entity->generation = slot->generation;
                *out_has_value = true;
                return AFORC_OK;
            }
        }
        return AFORC_OK;
    }
    {
        const AFORC_EcsComponentStore *driver =
            &ecs->stores[view->driver_type_id];
        while (view->cursor < driver->count) {
            const size_t driver_dense_index = view->cursor;
            const AFORC_Entity candidate =
                driver->dense_entities[driver_dense_index];
            bool matches = true;
            ++view->cursor;
            for (type_index = 0U; type_index < view->required_type_count;
                 ++type_index) {
                AFORC_EcsComponentStore *store =
                    &ecs->stores[view->required_types[type_index].id];
                const size_t dense_index =
                    view->required_types[type_index].id ==
                            view->driver_type_id
                        ? driver_dense_index
                        : aforc_ecs_find_component(store, candidate);

                if (dense_index == SIZE_MAX) {
                    size_t clear_index;

                    matches = false;
                    for (clear_index = 0U;
                         out_components != NULL && clear_index < type_index;
                         ++clear_index) {
                        out_components[clear_index] = NULL;
                    }
                    break;
                }
                if (out_components != NULL) {
                    out_components[type_index] =
                        aforc_ecs_component_at(store, dense_index);
                }
            }
            if (!matches) {
                continue;
            }
            *out_entity = candidate;
            *out_has_value = true;
            return AFORC_OK;
        }
    }
    return AFORC_OK;
}
