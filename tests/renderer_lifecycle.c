/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#define _XOPEN_SOURCE 600

#include "../include/aforc/renderer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct AllocationTracker
{
    size_t calls;
    size_t fail_at;
    size_t live;
} AllocationTracker;

typedef struct PtyPair
{
    int master;
    int slave;
} PtyPair;

static bool pty_open(PtyPair *pair)
{
    char *slave_name;

    pair->master = posix_openpt(O_RDWR | O_NOCTTY);
    pair->slave = -1;
    if (pair->master < 0 || grantpt(pair->master) < 0 ||
        unlockpt(pair->master) < 0)
    {
        return false;
    }
    slave_name = ptsname(pair->master);
    if (slave_name == NULL)
    {
        return false;
    }
    pair->slave = open(slave_name, O_RDWR | O_NOCTTY);
    return pair->slave >= 0;
}

static void pty_close(PtyPair *pair)
{
    if (pair->master >= 0)
    {
        (void)close(pair->master);
        pair->master = -1;
    }
    if (pair->slave >= 0)
    {
        (void)close(pair->slave);
        pair->slave = -1;
    }
}

static bool pty_set_size(int fd, unsigned short columns, unsigned short rows)
{
    struct winsize size;

    (void)memset(&size, 0, sizeof(size));
    size.ws_col = columns;
    size.ws_row = rows;
    return ioctl(fd, TIOCSWINSZ, &size) == 0;
}

static AFORC_TerminalConfig terminal_config(int fd)
{
    AFORC_TerminalConfig config = aforc_terminal_config_default();

    config.input_fd = fd;
    config.output_fd = fd;
    config.alternate_screen = false;
    config.hide_cursor = false;
    config.disable_line_wrap = false;
    config.mouse_events = false;
    config.focus_events = false;
    config.bracketed_paste = false;
    config.enhanced_keyboard = false;
    return config;
}

static void *tracked_allocate(void *context, size_t size)
{
    AllocationTracker *tracker = context;
    void *memory;

    ++tracker->calls;
    if (tracker->calls == tracker->fail_at)
    {
        return NULL;
    }
    memory = malloc(size);
    if (memory != NULL)
    {
        ++tracker->live;
    }
    return memory;
}

static void *tracked_reallocate(void *context, void *memory, size_t size)
{
    AllocationTracker *tracker = context;

    ++tracker->calls;
    if (tracker->calls == tracker->fail_at)
    {
        return NULL;
    }
    return realloc(memory, size);
}

static void tracked_deallocate(void *context, void *memory)
{
    AllocationTracker *tracker = context;

    if (memory != NULL)
    {
        --tracker->live;
        free(memory);
    }
}

static AFORC_Allocator tracked_allocator(AllocationTracker *tracker)
{
    return (AFORC_Allocator){
        tracker, tracked_allocate, tracked_reallocate, tracked_deallocate};
}

static bool colors_equal(AFORC_Color left, AFORC_Color right)
{
    if (left.mode != right.mode)
    {
        return false;
    }
    if (left.mode == AFORC_COLOR_DEFAULT)
    {
        return true;
    }
    if (left.mode == AFORC_COLOR_INDEXED)
    {
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

static bool creation_limit_precedes_allocation(void)
{
    AllocationTracker tracker = {0U, 1U, 0U};
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = (AFORC_Renderer *)(uintptr_t)1U;

    config.size = (AFORC_Size){65535, 65535};
    config.allocator = tracked_allocator(&tracker);
    return aforc_renderer_create(&renderer, &config) == AFORC_ERROR_LIMIT &&
           renderer == NULL && tracker.calls == 0U && tracker.live == 0U;
}

static bool resize_is_transactional(void)
{
    AllocationTracker tracker = {0U, SIZE_MAX, 0U};
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell marked = aforc_cell_default();
    AFORC_Cell *back;
    AFORC_Size size;
    size_t calls;
    bool passed = false;

    config.size = (AFORC_Size){2, 2};
    config.allocator = (AFORC_Allocator){
        &tracker, tracked_allocate, tracked_reallocate, tracked_deallocate};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK ||
        tracker.live != 3U)
    {
        goto cleanup;
    }
    marked.codepoint = (uint32_t)'X';
    if (aforc_renderer_put(renderer, (AFORC_Point){1, 1}, marked) != AFORC_OK)
    {
        goto cleanup;
    }
    back = aforc_renderer_back_buffer(renderer, NULL);

    calls = tracker.calls;
    tracker.fail_at = calls + 1U;
    if (aforc_renderer_resize(renderer, (AFORC_Size){65535, 65535}) !=
            AFORC_ERROR_LIMIT ||
        tracker.calls != calls ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked))
    {
        goto cleanup;
    }
    size = aforc_renderer_size(renderer);
    if (size.width != 2 || size.height != 2)
    {
        goto cleanup;
    }

    tracker.fail_at = tracker.calls + 1U;
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) !=
            AFORC_ERROR_OUT_OF_MEMORY ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked))
    {
        goto cleanup;
    }

    tracker.fail_at = tracker.calls + 2U;
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) !=
            AFORC_ERROR_OUT_OF_MEMORY ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked))
    {
        goto cleanup;
    }

    tracker.fail_at = SIZE_MAX;
    calls = tracker.calls;
    if (aforc_renderer_resize(renderer, (AFORC_Size){2, 2}) != AFORC_OK ||
        tracker.calls != calls ||
        aforc_renderer_back_buffer(renderer, NULL) != back)
    {
        goto cleanup;
    }
    if (aforc_renderer_resize(renderer, (AFORC_Size){0, 2}) !=
            AFORC_ERROR_INVALID_ARGUMENT ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked))
    {
        goto cleanup;
    }
    if (aforc_renderer_resize(renderer, (AFORC_Size){3, 3}) != AFORC_OK ||
        aforc_renderer_back_buffer(renderer, NULL) == back ||
        tracker.live != 3U ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked) ||
        !cell_at_is(renderer, (AFORC_Point){2, 2}, aforc_cell_default()))
    {
        goto cleanup;
    }
    passed = true;

