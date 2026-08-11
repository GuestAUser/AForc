/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#define _XOPEN_SOURCE 600

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

enum
{
    FIELDZERO_PTY_OUTPUT_CAPACITY = 2 * 1024 * 1024
};

typedef struct FieldzeroPtyProcess
{
    pid_t pid;
    int master;
    unsigned char *output;
    size_t output_size;
    int status;
    bool reaped;
} FieldzeroPtyProcess;

static const char fieldzero_keyboard_query[] = "\x1b[?u";
static const char fieldzero_supported_capabilities[] = "\x1b[?27u";
static const char fieldzero_unsupported_capabilities[] = "\x1b[?1u";
static const char fieldzero_title_marker[] = "ENTER BEGIN   ? HELP   Q QUIT";
static const char fieldzero_play_marker[] = "A/D MOVE   SPACE/Z JUMP   X DASH";
static const char fieldzero_resize_marker[] =
    "FIELD ZERO REQUIRES 80x24 - RESIZE TERMINAL";
static const char fieldzero_runtime_failure[] = "aforc-fieldzero runtime:";
static const char *const fieldzero_teardown_sequences[] = {
    "\x1b[<u", "\x1b[?25h", "\x1b[?1049l"};

static bool fieldzero_monotonic_ms(uint64_t *out_ms)
{
    struct timespec now;

    if (out_ms == NULL || clock_gettime(CLOCK_MONOTONIC, &now) < 0 ||
        now.tv_sec < 0)
    {
        return false;
    }
    *out_ms = (uint64_t)now.tv_sec * UINT64_C(1000) +
              (uint64_t)now.tv_nsec / UINT64_C(1000000);
    return true;
}

static bool
fieldzero_set_size(int fd, unsigned short columns, unsigned short rows)
{
    struct winsize size;

    (void)memset(&size, 0, sizeof(size));
    size.ws_col = columns;
    size.ws_row = rows;
    return ioctl(fd, TIOCSWINSZ, &size) == 0;
}

