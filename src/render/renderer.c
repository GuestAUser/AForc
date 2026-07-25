/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "renderer_internal.h"

#include <string.h>

/* Owns renderer allocation, buffer lifetime, resize, and presentation commit.
 * Drawing and ANSI encoding mutate private state but never advance front. */

AFORC_Color aforc_color_default(void)
{
    const AFORC_Color color = {AFORC_COLOR_DEFAULT, 0u, 0u, 0u};
    return color;
}

AFORC_Color aforc_color_indexed(uint8_t index)
{
    const AFORC_Color color = {AFORC_COLOR_INDEXED, index, 0u, 0u};
    return color;
}

AFORC_Color aforc_color_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    const AFORC_Color color = {AFORC_COLOR_RGB, red, green, blue};
    return color;
}

AFORC_Cell aforc_cell_default(void)
{
    AFORC_Cell cell;

    cell.codepoint = (uint32_t)' ';
    cell.foreground = aforc_color_default();
    cell.background = aforc_color_default();
    cell.style = AFORC_STYLE_NONE;
    return cell;
}

AFORC_RendererConfig aforc_renderer_config_default(void)
{
    AFORC_RendererConfig config;

    config.size.width = 80;
    config.size.height = 24;
    config.allocator = aforc_allocator_default();
    return config;
}

AFORC_Status aforc_renderer_create(AFORC_Renderer **out_renderer,
                               const AFORC_RendererConfig *config)
{
    AFORC_RendererConfig effective = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    const AFORC_Cell initial = aforc_cell_default();
    size_t count = 0u;
    AFORC_Status status;

    if (out_renderer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    if (config != NULL) {
        effective = *config;
    }
    if (!aforc_allocator_is_valid(&effective.allocator)) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_renderer_cell_count(effective.size, &count);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_alloc_array(&effective.allocator,
                             1u,
                             sizeof(*renderer),
                             (void **)&renderer);
    if (status != AFORC_OK) {
        return status;
    }
    (void)memset(renderer, 0, sizeof(*renderer));
    renderer->allocator = effective.allocator;
    status = aforc_alloc_array(&renderer->allocator,
                             count,
                             sizeof(*renderer->front),
                             (void **)&renderer->front);
    if (status == AFORC_OK) {
        status = aforc_alloc_array(&renderer->allocator,
                                 count,
                                 sizeof(*renderer->back),
                                 (void **)&renderer->back);
    }
    if (status != AFORC_OK) {
        aforc_renderer_destroy(renderer);
        return status;
    }
    renderer->size = effective.size;
    renderer->invalidated = true;
    aforc_renderer_fill_cells(renderer->front, count, initial);
    aforc_renderer_fill_cells(renderer->back, count, initial);
    *out_renderer = renderer;
    return AFORC_OK;
}

AFORC_Status aforc_renderer_create_for_terminal(
    AFORC_Renderer **out_renderer,
    AFORC_Terminal *terminal,
    const AFORC_Allocator *allocator)
{
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Status status;

    if (out_renderer == NULL || terminal == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    status = aforc_terminal_dimensions(terminal, &config.size);
    if (status != AFORC_OK) {
        return status;
    }
    if (allocator != NULL) {
        config.allocator = *allocator;
    }
    return aforc_renderer_create(out_renderer, &config);
}

void aforc_renderer_destroy(AFORC_Renderer *renderer)
{
    AFORC_Allocator allocator;

    if (renderer == NULL) {
        return;
    }
    allocator = renderer->allocator;
    aforc_free(&allocator, renderer->front);
    aforc_free(&allocator, renderer->back);
    aforc_free(&allocator, renderer->batch);
    (void)memset(renderer, 0, sizeof(*renderer));
    aforc_free(&allocator, renderer);
}

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
    /* Allocate both replacement buffers before publishing either pointer.
     * Failure leaves the renderer untouched; success preserves only caller
     * content from the old back buffer and invalidates front because terminal
     * coordinates and previously displayed cells can no longer be trusted. */
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
    AFORC_Status status = AFORC_OK;
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

AFORC_Size aforc_renderer_size(const AFORC_Renderer *renderer)
{
    const AFORC_Size empty = {0, 0};
    return renderer == NULL ? empty : renderer->size;
}

AFORC_Cell *aforc_renderer_back_buffer(AFORC_Renderer *renderer,
                                   size_t *out_stride)
{
    if (out_stride != NULL) {
        *out_stride = renderer == NULL ? 0u : (size_t)renderer->size.width;
    }
    return renderer == NULL ? NULL : renderer->back;
}

void aforc_renderer_invalidate(AFORC_Renderer *renderer)
{
    if (renderer != NULL) {
        renderer->invalidated = true;
    }
}

AFORC_Status aforc_renderer_present(AFORC_Renderer *renderer,
                                AFORC_Terminal *terminal)
{
    AFORC_Status status;
    size_t count = 0u;

    if (renderer == NULL || terminal == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_renderer_resize_to_terminal(renderer, terminal, NULL);
    if (status == AFORC_OK) {
        status = aforc_renderer_build_ansi(renderer);
    }
    if (status == AFORC_OK && renderer->batch_size > 0u) {
        status = aforc_terminal_write(terminal,
                                    renderer->batch,
                                    renderer->batch_size);
    }
    if (status != AFORC_OK) {
        return status;
    }
    /* Front advances only after the whole batch is written; any failure keeps
     * the previous frame intact so the same diff remains retryable. */
    status = aforc_renderer_cell_count(renderer->size, &count);
    if (status != AFORC_OK) {
        return status;
    }
    (void)memcpy(renderer->front,
                 renderer->back,
                 count * sizeof(*renderer->front));
    renderer->invalidated = false;
    return AFORC_OK;
}
