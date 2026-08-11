#ifndef FIELDZERO_PRESENTATION_H
#define FIELDZERO_PRESENTATION_H

#include "aforc/assets.h"
#include "aforc/effects.h"
#include "aforc/renderer.h"
#include "fieldzero/game.h"

enum
{
    FIELDZERO_PARTICLE_CAPACITY = 96
};

typedef struct FieldzeroPresentation
{
    AFORC_Particle particles[FIELDZERO_PARTICLE_CAPACITY];
    AFORC_ParticlePool particle_pool;
    AFORC_Rng decorative_rng;
    uint64_t frame_index;
    uint32_t particle_millisecond_remainder;
    int8_t camera_impulse_x;
    int8_t camera_impulse_y;
    bool reduced_motion;
    bool no_color;
} FieldzeroPresentation;

AFORC_Cell fieldzero_visual_cell(uint32_t codepoint,
                                 uint8_t role,
                                 AFORC_CellStyle style,
                                 bool no_color);
AFORC_Status
fieldzero_visual_plot(void *context, AFORC_Point position, AFORC_Cell cell);
AFORC_Status fieldzero_render_world(AFORC_Renderer *renderer,
                                    const FieldzeroGame *game,
                                    const FieldzeroPresentation *presentation,
                                    AFORC_Rect arena);
AFORC_Status fieldzero_render_ui(AFORC_Renderer *renderer,
                                 const FieldzeroGame *game,
                                 const FieldzeroPresentation *presentation,
                                 const FieldzeroViewState *view,
                                 AFORC_Rect arena);
AFORC_Status fieldzero_presentation_init(FieldzeroPresentation *presentation,
                                         const FieldzeroOptions *options);
void fieldzero_presentation_dispose(FieldzeroPresentation *presentation);
AFORC_Status fieldzero_presentation_update(FieldzeroPresentation *presentation,
                                           const FieldzeroGame *game,
                                           uint64_t fixed_tick);
AFORC_Status fieldzero_render(AFORC_Renderer *renderer,
                              const FieldzeroGame *game,
                              const FieldzeroPresentation *presentation,
                              const FieldzeroViewState *view);
AFORC_Status fieldzero_render_validate(const AFORC_Renderer *renderer,
                                       const FieldzeroViewState *view,
                                       bool no_color);

#endif