static bool fieldzero_output_contains(const FieldzeroPtyProcess *process,
                                      size_t start,
                                      const char *needle)
{
    const size_t needle_size = strlen(needle);
    size_t index;

    if (start > process->output_size ||
        needle_size > process->output_size - start)
    {
        return false;
    }
    for (index = start; index <= process->output_size - needle_size; ++index)
    {
        if (memcmp(process->output + index, needle, needle_size) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool fieldzero_collect(FieldzeroPtyProcess *process,
                              unsigned int timeout_ms,
                              const char *needle,
                              size_t start)
{
    uint64_t now_ms;
    uint64_t deadline_ms;

    if (!fieldzero_monotonic_ms(&now_ms))
    {
        return false;
    }
    deadline_ms = now_ms + (uint64_t)timeout_ms;
    for (;;)
    {
        struct pollfd descriptor;
        uint64_t remaining_ms;
        int poll_timeout;
        int poll_result;
        ssize_t count;

        if (needle != NULL && fieldzero_output_contains(process, start, needle))
        {
            return true;
        }
        if (!fieldzero_monotonic_ms(&now_ms))
        {
            return false;
        }
        if (now_ms >= deadline_ms)
        {
            return needle == NULL;
        }
        remaining_ms = deadline_ms - now_ms;
        poll_timeout =
            remaining_ms > (uint64_t)INT_MAX ? INT_MAX : (int)remaining_ms;
        descriptor.fd = process->master;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        do
        {
            poll_result = poll(&descriptor, 1u, poll_timeout);
        } while (poll_result < 0 && errno == EINTR);
        if (poll_result < 0)
        {
            return false;
        }
        if (poll_result == 0)
        {
            return needle == NULL;
        }
        if (process->output_size == FIELDZERO_PTY_OUTPUT_CAPACITY)
        {
            errno = EOVERFLOW;
            return false;
        }
        count = read(process->master,
                     process->output + process->output_size,
                     FIELDZERO_PTY_OUTPUT_CAPACITY - process->output_size);
        if (count > 0)
        {
            process->output_size += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            continue;
        }
        if (count == 0 || (count < 0 && errno == EIO))
        {
            return needle == NULL ||
                   fieldzero_output_contains(process, start, needle);
        }
        return false;
    }
}

static bool fieldzero_write_all(int fd, const char *data, size_t size)
{
    size_t offset = 0u;

    while (offset < size)
    {
        const ssize_t count = write(fd, data + offset, size - offset);

        if (count > 0)
        {
            offset += (size_t)count;
            continue;
        }
        if (count < 0 && errno == EINTR)
        {
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            struct pollfd descriptor;
            int poll_result;

            descriptor.fd = fd;
            descriptor.events = POLLOUT;
            descriptor.revents = 0;
            do
            {
                poll_result = poll(&descriptor, 1u, 500);
            } while (poll_result < 0 && errno == EINTR);
            if (poll_result > 0 && (descriptor.revents & POLLOUT) != 0)
            {
                continue;
            }
        }
        return false;
    }
    return true;
}

static pid_t fieldzero_waitpid(pid_t pid, int *status, int options)
{
    pid_t result;

    do
    {
        result = waitpid(pid, status, options);
    } while (result < 0 && errno == EINTR);
    return result;
}

static bool fieldzero_spawn(FieldzeroPtyProcess *process,
                            const char *executable)
{
    char *slave_name;
    int slave = -1;
    int flags;
    pid_t pid;

    (void)memset(process, 0, sizeof(*process));
    process->pid = -1;
    process->master = -1;
    process->output = malloc(FIELDZERO_PTY_OUTPUT_CAPACITY);
    if (process->output == NULL)
    {
        return false;
    }
    process->master = posix_openpt(O_RDWR | O_NOCTTY);
    if (process->master < 0 || grantpt(process->master) < 0 ||
        unlockpt(process->master) < 0)
    {
        goto fail;
    }
    slave_name = ptsname(process->master);
    if (slave_name == NULL)
    {
        goto fail;
    }
    slave = open(slave_name, O_RDWR | O_NOCTTY);
    if (slave < 0 || !fieldzero_set_size(slave, 100u, 30u))
    {
        goto fail;
    }
    flags = fcntl(process->master, F_GETFL);
    if (flags < 0 || fcntl(process->master, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        goto fail;
    }
    pid = fork();
    if (pid < 0)
    {
        goto fail;
    }
    if (pid == 0)
    {
        (void)close(process->master);
        if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) < 0 ||
            dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0 ||
            dup2(slave, STDERR_FILENO) < 0 ||
            setenv("TERM", "xterm-kitty", 1) < 0 ||
            setenv("LC_ALL", "C", 1) < 0)
        {
            _exit(127);
        }
        if (slave > STDERR_FILENO)
        {
            (void)close(slave);
        }
        execl(executable,
              executable,
              "--seed",
              "2026",
              "--reduced-motion",
              "--no-color",
              (char *)NULL);
        _exit(127);
    }
    process->pid = pid;
    (void)close(slave);
    return true;

fail:
    if (slave >= 0)
    {
        (void)close(slave);
    }
    if (process->master >= 0)
    {
        (void)close(process->master);
        process->master = -1;
    }
    free(process->output);
    process->output = NULL;
    return false;
}

static bool fieldzero_wait_for_exit(FieldzeroPtyProcess *process,
                                    unsigned int timeout_ms)
{
    uint64_t now_ms;
    uint64_t deadline_ms;

    if (!fieldzero_monotonic_ms(&now_ms))
    {
        return false;
    }
    deadline_ms = now_ms + (uint64_t)timeout_ms;
    while (now_ms < deadline_ms)
    {
        const pid_t result =
            fieldzero_waitpid(process->pid, &process->status, WNOHANG);

        if (result == process->pid)
        {
            process->reaped = true;
            (void)fieldzero_collect(process, 100u, NULL, 0u);
            return true;
        }
        if (result < 0 || !fieldzero_collect(process, 50u, NULL, 0u) ||
            !fieldzero_monotonic_ms(&now_ms))
        {
            return false;
        }
    }
    return false;
}

static bool fieldzero_process_exited(FieldzeroPtyProcess *process)
{
    const pid_t result =
        fieldzero_waitpid(process->pid, &process->status, WNOHANG);

    if (result == process->pid)
    {
        process->reaped = true;
        (void)fieldzero_collect(process, 100u, NULL, 0u);
        return true;
    }
    return result < 0;
}

static bool fieldzero_send_ignored_burst(FieldzeroPtyProcess *process)
{
    static const char ignored_release[] = "\x1b[49;1:3u";
    enum
    {
        IGNORED_EVENT_COUNT = 300
    };
    char burst[IGNORED_EVENT_COUNT * (sizeof(ignored_release) - 1u)];
    size_t index;

    for (index = 0u; index < IGNORED_EVENT_COUNT; ++index)
    {
        (void)memcpy(burst + index * (sizeof(ignored_release) - 1u),
                     ignored_release,
                     sizeof(ignored_release) - 1u);
    }
    return fieldzero_write_all(process->master, burst, sizeof(burst)) &&
           fieldzero_collect(process, 250u, NULL, 0u) &&
           !fieldzero_process_exited(process);
}

static void fieldzero_dispose_process(FieldzeroPtyProcess *process)
{
    if (process->pid > 0 && !process->reaped)
    {
        pid_t result =
            fieldzero_waitpid(process->pid, &process->status, WNOHANG);

        if (result == 0)
        {
            (void)kill(process->pid, SIGKILL);
            result = fieldzero_waitpid(process->pid, &process->status, 0);
        }
        process->reaped = result == process->pid;
    }
    if (process->master >= 0)
    {
        (void)close(process->master);
    }
    free(process->output);
    (void)memset(process, 0, sizeof(*process));
    process->pid = -1;
    process->master = -1;
}

static int fieldzero_exit_code(const FieldzeroPtyProcess *process)
{
    if (WIFEXITED(process->status))
    {
        return WEXITSTATUS(process->status);
    }
    if (WIFSIGNALED(process->status))
    {
        return -WTERMSIG(process->status);
    }
    return -255;
}

static void fieldzero_print_tail(const FieldzeroPtyProcess *process)
{
    const size_t start =
        process->output_size > 512u ? process->output_size - 512u : 0u;

    (void)fwrite(
        process->output + start, 1u, process->output_size - start, stderr);
    (void)fputc('\n', stderr);
}

static bool fieldzero_check(bool condition,
                            const FieldzeroPtyProcess *process,
                            const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "fieldzero PTY: %s\n", message);
        if (process != NULL && process->output != NULL)
        {
            fieldzero_print_tail(process);
        }
    }
    return condition;
}

