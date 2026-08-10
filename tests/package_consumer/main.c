/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/aforc.h"

int main(void)
{
    AFORC_EngineConfig config = aforc_engine_config_default();
    AFORC_Engine *engine = NULL;
    AFORC_Error error = {0};
    AFORC_Status status;

    config.quit_when_scene_stack_empty = false;
    config.target_frames_per_second = 0U;
    status = aforc_engine_create(&config, &engine, &error);
    if (status != AFORC_OK)
    {
        return 1;
    }

    status = aforc_engine_frame(engine, UINT64_C(0), &error);
    if (status == AFORC_OK && aforc_engine_frame_index(engine) != UINT64_C(1))
    {
        status = AFORC_ERROR_STATE;
    }
    aforc_engine_destroy(engine);
    return status == AFORC_OK ? 0 : 2;
}
