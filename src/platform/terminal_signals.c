/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L

#include "terminal_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Owns the single process-wide signal bridge. The handler records only
 * sig_atomic_t state and wakes normal code through the nonblocking self-pipe.
 */

static const int aforc_terminal_signals[AFORC_TERMINAL_SIGNAL_COUNT] = {
    SIGWINCH, SIGHUP, SIGINT, SIGQUIT, SIGTERM};

static volatile sig_atomic_t aforc_terminal_global_active = 0;
static volatile sig_atomic_t aforc_terminal_global_pipe = -1;
static volatile sig_atomic_t aforc_terminal_global_resize = 0;
static volatile sig_atomic_t aforc_terminal_global_signal = 0;
static int aforc_terminal_global_input_fd = -1;
static int aforc_terminal_global_output_fd = -1;
static int aforc_terminal_global_input_flags = 0;
static struct termios aforc_terminal_global_termios;
static bool aforc_terminal_atexit_registered = false;

/* Exit restoration uses a value snapshot instead of the heap-owned terminal,
 * which may already be unreachable during process teardown. */
static const char aforc_terminal_emergency_reset[] =
    "\x1b[0m"
    "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l"
    "\x1b[?1004l\x1b[?2004l\x1b[<u"
    "\x1b[?7h\x1b[?25h\x1b[?1049l";

static void aforc_terminal_signal_handler(int signal_number)
{
    const int saved_errno = errno;
    unsigned char marker = (unsigned char)'s';

    /* Restoration is not signal-safe; record the cause and wake poll so the
     * regular control path can restore termios and terminal modes. */
    if (signal_number == SIGWINCH)
    {
        aforc_terminal_global_resize = 1;
        marker = (unsigned char)'w';
    }
    else if (aforc_terminal_global_signal == 0)
    {
        aforc_terminal_global_signal = (sig_atomic_t)signal_number;
    }
    if (aforc_terminal_global_pipe >= 0)
    {
        const unsigned char byte = marker;
        ssize_t write_result;
        /* The nonblocking write may fail when the pipe is full. That is safe:
         * pending state lives in sig_atomic_t globals and any queued byte is
         * sufficient to wake the polling control path. */
        write_result = write((int)aforc_terminal_global_pipe, &byte, 1u);
        (void)write_result;
    }
    errno = saved_errno;
}

static void aforc_terminal_restore_at_exit(void)
{
    if (aforc_terminal_global_active == 0)
    {
        return;
    }
    if (aforc_terminal_global_input_fd >= 0)
    {
        (void)tcsetattr(aforc_terminal_global_input_fd,
                        TCSAFLUSH,
                        &aforc_terminal_global_termios);
        (void)fcntl(aforc_terminal_global_input_fd,
                    F_SETFL,
                    aforc_terminal_global_input_flags);
    }
    if (aforc_terminal_global_output_fd >= 0)
    {
        (void)aforc_terminal_write_best_effort(
            aforc_terminal_global_output_fd,
            aforc_terminal_emergency_reset,
            sizeof(aforc_terminal_emergency_reset) - 1u);
    }
    aforc_terminal_global_active = 0;
}

static void aforc_terminal_signal_set(sigset_t *set)
{
    size_t index;

    (void)sigemptyset(set);
    for (index = 0u; index < AFORC_TERMINAL_SIGNAL_COUNT; ++index)
    {
        (void)sigaddset(set, aforc_terminal_signals[index]);
    }
}

bool aforc_terminal_process_active(void)
{
    return aforc_terminal_global_active != 0;
}

int aforc_terminal_process_signal(void)
{
    return (int)aforc_terminal_global_signal;
}

bool aforc_terminal_take_resize(void)
{
    const bool pending = aforc_terminal_global_resize != 0;
    aforc_terminal_global_resize = 0;
    return pending;
}

void aforc_terminal_prepare_signal_state(int pipe_fd)
{
    aforc_terminal_global_pipe = (sig_atomic_t)pipe_fd;
    aforc_terminal_global_resize = 0;
    aforc_terminal_global_signal = 0;
}

void aforc_terminal_activate_process_state(const AFORC_Terminal *terminal)
{
    aforc_terminal_global_termios = terminal->original_termios;
    aforc_terminal_global_input_fd = terminal->config.input_fd;
    aforc_terminal_global_output_fd = terminal->config.output_fd;
    aforc_terminal_global_input_flags = terminal->original_input_flags;
    aforc_terminal_global_active = 1;
}

