/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EFFECTS_H
#define AFORC_EFFECTS_H

#include "common.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AFORC_EFFECT_FIXED_SHIFT 16
#define AFORC_EFFECT_FIXED_ONE INT32_C(65536)

typedef AFORC_Status (*AFORC_EffectPlotFn)(void *context,
                                       AFORC_Point position,
                                       AFORC_Cell cell);

typedef enum AFORC_SpriteRotation {
    AFORC_SPRITE_ROTATION_0 = 0,
    AFORC_SPRITE_ROTATION_90,
    AFORC_SPRITE_ROTATION_180,
    AFORC_SPRITE_ROTATION_270
} AFORC_SpriteRotation;

typedef struct AFORC_SpriteTransform {
    AFORC_Point position;
    uint32_t scale_x;
    uint32_t scale_y;
    AFORC_SpriteRotation rotation;
    bool flip_x;
    bool flip_y;
} AFORC_SpriteTransform;

typedef struct AFORC_SpriteDrawOptions {
    AFORC_SpriteTransform transform;
    AFORC_Rect clip;
    bool clip_enabled;
    bool transparency_enabled;
    uint32_t transparent_codepoint;
} AFORC_SpriteDrawOptions;

typedef struct AFORC_Sprite {
    const AFORC_Cell *cells;
    size_t stride;
    AFORC_Size size;
    bool initialized;
} AFORC_Sprite;

AFORC_API AFORC_SpriteDrawOptions aforc_sprite_draw_options_default(void);
AFORC_API AFORC_Status aforc_sprite_init(AFORC_Sprite *sprite,
                                   const AFORC_Cell *cells,
                                   AFORC_Size size,
                                   size_t stride);
AFORC_API void aforc_sprite_dispose(AFORC_Sprite *sprite);
AFORC_API AFORC_Status aforc_sprite_draw(const AFORC_Sprite *sprite,
                                   const AFORC_SpriteDrawOptions *options,
                                   AFORC_EffectPlotFn plot,
                                   void *context);

typedef struct AFORC_AnimationFrame {
    const AFORC_Sprite *sprite;
    uint32_t duration_ms;
} AFORC_AnimationFrame;

typedef enum AFORC_AnimationMode {
    AFORC_ANIMATION_ONCE = 0,
    AFORC_ANIMATION_LOOP,
    AFORC_ANIMATION_PING_PONG
} AFORC_AnimationMode;

typedef struct AFORC_Animation {
    const AFORC_AnimationFrame *frames;
    size_t frame_count;
    size_t frame_index;
    uint64_t frame_elapsed_ms;
    uint64_t cycle_duration_ms;
    AFORC_AnimationMode mode;
    int8_t direction;
    bool paused;
    bool finished;
    bool initialized;
} AFORC_Animation;

AFORC_API AFORC_Status aforc_animation_init(AFORC_Animation *animation,
                                      const AFORC_AnimationFrame *frames,
                                      size_t frame_count,
                                      AFORC_AnimationMode mode);
AFORC_API void aforc_animation_dispose(AFORC_Animation *animation);
AFORC_API AFORC_Status aforc_animation_restart(AFORC_Animation *animation);
AFORC_API AFORC_Status aforc_animation_set_paused(AFORC_Animation *animation,
                                            bool paused);
AFORC_API AFORC_Status aforc_animation_update(AFORC_Animation *animation,
                                        uint64_t delta_ms);
AFORC_API AFORC_Status aforc_animation_current(
    const AFORC_Animation *animation,
    const AFORC_AnimationFrame **out_frame);
AFORC_API AFORC_Status aforc_animation_draw(
    const AFORC_Animation *animation,
    const AFORC_SpriteDrawOptions *options,
    AFORC_EffectPlotFn plot,
    void *context);

typedef enum AFORC_Easing {
    AFORC_EASING_LINEAR = 0,
    AFORC_EASING_QUADRATIC_IN,
    AFORC_EASING_QUADRATIC_OUT,
    AFORC_EASING_QUADRATIC_IN_OUT,
    AFORC_EASING_CUBIC_IN,
    AFORC_EASING_CUBIC_OUT,
    AFORC_EASING_CUBIC_IN_OUT
} AFORC_Easing;

typedef struct AFORC_Tween {
    double start;
    double end;
    uint64_t duration_ms;
    uint64_t elapsed_ms;
    AFORC_Easing easing;
    bool paused;
    bool finished;
    bool initialized;
} AFORC_Tween;

