/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_PLATFORM_TERMINAL_INTERNAL_H
#define AFORC_PLATFORM_TERMINAL_INTERNAL_H

#include "../core/common_internal.h"
#include "aforc/terminal.h"

#include <signal.h>
#include <termios.h>

enum {
    AFORC_TERMINAL_SIGNAL_COUNT = 5
};

struct AFORC_Terminal {
    AFORC_TerminalConfig config;
    struct termios original_termios;
    int original_input_flags;
    int signal_pipe[2];
    struct sigaction previous_actions[AFORC_TERMINAL_SIGNAL_COUNT];
    bool action_installed[AFORC_TERMINAL_SIGNAL_COUNT];
    bool termios_changed;
    bool flags_changed;
    bool modes_enabled;
    bool active;
    bool restored;
    bool input_hangup;
    AFORC_Size size;
};

AFORC_INTERNAL AFORC_Status aforc_terminal_write_best_effort(
    int fd,
    const void *data,
    size_t size
);
AFORC_INTERNAL AFORC_Status aforc_terminal_write_fd(int fd,
                                               const void *data,
                                               size_t size);
AFORC_INTERNAL AFORC_Status aforc_terminal_emit_modes(AFORC_Terminal *terminal,
                                                 bool enable);
AFORC_INTERNAL AFORC_Status aforc_terminal_query_dimensions(
    AFORC_Terminal *terminal,
    bool *out_changed
);

AFORC_INTERNAL bool aforc_terminal_process_active(void);
AFORC_INTERNAL int aforc_terminal_process_signal(void);
AFORC_INTERNAL bool aforc_terminal_take_resize(void);
AFORC_INTERNAL void aforc_terminal_prepare_signal_state(int pipe_fd);
AFORC_INTERNAL void aforc_terminal_activate_process_state(
    const AFORC_Terminal *terminal
);
AFORC_INTERNAL void aforc_terminal_clear_process_state(void);
AFORC_INTERNAL int aforc_terminal_block_signals(sigset_t *previous);
AFORC_INTERNAL void aforc_terminal_unblock_signals(const sigset_t *previous);
AFORC_INTERNAL AFORC_Status aforc_terminal_open_signal_pipe(
    AFORC_Terminal *terminal
);
AFORC_INTERNAL void aforc_terminal_close_signal_pipe(AFORC_Terminal *terminal);
AFORC_INTERNAL void aforc_terminal_drain_signal_pipe(AFORC_Terminal *terminal);
AFORC_INTERNAL AFORC_Status aforc_terminal_install_actions(AFORC_Terminal *terminal);
AFORC_INTERNAL void aforc_terminal_restore_actions(AFORC_Terminal *terminal);
AFORC_INTERNAL bool aforc_terminal_register_exit_restore(void);

#endif
