/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_SURF_MAN_PRESENTATION_INTERNAL_H
#define AFORC_EXAMPLES_SURF_MAN_PRESENTATION_INTERNAL_H

#include "../include/surf_man/presentation.h"

#define SURF_MAN_COLOR_CANVAS ((uint8_t)234)
#define SURF_MAN_COLOR_INK ((uint8_t)255)
#define SURF_MAN_COLOR_FRAMEWORK ((uint8_t)243)
#define SURF_MAN_COLOR_SIGNAL ((uint8_t)77)

enum { SURF_MAN_VISUAL_FACE_UNITS = 5 };

typedef enum SurfManTone {
    SURF_MAN_TONE_CANVAS = 0,
    SURF_MAN_TONE_INK,
    SURF_MAN_TONE_FRAMEWORK,
    SURF_MAN_TONE_SIGNAL
} SurfManTone;

typedef struct SurfManLayout {
    AFORC_Rect screen;
    AFORC_Rect play;
    AFORC_Rect hud;
    int32_t separator_y;
} SurfManLayout;

SurfManLayout surf_man_layout_for_size(AFORC_Size size);
int32_t surf_man_q16_round_cell(int32_t value_q16);
int32_t surf_man_rider_center_x(const SurfManSimulation *simulation,
                                AFORC_Rect play);
int32_t surf_man_wave_surface_row(const SurfManWaveSample *sample,
                                  AFORC_Rect play);
AFORC_Cell surf_man_tone_cell(const SurfManApp *app,
                              uint32_t codepoint,
                              SurfManTone tone,
                              AFORC_CellStyle style);
AFORC_Status surf_man_plot_particle_cell(void *context,
                                         AFORC_Point position,
                                         AFORC_Cell cell);
AFORC_Status surf_man_draw_text(SurfManApp *app,
                                AFORC_Point position,
                                const char *text,
                                SurfManTone tone,
                                AFORC_CellStyle style);
AFORC_Status surf_man_draw_char(SurfManApp *app,
                                AFORC_Point position,
                                uint32_t codepoint,
                                SurfManTone tone,
                                AFORC_CellStyle style);
AFORC_Status surf_man_draw_panel(SurfManApp *app,
                                 AFORC_Rect rect,
                                 const char *title);
AFORC_Status surf_man_draw_instrument(SurfManApp *app,
                                      const SurfManLayout *layout);
const char *surf_man_phase_name(SurfManPhase phase);

AFORC_Status surf_man_render_shack_art(SurfManApp *app,
                                       const SurfManLayout *layout);
AFORC_Status surf_man_render_wave_art(SurfManApp *app,
                                      const SurfManLayout *layout);
AFORC_Status surf_man_render_rider(SurfManApp *app,
                                   const SurfManLayout *layout,
                                   int32_t surface_y);
AFORC_Status surf_man_render_menu(SurfManApp *app,
                                  const SurfManLayout *layout);
AFORC_Status surf_man_render_hud(SurfManApp *app,
                                 const SurfManLayout *layout);
AFORC_Status surf_man_render_overlay(SurfManApp *app,
                                     const SurfManLayout *layout);
AFORC_Status surf_man_render_phase_modal(SurfManApp *app,
                                         const SurfManLayout *layout);
AFORC_Status surf_man_render_resize(SurfManApp *app, AFORC_Size size);

#endif
