/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/engine.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct ReentryContext {
    bool attempted;
    AFORC_Status nested_frame_status;
    AFORC_Status nested_run_status;
} ReentryContext;

typedef struct SleepContext {
    uint64_t now_ns;
    size_t sleep_count;
} SleepContext;

typedef struct AllocationContext {
    size_t live_allocations;
    bool destroy_attempted;
} AllocationContext;

static void *tracked_allocate(void *context, size_t size)
{
    AllocationContext *allocations = context;
    void *memory = malloc(size);

    if (memory != NULL) {
        ++allocations->live_allocations;
    }
    return memory;
}

static void *tracked_reallocate(void *context, void *memory, size_t size)
{
    (void)context;
    return realloc(memory, size);
}

static void tracked_deallocate(void *context, void *memory)
{
    AllocationContext *allocations = context;

    if (memory != NULL) {
        --allocations->live_allocations;
        free(memory);
    }
}

static AFORC_Status quit_in_poll(void *context, AFORC_Engine *engine,
                                 AFORC_Error *error)
{
    (void)context;
    (void)error;
    aforc_engine_request_quit(engine);
    return AFORC_OK;
}

static AFORC_Status attempt_nested_ownership(void *context,
                                             AFORC_Engine *engine,
                                             AFORC_Error *error)
{
    ReentryContext *reentry = context;

    if (!reentry->attempted) {
        reentry->attempted = true;
        reentry->nested_frame_status = aforc_engine_frame(engine, 1U, error);
        reentry->nested_run_status = aforc_engine_run(engine, error);
    }
    return AFORC_OK;
}

static uint64_t fake_now(void *context)
{
    return ((SleepContext *)context)->now_ns;
}

static void count_sleep(void *context, uint64_t nanoseconds)
{
    SleepContext *sleep = context;

    (void)nanoseconds;
    ++sleep->sleep_count;
}

static AFORC_Status destroy_from_hook(void *context, AFORC_Engine *engine,
                                      AFORC_Error *error)
{
    AllocationContext *allocations = context;

    (void)error;
    allocations->destroy_attempted = true;
    aforc_engine_destroy(engine);
    return AFORC_ERROR_STATE;
}

static AFORC_Status quit_from_fixed_update(AFORC_Scene *scene,
                                           AFORC_Engine *engine,
                                           double seconds,
                                           AFORC_Error *error)
{
    size_t *updates = scene->user_data;

    (void)seconds;
    (void)error;
    ++*updates;
    aforc_engine_request_quit(engine);
    return AFORC_OK;
}

static bool test_nested_loop_ownership_is_rejected(void)
{
    ReentryContext context = {false, AFORC_OK, AFORC_OK};
    AFORC_EngineConfig config = aforc_engine_config_default();
    AFORC_Engine *engine = NULL;
    AFORC_Error error;
    AFORC_Status status;
    bool passed;

    config.quit_when_scene_stack_empty = false;
    config.target_frames_per_second = 0U;
    config.hooks.context = &context;
    config.hooks.poll_events = quit_in_poll;
    config.hooks.begin_frame = attempt_nested_ownership;
    status = aforc_engine_create(&config, &engine, &error);
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    passed = status == AFORC_OK && context.attempted &&
             context.nested_frame_status == AFORC_ERROR_STATE &&
             context.nested_run_status == AFORC_ERROR_STATE &&
             aforc_engine_state(engine) == AFORC_ENGINE_CREATED;
    aforc_engine_destroy(engine);
    return passed;
}

static bool test_quit_skips_frame_sleep(void)
{
    SleepContext context = {0U, 0U};
    AFORC_EngineConfig config = aforc_engine_config_default();
    AFORC_Engine *engine = NULL;
    AFORC_Error error;
    AFORC_Status status;

    config.quit_when_scene_stack_empty = false;
    config.hooks.context = &context;
    config.hooks.now = fake_now;
    config.hooks.sleep = count_sleep;
    config.hooks.poll_events = quit_in_poll;
    status = aforc_engine_create(&config, &engine, &error);
    if (status == AFORC_OK) {
        status = aforc_engine_run(engine, &error);
    }
    aforc_engine_destroy(engine);
    return status == AFORC_OK && context.sleep_count == 0U;
}

static bool test_destroy_is_deferred_during_frame(void)
{
    AllocationContext context = {0U, false};
    AFORC_EngineConfig config = aforc_engine_config_default();
    AFORC_Engine *engine = NULL;
    AFORC_Error error;
    AFORC_Status status;
    bool passed;

    config.quit_when_scene_stack_empty = false;
    config.allocator = (AFORC_Allocator){
        &context,
        tracked_allocate,
        tracked_reallocate,
        tracked_deallocate,
    };
    config.hooks.context = &context;
    config.hooks.begin_frame = destroy_from_hook;
    status = aforc_engine_create(&config, &engine, &error);
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    passed = status == AFORC_ERROR_STATE && context.destroy_attempted &&
             context.live_allocations != 0U;
    if (context.live_allocations != 0U) {
        aforc_engine_destroy(engine);
    }
    return passed && context.live_allocations == 0U;
}

static bool test_successful_fixed_update_advances_tick_before_quit(void)
{
    static const AFORC_SceneVTable vtable = {
        .fixed_update = quit_from_fixed_update,
    };
    size_t updates = 0U;
    AFORC_Scene scene = {&vtable, &updates, 0U};
    AFORC_EngineConfig config = aforc_engine_config_default();
    AFORC_Engine *engine = NULL;
    AFORC_Error error;
    AFORC_Status status;
    bool passed;

    config.fixed_updates_per_second = 10U;
    config.quit_when_scene_stack_empty = false;
    status = aforc_engine_create(&config, &engine, &error);
    if (status == AFORC_OK) {
        status = aforc_engine_request_push(engine, &scene, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, 0U, &error);
    }
    if (status == AFORC_OK) {
        status = aforc_engine_frame(engine, UINT64_C(100000000), &error);
    }
    passed = status == AFORC_OK && updates == 1U &&
             aforc_engine_fixed_tick(engine) == 1U;
    aforc_engine_destroy(engine);
    return passed;
}

int main(void)
{
    if (!test_nested_loop_ownership_is_rejected()) {
        (void)fprintf(stderr, "nested engine ownership guard failed\n");
        return 1;
    }
    if (!test_quit_skips_frame_sleep()) {
        (void)fprintf(stderr, "quit frame pacing guard failed\n");
        return 2;
    }
    if (!test_destroy_is_deferred_during_frame()) {
        (void)fprintf(stderr, "active destroy guard failed\n");
        return 3;
    }
    if (!test_successful_fixed_update_advances_tick_before_quit()) {
        (void)fprintf(stderr, "fixed tick accounting failed\n");
        return 4;
    }
    (void)puts("core lifecycle: ok");
    return 0;
}
