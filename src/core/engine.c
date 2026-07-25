/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "engine_internal.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Owns frame timing, sequencing, and run-loop state. Scene mutation is kept in
 * engine_commands.c so callbacks cannot invalidate an active stack walk. */

static uint64_t timespec_to_ns(const struct timespec *value) {
    uintmax_t wide_seconds;
    uint64_t seconds;
    uint64_t nanoseconds;

    if (value == NULL || value->tv_sec < 0 || value->tv_nsec < 0 ||
        value->tv_nsec >= 1000000000L) {
        return 0U;
    }
    wide_seconds = (uintmax_t)value->tv_sec;
    if (wide_seconds > UINT64_MAX) {
        return 0U;
    }
    seconds = (uint64_t)wide_seconds;
    nanoseconds = (uint64_t)value->tv_nsec;
    if (seconds > (UINT64_MAX - nanoseconds) / UINT64_C(1000000000)) {
        return 0U;
    }
    return seconds * UINT64_C(1000000000) + nanoseconds;
}

static uint64_t default_now(void *context) {
    struct timespec value = {0, 0};
    (void)context;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &value) == 0) {
        return timespec_to_ns(&value);
    }
#endif
    if (timespec_get(&value, TIME_UTC) != TIME_UTC || value.tv_sec < 0) {
        return 0U;
    }
    return timespec_to_ns(&value);
}

static void default_sleep(void *context, uint64_t nanoseconds) {
    struct timespec duration;
    (void)context;
    duration.tv_sec = (time_t)(nanoseconds / UINT64_C(1000000000));
    duration.tv_nsec = (long)(nanoseconds % UINT64_C(1000000000));
    while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {
    }
}

static void default_log(void *context, AFORC_LogLevel level,
                        const char *subsystem, const char *message) {
    static const char *const levels[] = {"trace", "debug", "info", "warning",
                                         "error"};
    const size_t index = (size_t)level;
    (void)context;
    (void)fprintf(stderr, "[aforc:%s:%s] %s\n",
                  index < sizeof(levels) / sizeof(levels[0]) ? levels[index]
                                                             : "unknown",
                  subsystem != NULL ? subsystem : "core",
                  message != NULL ? message : "");
}

AFORC_EngineConfig aforc_engine_config_default(void) {
    AFORC_EngineConfig config;
    memset(&config, 0, sizeof(config));
    config.fixed_updates_per_second = 60U;
    config.maximum_fixed_updates_per_frame = 8U;
    config.target_frames_per_second = 60U;
    config.maximum_frame_seconds = 0.25;
    config.scene_capacity = 16U;
    config.scene_command_capacity = 32U;
    config.quit_when_scene_stack_empty = true;
    config.allocator = aforc_allocator_default();
    config.logger.write = default_log;
    config.logger.minimum_level = AFORC_LOG_INFO;
    config.hooks.now = default_now;
    config.hooks.sleep = default_sleep;
    return config;
}

static bool config_valid(const AFORC_EngineConfig *config) {
    return config != NULL && config->fixed_updates_per_second > 0U &&
           config->fixed_updates_per_second <= UINT32_C(1000000000) &&
           config->maximum_fixed_updates_per_frame > 0U &&
           config->maximum_frame_seconds > 0.0 &&
           config->maximum_frame_seconds <= 3600.0 &&
           isfinite(config->maximum_frame_seconds) &&
           config->scene_capacity > 0U &&
           config->scene_command_capacity > 0U &&
           config->allocator.allocate != NULL &&
           config->allocator.reallocate != NULL &&
           config->allocator.deallocate != NULL && config->hooks.now != NULL;
}

AFORC_Status aforc_engine_create(const AFORC_EngineConfig *config,
                             AFORC_Engine **out_engine, AFORC_Error *error) {
    AFORC_Engine *engine = NULL;
    AFORC_Status status;
    if (out_engine == NULL) {
        return aforc_engine_set_error(error, AFORC_ERROR_INVALID_ARGUMENT,
                                    "invalid engine configuration");
    }
    *out_engine = NULL;
    if (!config_valid(config)) {
        return aforc_engine_set_error(error, AFORC_ERROR_INVALID_ARGUMENT,
                                    "invalid engine configuration");
    }
    status = aforc_alloc_array(&config->allocator, 1U, sizeof(*engine),
                             (void **)&engine);
    if (status != AFORC_OK) {
        return aforc_engine_set_error(error, status,
                                    "could not allocate engine");
    }
    memset(engine, 0, sizeof(*engine));
    engine->config = *config;
    engine->fixed_step_ns =
        UINT64_C(1000000000) / config->fixed_updates_per_second;
    if (engine->fixed_step_ns == 0U) {
        aforc_free(&config->allocator, engine);
        return aforc_engine_set_error(
            error,
            AFORC_ERROR_INVALID_ARGUMENT,
            "fixed update frequency exceeds timer resolution");
    }
    status = aforc_scene_stack_init(&engine->scenes, config->scene_capacity,
                                  config->allocator, error);
    if (status != AFORC_OK) {
        aforc_free(&config->allocator, engine);
        return status;
    }
    status = aforc_alloc_array(&config->allocator,
                             config->scene_command_capacity,
                             sizeof(*engine->commands),
                             (void **)&engine->commands);
    if (status != AFORC_OK) {
        aforc_scene_stack_dispose(&engine->scenes, engine);
        aforc_free(&config->allocator, engine);
        return aforc_engine_set_error(error, status,
                                    "could not allocate scene commands");
    }
    engine->state = AFORC_ENGINE_CREATED;
    *out_engine = engine;
    aforc_error_clear(error);
    return AFORC_OK;
}

