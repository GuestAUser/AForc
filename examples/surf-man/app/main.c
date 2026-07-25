/*
 * AForc Surf-Man
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "surf_man/app.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct SurfManProgramOptions {
    uint64_t seed;
    bool seed_supplied;
    bool smoke;
    bool help;
} SurfManProgramOptions;

static void surf_man_print_usage(FILE *stream, const char *program)
{
    (void)fprintf(stream,
                  "Usage: %s [--seed NUMBER] [--smoke] [--help]\n"
                  "\n"
                  "A deterministic terminal surfing game and AFORC showcase.\n"
                  "\n"
                  "  --seed NUMBER  Reproduce a deterministic session\n"
                  "  --smoke        Run deterministic off-screen checks\n"
                  "  --help         Show this help and exit\n"
                  "\n"
                  "In-game: arrows/WASD steer, Space acts, Enter confirms,\n"
                  "? opens help, P or Escape pauses/goes back, and Q quits.\n",
                  program);
}

static AFORC_Status surf_man_parse_seed(const char *text, uint64_t *out_seed)
{
    char *end = NULL;
    unsigned long long value;
    size_t index;

    if (text == NULL || out_seed == NULL || text[0] == '\0') {
        return AFORC_ERROR_FORMAT;
    }
    for (index = 0U; text[index] != '\0'; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return AFORC_ERROR_FORMAT;
        }
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        value > (unsigned long long)UINT64_MAX) {
        return AFORC_ERROR_FORMAT;
    }
    *out_seed = (uint64_t)value;
    return AFORC_OK;
}

static AFORC_Status surf_man_parse_options(int argc,
                                           char **argv,
                                           SurfManProgramOptions *options)
{
    int index;

    if (argc < 1 || argv == NULL || options == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(options, 0, sizeof(*options));
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0) {
            if (options->help) {
                return AFORC_ERROR_INVALID_ARGUMENT;
            }
            options->help = true;
        } else if (strcmp(argv[index], "--smoke") == 0) {
            if (options->smoke) {
                return AFORC_ERROR_INVALID_ARGUMENT;
            }
            options->smoke = true;
        } else if (strcmp(argv[index], "--seed") == 0) {
            AFORC_Status status;

            if (options->seed_supplied || index + 1 >= argc) {
                return AFORC_ERROR_INVALID_ARGUMENT;
            }
            status = surf_man_parse_seed(argv[++index], &options->seed);
            if (status != AFORC_OK) {
                return status;
            }
            options->seed_supplied = true;
        } else {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
    }
    return AFORC_OK;
}

static uint64_t surf_man_default_seed(void)
{
    struct timespec now = {0, 0};

    if (timespec_get(&now, TIME_UTC) != TIME_UTC || now.tv_sec < 0) {
        return UINT64_C(0x535552464d414e);
    }
    return (uint64_t)now.tv_sec ^
           ((uint64_t)(uint32_t)now.tv_nsec << 32U);
}

int main(int argc, char **argv)
{
    SurfManProgramOptions options;
    AFORC_Status status = surf_man_parse_options(argc, argv, &options);

    if (status != AFORC_OK) {
        (void)fprintf(stderr,
                      "aforc-surf-man: invalid command line: %s\n",
                      aforc_status_string(status));
        surf_man_print_usage(stderr,
                             argc > 0 && argv != NULL ? argv[0]
                                                      : "aforc-surf-man");
        return EXIT_FAILURE;
    }
    if (options.help) {
        surf_man_print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (!options.seed_supplied) {
        options.seed = options.smoke ? UINT64_C(20260725)
                                     : surf_man_default_seed();
    }
    return options.smoke ? surf_man_run_smoke(options.seed)
                         : surf_man_run_interactive(options.seed);
}
