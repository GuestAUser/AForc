/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TERMINAL_H
#define AFORC_TERMINAL_H

#include "common.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct AFORC_Terminal AFORC_Terminal;

typedef struct AFORC_TerminalConfig
{
    /* Borrowed descriptors. Close restores their state but never closes them.
     */
    int input_fd;
    int output_fd;
    bool alternate_screen;
    bool hide_cursor;
    bool disable_line_wrap;
    bool mouse_events;
    bool focus_events;
    bool bracketed_paste;
    bool enhanced_keyboard;
    AFORC_Allocator allocator;
} AFORC_TerminalConfig;

/* The POSIX backend owns process-global terminal modes and handlers for
 * SIGWINCH, SIGHUP, SIGINT, SIGQUIT, and SIGTERM. Open and close it on the main
 * thread; only one instance may be active per process. Do not combine it with
 * another raw-terminal or signal owner without coordination.
 *
 * Terminating signals are recorded rather than re-raised in the handler. Poll,
 * read, or write restores runtime terminal state and returns
 * AFORC_ERROR_INTERRUPTED. The application can inspect pending_signal, close
 * the handle to restore prior handlers, then decide whether to re-raise. An
 * atexit hook provides best-effort restoration if normal cleanup is skipped. */

AFORC_API AFORC_TerminalConfig aforc_terminal_config_default(void);

AFORC_API AFORC_Status aforc_terminal_open(AFORC_Terminal **out_terminal,
                                           const AFORC_TerminalConfig *config);

/* NULL is accepted. A non-NULL handle becomes invalid after this call. */
AFORC_API void aforc_terminal_close(AFORC_Terminal *terminal);
AFORC_API bool aforc_terminal_is_active(const AFORC_Terminal *terminal);
AFORC_API int aforc_terminal_input_fd(const AFORC_Terminal *terminal);
AFORC_API int aforc_terminal_output_fd(const AFORC_Terminal *terminal);

AFORC_API AFORC_Status aforc_terminal_dimensions(const AFORC_Terminal *terminal,
                                                 AFORC_Size *out_size);

AFORC_API AFORC_Status
aforc_terminal_refresh_dimensions(AFORC_Terminal *terminal, bool *out_changed);

/* timeout_ms uses poll semantics: -1 waits indefinitely, zero is nonblocking.
 * Optional poll outputs are reset to false before validation. */
AFORC_API AFORC_Status aforc_terminal_poll(AFORC_Terminal *terminal,
                                           int timeout_ms,
                                           bool *out_readable,
                                           bool *out_resized);

AFORC_API AFORC_Status aforc_terminal_read(AFORC_Terminal *terminal,
                                           unsigned char *buffer,
                                           size_t capacity,
                                           size_t *out_count);

AFORC_API AFORC_Status aforc_terminal_write(AFORC_Terminal *terminal,
                                            const void *data,
                                            size_t size);

AFORC_API int aforc_terminal_pending_signal(const AFORC_Terminal *terminal);

#ifdef __cplusplus
}
#endif

#endif