void aforc_engine_destroy(AFORC_Engine *engine) {
    AFORC_Allocator allocator;
    if (engine == NULL) {
        return;
    }
    if (engine->frame_active || engine->scenes.dispatching ||
        engine->state == AFORC_ENGINE_RUNNING ||
        engine->state == AFORC_ENGINE_STOPPING) {
        return;
    }
    allocator = engine->config.allocator;
    aforc_scene_stack_dispose(&engine->scenes, engine);
    aforc_free(&allocator, engine->commands);
    memset(engine, 0, sizeof(*engine));
    aforc_free(&allocator, engine);
}

static AFORC_Status invoke_hook(AFORC_EngineHookFn hook, AFORC_Engine *engine,
                              AFORC_Error *error) {
    return hook == NULL ? AFORC_OK
                        : hook(engine->config.hooks.context, engine, error);
}

static AFORC_Status engine_frame_impl(AFORC_Engine *engine, uint64_t now_ns,
                                      AFORC_Error *error) {
    uint64_t elapsed_ns = 0U;
    uint64_t maximum_ns;
    uint32_t updates = 0U;
    AFORC_Status status;
    status = aforc_engine_apply_scene_commands(engine, error);
    if (status != AFORC_OK || engine->quit_requested) {
        return status;
    }
    /* The first sample establishes a baseline and contributes no simulation
     * time. Later samples are clamped before accumulation, bounding catch-up
     * work after suspension and preventing a regressing clock from creating a
     * huge unsigned delta. */
    if (!engine->clock_started) {
        engine->last_time_ns = now_ns;
        engine->clock_started = true;
    } else if (now_ns >= engine->last_time_ns) {
        elapsed_ns = now_ns - engine->last_time_ns;
        engine->last_time_ns = now_ns;
    } else {
        /* Advance the baseline even on regression so one bad sample cannot
         * inflate the following frame's elapsed time. */
        engine->last_time_ns = now_ns;
        aforc_log(&engine->config.logger, AFORC_LOG_WARNING, "engine",
                "clock moved backwards; frame delta clamped to zero");
    }
    maximum_ns = (uint64_t)(engine->config.maximum_frame_seconds * 1.0e9);
    if (elapsed_ns > maximum_ns) {
        elapsed_ns = maximum_ns;
    }
    if (UINT64_MAX - engine->accumulator_ns < elapsed_ns) {
        engine->accumulator_ns = maximum_ns;
    } else {
        engine->accumulator_ns += elapsed_ns;
    }
    status = invoke_hook(engine->config.hooks.begin_frame, engine, error);
    if (status != AFORC_OK) {
        return status;
    }
    if (engine->quit_requested) {
        return AFORC_OK;
    }
    /* Consume whole fixed quanta only. Commands are committed after every
     * callback boundary so a transition requested by one tick is visible to
     * the next without mutating a scene stack during traversal. */
    while (engine->accumulator_ns >= engine->fixed_step_ns &&
           updates < engine->config.maximum_fixed_updates_per_frame) {
        status = aforc_scene_stack_fixed_update(
            &engine->scenes, engine, (double)engine->fixed_step_ns / 1.0e9,
            error);
        if (status != AFORC_OK) {
            return status;
        }
        engine->accumulator_ns -= engine->fixed_step_ns;
        engine->fixed_tick++;
        updates++;
        status = aforc_engine_apply_scene_commands(engine, error);
        if (status != AFORC_OK || engine->quit_requested) {
            return status;
        }
    }
    if (engine->accumulator_ns >= engine->fixed_step_ns) {
        /* The per-frame update budget is a hard latency bound. Retaining the
         * backlog would make every subsequent frame perform maximum catch-up
         * work, so preserve only the fractional remainder used for rendering. */
        engine->accumulator_ns %= engine->fixed_step_ns;
        aforc_log(&engine->config.logger, AFORC_LOG_WARNING, "engine",
                "fixed-update backlog dropped to prevent a spiral of death");
    }
    status = aforc_scene_stack_update(&engine->scenes, engine,
                                    (double)elapsed_ns / 1.0e9, error);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_engine_apply_scene_commands(engine, error);
    if (status != AFORC_OK || engine->quit_requested) {
        return status;
    }
    /* The remainder is strictly smaller than one fixed step here, making the
     * render interpolation stable in [0, 1) without another clamp. */
    engine->interpolation = (double)engine->accumulator_ns /
                            (double)engine->fixed_step_ns;
    status = aforc_scene_stack_render(&engine->scenes, engine,
                                    engine->interpolation, error);
    if (status != AFORC_OK) {
        return status;
    }
    status = invoke_hook(engine->config.hooks.present, engine, error);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_engine_apply_scene_commands(engine, error);
    if (status == AFORC_OK) {
        engine->frame_index++;
    }
    return status;
}

