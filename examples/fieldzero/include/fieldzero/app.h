#ifndef FIELDZERO_APP_H
#define FIELDZERO_APP_H

#include "aforc/engine.h"
#include "aforc/input.h"
#include "aforc/terminal.h"
#include "fieldzero/presentation.h"

typedef struct FieldzeroApp
{
    AFORC_Terminal *terminal;
    AFORC_Renderer *renderer;
    AFORC_Input *input;
    AFORC_Engine *engine;
    FieldzeroGame game;
    FieldzeroPresentation presentation;
    FieldzeroOptions options;
    FieldzeroViewState view;
    AFORC_Scene title_scene;
    AFORC_Scene play_scene;
    AFORC_Scene completion_scene;
    AFORC_Scene overlay_scene;
    uint64_t last_fixed_tick;
    int pending_signal;
    bool initialized;
} FieldzeroApp;

static inline uint32_t fieldzero_event_codepoint(const AFORC_InputEvent *event)
{
    uint32_t codepoint = event->data.key.codepoint;

    if (codepoint == 0U && event->data.key.key >= AFORC_KEY_A &&
        event->data.key.key <= AFORC_KEY_Z)
    {
        codepoint = (uint32_t)event->data.key.key;
        if ((event->data.key.modifiers & AFORC_MOD_SHIFT) == 0U)
        {
            codepoint += (uint32_t)('a' - 'A');
        }
    }
    return codepoint;
}

static inline bool fieldzero_codepoint_is(uint32_t codepoint, char letter)
{
    return codepoint == (uint32_t)letter ||
           codepoint == (uint32_t)(letter - ('a' - 'A'));
}

int fieldzero_run_interactive(const FieldzeroOptions *options);
int fieldzero_run_smoke(const FieldzeroOptions *options);

AFORC_Status fieldzero_app_init(FieldzeroApp *app,
                                const FieldzeroOptions *options,
                                bool interactive,
                                AFORC_Error *error);
void fieldzero_app_dispose(FieldzeroApp *app);
AFORC_Status fieldzero_app_poll_events(void *context,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error);
AFORC_Status fieldzero_app_begin_frame(void *context,
                                       AFORC_Engine *engine,
                                       AFORC_Error *error);
AFORC_Status
fieldzero_app_present(void *context, AFORC_Engine *engine, AFORC_Error *error);
void fieldzero_app_configure_scenes(FieldzeroApp *app);
void fieldzero_app_handle_input(FieldzeroApp *app,
                                const AFORC_InputEvent *event,
                                AFORC_Engine *engine);
void fieldzero_app_reconcile_input(FieldzeroApp *app);

#endif
