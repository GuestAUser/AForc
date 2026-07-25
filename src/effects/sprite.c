/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/effects.h"

#include "effects_internal.h"

#include <limits.h>

/*
 * Allocation-free sprite projection.
 *
 * Sprites borrow immutable, strided cell storage. Drawing clips in destination
 * space and maps each visible destination cell back to its source, avoiding a
 * transformed scratch buffer and keeping plot order deterministic.
 */

static bool sprite_rotation_valid(AFORC_SpriteRotation rotation) {
    return rotation == AFORC_SPRITE_ROTATION_0 ||
           rotation == AFORC_SPRITE_ROTATION_90 ||
           rotation == AFORC_SPRITE_ROTATION_180 ||
           rotation == AFORC_SPRITE_ROTATION_270;
}

static int64_t maximum_i64(int64_t left, int64_t right) {
    return left > right ? left : right;
}

static int64_t minimum_i64(int64_t left, int64_t right) {
    return left < right ? left : right;
}

AFORC_SpriteDrawOptions aforc_sprite_draw_options_default(void) {
    AFORC_SpriteDrawOptions options;

    options.transform.position = (AFORC_Point){0, 0};
    options.transform.scale_x = 1U;
    options.transform.scale_y = 1U;
    options.transform.rotation = AFORC_SPRITE_ROTATION_0;
    options.transform.flip_x = false;
    options.transform.flip_y = false;
    options.clip = (AFORC_Rect){0, 0, 0, 0};
    options.clip_enabled = false;
    options.transparency_enabled = false;
    options.transparent_codepoint = 0U;
    return options;
}

AFORC_Status aforc_sprite_init(AFORC_Sprite *sprite,
                           const AFORC_Cell *cells,
                           AFORC_Size size,
                           size_t stride) {
    size_t last_row;

    if (sprite == NULL || cells == NULL || size.width <= 0 ||
        size.height <= 0 || stride < (size_t)size.width) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (size.height > 1 &&
        stride > SIZE_MAX / ((size_t)size.height - 1U)) {
        return AFORC_ERROR_OVERFLOW;
    }
    last_row = ((size_t)size.height - 1U) * stride;
    if ((size_t)size.width - 1U > SIZE_MAX - last_row) {
        return AFORC_ERROR_OVERFLOW;
    }

    sprite->cells = cells;
    sprite->stride = stride;
    sprite->size = size;
    sprite->initialized = true;
    return AFORC_OK;
}

void aforc_sprite_dispose(AFORC_Sprite *sprite) {
    if (sprite == NULL) {
        return;
    }
    sprite->cells = NULL;
    sprite->stride = 0U;
    sprite->size = (AFORC_Size){0, 0};
    sprite->initialized = false;
}

AFORC_Status aforc_sprite_draw(const AFORC_Sprite *sprite,
                           const AFORC_SpriteDrawOptions *options,
                           AFORC_EffectPlotFn plot,
                           void *context) {
    int64_t unrotated_width;
    int64_t unrotated_height;
    int64_t rendered_width;
    int64_t rendered_height;
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;

    if (sprite == NULL || options == NULL || plot == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!sprite->initialized || sprite->cells == NULL ||
        sprite->size.width <= 0 || sprite->size.height <= 0 ||
        sprite->stride < (size_t)sprite->size.width) {
        return AFORC_ERROR_STATE;
    }
    if (options->transform.scale_x == 0U ||
        options->transform.scale_y == 0U ||
        !sprite_rotation_valid(options->transform.rotation) ||
        (options->clip_enabled && !aforc_effect_clip_valid(options->clip))) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    unrotated_width = (int64_t)sprite->size.width *
                      (int64_t)options->transform.scale_x;
    unrotated_height = (int64_t)sprite->size.height *
                       (int64_t)options->transform.scale_y;
    if (options->transform.rotation == AFORC_SPRITE_ROTATION_90 ||
        options->transform.rotation == AFORC_SPRITE_ROTATION_270) {
        rendered_width = unrotated_height;
        rendered_height = unrotated_width;
    } else {
        rendered_width = unrotated_width;
        rendered_height = unrotated_height;
    }

    left = options->transform.position.x;
    top = options->transform.position.y;
    right = left + rendered_width;
    bottom = top + rendered_height;

    left = maximum_i64(left, INT32_MIN);
    top = maximum_i64(top, INT32_MIN);
    right = minimum_i64(right, (int64_t)INT32_MAX + 1);
    bottom = minimum_i64(bottom, (int64_t)INT32_MAX + 1);
    if (options->clip_enabled) {
        const int64_t clip_right = (int64_t)options->clip.x +
                                   options->clip.width;
        const int64_t clip_bottom = (int64_t)options->clip.y +
                                    options->clip.height;

        left = maximum_i64(left, options->clip.x);
        top = maximum_i64(top, options->clip.y);
        right = minimum_i64(right, clip_right);
        bottom = minimum_i64(bottom, clip_bottom);
    }
    if (left >= right || top >= bottom) {
        return AFORC_OK;
    }

    /* Iterate in destination plotting order, then invert rotation, scale,
       and flips to recover the source cell without intermediate storage. */
    for (int64_t y = top; y < bottom; ++y) {
        for (int64_t x = left; x < right; ++x) {
            const int64_t rotated_x = x - options->transform.position.x;
            const int64_t rotated_y = y - options->transform.position.y;
            int64_t unrotated_x;
            int64_t unrotated_y;
            uint32_t source_x;
            uint32_t source_y;
            AFORC_Cell cell;
            AFORC_Status status;

            switch (options->transform.rotation) {
            case AFORC_SPRITE_ROTATION_0:
                unrotated_x = rotated_x;
                unrotated_y = rotated_y;
                break;
            case AFORC_SPRITE_ROTATION_90:
                unrotated_x = rotated_y;
                unrotated_y = unrotated_height - 1 - rotated_x;
                break;
            case AFORC_SPRITE_ROTATION_180:
                unrotated_x = unrotated_width - 1 - rotated_x;
                unrotated_y = unrotated_height - 1 - rotated_y;
                break;
            case AFORC_SPRITE_ROTATION_270:
                unrotated_x = unrotated_width - 1 - rotated_y;
                unrotated_y = rotated_x;
                break;
            default:
                return AFORC_ERROR_INVALID_ARGUMENT;
            }

            source_x = (uint32_t)(unrotated_x /
                                  (int64_t)options->transform.scale_x);
            source_y = (uint32_t)(unrotated_y /
                                  (int64_t)options->transform.scale_y);
            if (options->transform.flip_x) {
                source_x = (uint32_t)sprite->size.width - 1U - source_x;
            }
            if (options->transform.flip_y) {
                source_y = (uint32_t)sprite->size.height - 1U - source_y;
            }
            cell = sprite->cells[(size_t)source_y * sprite->stride +
                                 (size_t)source_x];
            if (options->transparency_enabled &&
                cell.codepoint == options->transparent_codepoint) {
                continue;
            }
            status = plot(context, (AFORC_Point){(int32_t)x, (int32_t)y}, cell);
            if (status != AFORC_OK) {
                return status;
            }
        }
    }
    return AFORC_OK;
}
