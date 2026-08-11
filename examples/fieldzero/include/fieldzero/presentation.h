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
