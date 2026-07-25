/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "engine_internal.h"

#include <string.h>

/* Owns the bounded deferred-command queue. Requests retain borrowed scene
 * pointers and are applied only between scene-stack dispatches. */

static AFORC_Status aforc_engine_queue_scene_command(
    AFORC_Engine *engine,
    AFORC_EngineSceneCommandType type,
    AFORC_Scene *scene,
    AFORC_Error *error
)
{
    if (engine == NULL ||
        ((type == AFORC_ENGINE_SCENE_COMMAND_PUSH ||
          type == AFORC_ENGINE_SCENE_COMMAND_REPLACE) &&
         scene == NULL)) {
        return aforc_engine_set_error(error,
                                    AFORC_ERROR_INVALID_ARGUMENT,
                                    "invalid scene command");
    }
    if (engine->command_count == engine->config.scene_command_capacity) {
        return aforc_engine_set_error(error,
                                    AFORC_ERROR_LIMIT,
                                    "scene command queue is full");
    }
    engine->commands[engine->command_count++] =
        (AFORC_EngineSceneCommand){type, scene};
    return AFORC_OK;
}

AFORC_Status aforc_engine_request_push(AFORC_Engine *engine, AFORC_Scene *scene,
                                   AFORC_Error *error)
{
    return aforc_engine_queue_scene_command(engine,
                                          AFORC_ENGINE_SCENE_COMMAND_PUSH,
                                          scene,
                                          error);
}

AFORC_Status aforc_engine_request_pop(AFORC_Engine *engine, AFORC_Error *error)
{
    return aforc_engine_queue_scene_command(engine,
                                          AFORC_ENGINE_SCENE_COMMAND_POP,
                                          NULL,
                                          error);
}

AFORC_Status aforc_engine_request_replace(AFORC_Engine *engine, AFORC_Scene *scene,
                                      AFORC_Error *error)
{
    return aforc_engine_queue_scene_command(engine,
                                          AFORC_ENGINE_SCENE_COMMAND_REPLACE,
                                          scene,
                                          error);
}

AFORC_Status aforc_engine_apply_scene_commands(AFORC_Engine *engine,
                                           AFORC_Error *error)
{
    size_t processed = 0U;

    while (processed < engine->command_count) {
        const AFORC_EngineSceneCommand command = engine->commands[processed++];
        AFORC_Status status;

        if (command.type == AFORC_ENGINE_SCENE_COMMAND_PUSH) {
            status = aforc_scene_stack_push(&engine->scenes,
                                          engine,
                                          command.scene,
                                          error);
        } else if (command.type == AFORC_ENGINE_SCENE_COMMAND_REPLACE) {
            status = aforc_scene_stack_replace(&engine->scenes,
                                             engine,
                                             command.scene,
                                             error);
        } else {
            status = aforc_scene_stack_pop(&engine->scenes,
                                         engine,
                                         NULL,
                                         error);
        }
        if (status != AFORC_OK) {
            /* Failed and completed commands leave the queue; later requests
             * retain their original order for the next safe boundary. */
            if (processed < engine->command_count) {
                (void)memmove(engine->commands,
                              &engine->commands[processed],
                              (engine->command_count - processed) *
                                  sizeof(*engine->commands));
            }
            engine->command_count -= processed;
            return status;
        }
    }
    engine->command_count = 0U;
    if (engine->config.quit_when_scene_stack_empty &&
        aforc_scene_stack_count(&engine->scenes) == 0U) {
        engine->quit_requested = true;
    }
    return AFORC_OK;
}
