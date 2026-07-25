/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/ui.h"

#include "ui_internal.h"

/*
 * Clipped plotting adapter for renderer-independent UI code.
 *
 * Child canvases borrow the parent's callback/context and can only narrow its
 * half-open clip. Plot callbacks execute synchronously; the first downstream
 * error aborts a fill and is returned unchanged to the caller.
 */

static inline AFORC_Rect canvas_intersection(AFORC_Rect first, AFORC_Rect second) {
    const int64_t left = first.x > second.x ? first.x : second.x;
    const int64_t top = first.y > second.y ? first.y : second.y;
    int64_t right = (int64_t)first.x + first.width;
    int64_t bottom = (int64_t)first.y + first.height;
    const int64_t second_right = (int64_t)second.x + second.width;
    const int64_t second_bottom = (int64_t)second.y + second.height;

    if (second_right < right) {
        right = second_right;
    }
    if (second_bottom < bottom) {
        bottom = second_bottom;
    }
    /* Clip each half-open axis independently so an empty axis retains the
       orthogonal extent used by child canvases. */
    if (right < left) {
        right = left;
    }
    if (bottom < top) {
        bottom = top;
    }
    return (AFORC_Rect){(int32_t)left, (int32_t)top,
                      (int32_t)(right - left),
                      (int32_t)(bottom - top)};
}

static bool canvas_valid(const AFORC_UICanvas *canvas) {
    return canvas != NULL && canvas->plot != NULL &&
           aforc_ui_rect_valid(canvas->clip);
}

AFORC_Status aforc_ui_canvas_init(AFORC_UICanvas *canvas,
                              AFORC_Rect clip,
                              AFORC_UIPlotFn plot,
                              void *context) {
    if (canvas == NULL || plot == NULL || !aforc_ui_rect_valid(clip)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    canvas->clip = clip;
    canvas->plot = plot;
    canvas->context = context;
    return AFORC_OK;
}

AFORC_Status aforc_ui_canvas_child(const AFORC_UICanvas *parent,
                               AFORC_Rect clip,
                               AFORC_UICanvas *out_canvas) {
    if (!canvas_valid(parent) || out_canvas == NULL ||
        !aforc_ui_rect_valid(clip)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    out_canvas->clip = canvas_intersection(parent->clip, clip);
    out_canvas->plot = parent->plot;
    out_canvas->context = parent->context;
    return AFORC_OK;
}

AFORC_Status aforc_ui_canvas_plot(const AFORC_UICanvas *canvas,
                              AFORC_Point position,
                              AFORC_Cell cell) {
    if (!canvas_valid(canvas)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_rect_contains(canvas->clip, position)) {
        return AFORC_OK;
    }
    return canvas->plot(canvas->context, position, cell);
}

AFORC_Status aforc_ui_canvas_fill(const AFORC_UICanvas *canvas,
                              AFORC_Rect rect,
                              AFORC_Cell cell) {
    AFORC_Rect visible;

    if (!canvas_valid(canvas) || !aforc_ui_rect_valid(rect)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    visible = canvas_intersection(canvas->clip, rect);
    if (visible.width == 0 || visible.height == 0) {
        return AFORC_OK;
    }

    for (int64_t y = visible.y;
         y < (int64_t)visible.y + visible.height;
         ++y) {
        for (int64_t x = visible.x;
             x < (int64_t)visible.x + visible.width;
             ++x) {
            const AFORC_Status status = canvas->plot(
                canvas->context,
                (AFORC_Point){(int32_t)x, (int32_t)y},
                cell);

            if (status != AFORC_OK) {
                return status;
            }
        }
    }
    return AFORC_OK;
}