AFORC_API AFORC_Status aforc_easing_sample(AFORC_Easing easing,
                                     double progress,
                                     double *out_value);
AFORC_API AFORC_Status aforc_tween_init(AFORC_Tween *tween,
                                  double start,
                                  double end,
                                  uint64_t duration_ms,
                                  AFORC_Easing easing);
AFORC_API void aforc_tween_dispose(AFORC_Tween *tween);
AFORC_API AFORC_Status aforc_tween_restart(AFORC_Tween *tween);
AFORC_API AFORC_Status aforc_tween_set_paused(AFORC_Tween *tween, bool paused);
AFORC_API AFORC_Status aforc_tween_update(AFORC_Tween *tween, uint64_t delta_ms);
AFORC_API AFORC_Status aforc_tween_sample(const AFORC_Tween *tween,
                                    double *out_value);

typedef struct AFORC_FixedPoint {
    int32_t x;
    int32_t y;
} AFORC_FixedPoint;

typedef struct AFORC_ParticleI32Range {
    int32_t minimum;
    int32_t maximum;
} AFORC_ParticleI32Range;

typedef struct AFORC_ParticleU32Range {
    uint32_t minimum;
    uint32_t maximum;
} AFORC_ParticleU32Range;

typedef struct AFORC_ParticleDesc {
    AFORC_FixedPoint position;
    AFORC_FixedPoint velocity;
    AFORC_FixedPoint acceleration;
    uint32_t lifetime_ms;
    AFORC_Cell cell;
} AFORC_ParticleDesc;

typedef struct AFORC_ParticleEmitter {
    AFORC_ParticleI32Range x;
    AFORC_ParticleI32Range y;
    AFORC_ParticleI32Range velocity_x;
    AFORC_ParticleI32Range velocity_y;
    AFORC_ParticleI32Range acceleration_x;
    AFORC_ParticleI32Range acceleration_y;
    AFORC_ParticleU32Range lifetime_ms;
    const AFORC_Cell *cells;
    size_t cell_count;
} AFORC_ParticleEmitter;

typedef struct AFORC_Particle {
    AFORC_FixedPoint position;
    AFORC_FixedPoint velocity;
    AFORC_FixedPoint acceleration;
    uint32_t age_ms;
    uint32_t lifetime_ms;
    AFORC_Cell cell;
    bool active;
} AFORC_Particle;

typedef struct AFORC_ParticlePool {
    AFORC_Particle *particles;
    size_t capacity;
    size_t active_count;
    uint32_t random_state;
    bool initialized;
} AFORC_ParticlePool;

typedef struct AFORC_ParticleDrawOptions {
    AFORC_Point offset;
    AFORC_Rect clip;
    bool clip_enabled;
} AFORC_ParticleDrawOptions;

AFORC_API AFORC_ParticleDrawOptions aforc_particle_draw_options_default(void);
AFORC_API AFORC_Status aforc_particle_pool_init(AFORC_ParticlePool *pool,
                                           AFORC_Particle *storage,
                                           size_t capacity,
                                          uint32_t seed);
AFORC_API void aforc_particle_pool_dispose(AFORC_ParticlePool *pool);
AFORC_API AFORC_Status aforc_particle_pool_clear(AFORC_ParticlePool *pool);
AFORC_API AFORC_Status aforc_particle_pool_reseed(AFORC_ParticlePool *pool,
                                             uint32_t seed);
/* Output index/count pointers must not alias pool->capacity or pool->active_count.
 * Such aliases return AFORC_ERROR_INVALID_ARGUMENT without mutating the pool. */
AFORC_API AFORC_Status aforc_particle_pool_spawn(
    AFORC_ParticlePool *pool,
    const AFORC_ParticleDesc *description,
    size_t *out_particle_index);
AFORC_API AFORC_Status aforc_particle_pool_emit(
    AFORC_ParticlePool *pool,
    const AFORC_ParticleEmitter *emitter,
    size_t requested_count,
    size_t *out_spawned_count);
AFORC_API AFORC_Status aforc_particle_pool_kill(AFORC_ParticlePool *pool,
                                          size_t particle_index);
AFORC_API AFORC_Status aforc_particle_pool_update(AFORC_ParticlePool *pool,
                                            uint32_t delta_ms);
AFORC_API AFORC_Status aforc_particle_pool_draw(
    const AFORC_ParticlePool *pool,
    const AFORC_ParticleDrawOptions *options,
    AFORC_EffectPlotFn plot,
    void *context);

#ifdef __cplusplus
}
#endif

#endif