static bool fieldzero_check_teardown(const FieldzeroPtyProcess *process)
{
    const size_t count = sizeof(fieldzero_teardown_sequences) /
                         sizeof(fieldzero_teardown_sequences[0]);
    size_t index;

    for (index = 0u; index < count; ++index)
    {
        if (!fieldzero_output_contains(
                process, 0u, fieldzero_teardown_sequences[index]))
        {
            return fieldzero_check(
                false, process, "terminal teardown sequence missing");
        }
    }
    return true;
}

static bool fieldzero_resize(FieldzeroPtyProcess *process,
                             unsigned short columns,
                             unsigned short rows)
{
    return fieldzero_set_size(process->master, columns, rows) &&
           kill(process->pid, SIGWINCH) == 0;
}

static bool fieldzero_run_supported(const char *executable)
{
    static const char enter[] = "\x1b[13;1:1u\x1b[13;1:3u";
    static const char move_left[] = "\x1b[97;1:1u";
    static const char jump[] = "\x1b[32;1:1u";
    static const char dash[] = "\x1b[120;1:1u";
    static const char release_actions[] =
        "\x1b[120;1:3u\x1b[32;1:3u\x1b[97;1:3u";
    static const char quit_key[] = "\x1b[113;1:1u\x1b[113;1:3u";
    FieldzeroPtyProcess process;
    size_t start;
    bool passed = false;

    if (!fieldzero_spawn(&process, executable))
    {
        (void)fprintf(
            stderr, "fieldzero PTY: spawn failed: %s\n", strerror(errno));
        return false;
    }
    if (!fieldzero_check(
            fieldzero_collect(&process, 1000u, fieldzero_keyboard_query, 0u),
            &process,
            "supported run did not query keyboard capabilities") ||
        !fieldzero_check(
            fieldzero_write_all(process.master,
                                fieldzero_supported_capabilities,
                                sizeof(fieldzero_supported_capabilities) - 1u),
            &process,
            "supported capability response failed") ||
        !fieldzero_check(
            fieldzero_collect(&process, 1000u, fieldzero_title_marker, 0u),
            &process,
            "supported run did not reach the title screen") ||
        !fieldzero_check(
            fieldzero_write_all(process.master, enter, sizeof(enter) - 1u),
            &process,
            "enter input failed") ||
        !fieldzero_check(
            fieldzero_collect(&process, 1000u, fieldzero_play_marker, 0u),
            &process,
            "supported run did not enter play") ||
        !fieldzero_check(fieldzero_send_ignored_burst(&process),
                         &process,
                         "ignored input overflow terminated the run") ||
        !fieldzero_check(
            fieldzero_write_all(
                process.master, move_left, sizeof(move_left) - 1u) &&
                fieldzero_collect(&process, 100u, NULL, 0u) &&
                fieldzero_write_all(process.master, jump, sizeof(jump) - 1u) &&
                fieldzero_collect(&process, 80u, NULL, 0u) &&
                fieldzero_write_all(process.master, dash, sizeof(dash) - 1u) &&
                fieldzero_collect(&process, 250u, NULL, 0u),
            &process,
            "left air-dash input failed") ||
        !fieldzero_check(!fieldzero_process_exited(&process),
                         &process,
                         "FIELD ZERO exited during left air-dash") ||
        !fieldzero_check(fieldzero_write_all(process.master,
                                             release_actions,
                                             sizeof(release_actions) - 1u) &&
                             fieldzero_collect(&process, 100u, NULL, 0u),
                         &process,
                         "action release input failed"))
    {
        goto done;
    }

    start = process.output_size;
    if (!fieldzero_check(
            fieldzero_resize(&process, 79u, 23u) &&
                fieldzero_collect(
                    &process, 1000u, fieldzero_resize_marker, start),
            &process,
            "small-terminal resize screen was not rendered"))
    {
        goto done;
    }
    start = process.output_size;
    if (!fieldzero_check(fieldzero_resize(&process, 120u, 40u) &&
                             fieldzero_collect(
                                 &process, 1000u, fieldzero_play_marker, start),
                         &process,
                         "play screen was not restored after resize") ||
        !fieldzero_check(fieldzero_write_all(
                             process.master, quit_key, sizeof(quit_key) - 1u) &&
                             fieldzero_collect(&process, 100u, NULL, 0u) &&
                             fieldzero_write_all(process.master,
                                                 quit_key,
                                                 sizeof(quit_key) - 1u),
                         &process,
                         "quit input failed") ||
        !fieldzero_check(fieldzero_wait_for_exit(&process, 2000u),
                         &process,
                         "supported run did not exit before timeout") ||
        !fieldzero_check(fieldzero_exit_code(&process) == 0,
                         &process,
                         "supported run exited unsuccessfully") ||
        !fieldzero_check(
            !fieldzero_output_contains(&process, 0u, fieldzero_runtime_failure),
            &process,
            "supported run reported a runtime failure") ||
        !fieldzero_check_teardown(&process))
    {
        goto done;
    }
    passed = true;

done:
    fieldzero_dispose_process(&process);
    return passed;
}

