/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "terminal_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Owns complete writes, terminal-mode sequences, and size queries. It neither
 * owns handle lifetime nor mutates process-global signal registration. */

typedef enum AFORC_TerminalMode
{
    AFORC_TERMINAL_MODE_ALTERNATE_SCREEN,
    AFORC_TERMINAL_MODE_CURSOR,
    AFORC_TERMINAL_MODE_LINE_WRAP,
    AFORC_TERMINAL_MODE_MOUSE,
    AFORC_TERMINAL_MODE_FOCUS,
    AFORC_TERMINAL_MODE_PASTE,
    AFORC_TERMINAL_MODE_KEYBOARD
} AFORC_TerminalMode;

typedef struct AFORC_TerminalModeSequence
{
    AFORC_TerminalMode mode;
    const char *enable;
    const char *disable;
} AFORC_TerminalModeSequence;

static const AFORC_TerminalModeSequence aforc_terminal_mode_sequences[] = {
    {AFORC_TERMINAL_MODE_ALTERNATE_SCREEN,
     "\x1b[?1049h\x1b[H\x1b[2J",
     "\x1b[?1049l"},
    {AFORC_TERMINAL_MODE_CURSOR, "\x1b[?25l", "\x1b[?25h"},
    {AFORC_TERMINAL_MODE_LINE_WRAP, "\x1b[?7l", "\x1b[?7h"},
    {AFORC_TERMINAL_MODE_MOUSE,
     "\x1b[?1000h\x1b[?1002h\x1b[?1006h",
     "\x1b[?1006l\x1b[?1002l\x1b[?1000l"},
    {AFORC_TERMINAL_MODE_FOCUS, "\x1b[?1004h", "\x1b[?1004l"},
    {AFORC_TERMINAL_MODE_PASTE, "\x1b[?2004h", "\x1b[?2004l"},
    {AFORC_TERMINAL_MODE_KEYBOARD, "\x1b[>27u\x1b[?u\x1b[c", "\x1b[<u"}};

static size_t aforc_terminal_write_request(size_t remaining)
{
    return remaining > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : remaining;
}

AFORC_Status
aforc_terminal_write_best_effort(int fd, const void *data, size_t size)
{
    const unsigned char *cursor = (const unsigned char *)data;
    size_t remaining = size;
    const int original_flags = fcntl(fd, F_GETFL);
    bool flags_changed = false;
    AFORC_Status status = AFORC_OK;

    if (original_flags < 0)
    {
        return AFORC_ERROR_IO;
    }
    if ((original_flags & O_NONBLOCK) == 0)
    {
        if (fcntl(fd, F_SETFL, original_flags | O_NONBLOCK) < 0)
        {
            return AFORC_ERROR_IO;
        }
        flags_changed = true;
    }
    while (remaining > 0u)
    {
        const size_t request = aforc_terminal_write_request(remaining);
        const ssize_t written = write(fd, cursor, request);

        if (written > 0)
        {
            const size_t count = (size_t)written;
            cursor += count;
            remaining -= count;
            continue;
        }
        if (written < 0 && errno == EINTR &&
            aforc_terminal_process_signal() == 0)
        {
            continue;
        }
        status = AFORC_ERROR_IO;
        break;
    }
    if (flags_changed && fcntl(fd, F_SETFL, original_flags) < 0)
    {
        status = AFORC_ERROR_IO;
    }
    return status;
}

AFORC_Status aforc_terminal_write_fd(int fd, const void *data, size_t size)
{
    const unsigned char *cursor = (const unsigned char *)data;
    size_t remaining = size;

    /* A presentation is committed only after a complete batch write. Handle
     * short writes explicitly and wait for POLLOUT on nonblocking backpressure;
     * the signal bridge interrupts either path so restoration can run in normal
     * control flow rather than inside the asynchronous handler. */
    while (remaining > 0u)
    {
        const size_t request = aforc_terminal_write_request(remaining);
        ssize_t written;

        if (aforc_terminal_process_signal() != 0)
        {
            return AFORC_ERROR_INTERRUPTED;
        }
        written = write(fd, cursor, request);
        if (written > 0)
        {
            const size_t count = (size_t)written;
            cursor += count;
            remaining -= count;
            continue;
        }
        if (written < 0 && errno == EINTR &&
            aforc_terminal_process_signal() == 0)
        {
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            struct pollfd descriptor;
            int poll_result = 0;

            descriptor.fd = fd;
            descriptor.events = POLLOUT;
            descriptor.revents = 0;
            do
            {
                poll_result = poll(&descriptor, 1u, -1);
            } while (poll_result < 0 && errno == EINTR &&
                     aforc_terminal_process_signal() == 0);
            if (aforc_terminal_process_signal() != 0)
            {
                return AFORC_ERROR_INTERRUPTED;
            }
            if (poll_result > 0 && (descriptor.revents & POLLOUT) != 0)
            {
                continue;
            }
        }
        if (written == 0)
        {
            errno = EIO;
        }
        return AFORC_ERROR_IO;
    }
    return AFORC_OK;
}

