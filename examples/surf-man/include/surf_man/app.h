/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_SURF_MAN_APP_H
#define AFORC_EXAMPLES_SURF_MAN_APP_H

#include "aforc/engine.h"
#include "aforc/input.h"
#include "aforc/terminal.h"
#include "surf_man/game.h"
#include "surf_man/presentation.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum SurfManMenuItem {
    SURF_MAN_MENU_SURF = 0,
    SURF_MAN_MENU_PRACTICE,
    SURF_MAN_MENU_HELP,
    SURF_MAN_MENU_ACCESSIBILITY,
    SURF_MAN_MENU_QUIT,
    SURF_MAN_MENU_COUNT
} SurfManMenuItem;

typedef struct SurfManInputState {
    int8_t vertical;
    int8_t horizontal;
    int8_t vertical_tap;
    int8_t horizontal_tap;
    uint8_t vertical_lease;
    uint8_t horizontal_lease;
    uint8_t action_lease;
    bool action_tap;
    bool confirm_latched;
    bool back_latched;
} SurfManInputState;

typedef struct SurfManInputKey {
    AFORC_Key key;
    uint32_t codepoint;
    int vertical;
    int horizontal;
    bool repeat;
    bool quit;
    bool pause;
} SurfManInputKey;

struct SurfManApp {
    AFORC_Allocator allocator;
    AFORC_Terminal *terminal;
    AFORC_Renderer *renderer;
    AFORC_Input *input;
    AFORC_Scene scene;
    SurfManSimulation simulation;
    SurfManVisuals visuals;
    SurfManSettings settings;
    SurfManInputState controls;
    uint64_t seed;
    SurfManOverlay overlay;
    SurfManMenuItem menu_item;
    AFORC_Size terminal_size;
    bool focused;
    bool smoke;
    bool initialized;
    char message[SURF_MAN_MESSAGE_CAPACITY];
};

extern const AFORC_SceneVTable surf_man_scene_vtable;

AFORC_Status surf_man_error(AFORC_Error *error,
                            AFORC_Status status,
                            const char *subsystem,
                            const char *message);
void surf_man_set_message(SurfManApp *app, const char *format, ...);
AFORC_Status surf_man_app_init(SurfManApp *app,
                               AFORC_Renderer *renderer,
                               AFORC_Input *input,
                               AFORC_Terminal *terminal,
                               uint64_t seed,
                               bool smoke);
void surf_man_app_dispose(SurfManApp *app);
AFORC_Status surf_man_app_handle_event(SurfManApp *app,
                                        AFORC_Engine *engine,
                                        const AFORC_InputEvent *event);
AFORC_Status surf_man_menu_handle_modal_key(
    SurfManApp *app,
    AFORC_Engine *engine,
    const SurfManInputKey *input);
AFORC_Status surf_man_menu_handle_shack_key(
    SurfManApp *app,
    AFORC_Engine *engine,
    const SurfManInputKey *input);
void surf_man_input_take_command(SurfManApp *app,
                                  SurfManCommand *out_command);
AFORC_Status surf_man_scene_fixed_update(AFORC_Scene *scene,
                                         AFORC_Engine *engine,
                                         double seconds,
                                         AFORC_Error *error);
AFORC_Status surf_man_scene_update(AFORC_Scene *scene,
                                   AFORC_Engine *engine,
                                   double seconds,
                                   AFORC_Error *error);
AFORC_Status surf_man_scene_render(AFORC_Scene *scene,
                                   AFORC_Engine *engine,
                                   double interpolation,
                                   AFORC_Error *error);
AFORC_Status surf_man_scene_event(AFORC_Scene *scene,
                                  AFORC_Engine *engine,
                                  const void *event_data,
                                  bool *consumed,
                                  AFORC_Error *error);
AFORC_Status surf_man_poll_events(void *context,
                                  AFORC_Engine *engine,
                                  AFORC_Error *error);
AFORC_Status surf_man_begin_frame(void *context,
                                  AFORC_Engine *engine,
                                  AFORC_Error *error);
AFORC_Status surf_man_present(void *context,
                              AFORC_Engine *engine,
                              AFORC_Error *error);
int surf_man_run_smoke(uint64_t seed);
int surf_man_run_interactive(uint64_t seed);

#endif