AFORC_Status aforc_engine_frame(AFORC_Engine *engine, uint64_t now_ns,
                                AFORC_Error *error) {
    AFORC_Status status;

    if (engine == NULL || engine->state != AFORC_ENGINE_CREATED ||
        engine->frame_active) {
        return aforc_engine_set_error(error, AFORC_ERROR_STATE,
                                      "engine frame is already owned");
    }
    engine->frame_active = true;
    status = engine_frame_impl(engine, now_ns, error);
    engine->frame_active = false;
    return status;
}

AFORC_Status aforc_engine_run(AFORC_Engine *engine, AFORC_Error *error) {
    AFORC_Status status = AFORC_OK;
    if (engine == NULL || (engine->state != AFORC_ENGINE_CREATED &&
                           engine->state != AFORC_ENGINE_STOPPED) ||
        engine->frame_active) {
        return aforc_engine_set_error(
            error,
            AFORC_ERROR_STATE,
            "engine cannot enter run loop from current state");
    }
    engine->state = AFORC_ENGINE_RUNNING;
    engine->quit_requested = false;
    engine->clock_started = false;
    while (!engine->quit_requested) {
        /* One timestamp anchors both simulation delta and frame pacing. Event
         * polling happens first, but its time is still charged to this frame's
         * budget rather than silently shifting into the next frame. */
        const uint64_t frame_start = engine->config.hooks.now(
            engine->config.hooks.context);
        status = invoke_hook(engine->config.hooks.poll_events, engine, error);
        if (status != AFORC_OK || engine->quit_requested) {
            break;
        }
        engine->frame_active = true;
        status = engine_frame_impl(engine, frame_start, error);
        engine->frame_active = false;
        if (status != AFORC_OK || engine->quit_requested) {
            break;
        }
        if (engine->config.target_frames_per_second > 0U &&
            engine->config.hooks.sleep != NULL) {
            const uint64_t budget = UINT64_C(1000000000) /
                                    engine->config.target_frames_per_second;
            const uint64_t end = engine->config.hooks.now(
                engine->config.hooks.context);
            if (end >= frame_start && end - frame_start < budget) {
                engine->config.hooks.sleep(engine->config.hooks.context,
                                           budget - (end - frame_start));
            }
        }
    }
    engine->state = AFORC_ENGINE_STOPPED;
    return status;
}

void aforc_engine_request_quit(AFORC_Engine *engine) {
    if (engine != NULL) {
        engine->quit_requested = true;
        if (engine->state == AFORC_ENGINE_RUNNING) {
            engine->state = AFORC_ENGINE_STOPPING;
        }
    }
}

AFORC_Status aforc_engine_dispatch_event(AFORC_Engine *engine, const void *event,
                                     bool *consumed, AFORC_Error *error) {
    if (engine == NULL || event == NULL) {
        return aforc_engine_set_error(error,
                                    AFORC_ERROR_INVALID_ARGUMENT,
                                    "invalid event dispatch arguments");
    }
    return aforc_scene_stack_event(&engine->scenes, engine, event, consumed,
                                 error);
}

AFORC_EngineState aforc_engine_state(const AFORC_Engine *engine) {
    return engine == NULL ? AFORC_ENGINE_STOPPED : engine->state;
}

uint64_t aforc_engine_frame_index(const AFORC_Engine *engine) {
    return engine == NULL ? 0U : engine->frame_index;
}

uint64_t aforc_engine_fixed_tick(const AFORC_Engine *engine) {
    return engine == NULL ? 0U : engine->fixed_tick;
}

double aforc_engine_interpolation(const AFORC_Engine *engine) {
    return engine == NULL ? 0.0 : engine->interpolation;
}

void *aforc_engine_user_data(AFORC_Engine *engine) {
    return engine == NULL ? NULL : engine->config.user_data;
}

const AFORC_EngineConfig *aforc_engine_config(const AFORC_Engine *engine) {
    return engine == NULL ? NULL : &engine->config;
}

AFORC_Scene *aforc_engine_scene(const AFORC_Engine *engine) {
    return engine == NULL ? NULL : aforc_scene_stack_top(&engine->scenes);
}