void aforc_terminal_clear_process_state(void)
{
    aforc_terminal_global_active = 0;
    aforc_terminal_global_pipe = -1;
    aforc_terminal_global_resize = 0;
    aforc_terminal_global_signal = 0;
    aforc_terminal_global_input_fd = -1;
    aforc_terminal_global_output_fd = -1;
    aforc_terminal_global_input_flags = 0;
}

int aforc_terminal_block_signals(sigset_t *previous)
{
    sigset_t set;

    aforc_terminal_signal_set(&set);
    return sigprocmask(SIG_BLOCK, &set, previous);
}

void aforc_terminal_unblock_signals(const sigset_t *previous)
{
    (void)sigprocmask(SIG_SETMASK, previous, NULL);
}

static AFORC_Status aforc_terminal_set_fd_flag(int fd, int command, int flag)
{
    const int current = fcntl(fd, command);
    const int set_command = command == F_GETFD ? F_SETFD : F_SETFL;

    if (current < 0 || fcntl(fd, set_command, current | flag) < 0)
    {
        return AFORC_ERROR_PLATFORM;
    }
    return AFORC_OK;
}

AFORC_Status aforc_terminal_open_signal_pipe(AFORC_Terminal *terminal)
{
    AFORC_Status status = AFORC_OK;

    if (pipe(terminal->signal_pipe) < 0)
    {
        return AFORC_ERROR_PLATFORM;
    }
    status = aforc_terminal_set_fd_flag(
        terminal->signal_pipe[0], F_GETFL, O_NONBLOCK);
    if (status == AFORC_OK)
    {
        status = aforc_terminal_set_fd_flag(
            terminal->signal_pipe[1], F_GETFL, O_NONBLOCK);
    }
    if (status == AFORC_OK)
    {
        status = aforc_terminal_set_fd_flag(
            terminal->signal_pipe[0], F_GETFD, FD_CLOEXEC);
    }
    if (status == AFORC_OK)
    {
        status = aforc_terminal_set_fd_flag(
            terminal->signal_pipe[1], F_GETFD, FD_CLOEXEC);
    }
    if (status != AFORC_OK)
    {
        const int saved_errno = errno;
        (void)close(terminal->signal_pipe[0]);
        (void)close(terminal->signal_pipe[1]);
        terminal->signal_pipe[0] = -1;
        terminal->signal_pipe[1] = -1;
        errno = saved_errno;
    }
    return status;
}

void aforc_terminal_close_signal_pipe(AFORC_Terminal *terminal)
{
    if (terminal->signal_pipe[0] >= 0)
    {
        (void)close(terminal->signal_pipe[0]);
        terminal->signal_pipe[0] = -1;
    }
    if (terminal->signal_pipe[1] >= 0)
    {
        (void)close(terminal->signal_pipe[1]);
        terminal->signal_pipe[1] = -1;
    }
}

void aforc_terminal_drain_signal_pipe(AFORC_Terminal *terminal)
{
    unsigned char bytes[64];

    for (;;)
    {
        const ssize_t count =
            read(terminal->signal_pipe[0], bytes, sizeof(bytes));
        if (count > 0)
        {
            continue;
        }
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        break;
    }
}

AFORC_Status aforc_terminal_install_actions(AFORC_Terminal *terminal)
{
    struct sigaction action;
    size_t index;

    (void)memset(&action, 0, sizeof(action));
    action.sa_handler = aforc_terminal_signal_handler;
    (void)sigemptyset(&action.sa_mask);
    for (index = 0u; index < AFORC_TERMINAL_SIGNAL_COUNT; ++index)
    {
        if (sigaction(aforc_terminal_signals[index],
                      &action,
                      &terminal->previous_actions[index]) < 0)
        {
            return AFORC_ERROR_PLATFORM;
        }
        terminal->action_installed[index] = true;
    }
    return AFORC_OK;
}

void aforc_terminal_restore_actions(AFORC_Terminal *terminal)
{
    size_t index = AFORC_TERMINAL_SIGNAL_COUNT;

    while (index > 0u)
    {
        --index;
        if (terminal->action_installed[index])
        {
            (void)sigaction(aforc_terminal_signals[index],
                            &terminal->previous_actions[index],
                            NULL);
            terminal->action_installed[index] = false;
        }
    }
}

bool aforc_terminal_register_exit_restore(void)
{
    if (!aforc_terminal_atexit_registered)
    {
        if (atexit(aforc_terminal_restore_at_exit) != 0)
        {
            return false;
        }
        aforc_terminal_atexit_registered = true;
    }
    return true;
}
