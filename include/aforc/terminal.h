/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_TERMINAL_H
#define AFORC_TERMINAL_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AFORC_Terminal AFORC_Terminal;

typedef struct AFORC_TerminalConfig {
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

AFORC_API AFORC_TerminalConfig aforc_terminal_config_default(void);

AFORC_API AFORC_Status aforc_terminal_open(
    AFORC_Terminal **out_terminal,
    const AFORC_TerminalConfig *config
);

AFORC_API void aforc_terminal_close(AFORC_Terminal *terminal);
AFORC_API bool aforc_terminal_is_active(const AFORC_Terminal *terminal);
AFORC_API int aforc_terminal_input_fd(const AFORC_Terminal *terminal);
AFORC_API int aforc_terminal_output_fd(const AFORC_Terminal *terminal);

AFORC_API AFORC_Status aforc_terminal_dimensions(
    const AFORC_Terminal *terminal,
    AFORC_Size *out_size
);

AFORC_API AFORC_Status aforc_terminal_refresh_dimensions(
    AFORC_Terminal *terminal,
    bool *out_changed
);

AFORC_API AFORC_Status aforc_terminal_poll(
    AFORC_Terminal *terminal,
    int timeout_ms,
    bool *out_readable,
    bool *out_resized
);

AFORC_API AFORC_Status aforc_terminal_read(
    AFORC_Terminal *terminal,
    unsigned char *buffer,
    size_t capacity,
    size_t *out_count
);

AFORC_API AFORC_Status aforc_terminal_write(
    AFORC_Terminal *terminal,
    const void *data,
    size_t size
);

AFORC_API int aforc_terminal_pending_signal(const AFORC_Terminal *terminal);

#ifdef __cplusplus
}
#endif

#endif
