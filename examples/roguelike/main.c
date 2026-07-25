/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ProgramOptions {
    uint64_t seed;
    bool seed_supplied;
    bool smoke;
    bool help;
} ProgramOptions;

static void print_usage(FILE *stream, const char *program) {
    (void)fprintf(stream,
                  "Usage: %s [--seed NUMBER] [--smoke] [--help]\n"
                  "\n"
                  "A procedural terminal roguelike and full AFORC integration "
                  "showcase.\n"
                  "\n"
                  "  --seed NUMBER  Reproduce a deterministic run\n"
                  "  --smoke        Run non-interactive subsystem checks\n"
                  "  --help         Show this help and exit\n"
                  "\n"
                  "In-game: arrows/WASD/HJKL move, . waits, > descends, "
                  "S/L save/load,\n"
                  "? shows controls, and Q or Escape quits.\n",
                  program);
}

static AFORC_Status parse_options(int argc,
                                char **argv,
                                ProgramOptions *options) {
    (void)memset(options, 0, sizeof(*options));
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0 ||
            strcmp(argv[index], "-h") == 0) {
            options->help = true;
        } else if (strcmp(argv[index], "--smoke") == 0) {
            options->smoke = true;
        } else if (strcmp(argv[index], "--seed") == 0) {
            char *end = NULL;
            unsigned long long value;

            if (index + 1 >= argc) {
                return AFORC_ERROR_INVALID_ARGUMENT;
            }
            if (argv[index + 1][0] == '-') {
                return AFORC_ERROR_FORMAT;
            }
            errno = 0;
            value = strtoull(argv[++index], &end, 10);
            if (errno != 0 || end == argv[index] || *end != '\0' ||
                value > UINT64_MAX) {
                return AFORC_ERROR_FORMAT;
            }
            options->seed = (uint64_t)value;
            options->seed_supplied = true;
        } else {
            return AFORC_ERROR_INVALID_ARGUMENT;
        }
    }
    return AFORC_OK;
}

static uint64_t default_seed(void) {
    struct timespec now = {0, 0};

    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return UINT64_C(0x41464f5243);
    }
    return (uint64_t)now.tv_sec ^
           ((uint64_t)(uint32_t)now.tv_nsec << 32U);
}

int main(int argc, char **argv) {
    ProgramOptions options;
    AFORC_Status status = parse_options(argc, argv, &options);

    if (status != AFORC_OK) {
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }
    if (options.help) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (!options.seed_supplied) {
        options.seed = options.smoke ? UINT64_C(20260724) : default_seed();
    }
    return options.smoke ? game_run_smoke(options.seed)
                         : game_run_interactive(options.seed);
}
