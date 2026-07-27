/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ENGINE_H
#define AFORC_ENGINE_H

#include "aforc/scene.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t (*AFORC_NowFn)(void *context);
typedef void (*AFORC_SleepFn)(void *context, uint64_t nanoseconds);
typedef AFORC_Status (*AFORC_EngineHookFn)(void *context, AFORC_Engine *engine,
                                      AFORC_Error *error);

typedef struct AFORC_EngineHooks {
    void *context;
    AFORC_NowFn now;
    AFORC_SleepFn sleep;
    AFORC_EngineHookFn poll_events;
    AFORC_EngineHookFn begin_frame;
    AFORC_EngineHookFn present;
} AFORC_EngineHooks;

typedef struct AFORC_EngineConfig {
    uint32_t fixed_updates_per_second;
    uint32_t maximum_fixed_updates_per_frame;
    uint32_t target_frames_per_second;
    double maximum_frame_seconds;
    size_t scene_capacity;
    size_t scene_command_capacity;
    bool quit_when_scene_stack_empty;
    void *user_data;
    AFORC_Allocator allocator;
    AFORC_Logger logger;
    AFORC_EngineHooks hooks;
} AFORC_EngineConfig;

/*
 * Engine instances are single-thread-affine. The config, allocator, logger,
 * and hook tables are copied by create; their context pointers remain borrowed
 * and must outlive the engine. Scene objects and user_data are also borrowed.
 * Destroy never frees them.
 *
 * The fixed-step accumulator clamps long frames and drops excess backlog after
 * maximum_fixed_updates_per_frame to avoid an unbounded spiral of death.
 * fixed_updates_per_second and a nonzero target_frames_per_second may not
 * exceed the nanosecond timer resolution. Setting target_frames_per_second to
 * zero disables run-loop sleeping.
 * Run and frame own engine execution until they return and are non-reentrant.
 * Calling either from an engine hook or scene callback returns
 * AFORC_ERROR_STATE. Destroy requested during active execution is ignored; the
 * owner must destroy the engine after the outer operation returns.
 */

typedef enum AFORC_EngineState {
    AFORC_ENGINE_CREATED = 0,
    AFORC_ENGINE_RUNNING,
    AFORC_ENGINE_STOPPING,
    AFORC_ENGINE_STOPPED
} AFORC_EngineState;

AFORC_API AFORC_EngineConfig aforc_engine_config_default(void);
/* On every failure with a non-NULL out_engine, stores NULL in *out_engine. */
AFORC_API AFORC_Status aforc_engine_create(const AFORC_EngineConfig *config,
                                     AFORC_Engine **out_engine,
                                     AFORC_Error *error);
AFORC_API void aforc_engine_destroy(AFORC_Engine *engine);
/* Owns the loop until quit/error. poll_events runs before each frame; render is
 * followed by present. The engine may be run again from STOPPED state. A quit
 * request skips remaining frame work and frame-rate sleep. */
AFORC_API AFORC_Status aforc_engine_run(AFORC_Engine *engine, AFORC_Error *error);
/* Deterministic embedding entry point for CREATED engines. now_ns must be
 * nondecreasing and use one clock domain. It does not call poll_events or
 * sleep, but does call begin_frame and present. Each successful fixed callback
 * advances fixed_tick even when that callback requests quit. */
AFORC_API AFORC_Status aforc_engine_frame(AFORC_Engine *engine, uint64_t now_ns,
                                    AFORC_Error *error);
AFORC_API void aforc_engine_request_quit(AFORC_Engine *engine);
/* Scene mutations are queued and applied only at safe dispatch boundaries.
 * Requests can therefore be issued from scene callbacks. The caller retains
 * ownership and must keep each referenced scene alive until it leaves. */
AFORC_API AFORC_Status aforc_engine_request_push(AFORC_Engine *engine,
                                           AFORC_Scene *scene,
                                           AFORC_Error *error);
AFORC_API AFORC_Status aforc_engine_request_pop(AFORC_Engine *engine,
                                          AFORC_Error *error);
AFORC_API AFORC_Status aforc_engine_request_replace(AFORC_Engine *engine,
                                              AFORC_Scene *scene,
                                              AFORC_Error *error);
AFORC_API AFORC_Status aforc_engine_dispatch_event(AFORC_Engine *engine,
                                             const void *event,
                                             bool *consumed,
                                             AFORC_Error *error);
AFORC_API AFORC_EngineState aforc_engine_state(const AFORC_Engine *engine);
AFORC_API uint64_t aforc_engine_frame_index(const AFORC_Engine *engine);
AFORC_API uint64_t aforc_engine_fixed_tick(const AFORC_Engine *engine);
AFORC_API double aforc_engine_interpolation(const AFORC_Engine *engine);
AFORC_API void *aforc_engine_user_data(AFORC_Engine *engine);
AFORC_API const AFORC_EngineConfig *aforc_engine_config(const AFORC_Engine *engine);
AFORC_API AFORC_Scene *aforc_engine_scene(const AFORC_Engine *engine);

#ifdef __cplusplus
}
#endif

#endif
