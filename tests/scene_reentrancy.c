/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/scene.h"

#include <stdio.h>
#include <string.h>

typedef struct MutationAttempt {
    AFORC_SceneStack *stack;
    AFORC_Status status;
    bool attempted;
} MutationAttempt;
static AFORC_Status attempt_pop_on_enter(AFORC_Scene *scene,
                                         AFORC_Engine *engine,
                                         AFORC_Error *error)
{
    MutationAttempt *attempt = (MutationAttempt *)scene->user_data;
    attempt->attempted = true;
    attempt->status = aforc_scene_stack_pop(attempt->stack,
                                            engine,
                                            NULL,
                                            error);
    return AFORC_OK;
}
static AFORC_Status reject_enter(AFORC_Scene *scene,
                                 AFORC_Engine *engine,
                                 AFORC_Error *error)
{
    (void)scene;
    (void)engine;
    (void)error;
    return AFORC_ERROR_STATE;
}
static void attempt_pop_on_state(AFORC_Scene *scene, AFORC_Engine *engine)
{
    MutationAttempt *attempt = (MutationAttempt *)scene->user_data;
    AFORC_Error error;
    if (attempt->attempted) {
        return;
    }
    attempt->attempted = true;
    aforc_error_clear(&error);
    attempt->status = aforc_scene_stack_pop(attempt->stack,
                                            engine,
                                            NULL,
                                            &error);
}
static bool initialize_stack(AFORC_SceneStack *stack, AFORC_Error *error)
{
    (void)memset(stack, 0, sizeof(*stack));
    aforc_error_clear(error);
    return aforc_scene_stack_init(stack,
                                  4u,
                                  aforc_allocator_default(),
                                  error) == AFORC_OK;
}
static bool test_replace_rejects_pop_from_enter(void)
{
    static const AFORC_SceneVTable base_vtable = {0};
    static const AFORC_SceneVTable replacement_vtable = {
        .enter = attempt_pop_on_enter,
    };
    AFORC_SceneStack stack;
    AFORC_Error error;
    AFORC_Scene base = {.vtable = &base_vtable};
    MutationAttempt attempt = {0};
    AFORC_Scene replacement = {
        .vtable = &replacement_vtable,
        .user_data = &attempt,
    };
    AFORC_Status status;
    bool passed;

    if (!initialize_stack(&stack, &error)) {
        return false;
    }
    attempt.stack = &stack;
    status = aforc_scene_stack_push(&stack, NULL, &base, &error);
    if (status == AFORC_OK) {
        status = aforc_scene_stack_replace(&stack,
                                           NULL,
                                           &replacement,
                                           &error);
    }
    passed = status == AFORC_OK && attempt.attempted &&
             attempt.status == AFORC_ERROR_STATE && stack.count == 1u &&
             aforc_scene_stack_top(&stack) == &replacement;
    aforc_scene_stack_dispose(&stack, NULL);
    return passed;
}

static bool test_push_rejects_pop_from_pause(void)
{
    static const AFORC_SceneVTable base_vtable = {
        .pause = attempt_pop_on_state,
    };
    static const AFORC_SceneVTable overlay_vtable = {0};
    AFORC_SceneStack stack;
    AFORC_Error error;
    MutationAttempt attempt = {0};
    AFORC_Scene base = {.vtable = &base_vtable, .user_data = &attempt};
    AFORC_Scene overlay = {.vtable = &overlay_vtable};
    AFORC_Status status;
    bool passed;

    if (!initialize_stack(&stack, &error)) {
        return false;
    }
    attempt.stack = &stack;
    status = aforc_scene_stack_push(&stack, NULL, &base, &error);
    if (status == AFORC_OK) {
        status = aforc_scene_stack_push(&stack, NULL, &overlay, &error);
    }
    passed = status == AFORC_OK && attempt.attempted &&
             attempt.status == AFORC_ERROR_STATE && stack.count == 2u &&
             aforc_scene_stack_top(&stack) == &overlay;
    aforc_scene_stack_dispose(&stack, NULL);
    return passed;
}

