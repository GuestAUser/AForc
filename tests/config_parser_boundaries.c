/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/assets.h"

#include <stdio.h>
#include <string.h>

static bool parse_status(const char *input,
                         size_t input_size,
                         const AFORC_ConfigLimits *limits,
                         AFORC_Status expected)
{
    AFORC_Config config = {0};
    const AFORC_Status status =
        aforc_config_parse(input, input_size, limits, &config);
    const bool passed =
        status == expected &&
        (status == AFORC_OK || (config.entries == NULL && config.count == 0u));

    aforc_config_release(&config);
    return passed;
}

static bool test_empty_and_lookup(void)
{
    static const char input[] = "root=top\n"
                                "[section]\n"
                                "key=value\n"
                                "[section]\n"
                                "empty=\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    AFORC_Config config = {0};
    AFORC_Status status;

    status = aforc_config_parse(NULL, 0u, &limits, &config);
    if (status != AFORC_OK || config.entries != NULL || config.count != 0u)
    {
        aforc_config_release(&config);
        return false;
    }
    status = aforc_config_parse(input, sizeof(input) - 1u, &limits, &config);
    if (status != AFORC_OK || config.count != 3u ||
        strcmp(aforc_config_get(&config, "", "root"), "top") != 0 ||
        strcmp(aforc_config_get(&config, "section", "key"), "value") != 0 ||
        strcmp(aforc_config_get(&config, "section", "empty"), "") != 0 ||
        aforc_config_get(&config, "section", "missing") != NULL ||
        aforc_config_get(NULL, "section", "key") != NULL)
    {
        aforc_config_release(&config);
        return false;
    }
    aforc_config_release(&config);
    return true;
}

static bool test_exact_limits(void)
{
    static const char input[] = "[s]\nk=v\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();
    AFORC_Config config = {0};
    const size_t input_size = sizeof(input) - 1u;
    AFORC_Status status;

    limits.max_input_bytes = input_size;
    limits.max_line_bytes = 3u;
    limits.max_entries = 1u;
    limits.max_section_bytes = 1u;
    limits.max_key_bytes = 1u;
    limits.max_value_bytes = 1u;
    status = aforc_config_parse(input, input_size, &limits, &config);
    if (status != AFORC_OK || config.count != 1u ||
        strcmp(config.entries[0].section, "s") != 0 ||
        strcmp(config.entries[0].key, "k") != 0 ||
        strcmp(config.entries[0].value, "v") != 0)
    {
        aforc_config_release(&config);
        return false;
    }
    aforc_config_release(&config);
    return true;
}

static bool test_individual_limits(void)
{
    static const char input[] = "[section]\nkey=value\nother=second\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();

    limits.max_input_bytes = sizeof(input) - 2u;
    if (!parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT))
    {
        return false;
    }
    limits = aforc_config_limits_default();
    limits.max_line_bytes = 4u;
    if (!parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT))
    {
        return false;
    }
    limits = aforc_config_limits_default();
    limits.max_section_bytes = 6u;
    if (!parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT))
    {
        return false;
    }
    limits = aforc_config_limits_default();
    limits.max_key_bytes = 2u;
    if (!parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT))
    {
        return false;
    }
    limits = aforc_config_limits_default();
    limits.max_value_bytes = 4u;
    if (!parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT))
    {
        return false;
    }
    limits = aforc_config_limits_default();
    limits.max_entries = 1u;
    return parse_status(input, sizeof(input) - 1u, &limits, AFORC_ERROR_LIMIT);
}

static bool test_hostile_format_and_duplicates(void)
{
    static const char control[] = {'k', '=', 'v', '\x1f'};
    static const char duplicate[] = "[same]\nkey=one\n[same]\nkey=two\n";
    AFORC_ConfigLimits limits = aforc_config_limits_default();

    return parse_status(NULL, 1u, &limits, AFORC_ERROR_INVALID_ARGUMENT) &&
           parse_status(
               control, sizeof(control), &limits, AFORC_ERROR_FORMAT) &&
           parse_status(
               duplicate, sizeof(duplicate) - 1u, &limits, AFORC_ERROR_EXISTS);
}

int main(void)
{
    if (!test_empty_and_lookup() || !test_exact_limits() ||
        !test_individual_limits() || !test_hostile_format_and_duplicates())
    {
        (void)fputs("config boundary regression failed\n", stderr);
        return 1;
    }
    (void)puts("config boundaries: ok");
    return 0;
}
