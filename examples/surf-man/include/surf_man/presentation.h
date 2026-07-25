/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_SURF_MAN_PRESENTATION_H
#define AFORC_EXAMPLES_SURF_MAN_PRESENTATION_H

#include "aforc/effects.h"
#include "aforc/renderer.h"
#include "surf_man/game.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct SurfManApp SurfManApp;

typedef struct SurfManVisuals {
    AFORC_Particle particles[SURF_MAN_PARTICLE_CAPACITY];
    AFORC_ParticlePool particle_pool;
    uint64_t visual_tick;
    bool dirty;
    bool initialized;
} SurfManVisuals;

AFORC_Cell surf_man_cell(uint32_t codepoint,
                         uint8_t color_index,
                         AFORC_CellStyle style);
AFORC_Status surf_man_plot_cell(void *context,
                                AFORC_Point position,
                                AFORC_Cell cell);
AFORC_Status surf_man_visuals_init(SurfManVisuals *visuals, uint32_t seed);
void surf_man_visuals_dispose(SurfManVisuals *visuals);
AFORC_Status surf_man_visuals_step(SurfManApp *app);
void surf_man_visuals_mark_dirty(SurfManVisuals *visuals);
AFORC_Status surf_man_render_frame(SurfManApp *app,
                                   double interpolation,
                                   AFORC_Error *error);
AFORC_Status surf_man_emit_spray(SurfManApp *app, bool wipeout);

#endif
