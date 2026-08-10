/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/scene.h"

#include <string.h>

/* Owns scene lifecycle ordering and guarded stack traversal. dispatching is a
 * mutation barrier; engine commands defer changes until the walk completes. */

static AFORC_Status
scene_error(AFORC_Error *error, AFORC_Status status, const char *message)
{
    aforc_error_set(error, status, "scene", "%s", message);
    return status;
}

AFORC_Status aforc_scene_stack_init(AFORC_SceneStack *stack,
                                    size_t capacity,
                                    AFORC_Allocator allocator,
                                    AFORC_Error *error)
{
    AFORC_Status status;
    if (stack == NULL || capacity == 0U || allocator.allocate == NULL ||
        allocator.reallocate == NULL || allocator.deallocate == NULL)
    {
        return scene_error(error,
                           AFORC_ERROR_INVALID_ARGUMENT,
                           "invalid scene stack configuration");
    }
    memset(stack, 0, sizeof(*stack));
    stack->allocator = allocator;
    status = aforc_alloc_array(
        &allocator, capacity, sizeof(*stack->items), (void **)&stack->items);
    if (status != AFORC_OK)
    {
        return scene_error(error, status, "could not allocate scene stack");
    }
    stack->capacity = capacity;
    return AFORC_OK;
}

void aforc_scene_stack_dispose(AFORC_SceneStack *stack, AFORC_Engine *engine)
{
    if (stack == NULL || stack->items == NULL || stack->dispatching)
    {
        return;
    }
    stack->dispatching = true;
    while (stack->count != 0U)
    {
        AFORC_Scene *scene = stack->items[stack->count - 1U];
        if (scene != NULL && scene->vtable != NULL &&
            scene->vtable->leave != NULL)
        {
            scene->vtable->leave(scene, engine);
        }
        stack->count--;
    }
    aforc_free(&stack->allocator, stack->items);
    memset(stack, 0, sizeof(*stack));
}

static bool stack_can_mutate(const AFORC_SceneStack *stack)
{
    return stack != NULL && stack->items != NULL && !stack->dispatching;
}

static AFORC_Status enter_scene(AFORC_Scene *previous,
                                AFORC_Scene *next,
                                AFORC_Engine *engine,
                                AFORC_Error *error)
{
    AFORC_Status status = AFORC_OK;
    if (previous != NULL && previous->vtable->pause != NULL)
    {
        previous->vtable->pause(previous, engine);
    }
    if (next->vtable->enter != NULL)
    {
        status = next->vtable->enter(next, engine, error);
    }
    /* A rejected transition must restore the lifecycle state of the scene
     * that remains active on the unchanged stack. */
    if (status != AFORC_OK && previous != NULL &&
        previous->vtable->resume != NULL)
    {
        previous->vtable->resume(previous, engine);
    }
    return status;
}

AFORC_Status aforc_scene_stack_push(AFORC_SceneStack *stack,
                                    AFORC_Engine *engine,
                                    AFORC_Scene *scene,
                                    AFORC_Error *error)
{
    AFORC_Scene *previous;
    AFORC_Status status;
    if (!stack_can_mutate(stack) || scene == NULL || scene->vtable == NULL)
    {
        return scene_error(
            error,
            AFORC_ERROR_STATE,
            "scene push attempted during dispatch or with invalid state");
    }
    if (stack->count == stack->capacity)
    {
        return scene_error(error, AFORC_ERROR_LIMIT, "scene stack is full");
    }
    previous = aforc_scene_stack_top(stack);
    stack->dispatching = true;
    status = enter_scene(previous, scene, engine, error);
    stack->dispatching = false;
    if (status != AFORC_OK)
    {
        return status;
    }
    stack->items[stack->count++] = scene;
    return AFORC_OK;
}

AFORC_Status aforc_scene_stack_pop(AFORC_SceneStack *stack,
                                   AFORC_Engine *engine,
                                   AFORC_Scene **out_scene,
                                   AFORC_Error *error)
{
    AFORC_Scene *removed;
    if (!stack_can_mutate(stack))
    {
        return scene_error(
            error,
            AFORC_ERROR_STATE,
            "scene pop attempted during dispatch or with invalid state");
    }
    if (stack->count == 0U)
    {
        return scene_error(
            error, AFORC_ERROR_NOT_FOUND, "scene stack is empty");
    }
    removed = stack->items[stack->count - 1U];
    stack->dispatching = true;
    if (removed->vtable->leave != NULL)
    {
        removed->vtable->leave(removed, engine);
    }
    stack->items[--stack->count] = NULL;
    if (stack->count != 0U)
    {
        AFORC_Scene *resumed = stack->items[stack->count - 1U];
        if (resumed->vtable->resume != NULL)
        {
            resumed->vtable->resume(resumed, engine);
        }
    }
    stack->dispatching = false;
    if (out_scene != NULL)
    {
        *out_scene = removed;
    }
    return AFORC_OK;
}

