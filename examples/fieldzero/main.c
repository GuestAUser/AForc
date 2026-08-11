#include "fieldzero/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool fieldzero_parse_seed(const char *text, uint64_t *out_seed)
{
    uint64_t value = 0U;

    if (text == NULL || out_seed == NULL || text[0] == '\0')
    {
        return false;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index)
    {
        uint64_t digit;

        if (text[index] < '0' || text[index] > '9')
        {
            return false;
        }
        digit = (uint64_t)(text[index] - '0');
        if (value > (UINT64_MAX - digit) / UINT64_C(10))
        {
            return false;
        }
        value = value * UINT64_C(10) + digit;
    }
    *out_seed = value;
    return true;
}

static uint64_t fieldzero_default_seed(void)
{
    struct timespec now = {0, 0};

    if (timespec_get(&now, TIME_UTC) != TIME_UTC)
    {
        return UINT64_C(0x4649454c445a4552);
    }
    return (uint64_t)now.tv_sec ^ ((uint64_t)(uint32_t)now.tv_nsec << 32U);
}

static void fieldzero_print_usage(FILE *stream, const char *program_name)
{
    (void)fprintf(
        stream,
        "Usage: %s [--seed NUMBER] [--smoke] [--reduced-motion] "
        "[--no-color] [--help]\n"
        "\n"
        "FIELD ZERO - a deterministic survey-recovery platformer.\n"
        "Register each + in sequence, then reach > to record the room.\n"
        "\n"
        "  --seed NUMBER     Set the deterministic scenery seed\n"
        "  --smoke           Run deterministic non-interactive checks\n"
        "  --reduced-motion  Reduce nonessential presentation motion\n"
        "  --no-color        Use monochrome presentation\n"
        "  --help            Show this help and exit\n"
        "\n"
        "Controls: arrows or A/D move, Space or Z jumps, K dashes,\n"
        "          ? help, P pause, R restart, Q or Escape quit/back.\n",
        program_name != NULL ? program_name : "aforc-fieldzero");
}

static bool fieldzero_parse_options(int argc,
                                    char **argv,
                                    FieldzeroOptions *out_options,
                                    bool *out_help)
{
    FieldzeroOptions options = {0};
    bool help = false;
    bool seed_supplied = false;
    bool smoke_supplied = false;
    bool reduced_motion_supplied = false;
    bool no_color_supplied = false;

    if (out_options == NULL || out_help == NULL)
    {
        return false;
    }
    (void)memset(out_options, 0, sizeof(*out_options));
    *out_help = false;
    if (argc < 1 || argv == NULL)
    {
        return false;
    }
    for (int index = 1; index < argc; ++index)
    {
        if (argv[index] == NULL)
        {
            return false;
        }
        if (strcmp(argv[index], "--help") == 0)
        {
            if (help)
            {
                return false;
            }
            help = true;
        }
        else if (strcmp(argv[index], "--seed") == 0)
        {
            if (seed_supplied || index + 1 >= argc || argv[index + 1] == NULL ||
                !fieldzero_parse_seed(argv[index + 1], &options.seed))
            {
                return false;
            }
            ++index;
            seed_supplied = true;
        }
        else if (strcmp(argv[index], "--smoke") == 0)
        {
            if (smoke_supplied)
            {
                return false;
            }
            options.smoke = true;
            smoke_supplied = true;
        }
        else if (strcmp(argv[index], "--reduced-motion") == 0)
        {
            if (reduced_motion_supplied)
            {
                return false;
            }
            options.reduced_motion = true;
            reduced_motion_supplied = true;
        }
        else if (strcmp(argv[index], "--no-color") == 0)
        {
            if (no_color_supplied)
            {
                return false;
            }
            options.no_color = true;
            no_color_supplied = true;
        }
        else
        {
            return false;
        }
    }
    if (!seed_supplied)
    {
        options.seed =
            options.smoke ? UINT64_C(2026) : fieldzero_default_seed();
    }
    *out_options = options;
    *out_help = help;
    return true;
}

int main(int argc, char **argv)
{
    const char *program_name = argc > 0 && argv != NULL && argv[0] != NULL
                                   ? argv[0]
                                   : "aforc-fieldzero";
    FieldzeroOptions options;
    bool help = false;

    if (!fieldzero_parse_options(argc, argv, &options, &help))
    {
        (void)fprintf(stderr, "%s: invalid command line\n", program_name);
        fieldzero_print_usage(stderr, program_name);
        return EXIT_FAILURE;
    }
    if (help)
    {
        fieldzero_print_usage(stdout, program_name);
        return EXIT_SUCCESS;
    }
    return options.smoke ? fieldzero_run_smoke(&options)
                         : fieldzero_run_interactive(&options);
}
