/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_CORE_ENGINE_INTERNAL_H
#define AFORC_CORE_ENGINE_INTERNAL_H

#include "common_internal.h"
#include "aforc/engine.h"

typedef enum AFORC_EngineSceneCommandType {
    AFORC_ENGINE_SCENE_COMMAND_PUSH,
    AFORC_ENGINE_SCENE_COMMAND_POP,
    AFORC_ENGINE_SCENE_COMMAND_REPLACE
} AFORC_EngineSceneCommandType;

typedef struct AFORC_EngineSceneCommand {
    AFORC_EngineSceneCommandType type;
    AFORC_Scene *scene;
} AFORC_EngineSceneCommand;

struct AFORC_Engine {
    AFORC_EngineConfig config;
    AFORC_SceneStack scenes;
    AFORC_EngineSceneCommand *commands;
    size_t command_count;
    uint64_t last_time_ns;
    uint64_t accumulator_ns;
    uint64_t fixed_step_ns;
    uint64_t frame_index;
    uint64_t fixed_tick;
    double interpolation;
    AFORC_EngineState state;
    bool clock_started;
    bool quit_requested;
    bool frame_active;
};

static inline AFORC_Status aforc_engine_set_error(AFORC_Error *error,
                                               AFORC_Status status,
                                               const char *message)
{
    aforc_error_set(error, status, "engine", "%s", message);
    return status;
}

AFORC_INTERNAL AFORC_Status aforc_engine_apply_scene_commands(
    AFORC_Engine *engine,
    AFORC_Error *error
);

#endif
