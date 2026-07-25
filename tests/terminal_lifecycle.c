/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#define _XOPEN_SOURCE 600

#include "aforc/terminal.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

typedef struct PtyPair {
    int master;
    int slave;
} PtyPair;

static bool pty_open(PtyPair *pair)
{
    char *slave_name;

    pair->master = posix_openpt(O_RDWR | O_NOCTTY);
    pair->slave = -1;
    if (pair->master < 0 || grantpt(pair->master) < 0 ||
        unlockpt(pair->master) < 0) {
        return false;
    }
    slave_name = ptsname(pair->master);
    if (slave_name == NULL) {
        return false;
    }
    pair->slave = open(slave_name, O_RDWR | O_NOCTTY);
    return pair->slave >= 0;
}

static void pty_close(PtyPair *pair)
{
    if (pair->master >= 0) {
        (void)close(pair->master);
        pair->master = -1;
    }
    if (pair->slave >= 0) {
        (void)close(pair->slave);
        pair->slave = -1;
    }
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

static bool contract_check(bool condition, const char *label)
{
    if (!condition) {
        (void)fprintf(stderr, "terminal contract check failed: %s\n", label);
    }
    return condition;
}

static bool termios_equal(const struct termios *left,
                          const struct termios *right)
{
    return left->c_iflag == right->c_iflag &&
           left->c_oflag == right->c_oflag &&
           left->c_cflag == right->c_cflag &&
           left->c_lflag == right->c_lflag &&
           memcmp(left->c_cc, right->c_cc, sizeof(left->c_cc)) == 0;
}

static bool set_size(int fd, unsigned short columns, unsigned short rows)
{
    struct winsize size;

    (void)memset(&size, 0, sizeof(size));
    size.ws_col = columns;
    size.ws_row = rows;
    return ioctl(fd, TIOCSWINSZ, &size) == 0;
}

static bool test_open_close_restores_and_borrows_fds(void)
{
    PtyPair pair;
    struct termios before;
    struct termios raw;
    struct termios after;
    AFORC_Terminal *terminal = NULL;
    AFORC_Terminal *duplicate = NULL;
    AFORC_TerminalConfig config;
    AFORC_Status open_status;
    int flags_before;
    bool passed;

    if (!pty_open(&pair)) {
        (void)fprintf(stderr, "terminal contract setup failed: open PTY\n");
        pty_close(&pair);
        return false;
    }
    config = terminal_config(pair.slave);
    flags_before = fcntl(pair.slave, F_GETFL);
    if (flags_before < 0) {
        (void)fprintf(stderr, "terminal contract setup failed: read flags\n");
        pty_close(&pair);
        return false;
    }
    if (tcgetattr(pair.slave, &before) < 0) {
        (void)fprintf(stderr, "terminal contract setup failed: read termios\n");
        pty_close(&pair);
        return false;
    }
    open_status = aforc_terminal_open(&terminal, &config);
    if (open_status != AFORC_OK || tcgetattr(pair.slave, &raw) < 0) {
        (void)fprintf(stderr,
                      "terminal contract setup failed: engine open status %d\n",
                      (int)open_status);
        aforc_terminal_close(terminal);
        pty_close(&pair);
        return false;
    }
    passed = contract_check(aforc_terminal_is_active(terminal), "active") &&
              contract_check(aforc_terminal_input_fd(terminal) == pair.slave,
                             "input descriptor") &&
              contract_check(aforc_terminal_output_fd(terminal) == pair.slave,
                             "output descriptor") &&
              contract_check((raw.c_lflag & (ECHO | ICANON | ISIG)) == 0U,
                             "raw local flags") &&
              contract_check((fcntl(pair.slave, F_GETFL) & O_NONBLOCK) != 0,
                             "nonblocking input") &&
              contract_check(
                  aforc_terminal_open(&duplicate, &config) ==
                      AFORC_ERROR_EXISTS,
                  "duplicate open") &&
              contract_check(duplicate == NULL, "duplicate output");
    aforc_terminal_close(terminal);
    if (passed) {
        passed = contract_check(tcgetattr(pair.slave, &after) == 0,
                                "read restored termios");
    }
    if (passed) {
        passed = contract_check(termios_equal(&before, &after),
                                "restored termios");
    }
    if (passed) {
        passed = contract_check(fcntl(pair.slave, F_GETFL) == flags_before,
                                "restored descriptor flags");
    }
    if (passed) {
        passed = contract_check(fcntl(pair.master, F_GETFL) >= 0,
                                "borrowed descriptors");
    }
    pty_close(&pair);
    return passed;
}

static bool test_resize_signal_is_coalesced(void)
{
    PtyPair pair;
    AFORC_Terminal *terminal = NULL;
    AFORC_TerminalConfig config;
    AFORC_Size size;
    bool readable = true;
    bool resized = false;
    bool passed;

    if (!pty_open(&pair) || !set_size(pair.slave, 80U, 24U)) {
        pty_close(&pair);
        return false;
    }
    config = terminal_config(pair.slave);
    if (aforc_terminal_open(&terminal, &config) != AFORC_OK ||
        aforc_terminal_dimensions(terminal, &size) != AFORC_OK ||
        size.width != 80 || size.height != 24 ||
        !set_size(pair.slave, 100U, 40U) || raise(SIGWINCH) != 0) {
        aforc_terminal_close(terminal);
        pty_close(&pair);
        return false;
    }
    passed = aforc_terminal_poll(terminal, 100, &readable, &resized) ==
                 AFORC_OK &&
             !readable && resized &&
             aforc_terminal_dimensions(terminal, &size) == AFORC_OK &&
             size.width == 100 && size.height == 40;
    aforc_terminal_close(terminal);
    pty_close(&pair);
    return passed;
}

static bool test_terminating_signal_restores_runtime(void)
{
    PtyPair pair;
    struct sigaction before;
    struct sigaction after;
    AFORC_Terminal *terminal = NULL;
    AFORC_TerminalConfig config;
    bool passed;

    if (!pty_open(&pair) || sigaction(SIGTERM, NULL, &before) < 0) {
        pty_close(&pair);
        return false;
    }
    config = terminal_config(pair.slave);
    if (aforc_terminal_open(&terminal, &config) != AFORC_OK ||
        raise(SIGTERM) != 0) {
        aforc_terminal_close(terminal);
        pty_close(&pair);
        return false;
    }
    passed = aforc_terminal_poll(terminal, 100, NULL, NULL) ==
                 AFORC_ERROR_INTERRUPTED &&
             aforc_terminal_pending_signal(terminal) == SIGTERM &&
             !aforc_terminal_is_active(terminal);
    aforc_terminal_close(terminal);
    passed = passed && sigaction(SIGTERM, NULL, &after) == 0 &&
             after.sa_handler == before.sa_handler;
    pty_close(&pair);
    return passed;
}

static bool test_master_hangup_reports_end_of_stream(void)
{
    PtyPair pair;
    AFORC_Terminal *terminal = NULL;
    AFORC_TerminalConfig config;
    unsigned char byte = 0U;
    size_t count = 1U;
    bool readable = false;
    bool passed;

    if (!pty_open(&pair)) {
        pty_close(&pair);
        return false;
    }
    config = terminal_config(pair.slave);
    if (aforc_terminal_open(&terminal, &config) != AFORC_OK) {
        pty_close(&pair);
        return false;
    }
    (void)close(pair.master);
    pair.master = -1;
    passed = aforc_terminal_poll(terminal, 100, &readable, NULL) == AFORC_OK &&
             readable &&
             aforc_terminal_read(terminal, &byte, 1U, &count) ==
                 AFORC_ERROR_END_OF_STREAM &&
             count == 0U;
    aforc_terminal_close(terminal);
    pty_close(&pair);
    return passed;
}

int main(void)
{
    if (!test_open_close_restores_and_borrows_fds()) {
        (void)fprintf(stderr, "terminal open/close contract failed\n");
        return 1;
    }
    if (!test_resize_signal_is_coalesced()) {
        (void)fprintf(stderr, "terminal resize signal failed\n");
        return 2;
    }
    if (!test_terminating_signal_restores_runtime()) {
        (void)fprintf(stderr, "terminal terminating signal failed\n");
        return 3;
    }
    if (!test_master_hangup_reports_end_of_stream()) {
        (void)fprintf(stderr, "terminal hangup handling failed\n");
        return 4;
    }
    (void)puts("terminal lifecycle: ok");
    return 0;
}