static bool test_pop_rejects_mutation_from_leave_and_resume(void)
{
    static const AFORC_SceneVTable base_vtable = {
        .resume = attempt_pop_on_state,
    };
    static const AFORC_SceneVTable overlay_vtable = {
        .leave = attempt_pop_on_state,
    };
    AFORC_SceneStack stack;
    AFORC_Error error;
    MutationAttempt resume_attempt = {0};
    MutationAttempt leave_attempt = {0};
    AFORC_Scene base = {
        .vtable = &base_vtable,
        .user_data = &resume_attempt,
    };
    AFORC_Scene overlay = {
        .vtable = &overlay_vtable,
        .user_data = &leave_attempt,
    };
    AFORC_Status status;
    bool passed;

    if (!initialize_stack(&stack, &error)) {
        return false;
    }
    resume_attempt.stack = &stack;
    leave_attempt.stack = &stack;
    status = aforc_scene_stack_push(&stack, NULL, &base, &error);
    if (status == AFORC_OK) {
        status = aforc_scene_stack_push(&stack, NULL, &overlay, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_scene_stack_pop(&stack, NULL, NULL, &error);
    }
    passed = status == AFORC_OK && leave_attempt.attempted &&
             leave_attempt.status == AFORC_ERROR_STATE &&
             resume_attempt.attempted &&
             resume_attempt.status == AFORC_ERROR_STATE &&
             stack.count == 1u && aforc_scene_stack_top(&stack) == &base;
    aforc_scene_stack_dispose(&stack, NULL);
    return passed;
}

static bool test_failed_enter_rejects_pop_from_resume(void)
{
    static const AFORC_SceneVTable base_vtable = {
        .resume = attempt_pop_on_state,
    };
    static const AFORC_SceneVTable rejected_vtable = {
        .enter = reject_enter,
    };
    AFORC_SceneStack stack;
    AFORC_Error error;
    MutationAttempt attempt = {0};
    AFORC_Scene base = {.vtable = &base_vtable, .user_data = &attempt};
    AFORC_Scene rejected = {.vtable = &rejected_vtable};
    AFORC_Status status;
    bool passed;

    if (!initialize_stack(&stack, &error)) {
        return false;
    }
    attempt.stack = &stack;
    status = aforc_scene_stack_push(&stack, NULL, &base, &error);
    if (status == AFORC_OK) {
        status = aforc_scene_stack_push(&stack, NULL, &rejected, &error);
    }
    passed = status == AFORC_ERROR_STATE && attempt.attempted &&
             attempt.status == AFORC_ERROR_STATE && stack.count == 1u &&
             aforc_scene_stack_top(&stack) == &base;
    aforc_scene_stack_dispose(&stack, NULL);
    return passed;
}

static bool test_dispose_rejects_pop_from_leave(void)
{
    static const AFORC_SceneVTable scene_vtable = {
        .leave = attempt_pop_on_state,
    };
    AFORC_SceneStack stack;
    AFORC_Error error;
    MutationAttempt attempt = {0};
    AFORC_Scene scene = {.vtable = &scene_vtable, .user_data = &attempt};
    AFORC_Status status;

    if (!initialize_stack(&stack, &error)) {
        return false;
    }
    attempt.stack = &stack;
    status = aforc_scene_stack_push(&stack, NULL, &scene, &error);
    if (status != AFORC_OK) {
        aforc_scene_stack_dispose(&stack, NULL);
        return false;
    }
    aforc_scene_stack_dispose(&stack, NULL);
    return attempt.attempted && attempt.status == AFORC_ERROR_STATE &&
           stack.items == NULL && stack.count == 0u;
}

int main(void)
{
    if (!test_replace_rejects_pop_from_enter()) {
        (void)fprintf(stderr, "replace enter reentrancy guard failed\n");
        return 1;
    }
    if (!test_push_rejects_pop_from_pause()) {
        (void)fprintf(stderr, "push pause reentrancy guard failed\n");
        return 2;
    }
    if (!test_pop_rejects_mutation_from_leave_and_resume()) {
        (void)fprintf(stderr, "pop lifecycle reentrancy guard failed\n");
        return 3;
    }
    if (!test_failed_enter_rejects_pop_from_resume()) {
        (void)fprintf(stderr, "failed-enter resume guard failed\n");
        return 4;
    }
    if (!test_dispose_rejects_pop_from_leave()) {
        (void)fprintf(stderr, "dispose leave reentrancy guard failed\n");
        return 5;
    }
    (void)puts("scene reentrancy: ok");
    return 0;
}
