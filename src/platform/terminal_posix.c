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
#include <unistd.h>

/* Owns per-handle lifecycle and the public poll/read/write facade. Mode I/O
 * and process-global signal state remain isolated in private modules. */

static void aforc_terminal_restore_runtime(AFORC_Terminal *terminal)
{
    if (terminal == NULL || terminal->restored)
    {
        return;
    }
    if (terminal->termios_changed)
    {
        (void)tcsetattr(
            terminal->config.input_fd, TCSAFLUSH, &terminal->original_termios);
        terminal->termios_changed = false;
    }
    if (terminal->flags_changed)
    {
        (void)fcntl(
            terminal->config.input_fd, F_SETFL, terminal->original_input_flags);
        terminal->flags_changed = false;
    }
    if (terminal->modes_enabled)
    {
        (void)aforc_terminal_emit_modes(terminal, false);
        terminal->modes_enabled = false;
    }
    terminal->restored = true;
}

static void aforc_terminal_rollback_open(AFORC_Terminal *terminal)
{
    /* Open is a staged transaction; unwind only stages already committed,
     * while managed signals remain blocked and cannot observe partial state. */
    if (terminal->termios_changed)
    {
        (void)tcsetattr(
            terminal->config.input_fd, TCSAFLUSH, &terminal->original_termios);
    }
    if (terminal->flags_changed)
    {
        (void)fcntl(
            terminal->config.input_fd, F_SETFL, terminal->original_input_flags);
    }
    if (terminal->modes_enabled || terminal->termios_changed)
    {
        (void)aforc_terminal_emit_modes(terminal, false);
    }
    aforc_terminal_restore_actions(terminal);
    aforc_terminal_close_signal_pipe(terminal);
    aforc_terminal_clear_process_state();
}

AFORC_TerminalConfig aforc_terminal_config_default(void)
{
    AFORC_TerminalConfig config;

    config.input_fd = STDIN_FILENO;
    config.output_fd = STDOUT_FILENO;
    config.alternate_screen = true;
    config.hide_cursor = true;
    config.disable_line_wrap = true;
    config.mouse_events = true;
    config.focus_events = true;
    config.bracketed_paste = true;
    config.enhanced_keyboard = true;
    config.allocator = aforc_allocator_default();
    return config;
}