static bool aforc_terminal_mode_selected(const AFORC_TerminalConfig *config,
                                         AFORC_TerminalMode mode)
{
    switch (mode)
    {
        case AFORC_TERMINAL_MODE_ALTERNATE_SCREEN:
            return config->alternate_screen;
        case AFORC_TERMINAL_MODE_CURSOR:
            return config->hide_cursor;
        case AFORC_TERMINAL_MODE_LINE_WRAP:
            return config->disable_line_wrap;
        case AFORC_TERMINAL_MODE_MOUSE:
            return config->mouse_events;
        case AFORC_TERMINAL_MODE_FOCUS:
            return config->focus_events;
        case AFORC_TERMINAL_MODE_PASTE:
            return config->bracketed_paste;
        case AFORC_TERMINAL_MODE_KEYBOARD:
            return config->enhanced_keyboard;
    }
    return false;
}

static AFORC_Status aforc_terminal_emit(AFORC_Terminal *terminal,
                                        const char *sequence)
{
    return aforc_terminal_write_fd(
        terminal->config.output_fd, sequence, strlen(sequence));
}

static void aforc_terminal_disable_sequence(AFORC_Terminal *terminal,
                                            const char *sequence,
                                            AFORC_Status *status)
{
    const AFORC_Status mode_status = aforc_terminal_write_best_effort(
        terminal->config.output_fd, sequence, strlen(sequence));

    if (*status == AFORC_OK)
    {
        *status = mode_status;
    }
}

AFORC_Status aforc_terminal_emit_modes(AFORC_Terminal *terminal, bool enable)
{
    const size_t count = sizeof(aforc_terminal_mode_sequences) /
                         sizeof(aforc_terminal_mode_sequences[0]);
    AFORC_Status status = AFORC_OK;
    size_t index;

    if (enable)
    {
        for (index = 0u; index < count && status == AFORC_OK; ++index)
        {
            const AFORC_TerminalModeSequence *sequence =
                &aforc_terminal_mode_sequences[index];

            if (aforc_terminal_mode_selected(&terminal->config, sequence->mode))
            {
                status = aforc_terminal_emit(terminal, sequence->enable);
            }
        }
        return status;
    }

    /* Disable in reverse dependency order, but reset attributes before
     * leaving the alternate screen so the restored screen is clean. */
    for (index = count; index > 1u; --index)
    {
        const AFORC_TerminalModeSequence *sequence =
            &aforc_terminal_mode_sequences[index - 1u];

        if (aforc_terminal_mode_selected(&terminal->config, sequence->mode))
        {
            aforc_terminal_disable_sequence(
                terminal, sequence->disable, &status);
        }
    }
    aforc_terminal_disable_sequence(terminal, "\x1b[0m", &status);
    if (aforc_terminal_mode_selected(&terminal->config,
                                     aforc_terminal_mode_sequences[0].mode))
    {
        aforc_terminal_disable_sequence(
            terminal, aforc_terminal_mode_sequences[0].disable, &status);
    }
    return status;
}

AFORC_Status aforc_terminal_query_dimensions(AFORC_Terminal *terminal,
                                             bool *out_changed)
{
    struct winsize dimensions;
    AFORC_Size size;

    (void)memset(&dimensions, 0, sizeof(dimensions));
    if (ioctl(terminal->config.output_fd, TIOCGWINSZ, &dimensions) < 0 &&
        ioctl(terminal->config.input_fd, TIOCGWINSZ, &dimensions) < 0)
    {
        return AFORC_ERROR_PLATFORM;
    }
    size.width = dimensions.ws_col == 0u ? 80 : (int32_t)dimensions.ws_col;
    size.height = dimensions.ws_row == 0u ? 24 : (int32_t)dimensions.ws_row;
    if (out_changed != NULL)
    {
        *out_changed = size.width != terminal->size.width ||
                       size.height != terminal->size.height;
    }
    terminal->size = size;
    return AFORC_OK;
}
