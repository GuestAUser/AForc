/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "roguelike/internal.h"

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

AFORC_Status game_parse_seed(const char *text, uint64_t *out_seed) {
    uint64_t value = 0U;

    if (text == NULL || out_seed == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (text[0] == '\0') {
        return AFORC_ERROR_FORMAT;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        uint64_t digit;

        if (text[index] < '0' || text[index] > '9') {
            return AFORC_ERROR_FORMAT;
        }
        digit = (uint64_t)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
            return AFORC_ERROR_FORMAT;
        }
        value = value * UINT64_C(10) + digit;
    }
    *out_seed = value;
    return AFORC_OK;
}

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
    if (argc < 1 || argv == NULL || options == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(options, 0, sizeof(*options));
    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0 ||
            strcmp(argv[index], "-h") == 0) {
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
            status = game_parse_seed(argv[++index], &options->seed);
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

static uint64_t default_seed(void) {
    struct timespec now = {0, 0};

    if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
        return UINT64_C(0x41464f5243);
    }
    return (uint64_t)now.tv_sec ^
           ((uint64_t)(uint32_t)now.tv_nsec << 32U);
}

int main(int argc, char **argv) {
    const char *program = argc > 0 && argv != NULL && argv[0] != NULL
                              ? argv[0]
                              : "aforc-roguelike";
    ProgramOptions options;
    AFORC_Status status = parse_options(argc, argv, &options);

    if (status != AFORC_OK) {
        print_usage(stderr, program);
        return EXIT_FAILURE;
    }
    if (options.help) {
        print_usage(stdout, program);
        return EXIT_SUCCESS;
    }
    if (!options.seed_supplied) {
        options.seed = options.smoke ? UINT64_C(20260724) : default_seed();
    }
    return options.smoke ? game_run_smoke(options.seed)
                         : game_run_interactive(options.seed);
}
