/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/renderer.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct AllocationTracker {
    size_t calls;
    size_t fail_at;
    size_t live;
} AllocationTracker;

static void *tracked_allocate(void *context, size_t size)
{
    AllocationTracker *tracker = context;
    void *memory;

    ++tracker->calls;
    if (tracker->calls == tracker->fail_at) {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL) {
        ++tracker->live;
    }
    return memory;
}

static void *tracked_reallocate(void *context, void *memory, size_t size)
{
    AllocationTracker *tracker = context;

    ++tracker->calls;
    if (tracker->calls == tracker->fail_at) {
        return NULL;
    }
    return realloc(memory, size);
}

static void tracked_deallocate(void *context, void *memory)
{
    AllocationTracker *tracker = context;

    if (memory != NULL) {
        --tracker->live;
        free(memory);
    }
}

static bool colors_equal(AFORC_Color left, AFORC_Color right)
{
    if (left.mode != right.mode) {
        return false;
    }
    if (left.mode == AFORC_COLOR_DEFAULT) {
        return true;
    }
    if (left.mode == AFORC_COLOR_INDEXED) {
        return left.red == right.red;
    }
    return left.red == right.red && left.green == right.green &&
           left.blue == right.blue;
}

static bool cells_equal(AFORC_Cell left, AFORC_Cell right)
{
    return left.codepoint == right.codepoint && left.style == right.style &&
           colors_equal(left.foreground, right.foreground) &&
           colors_equal(left.background, right.background);
}

static bool cell_at_is(const AFORC_Renderer *renderer,
                       AFORC_Point position,
                       AFORC_Cell expected)
{
    AFORC_Cell actual;

    return aforc_renderer_get(renderer, position, &actual) == AFORC_OK &&
           cells_equal(actual, expected);
}

static bool resize_is_transactional(void)
{
    AllocationTracker tracker = {0U, SIZE_MAX, 0U};
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell marked = aforc_cell_default();
    AFORC_Cell *back;
    size_t calls;
    bool passed = false;

    config.size = (AFORC_Size){2, 2};
    config.allocator = (AFORC_Allocator){
        &tracker, tracked_allocate, tracked_reallocate, tracked_deallocate};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK ||
        tracker.live != 3U) {
        goto cleanup;
    }
    marked.codepoint = (uint32_t)'X';
    if (aforc_renderer_put(renderer, (AFORC_Point){1, 1}, marked) != AFORC_OK) {
        goto cleanup;
    }
    back = aforc_renderer_back_buffer(renderer, NULL);

    tracker.fail_at = tracker.calls + 1U;
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) !=
            AFORC_ERROR_OUT_OF_MEMORY ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked)) {
        goto cleanup;
    }

    tracker.fail_at = tracker.calls + 2U;
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) !=
            AFORC_ERROR_OUT_OF_MEMORY ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked)) {
        goto cleanup;
    }

    tracker.fail_at = SIZE_MAX;
    calls = tracker.calls;
    if (aforc_renderer_resize(renderer, (AFORC_Size){2, 2}) != AFORC_OK ||
        tracker.calls != calls ||
        aforc_renderer_back_buffer(renderer, NULL) != back) {
        goto cleanup;
    }
    if (aforc_renderer_resize(renderer, (AFORC_Size){0, 2}) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked)) {
        goto cleanup;
    }
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) != AFORC_OK ||
        aforc_renderer_back_buffer(renderer, NULL) == back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked) ||
        !cell_at_is(renderer, (AFORC_Point){2, 2}, aforc_cell_default())) {
        goto cleanup;
    }
    passed = true;

cleanup:
    aforc_renderer_destroy(renderer);
    return passed && tracker.live == 0U;
}

int main(void)
{
    if (!resize_is_transactional()) {
        (void)fputs("renderer lifecycle regression failed\n", stderr);
        return 1;
    }
    (void)puts("renderer lifecycle: ok");
    return 0;
}
