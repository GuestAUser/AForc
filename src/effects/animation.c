/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

/*
 * Frame animation state machine.
 *
 * The animation borrows its frame array and every referenced sprite; callers
 * must keep them alive until disposal. Updates accept arbitrarily large time
 * steps while preserving ONCE, LOOP, and endpoint-reflecting PING_PONG order.
 */

static bool animation_mode_valid(AFORC_AnimationMode mode)
{
    return mode == AFORC_ANIMATION_ONCE || mode == AFORC_ANIMATION_LOOP ||
           mode == AFORC_ANIMATION_PING_PONG;
}

static bool animation_ready(const AFORC_Animation *animation)
{
    return animation != NULL && animation->initialized &&
           animation->frames != NULL && animation->frame_count > 0U &&
           animation->frame_index < animation->frame_count &&
           animation_mode_valid(animation->mode) &&
           animation->cycle_duration_ms > 0U &&
           (animation->direction == 1 || animation->direction == -1) &&
           animation->frames[animation->frame_index].duration_ms > 0U &&
           animation->frame_elapsed_ms <=
               animation->frames[animation->frame_index].duration_ms;
}

AFORC_Status aforc_animation_init(AFORC_Animation *animation,
                                  const AFORC_AnimationFrame *frames,
                                  size_t frame_count,
                                  AFORC_AnimationMode mode)
{
    uint64_t cycle_duration = 0U;

    if (animation == NULL || frames == NULL || frame_count == 0U ||
        !animation_mode_valid(mode))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    for (size_t index = 0U; index < frame_count; ++index)
    {
        const uint64_t duration = frames[index].duration_ms;

        if (frames[index].sprite == NULL ||
            !frames[index].sprite->initialized || duration == 0U)
        {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
        if (duration > UINT64_MAX - cycle_duration)
        {
            return AFORC_ERROR_OVERFLOW;
        }
        cycle_duration += duration;
    }
    if (mode == AFORC_ANIMATION_PING_PONG && frame_count > 2U)
    {
        /* A ping-pong cycle visits endpoints once and interior frames twice. */
        for (size_t index = 1U; index + 1U < frame_count; ++index)
        {
            const uint64_t duration = frames[index].duration_ms;

            if (duration > UINT64_MAX - cycle_duration)
            {
                return AFORC_ERROR_OVERFLOW;
            }
            cycle_duration += duration;
        }
    }

    animation->frames = frames;
    animation->frame_count = frame_count;
    animation->frame_index = 0U;
    animation->frame_elapsed_ms = 0U;
    animation->cycle_duration_ms = cycle_duration;
    animation->mode = mode;
    animation->direction = 1;
    animation->paused = false;
    animation->finished = false;
    animation->initialized = true;
    return AFORC_OK;
}

void aforc_animation_dispose(AFORC_Animation *animation)
{
    if (animation == NULL)
    {
        return;
    }
    animation->frames = NULL;
    animation->frame_count = 0U;
    animation->frame_index = 0U;
    animation->frame_elapsed_ms = 0U;
    animation->cycle_duration_ms = 0U;
    animation->mode = AFORC_ANIMATION_ONCE;
    animation->direction = 0;
    animation->paused = false;
    animation->finished = false;
    animation->initialized = false;
}

AFORC_Status aforc_animation_restart(AFORC_Animation *animation)
{
    if (animation == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!animation_ready(animation))
    {
        return AFORC_ERROR_STATE;
    }
    animation->frame_index = 0U;
    animation->frame_elapsed_ms = 0U;
    animation->direction = 1;
    animation->paused = false;
    animation->finished = false;
    return AFORC_OK;
}

AFORC_Status aforc_animation_set_paused(AFORC_Animation *animation, bool paused)
{
    if (animation == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!animation_ready(animation))
    {
        return AFORC_ERROR_STATE;
    }
    animation->paused = paused;
    return AFORC_OK;
}

AFORC_Status aforc_animation_update(AFORC_Animation *animation,
                                    uint64_t delta_ms)
{
    if (animation == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!animation_ready(animation))
    {
        return AFORC_ERROR_STATE;
    }
    if (animation->paused || animation->finished || delta_ms == 0U)
    {
        return AFORC_OK;
    }
    if (animation->mode != AFORC_ANIMATION_ONCE)
    {
        /* Bound the loop without changing the state after a complete cycle. */
        delta_ms %= animation->cycle_duration_ms;
    }

    while (delta_ms > 0U && !animation->finished)
    {
        const uint64_t duration =
            animation->frames[animation->frame_index].duration_ms;
        const uint64_t remaining = duration - animation->frame_elapsed_ms;

        if (delta_ms < remaining)
        {
            animation->frame_elapsed_ms += delta_ms;
            break;
        }
        delta_ms -= remaining;
        animation->frame_elapsed_ms = 0U;

        if (animation->mode == AFORC_ANIMATION_ONCE)
        {
            if (animation->frame_index + 1U < animation->frame_count)
            {
                ++animation->frame_index;
            }
            else
            {
                animation->frame_elapsed_ms = duration;
                animation->finished = true;
            }
        }
        else if (animation->mode == AFORC_ANIMATION_LOOP)
        {
            animation->frame_index =
                (animation->frame_index + 1U) % animation->frame_count;
        }
        else if (animation->frame_count == 1U)
        {
            animation->frame_index = 0U;
        }
        else if (animation->direction > 0)
        {
            /* Reverse on the endpoint itself so neither endpoint is doubled. */
            ++animation->frame_index;
            if (animation->frame_index + 1U == animation->frame_count)
            {
                animation->direction = -1;
            }
        }
        else
        {
            --animation->frame_index;
            if (animation->frame_index == 0U)
            {
                animation->direction = 1;
            }
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_animation_current(const AFORC_Animation *animation,
                                     const AFORC_AnimationFrame **out_frame)
{
    if (animation == NULL || out_frame == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!animation_ready(animation))
    {
        return AFORC_ERROR_STATE;
    }
    *out_frame = &animation->frames[animation->frame_index];
    return AFORC_OK;
}

AFORC_Status aforc_animation_draw(const AFORC_Animation *animation,
                                  const AFORC_SpriteDrawOptions *options,
                                  AFORC_EffectPlotFn plot,
                                  void *context)
{
    const AFORC_AnimationFrame *frame;
    AFORC_Status status = aforc_animation_current(animation, &frame);

    if (status != AFORC_OK)
    {
        return status;
    }
    return aforc_sprite_draw(frame->sprite, options, plot, context);
}