static bool fieldzero_run_unsupported(const char *executable)
{
    FieldzeroPtyProcess process;
    bool passed = false;

    if (!fieldzero_spawn(&process, executable))
    {
        (void)fprintf(
            stderr, "fieldzero PTY: spawn failed: %s\n", strerror(errno));
        return false;
    }
    if (fieldzero_check(
            fieldzero_collect(&process, 1000u, fieldzero_keyboard_query, 0u),
            &process,
            "unsupported run did not query keyboard capabilities") &&
        fieldzero_check(fieldzero_write_all(
                            process.master,
                            fieldzero_unsupported_capabilities,
                            sizeof(fieldzero_unsupported_capabilities) - 1u),
                        &process,
                        "unsupported capability response failed") &&
        fieldzero_check(fieldzero_wait_for_exit(&process, 2000u),
                        &process,
                        "unsupported run did not exit before timeout") &&
        fieldzero_check(fieldzero_exit_code(&process) == 1,
                        &process,
                        "unsupported run did not exit with failure") &&
        fieldzero_check(fieldzero_output_contains(&process, 0u, "unsupported"),
                        &process,
                        "unsupported negotiation diagnostic missing") &&
        fieldzero_check_teardown(&process))
    {
        passed = true;
    }
    fieldzero_dispose_process(&process);
    return passed;
}