cleanup:
    aforc_renderer_destroy(renderer);
    return passed && tracker.live == 0U;
}

static bool terminal_limit_is_transactional(void)
{
    AllocationTracker tracker = {0U, 1U, 0U};
    AFORC_RendererConfig config = aforc_renderer_config_default();
    PtyPair pair = {-1, -1};
    AFORC_Terminal *terminal = NULL;
    AFORC_Renderer *renderer = (AFORC_Renderer *)(uintptr_t)1U;
    AFORC_Cell marked = aforc_cell_default();
    AFORC_Cell *back;
    AFORC_Size size;
    size_t calls;
    bool changed = true;
    bool passed = false;

    if (!pty_open(&pair) || !pty_set_size(pair.slave, 65535U, 65535U))
    {
        goto cleanup;
    }
    {
        AFORC_TerminalConfig terminal_options = terminal_config(pair.slave);

        if (aforc_terminal_open(&terminal, &terminal_options) != AFORC_OK)
        {
            goto cleanup;
        }
    }
    config.allocator = tracked_allocator(&tracker);
    if (aforc_renderer_create_for_terminal(
            &renderer, terminal, &config.allocator) != AFORC_ERROR_LIMIT ||
        renderer != NULL || tracker.calls != 0U)
    {
        goto cleanup;
    }
    tracker.fail_at = SIZE_MAX;
    if (!pty_set_size(pair.slave, 2U, 2U) ||
        aforc_terminal_refresh_dimensions(terminal, NULL) != AFORC_OK ||
        aforc_renderer_create_for_terminal(
            &renderer, terminal, &config.allocator) != AFORC_OK)
    {
        goto cleanup;
    }
    marked.codepoint = (uint32_t)'X';
    if (aforc_renderer_put(renderer, (AFORC_Point){1, 1}, marked) != AFORC_OK)
    {
        goto cleanup;
    }
    back = aforc_renderer_back_buffer(renderer, NULL);
    calls = tracker.calls;
    tracker.fail_at = calls + 1U;
    if (!pty_set_size(pair.slave, 65535U, 65535U) ||
        aforc_terminal_refresh_dimensions(terminal, NULL) != AFORC_OK ||
        aforc_renderer_resize_to_terminal(renderer, terminal, &changed) !=
            AFORC_ERROR_LIMIT ||
        changed || tracker.calls != calls ||
        aforc_renderer_back_buffer(renderer, NULL) != back ||
        !cell_at_is(renderer, (AFORC_Point){1, 1}, marked))
    {
        goto cleanup;
    }
    size = aforc_renderer_size(renderer);
    passed = size.width == 2 && size.height == 2;

cleanup:
    aforc_renderer_destroy(renderer);
    aforc_terminal_close(terminal);
    pty_close(&pair);
    return passed && tracker.live == 0U;
}

int main(void)
{
    if (!creation_limit_precedes_allocation())
    {
        (void)fputs("renderer creation limit regression failed\n", stderr);
        return 1;
    }
    if (!resize_is_transactional())
    {
        (void)fputs("renderer lifecycle regression failed\n", stderr);
        return 2;
    }
    if (!terminal_limit_is_transactional())
    {
        (void)fputs("renderer terminal limit regression failed\n", stderr);
        return 3;
    }
    (void)puts("renderer lifecycle: ok");
    return 0;
}
