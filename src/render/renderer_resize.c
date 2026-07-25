/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_internal.h"

#include <string.h>

AFORC_Status aforc_renderer_resize(AFORC_Renderer *renderer, AFORC_Size size)
{
    AFORC_Cell *front = NULL;
    AFORC_Cell *back = NULL;
    const AFORC_Cell initial = aforc_cell_default();
    size_t count = 0u;
    int32_t copy_width;
    int32_t copy_height;
    int32_t row;
    AFORC_Status status;

    if (renderer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (renderer->size.width == size.width &&
        renderer->size.height == size.height) {
        return AFORC_OK;
    }
    status = aforc_renderer_cell_count(size, &count);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_alloc_array(&renderer->allocator,
                               count,
                               sizeof(*front),
                               (void **)&front);
    if (status == AFORC_OK) {
        status = aforc_alloc_array(&renderer->allocator,
                                   count,
                                   sizeof(*back),
                                   (void **)&back);
    }
    if (status != AFORC_OK) {
        aforc_free(&renderer->allocator, front);
        aforc_free(&renderer->allocator, back);
        return status;
    }
    aforc_renderer_fill_cells(front, count, initial);
    aforc_renderer_fill_cells(back, count, initial);
    copy_width = renderer->size.width < size.width ? renderer->size.width
                                                   : size.width;
    copy_height = renderer->size.height < size.height ? renderer->size.height
                                                       : size.height;
    for (row = 0; row < copy_height; ++row) {
        const size_t old_offset =
            (size_t)row * (size_t)renderer->size.width;
        const size_t new_offset = (size_t)row * (size_t)size.width;

        (void)memcpy(back + new_offset,
                     renderer->back + old_offset,
                     (size_t)copy_width * sizeof(*back));
    }
    aforc_free(&renderer->allocator, renderer->front);
    aforc_free(&renderer->allocator, renderer->back);
    renderer->front = front;
    renderer->back = back;
    renderer->size = size;
    renderer->invalidated = true;
    return AFORC_OK;
}

AFORC_Status aforc_renderer_resize_to_terminal(AFORC_Renderer *renderer,
                                                AFORC_Terminal *terminal,
                                                bool *out_changed)
{
    AFORC_Size size;
    AFORC_Status status;
    bool changed;

    if (renderer == NULL || terminal == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (out_changed != NULL) {
        *out_changed = false;
    }
    status = aforc_terminal_dimensions(terminal, &size);
    if (status != AFORC_OK) {
        return status;
    }
    changed = size.width != renderer->size.width ||
              size.height != renderer->size.height;
    if (changed) {
        status = aforc_renderer_resize(renderer, size);
    }
    if (out_changed != NULL) {
        *out_changed = changed && status == AFORC_OK;
    }
    return status;
}
