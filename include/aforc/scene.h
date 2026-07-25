/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_SCENE_H
#define AFORC_SCENE_H

#include "aforc/common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AFORC_Engine AFORC_Engine;
typedef struct AFORC_Scene AFORC_Scene;

enum {
    AFORC_SCENE_RENDER_BELOW = 1u << 0,
    AFORC_SCENE_UPDATE_BELOW = 1u << 1,
    AFORC_SCENE_EVENTS_BELOW = 1u << 2
};

typedef AFORC_Status (*AFORC_SceneEnterFn)(AFORC_Scene *scene, AFORC_Engine *engine,
                                      AFORC_Error *error);
typedef void (*AFORC_SceneStateFn)(AFORC_Scene *scene, AFORC_Engine *engine);
typedef AFORC_Status (*AFORC_SceneUpdateFn)(AFORC_Scene *scene, AFORC_Engine *engine,
                                       double seconds, AFORC_Error *error);
typedef AFORC_Status (*AFORC_SceneRenderFn)(AFORC_Scene *scene, AFORC_Engine *engine,
                                       double interpolation, AFORC_Error *error);
typedef AFORC_Status (*AFORC_SceneEventFn)(AFORC_Scene *scene, AFORC_Engine *engine,
                                      const void *event, bool *consumed,
                                      AFORC_Error *error);

typedef struct AFORC_SceneVTable {
    AFORC_SceneEnterFn enter;
    AFORC_SceneStateFn leave;
    AFORC_SceneStateFn pause;
    AFORC_SceneStateFn resume;
    AFORC_SceneUpdateFn fixed_update;
    AFORC_SceneUpdateFn update;
    AFORC_SceneRenderFn render;
    AFORC_SceneEventFn event;
} AFORC_SceneVTable;

struct AFORC_Scene {
    const AFORC_SceneVTable *vtable;
    void *user_data;
    uint32_t flags;
};

/*
 * Scene callbacks are invoked on the engine thread. `enter` may reject a
 * transition; a failed push/replace restores the previously active scene.
 * `leave` is the last lifecycle callback but does not free the borrowed scene.
 * Direct stack mutation is rejected while callbacks are being dispatched;
 * callback code should use aforc_engine_request_* instead.
 *
 * A top scene opts into dispatch below it with the corresponding *_BELOW flag.
 * Update/render callbacks then execute from the lowest included scene upward;
 * events travel top-down until consumed or propagation is disabled.
 * Enter runs before a push/replace is committed, so stack_top still returns the
 * previous scene from that callback. Leave runs while the removed scene is
 * still top. Scene objects, vtables, and user_data remain caller-owned.
 */

typedef struct AFORC_SceneStack {
    AFORC_Scene **items;
    size_t count;
    size_t capacity;
    AFORC_Allocator allocator;
    bool dispatching;
} AFORC_SceneStack;

/* AFORC_SceneStack storage is public for caller allocation, but its fields are
 * read-only to consumers after init. Mutating or disposing a stack from one of
 * its callbacks is rejected or ignored until dispatch returns. */

AFORC_API AFORC_Status aforc_scene_stack_init(AFORC_SceneStack *stack,
                                        size_t capacity,
                                        AFORC_Allocator allocator,
                                        AFORC_Error *error);
AFORC_API void aforc_scene_stack_dispose(AFORC_SceneStack *stack,
                                     AFORC_Engine *engine);
AFORC_API AFORC_Status aforc_scene_stack_push(AFORC_SceneStack *stack,
                                        AFORC_Engine *engine, AFORC_Scene *scene,
                                        AFORC_Error *error);
AFORC_API AFORC_Status aforc_scene_stack_pop(AFORC_SceneStack *stack,
                                       AFORC_Engine *engine,
                                       AFORC_Scene **out_scene,
                                       AFORC_Error *error);
AFORC_API AFORC_Status aforc_scene_stack_replace(AFORC_SceneStack *stack,
                                           AFORC_Engine *engine,
                                           AFORC_Scene *scene,
                                           AFORC_Error *error);
AFORC_API AFORC_Scene *aforc_scene_stack_top(const AFORC_SceneStack *stack);
AFORC_API size_t aforc_scene_stack_count(const AFORC_SceneStack *stack);
AFORC_API AFORC_Status aforc_scene_stack_fixed_update(AFORC_SceneStack *stack,
                                                AFORC_Engine *engine,
                                                double seconds,
                                                AFORC_Error *error);
AFORC_API AFORC_Status aforc_scene_stack_update(AFORC_SceneStack *stack,
                                          AFORC_Engine *engine, double seconds,
                                          AFORC_Error *error);
AFORC_API AFORC_Status aforc_scene_stack_render(AFORC_SceneStack *stack,
                                          AFORC_Engine *engine,
                                          double interpolation,
                                          AFORC_Error *error);
AFORC_API AFORC_Status aforc_scene_stack_event(AFORC_SceneStack *stack,
                                         AFORC_Engine *engine,
                                         const void *event, bool *consumed,
                                         AFORC_Error *error);

#ifdef __cplusplus
}
#endif

#endif