AFORC_Status aforc_scene_stack_replace(AFORC_SceneStack *stack,
                                       AFORC_Engine *engine,
                                       AFORC_Scene *scene,
                                       AFORC_Error *error)
{
    AFORC_Scene *previous;
    AFORC_Status status;
    if (!stack_can_mutate(stack) || scene == NULL || scene->vtable == NULL)
    {
        return scene_error(
            error, AFORC_ERROR_STATE, "scene replacement has invalid state");
    }
    if (stack->count == 0U)
    {
        return aforc_scene_stack_push(stack, engine, scene, error);
    }
    previous = stack->items[stack->count - 1U];
    stack->dispatching = true;
    status = enter_scene(previous, scene, engine, error);
    if (status != AFORC_OK)
    {
        stack->dispatching = false;
        return status;
    }
    if (previous->vtable->leave != NULL)
    {
        previous->vtable->leave(previous, engine);
    }
    stack->items[stack->count - 1U] = scene;
    stack->dispatching = false;
    return AFORC_OK;
}

AFORC_Scene *aforc_scene_stack_top(const AFORC_SceneStack *stack)
{
    return stack == NULL || stack->count == 0U
               ? NULL
               : stack->items[stack->count - 1U];
}

size_t aforc_scene_stack_count(const AFORC_SceneStack *stack)
{
    return stack == NULL ? 0U : stack->count;
}

typedef enum DispatchKind
{
    DISPATCH_FIXED,
    DISPATCH_UPDATE,
    DISPATCH_RENDER
} DispatchKind;

static AFORC_Status dispatch_frames(AFORC_SceneStack *stack,
                                    AFORC_Engine *engine,
                                    double value,
                                    DispatchKind kind,
                                    AFORC_Error *error)
{
    size_t first;
    AFORC_Status status = AFORC_OK;
    if (stack == NULL || stack->items == NULL || stack->dispatching)
    {
        return scene_error(
            error, AFORC_ERROR_STATE, "invalid scene dispatch state");
    }
    if (stack->count == 0U)
    {
        return AFORC_OK;
    }
    first = stack->count - 1U;
    while (first > 0U)
    {
        /* Fixed and variable updates share one propagation boundary so a
         * scene cannot expose only one half of its simulation below itself. */
        const uint32_t flag = kind == DISPATCH_RENDER
                                  ? AFORC_SCENE_RENDER_BELOW
                                  : AFORC_SCENE_UPDATE_BELOW;
        if ((stack->items[first]->flags & flag) == 0U)
        {
            break;
        }
        --first;
    }
    stack->dispatching = true;
    for (size_t index = first; index < stack->count; ++index)
    {
        AFORC_Scene *scene = stack->items[index];
        if (kind == DISPATCH_FIXED && scene->vtable->fixed_update != NULL)
        {
            status = scene->vtable->fixed_update(scene, engine, value, error);
        }
        else if (kind == DISPATCH_UPDATE && scene->vtable->update != NULL)
        {
            status = scene->vtable->update(scene, engine, value, error);
        }
        else if (kind == DISPATCH_RENDER && scene->vtable->render != NULL)
        {
            status = scene->vtable->render(scene, engine, value, error);
        }
        if (status != AFORC_OK)
        {
            break;
        }
    }
    stack->dispatching = false;
    return status;
}

AFORC_Status aforc_scene_stack_fixed_update(AFORC_SceneStack *stack,
                                            AFORC_Engine *engine,
                                            double seconds,
                                            AFORC_Error *error)
{
    return dispatch_frames(stack, engine, seconds, DISPATCH_FIXED, error);
}

AFORC_Status aforc_scene_stack_update(AFORC_SceneStack *stack,
                                      AFORC_Engine *engine,
                                      double seconds,
                                      AFORC_Error *error)
{
    return dispatch_frames(stack, engine, seconds, DISPATCH_UPDATE, error);
}

AFORC_Status aforc_scene_stack_render(AFORC_SceneStack *stack,
                                      AFORC_Engine *engine,
                                      double interpolation,
                                      AFORC_Error *error)
{
    return dispatch_frames(
        stack, engine, interpolation, DISPATCH_RENDER, error);
}

AFORC_Status aforc_scene_stack_event(AFORC_SceneStack *stack,
                                     AFORC_Engine *engine,
                                     const void *event,
                                     bool *consumed,
                                     AFORC_Error *error)
{
    AFORC_Status status = AFORC_OK;
    bool handled = false;
    if (stack == NULL || stack->items == NULL || stack->dispatching ||
        event == NULL)
    {
        return scene_error(
            error, AFORC_ERROR_STATE, "invalid event dispatch state");
    }
    stack->dispatching = true;
    for (size_t index = stack->count; index > 0U; --index)
    {
        AFORC_Scene *scene = stack->items[index - 1U];
        if (scene->vtable->event != NULL)
        {
            status =
                scene->vtable->event(scene, engine, event, &handled, error);
            if (status != AFORC_OK || handled)
            {
                break;
            }
        }
        if ((scene->flags & AFORC_SCENE_EVENTS_BELOW) == 0U)
        {
            break;
        }
    }
    stack->dispatching = false;
    if (consumed != NULL)
    {
        *consumed = handled;
    }
    return status;
}
