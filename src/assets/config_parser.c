/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * Owns bounded parsing of caller-supplied bytes. Input is untrusted and output
 * is committed only after the full document validates.
 */

#include "../../include/aforc/assets.h"

#include "config_internal.h"

#include <stdlib.h>

AFORC_ConfigLimits aforc_config_limits_default(void)
{
    AFORC_ConfigLimits limits;

    limits.max_input_bytes = AFORC_CONFIG_DEFAULT_MAX_INPUT_BYTES;
    limits.max_line_bytes = AFORC_CONFIG_DEFAULT_MAX_LINE_BYTES;
    limits.max_entries = AFORC_CONFIG_DEFAULT_MAX_ENTRIES;
    limits.max_section_bytes = AFORC_CONFIG_DEFAULT_MAX_SECTION_BYTES;
    limits.max_key_bytes = AFORC_CONFIG_DEFAULT_MAX_KEY_BYTES;
    limits.max_value_bytes = AFORC_CONFIG_DEFAULT_MAX_VALUE_BYTES;
    return limits;
}

static bool aforc_config_space(unsigned char character)
{
    return character == (unsigned char)' ' || character == (unsigned char)'\t';
}

static void aforc_config_trim(const char *input, size_t *start, size_t *end)
{
    while (*start < *end && aforc_config_space((unsigned char)input[*start]))
    {
        ++(*start);
    }
    while (*end > *start && aforc_config_space((unsigned char)input[*end - 1u]))
    {
        --(*end);
    }
}

static bool aforc_config_name_character(unsigned char character)
{
    return (character >= (unsigned char)'a' &&
            character <= (unsigned char)'z') ||
           (character >= (unsigned char)'A' &&
            character <= (unsigned char)'Z') ||
           (character >= (unsigned char)'0' &&
            character <= (unsigned char)'9') ||
           character == (unsigned char)'_' || character == (unsigned char)'-' ||
           character == (unsigned char)'.';
}

static bool aforc_config_name_valid(const char *input, size_t start, size_t end)
{
    size_t index;

    if (start == end)
    {
        return false;
    }
    for (index = start; index < end; ++index)
    {
        if (!aforc_config_name_character((unsigned char)input[index]))
        {
            return false;
        }
    }
    return true;
}

static AFORC_Status aforc_config_validate_input(const char *input,
                                                size_t input_size)
{
    size_t index;

    for (index = 0u; index < input_size; ++index)
    {
        unsigned char character = (unsigned char)input[index];

        if (character == UINT8_C(0) || character == UINT8_C(127) ||
            (character < UINT8_C(32) && character != (unsigned char)'\n' &&
             character != (unsigned char)'\r' &&
             character != (unsigned char)'\t'))
        {
            return AFORC_ERROR_FORMAT;
        }
        if (character == (unsigned char)'\r' &&
            (index == input_size - 1u || input[index + 1u] != '\n'))
        {
            return AFORC_ERROR_FORMAT;
        }
    }
    return AFORC_OK;
}

AFORC_Status aforc_config_parse(const char *input,
                                size_t input_size,
                                const AFORC_ConfigLimits *limits,
                                AFORC_Config *output)
{
    AFORC_ConfigBuilder builder = {NULL, 0u, 0u, 0u, {NULL, 0u}};
    char *section = NULL;
    size_t section_size = 0u;
    size_t position = 0u;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->entries = NULL;
    output->count = 0u;
    if ((input == NULL && input_size != 0u) || limits == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (input_size > limits->max_input_bytes)
    {
        return AFORC_ERROR_LIMIT;
    }
    status = aforc_config_validate_input(input, input_size);
    if (status != AFORC_OK)
    {
        return status;
    }
    builder.maximum = limits->max_entries;

    while (position < input_size)
    {
        size_t raw_start = position;
        size_t raw_end;
        size_t start;
        size_t end;

        while (position < input_size && input[position] != '\n')
        {
            ++position;
        }
        raw_end = position;
        if (position < input_size)
        {
            ++position;
        }
        if (raw_end > raw_start && input[raw_end - 1u] == '\r')
        {
            --raw_end;
        }

        /* Raw lines are bounded before trimming; token limits apply afterward.
         */
        if (raw_end - raw_start > limits->max_line_bytes)
        {
            status = AFORC_ERROR_LIMIT;
            goto fail;
        }
        start = raw_start;
        end = raw_end;
        aforc_config_trim(input, &start, &end);
        if (start == end || input[start] == '#' || input[start] == ';')
        {
            continue;
        }

        if (input[start] == '[')
        {
            size_t name_start;
            size_t name_end;
            AFORC_ConfigRange name;
            char *new_section;

            if (end - start < 3u || input[end - 1u] != ']')
            {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            name_start = start + 1u;
            name_end = end - 1u;
            aforc_config_trim(input, &name_start, &name_end);
            name.data = input + name_start;
            name.size = name_end - name_start;
            if (name.size > limits->max_section_bytes)
            {
                status = AFORC_ERROR_LIMIT;
                goto fail;
            }
            if (!aforc_config_name_valid(input, name_start, name_end))
            {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            status = aforc_config_duplicate_range(name, &new_section);
            if (status != AFORC_OK)
            {
                goto fail;
            }
            free(section);
            section = new_section;
            section_size = name.size;
        }
        else
        {
            size_t equals = start;
            size_t key_start = start;
            size_t key_end;
            size_t value_start;
            size_t value_end = end;
            AFORC_ConfigRange section_range = {section, section_size};
            AFORC_ConfigRange key;
            AFORC_ConfigRange value;

            while (equals < end && input[equals] != '=')
            {
                ++equals;
            }
            if (equals == end)
            {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            key_start = start;
            key_end = equals;
            value_start = equals + 1u;
            aforc_config_trim(input, &key_start, &key_end);
            aforc_config_trim(input, &value_start, &value_end);
            key.data = input + key_start;
            key.size = key_end - key_start;
            value.data = input + value_start;
            value.size = value_end - value_start;

            if (key.size > limits->max_key_bytes ||
                value.size > limits->max_value_bytes)
            {
                status = AFORC_ERROR_LIMIT;
                goto fail;
            }
            if (!aforc_config_name_valid(input, key_start, key_end))
            {
                status = AFORC_ERROR_FORMAT;
                goto fail;
            }
            status = aforc_config_builder_append(
                &builder, section_range, key, value);
            if (status != AFORC_OK)
            {
                goto fail;
            }
        }
    }

    free(section);
    aforc_config_index_release(&builder.index);
    output->entries = builder.entries;
    output->count = builder.count;
    return AFORC_OK;

fail:
    free(section);
    aforc_config_index_release(&builder.index);
    aforc_config_entries_release(builder.entries, builder.count);
    return status;
}
