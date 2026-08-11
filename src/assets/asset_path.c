/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns lexical path policy and joining without claiming filesystem confinement.
 */

#include "../../include/aforc/assets.h"

#include "assets_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct AFORC_PathParts
{
    size_t root_size;
    size_t relative_size;
    size_t total_size;
    bool add_separator;
} AFORC_PathParts;

static AFORC_Status
aforc_bounded_string_size(const char *text, size_t limit, size_t *output)
{
    const char *terminator;

    if (text == NULL || output == NULL || limit == SIZE_MAX)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    terminator = memchr(text, '\0', limit + 1u);
    if (terminator == NULL)
    {
        return AFORC_ERROR_LIMIT;
    }
    *output = (size_t)(terminator - text);
    return AFORC_OK;
}

static bool aforc_path_character_rejected(unsigned char character)
{
    return character < UINT8_C(32) || character == UINT8_C(127) ||
           character == (unsigned char)'\\' || character == (unsigned char)':';
}

AFORC_AssetPathPolicy aforc_asset_path_policy_default(void)
{
    return (AFORC_AssetPathPolicy){AFORC_ASSET_DEFAULT_MAX_PATH_BYTES,
                                   AFORC_ASSET_DEFAULT_MAX_COMPONENT_BYTES,
                                   AFORC_ASSET_DEFAULT_MAX_DEPTH,
                                   false};
}

AFORC_Status aforc_asset_path_validate(const char *relative_path,
                                       const AFORC_AssetPathPolicy *policy)
{
    size_t path_size;
    size_t component_start;
    size_t index;
    size_t depth;
    AFORC_Status status;

    if (relative_path == NULL || policy == NULL ||
        policy->max_path_bytes == SIZE_MAX)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_bounded_string_size(
        relative_path, policy->max_path_bytes, &path_size);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (path_size == 0u || relative_path[0] == '/')
    {
        return AFORC_ERROR_FORMAT;
    }

    component_start = 0u;
    depth = 0u;
    for (index = 0u; index <= path_size; ++index)
    {
        if (index < path_size)
        {
            unsigned char character = (unsigned char)relative_path[index];

            if (aforc_path_character_rejected(character))
            {
                return AFORC_ERROR_FORMAT;
            }
            if (character != (unsigned char)'/')
            {
                continue;
            }
        }

        {
            size_t component_size = index - component_start;

            if (component_size == 0u)
            {
                return AFORC_ERROR_FORMAT;
            }
            if (component_size > policy->max_component_bytes)
            {
                return AFORC_ERROR_LIMIT;
            }
            if ((component_size == 1u &&
                 relative_path[component_start] == '.') ||
                (component_size == 2u &&
                 relative_path[component_start] == '.' &&
                 relative_path[component_start + 1u] == '.'))
            {
                return AFORC_ERROR_FORMAT;
            }
            if (!policy->allow_hidden_components &&
                relative_path[component_start] == '.')
            {
                return AFORC_ERROR_FORMAT;
            }
            if (depth == policy->max_depth)
            {
                return AFORC_ERROR_LIMIT;
            }
            ++depth;
            component_start = index + 1u;
        }
    }
    return AFORC_OK;
}

static AFORC_Status aforc_path_measure(const char *root,
                                       const char *relative_path,
                                       const AFORC_AssetPathPolicy *policy,
                                       AFORC_PathParts *parts)
{
    AFORC_Status status;
    size_t index;
    size_t total_size;

    if (root == NULL || parts == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_asset_path_validate(relative_path, policy);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_bounded_string_size(
        root, policy->max_path_bytes, &parts->root_size);
    if (status != AFORC_OK)
    {
        return status;
    }
    status = aforc_bounded_string_size(
        relative_path, policy->max_path_bytes, &parts->relative_size);
    if (status != AFORC_OK)
    {
        return status;
    }

    for (index = 0u; index < parts->root_size; ++index)
    {
        unsigned char character = (unsigned char)root[index];

        if (character < UINT8_C(32) || character == UINT8_C(127))
        {
            return AFORC_ERROR_FORMAT;
        }
    }

    parts->add_separator = parts->root_size > 0u &&
                           root[parts->root_size - 1u] != '/' &&
                           root[parts->root_size - 1u] != '\\';
    total_size = parts->root_size;
    if (parts->add_separator)
    {
        if (total_size == policy->max_path_bytes)
        {
            return AFORC_ERROR_LIMIT;
        }
        ++total_size;
    }
    if (parts->relative_size > policy->max_path_bytes - total_size)
    {
        return AFORC_ERROR_LIMIT;
    }
    total_size += parts->relative_size;

    parts->total_size = total_size;
    return AFORC_OK;
}

static void aforc_path_copy(const char *root,
                            const char *relative_path,
                            const AFORC_PathParts *parts,
                            char *destination)
{
    size_t offset = 0u;

    if (parts->root_size > 0u)
    {
        memcpy(destination, root, parts->root_size);
        offset = parts->root_size;
    }
    if (parts->add_separator)
    {
        destination[offset] = '/';
        ++offset;
    }
    memcpy(destination + offset, relative_path, parts->relative_size);
    destination[parts->total_size] = '\0';
}

AFORC_Status aforc_asset_path_join(const char *root,
                                   const char *relative_path,
                                   const AFORC_AssetPathPolicy *policy,
                                   char *destination,
                                   size_t destination_capacity)
{
    AFORC_PathParts parts;
    AFORC_Status status;
    char *joined;

    if (destination == NULL || destination_capacity == 0u)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_path_measure(root, relative_path, policy, &parts);
    if (status != AFORC_OK)
    {
        destination[0] = '\0';
        return status;
    }
    if (parts.total_size >= destination_capacity)
    {
        destination[0] = '\0';
        return AFORC_ERROR_LIMIT;
    }
    joined = malloc(parts.total_size + 1u);
    if (joined == NULL)
    {
        destination[0] = '\0';
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    aforc_path_copy(root, relative_path, &parts, joined);
    memcpy(destination, joined, parts.total_size + 1u);
    free(joined);
    return AFORC_OK;
}

AFORC_Status aforc_asset_path_allocate(const char *root,
                                       const char *relative_path,
                                       const AFORC_AssetPathPolicy *policy,
                                       char **output)
{
    AFORC_PathParts parts;
    AFORC_Status status;
    char *path;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;
    status = aforc_path_measure(root, relative_path, policy, &parts);
    if (status != AFORC_OK)
    {
        return status;
    }
    path = malloc(parts.total_size + 1u);
    if (path == NULL)
    {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    aforc_path_copy(root, relative_path, &parts, path);
    *output = path;
    return AFORC_OK;
}
