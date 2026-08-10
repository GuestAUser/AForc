/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "aforc/assets.h"
#include "config_parser_scale_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool test_valid_grammar_and_order(void)
{
    static const char text[] = "  # ignored\r\n"
                               "[ first ]\r\n"
                               " alpha = one \r\n"
                               "[second]\n"
                               "alpha=\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    AFORC_Config config = {0};
    const AFORC_Status status =
        aforc_config_parse(text, sizeof(text) - 1u, &limits, &config);
    const bool passed = status == AFORC_OK && config.count == 2u &&
                        strcmp(config.entries[0u].section, "first") == 0 &&
                        strcmp(config.entries[0u].key, "alpha") == 0 &&
                        strcmp(config.entries[0u].value, "one") == 0 &&
                        strcmp(config.entries[1u].section, "second") == 0 &&
                        strcmp(config.entries[1u].key, "alpha") == 0 &&
                        strcmp(config.entries[1u].value, "") == 0;

    aforc_config_release(&config);
    return passed;
}

static bool test_malformed_inputs(void)
{
    static const char missing_equals[] = "key\n";
    static const char invalid_section[] = "[bad section]\nkey=value\n";
    static const char invalid_cr[] = "key=value\rbroken\n";
    static const char embedded_nul[] = {'k', 'e', 'y', '=', 'v', '\0', 'x'};

    return aforc_config_scale_parse_status(missing_equals,
                                           sizeof(missing_equals) - 1u,
                                           AFORC_ERROR_FORMAT) &&
           aforc_config_scale_parse_status(invalid_section,
                                           sizeof(invalid_section) - 1u,
                                           AFORC_ERROR_FORMAT) &&
           aforc_config_scale_parse_status(
               invalid_cr, sizeof(invalid_cr) - 1u, AFORC_ERROR_FORMAT) &&
           aforc_config_scale_parse_status(
               embedded_nul, sizeof(embedded_nul), AFORC_ERROR_FORMAT);
}

static bool test_early_duplicate(void)
{
    static const char text[] = "[same]\nkey=one\nkey=two\n";

    return aforc_config_scale_parse_status(
        text, sizeof(text) - 1u, AFORC_ERROR_EXISTS);
}

static bool test_late_duplicate(void)
{
    char *text;
    size_t text_size;
    bool passed;

    text = aforc_config_scale_make_config(
        AFORC_CONFIG_DEFAULT_MAX_ENTRIES, true, &text_size);
    if (text == NULL)
    {
        return false;
    }
    passed =
        aforc_config_scale_parse_status(text, text_size, AFORC_ERROR_EXISTS);
    free(text);
    return passed;
}

static bool test_unique_entry_counts(void)
{
    static const size_t counts[] = {512u, 1024u, 2048u, 4096u};
    size_t index;

    for (index = 0u; index < sizeof(counts) / sizeof(counts[0u]); ++index)
    {
        AFORC_ConfigLimits limits = aforc_config_limits_default();
        AFORC_Config config = {0};
        char expected_key[32];
        char *text;
        size_t text_size;
        int written;
        AFORC_Status status;
        bool passed;

        text = aforc_config_scale_make_config(counts[index], false, &text_size);
        if (text == NULL)
        {
            return false;
        }
        written = snprintf(
            expected_key, sizeof(expected_key), "key%zu", counts[index] - 1u);
        if (written < 0 || (size_t)written >= sizeof(expected_key))
        {
            free(text);
            return false;
        }
        status = aforc_config_parse(text, text_size, &limits, &config);
        passed =
            status == AFORC_OK && config.count == counts[index] &&
            strcmp(config.entries[0u].section, "dense") == 0 &&
            strcmp(config.entries[0u].key, "key0") == 0 &&
            strcmp(config.entries[config.count - 1u].key, expected_key) == 0;
        aforc_config_release(&config);
        free(text);
        if (!passed)
        {
            return false;
        }
    }
    return true;
}

static bool test_allocation_failure_cleanup(void)
{
    static const char text[] =
        "[one]\nalpha=one\nbeta=two\n[two]\nalpha=three\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    AFORC_Config config = {0};
    size_t allocation_count;
    size_t failure;

    aforc_config_scale_reset_allocations(0u);
    if (aforc_config_parse(text, sizeof(text) - 1u, &limits, &config) !=
            AFORC_OK ||
        config.count != 3u)
    {
        aforc_config_release(&config);
        return false;
    }
    allocation_count = aforc_config_scale_allocation_attempts();
    aforc_config_release(&config);
    if (allocation_count == 0u || aforc_config_scale_live_allocations() != 0u)
    {
        return false;
    }
    for (failure = 1u; failure <= allocation_count; ++failure)
    {
        config.entries = NULL;
        config.count = 0u;
        aforc_config_scale_reset_allocations(failure);
        if (aforc_config_parse(text, sizeof(text) - 1u, &limits, &config) !=
                AFORC_ERROR_OUT_OF_MEMORY ||
            config.entries != NULL || config.count != 0u ||
            aforc_config_scale_live_allocations() != 0u)
        {
            aforc_config_release(&config);
            return false;
        }
    }
    return true;
}

static int test_benchmark(void)
{
    static const size_t counts[] = {512u, 1024u, 2048u, 4096u};
    const size_t repetitions = 5u;
    size_t index;

    for (index = 0u; index < sizeof(counts) / sizeof(counts[0u]); ++index)
    {
        AFORC_ConfigLimits limits = aforc_config_limits_default();
        struct timespec start;
        struct timespec end;
        char *text;
        double elapsed;
        size_t text_size;
        size_t repetition;

        text = aforc_config_scale_make_config(counts[index], false, &text_size);
        if (text == NULL || clock_gettime(CLOCK_MONOTONIC, &start) != 0)
        {
            free(text);
            return 1;
        }
        for (repetition = 0u; repetition < repetitions; ++repetition)
        {
            AFORC_Config config = {0};

            if (aforc_config_parse(text, text_size, &limits, &config) !=
                AFORC_OK)
            {
                aforc_config_release(&config);
                free(text);
                return 1;
            }
            aforc_config_release(&config);
        }
        if (clock_gettime(CLOCK_MONOTONIC, &end) != 0)
        {
            free(text);
            return 1;
        }
        elapsed = aforc_config_scale_elapsed_milliseconds(&start, &end);
        (void)printf("entries=%zu total_ms=%.3f per_parse_ms=%.3f\n",
                     counts[index],
                     elapsed,
                     elapsed / (double)repetitions);
        free(text);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--benchmark") == 0)
    {
        return test_benchmark();
    }
    if (argc != 1)
    {
        (void)fputs("usage: config_parser_scale [--benchmark]\n", stderr);
        return 1;
    }
    if (!test_valid_grammar_and_order() || !test_malformed_inputs() ||
        !test_early_duplicate() || !test_late_duplicate() ||
        !test_unique_entry_counts() || !test_allocation_failure_cleanup())
    {
        (void)fputs("config parser scale regression failed\n", stderr);
        return 1;
    }
    (void)puts("config parser scale: ok");
    return 0;
}