AFORC_Status aforc_terminal_open(AFORC_Terminal **out_terminal,
                                 const AFORC_TerminalConfig *config)
{
    AFORC_TerminalConfig effective_config = aforc_terminal_config_default();
    AFORC_Terminal *terminal = NULL;
    AFORC_Status status = AFORC_OK;
    struct termios raw;
    sigset_t previous_mask;
    bool signals_blocked = false;
    int saved_errno = 0;

    if (out_terminal == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *out_terminal = NULL;
    if (config != NULL)
    {
        effective_config = *config;
    }
    if (effective_config.input_fd < 0 || effective_config.output_fd < 0 ||
        !aforc_allocator_is_valid(&effective_config.allocator))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (aforc_terminal_process_active())
    {
        return AFORC_ERROR_EXISTS;
    }
    if (isatty(effective_config.input_fd) == 0 ||
        isatty(effective_config.output_fd) == 0)
    {
        return AFORC_ERROR_UNSUPPORTED;
    }

    status = aforc_alloc_array(
        &effective_config.allocator, 1u, sizeof(*terminal), (void **)&terminal);
    if (status != AFORC_OK)
    {
        return status;
    }
    (void)memset(terminal, 0, sizeof(*terminal));
    terminal->config = effective_config;
    terminal->signal_pipe[0] = -1;
    terminal->signal_pipe[1] = -1;
    terminal->original_input_flags = -1;

    /* Managed signals remain blocked until handler state and restoration
     * snapshots are published as one process-wide transaction. */
    if (aforc_terminal_block_signals(&previous_mask) < 0)
    {
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }
    signals_blocked = true;
    if (aforc_terminal_process_active())
    {
        status = AFORC_ERROR_EXISTS;
        goto fail;
    }
    if (tcgetattr(terminal->config.input_fd, &terminal->original_termios) < 0)
    {
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }
    terminal->original_input_flags = fcntl(terminal->config.input_fd, F_GETFL);
    if (terminal->original_input_flags < 0)
    {
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }
    status = aforc_terminal_open_signal_pipe(terminal);
    if (status != AFORC_OK)
    {
        saved_errno = errno;
        goto fail;
    }
    status = aforc_terminal_query_dimensions(terminal, NULL);
    if (status != AFORC_OK)
    {
        saved_errno = errno;
        goto fail;
    }

    raw = terminal->original_termios;
    raw.c_iflag &= (tcflag_t) ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP |
                                INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= (tcflag_t)~OPOST;
    raw.c_cflag &= (tcflag_t) ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_lflag &= (tcflag_t) ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    aforc_terminal_prepare_signal_state(terminal->signal_pipe[1]);
    status = aforc_terminal_install_actions(terminal);
    if (status != AFORC_OK)
    {
        saved_errno = errno;
        goto fail;
    }
    if (tcsetattr(terminal->config.input_fd, TCSAFLUSH, &raw) < 0)
    {
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }
    terminal->termios_changed = true;
    if (fcntl(terminal->config.input_fd,
              F_SETFL,
              terminal->original_input_flags | O_NONBLOCK) < 0)
    {
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }
    terminal->flags_changed = true;
    status = aforc_terminal_emit_modes(terminal, true);
    if (status != AFORC_OK)
    {
        saved_errno = errno;
        goto fail;
    }
    terminal->modes_enabled = true;

    if (!aforc_terminal_register_exit_restore())
    {
        errno = ENOMEM;
        saved_errno = errno;
        status = AFORC_ERROR_PLATFORM;
        goto fail;
    }

    aforc_terminal_activate_process_state(terminal);
    terminal->active = true;
    aforc_terminal_unblock_signals(&previous_mask);
    *out_terminal = terminal;
    return AFORC_OK;

fail:
    aforc_terminal_rollback_open(terminal);
    if (signals_blocked)
    {
        aforc_terminal_unblock_signals(&previous_mask);
    }
    aforc_free(&terminal->config.allocator, terminal);
    if (saved_errno != 0)
    {
        errno = saved_errno;
    }
    return status;
}

void aforc_terminal_close(AFORC_Terminal *terminal)
{
    AFORC_Allocator allocator;
    sigset_t previous_mask;
    bool signals_blocked;

    if (terminal == NULL)
    {
        return;
    }
    allocator = terminal->config.allocator;
    signals_blocked = aforc_terminal_block_signals(&previous_mask) == 0;
    /* Restore user-visible terminal state before removing the handler bridge
     * that interrupts blocking I/O and triggers emergency restoration. */
    aforc_terminal_restore_runtime(terminal);
    aforc_terminal_restore_actions(terminal);
    aforc_terminal_clear_process_state();
    aforc_terminal_close_signal_pipe(terminal);
    terminal->active = false;
    if (signals_blocked)
    {
        aforc_terminal_unblock_signals(&previous_mask);
    }
    (void)memset(terminal, 0, sizeof(*terminal));
    aforc_free(&allocator, terminal);
}

bool aforc_terminal_is_active(const AFORC_Terminal *terminal)
{
    return terminal != NULL && terminal->active && !terminal->restored;
}

int aforc_terminal_input_fd(const AFORC_Terminal *terminal)
{
    return terminal == NULL ? -1 : terminal->config.input_fd;
}

int aforc_terminal_output_fd(const AFORC_Terminal *terminal)
{
    return terminal == NULL ? -1 : terminal->config.output_fd;
}

AFORC_Status aforc_terminal_dimensions(const AFORC_Terminal *terminal,
                                       AFORC_Size *out_size)
{
    if (terminal == NULL || out_size == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!terminal->active || terminal->restored)
    {
        return AFORC_ERROR_STATE;
    }
    *out_size = terminal->size;
    return AFORC_OK;
}

AFORC_Status aforc_terminal_refresh_dimensions(AFORC_Terminal *terminal,
                                               bool *out_changed)
{
    if (terminal == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!terminal->active || terminal->restored)
    {
        return AFORC_ERROR_STATE;
    }
    return aforc_terminal_query_dimensions(terminal, out_changed);
}

AFORC_Status aforc_terminal_poll(AFORC_Terminal *terminal,
                                 int timeout_ms,
                                 bool *out_readable,
                                 bool *out_resized)
{
    struct pollfd descriptors[2];
    int poll_result = 0;
    bool resize_pending;

    if (out_readable != NULL)
    {
        *out_readable = false;
    }
    if (out_resized != NULL)
    {
        *out_resized = false;
    }
    if (terminal == NULL || timeout_ms < -1)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!terminal->active || terminal->restored)
    {
        return AFORC_ERROR_STATE;
    }
    if (aforc_terminal_process_signal() != 0)
    {
        aforc_terminal_restore_runtime(terminal);
        return AFORC_ERROR_INTERRUPTED;
    }

    /* Poll terminal input and the signal self-pipe together. The pipe is only
     * a wakeup channel; sig_atomic_t globals remain authoritative if a burst
     * fills it. Resize state is consumed while managed signals are blocked so
     * a concurrent SIGWINCH cannot be cleared between observation and reset. */
    descriptors[0].fd = terminal->config.input_fd;
    descriptors[0].events = POLLIN;
    descriptors[0].revents = 0;
    descriptors[1].fd = terminal->signal_pipe[0];
    descriptors[1].events = POLLIN;
    descriptors[1].revents = 0;
    do
    {
        poll_result = poll(descriptors, 2u, timeout_ms);
    } while (poll_result < 0 && errno == EINTR &&
             aforc_terminal_process_signal() == 0);

    if (aforc_terminal_process_signal() != 0)
    {
        aforc_terminal_restore_runtime(terminal);
        return AFORC_ERROR_INTERRUPTED;
    }
    if (poll_result < 0)
    {
        return AFORC_ERROR_IO;
    }
    {
        sigset_t previous_mask;
        const bool signals_blocked =
            aforc_terminal_block_signals(&previous_mask) == 0;

        if ((descriptors[1].revents & POLLIN) != 0)
        {
            aforc_terminal_drain_signal_pipe(terminal);
        }
        resize_pending = aforc_terminal_take_resize();
        if (signals_blocked)
        {
            aforc_terminal_unblock_signals(&previous_mask);
        }
    }
    if (resize_pending)
    {
        bool changed = false;
        const AFORC_Status status =
            aforc_terminal_query_dimensions(terminal, &changed);

        if (status != AFORC_OK)
        {
            return status;
        }
        if (out_resized != NULL)
        {
            *out_resized = changed;
        }
    }
    if ((descriptors[0].revents & POLLHUP) != 0)
    {
        terminal->input_hangup = true;
    }
    if ((descriptors[0].revents & (POLLIN | POLLHUP)) != 0 &&
        out_readable != NULL)
    {
        *out_readable = true;
    }
    if ((descriptors[0].revents & POLLNVAL) != 0 ||
        ((descriptors[0].revents & POLLERR) != 0 &&
         (descriptors[0].revents & POLLHUP) == 0) ||
        (descriptors[1].revents & (POLLERR | POLLNVAL)) != 0)
    {
        errno = EIO;
        return AFORC_ERROR_IO;
    }
    return AFORC_OK;
}