static bool
fieldzero_run_signal(const char *executable, int signal_number, bool running)
{
    FieldzeroPtyProcess process;
    bool passed = false;

    if (!fieldzero_spawn(&process, executable))
    {
        (void)fprintf(
            stderr, "fieldzero PTY: spawn failed: %s\n", strerror(errno));
        return false;
    }
    if (!fieldzero_check(
            fieldzero_collect(&process, 1000u, fieldzero_keyboard_query, 0u),
            &process,
            "signal run did not query keyboard capabilities") ||
        (running &&
         (!fieldzero_check(fieldzero_write_all(
                               process.master,
                               fieldzero_supported_capabilities,
                               sizeof(fieldzero_supported_capabilities) - 1u),
                           &process,
                           "signal capability response failed") ||
          !fieldzero_check(
              fieldzero_collect(&process, 1000u, fieldzero_title_marker, 0u),
              &process,
              "signal run did not reach the title screen"))) ||
        !fieldzero_check(kill(process.pid, signal_number) == 0,
                         &process,
                         "terminating signal delivery failed") ||
        !fieldzero_check(fieldzero_wait_for_exit(&process, 2000u),
                         &process,
                         "signaled run did not exit before timeout") ||
        !fieldzero_check(fieldzero_exit_code(&process) == -signal_number,
                         &process,
                         "signaled run lost terminating signal semantics") ||
        !fieldzero_check(
            !fieldzero_output_contains(&process, 0u, fieldzero_runtime_failure),
            &process,
            "signaled run reported a runtime failure") ||
        !fieldzero_check_teardown(&process))
    {
        goto done;
    }
    passed = true;

done:
    fieldzero_dispose_process(&process);
    return passed;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv == NULL || argv[1] == NULL)
    {
        (void)fprintf(stderr,
                      "Usage: %s FIELDZERO_EXECUTABLE\n",
                      argc > 0 && argv != NULL && argv[0] != NULL
                          ? argv[0]
                          : "aforc-fieldzero-pty-test");
        return 2;
    }
    if (access(argv[1], X_OK) < 0)
    {
        (void)fprintf(
            stderr, "FIELD ZERO executable is not runnable: %s\n", argv[1]);
        return 2;
    }
    if (!fieldzero_run_supported(argv[1]) ||
        !fieldzero_run_unsupported(argv[1]) ||
        !fieldzero_run_signal(argv[1], SIGTERM, false) ||
        !fieldzero_run_signal(argv[1], SIGINT, true))
    {
        return 1;
    }
    (void)puts("fieldzero PTY: ok");
    return 0;
}