AFORC_Status aforc_terminal_read(AFORC_Terminal *terminal,
                                 unsigned char *buffer,
                                 size_t capacity,
                                 size_t *out_count)
{
    ssize_t count;
    const size_t request =
        capacity > (size_t)SSIZE_MAX ? (size_t)SSIZE_MAX : capacity;

    if (out_count != NULL)
    {
        *out_count = 0u;
    }
    if (terminal == NULL || buffer == NULL || capacity == 0u ||
        out_count == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!terminal->active || terminal->restored)
    {
        return AFORC_ERROR_STATE;
    }
    do
    {
        count = read(terminal->config.input_fd, buffer, request);
    } while (count < 0 && errno == EINTR &&
             aforc_terminal_process_signal() == 0);
    if (aforc_terminal_process_signal() != 0)
    {
        aforc_terminal_restore_runtime(terminal);
        return AFORC_ERROR_INTERRUPTED;
    }
    if (count > 0)
    {
        *out_count = (size_t)count;
        return AFORC_OK;
    }
    if (count == 0)
    {
        return terminal->input_hangup ? AFORC_ERROR_END_OF_STREAM : AFORC_OK;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return AFORC_OK;
    }
    return AFORC_ERROR_IO;
}

AFORC_Status
aforc_terminal_write(AFORC_Terminal *terminal, const void *data, size_t size)
{
    if (terminal == NULL || (data == NULL && size != 0u))
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!terminal->active || terminal->restored)
    {
        return AFORC_ERROR_STATE;
    }
    if (aforc_terminal_process_signal() != 0)
    {
        aforc_terminal_restore_runtime(terminal);
        return AFORC_ERROR_INTERRUPTED;
    }
    {
        const AFORC_Status status =
            aforc_terminal_write_fd(terminal->config.output_fd, data, size);

        if (status == AFORC_ERROR_INTERRUPTED)
        {
            aforc_terminal_restore_runtime(terminal);
        }
        return status;
    }
}

int aforc_terminal_pending_signal(const AFORC_Terminal *terminal)
{
    if (terminal == NULL || !terminal->active)
    {
        return 0;
    }
    return aforc_terminal_process_signal();
}
