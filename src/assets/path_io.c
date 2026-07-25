/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * Owns lexical path policy and bounded stdio blob/text I/O. Filesystem
 * resolution and confinement remain caller/platform trust boundaries.
 */

#include "../../include/aforc/assets.h"

#include "assets_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AFORC_PathParts {
    size_t root_size;
    size_t relative_size;
    size_t total_size;
    bool add_separator;
} AFORC_PathParts;

static AFORC_Status aforc_bounded_string_size(
    const char *text,
    size_t limit,
    size_t *output)
{
    size_t index;

    if (text == NULL || output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u;; ++index) {
        if (text[index] == '\0') {
            *output = index;
            return AFORC_OK;
        }
        if (index == limit) {
            return AFORC_ERROR_LIMIT;
        }
    }
}

static bool aforc_path_character_rejected(unsigned char character)
{
    return character < UINT8_C(32) || character == UINT8_C(127) ||
           character == (unsigned char)'\\' ||
           character == (unsigned char)':';
}

AFORC_AssetPathPolicy aforc_asset_path_policy_default(void)
{
    AFORC_AssetPathPolicy policy;

    policy.max_path_bytes = AFORC_ASSET_DEFAULT_MAX_PATH_BYTES;
    policy.max_component_bytes = AFORC_ASSET_DEFAULT_MAX_COMPONENT_BYTES;
    policy.max_depth = AFORC_ASSET_DEFAULT_MAX_DEPTH;
    policy.allow_hidden_components = false;
    return policy;
}

AFORC_Status aforc_asset_path_validate(
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy)
{
    size_t path_size;
    size_t component_start;
    size_t index;
    size_t depth;
    AFORC_Status status;

    if (relative_path == NULL || policy == NULL ||
        policy->max_path_bytes == SIZE_MAX) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_bounded_string_size(
        relative_path,
        policy->max_path_bytes,
        &path_size);
    if (status != AFORC_OK) {
        return status;
    }
    if (path_size == 0u || relative_path[0] == '/') {
        return AFORC_ERROR_FORMAT;
    }

    component_start = 0u;
    depth = 0u;
    for (index = 0u; index <= path_size; ++index) {
        if (index < path_size) {
            unsigned char character = (unsigned char)relative_path[index];

            if (aforc_path_character_rejected(character)) {
                return AFORC_ERROR_FORMAT;
            }
            if (character != (unsigned char)'/') {
                continue;
            }
        }

        {
            size_t component_size = index - component_start;

            if (component_size == 0u) {
                return AFORC_ERROR_FORMAT;
            }
            if (component_size > policy->max_component_bytes) {
                return AFORC_ERROR_LIMIT;
            }
            if ((component_size == 1u &&
                 relative_path[component_start] == '.') ||
                (component_size == 2u &&
                 relative_path[component_start] == '.' &&
                 relative_path[component_start + 1u] == '.')) {
                return AFORC_ERROR_FORMAT;
            }
            if (!policy->allow_hidden_components &&
                relative_path[component_start] == '.') {
                return AFORC_ERROR_FORMAT;
            }
            if (depth == policy->max_depth) {
                return AFORC_ERROR_LIMIT;
            }
            ++depth;
            component_start = index + 1u;
        }
    }
    return AFORC_OK;
}

static AFORC_Status aforc_path_measure(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    AFORC_PathParts *parts)
{
    AFORC_Status status;
    size_t index;
    size_t total_size;
    bool add_separator;

    if (root == NULL || parts == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_asset_path_validate(relative_path, policy);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_bounded_string_size(
        root,
        policy->max_path_bytes,
        &parts->root_size);
    if (status != AFORC_OK) {
        return status;
    }
    status = aforc_bounded_string_size(
        relative_path,
        policy->max_path_bytes,
        &parts->relative_size);
    if (status != AFORC_OK) {
        return status;
    }

    for (index = 0u; index < parts->root_size; ++index) {
        unsigned char character = (unsigned char)root[index];

        if (character < UINT8_C(32) || character == UINT8_C(127)) {
            return AFORC_ERROR_FORMAT;
        }
    }

    add_separator = parts->root_size > 0u &&
                    root[parts->root_size - 1u] != '/' &&
                    root[parts->root_size - 1u] != '\\';
    total_size = parts->root_size;
    if (add_separator) {
        if (total_size == policy->max_path_bytes) {
            return AFORC_ERROR_LIMIT;
        }
        ++total_size;
    }
    if (parts->relative_size > policy->max_path_bytes - total_size) {
        return AFORC_ERROR_LIMIT;
    }
    total_size += parts->relative_size;

    parts->total_size = total_size;
    parts->add_separator = add_separator;
    return AFORC_OK;
}

static void aforc_path_copy(
    const char *root,
    const char *relative_path,
    const AFORC_PathParts *parts,
    char *destination)
{
    size_t offset = 0u;

    if (parts->root_size > 0u) {
        memcpy(destination, root, parts->root_size);
        offset = parts->root_size;
    }
    if (parts->add_separator) {
        destination[offset] = '/';
        ++offset;
    }
    memcpy(destination + offset, relative_path, parts->relative_size);
    destination[parts->total_size] = '\0';
}

AFORC_Status aforc_asset_path_join(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    char *destination,
    size_t destination_capacity)
{
    AFORC_PathParts parts;
    AFORC_Status status;
    char *joined;
    size_t allocation_size;

    if (destination == NULL || destination_capacity == 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_path_measure(root, relative_path, policy, &parts);
    if (status != AFORC_OK) {
        destination[0] = '\0';
        return status;
    }
    if (parts.total_size >= destination_capacity) {
        destination[0] = '\0';
        return AFORC_ERROR_LIMIT;
    }
    if (!aforc_size_add(parts.total_size, 1u, &allocation_size)) {
        destination[0] = '\0';
        return AFORC_ERROR_LIMIT;
    }

    joined = malloc(allocation_size);
    if (joined == NULL) {
        destination[0] = '\0';
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    aforc_path_copy(root, relative_path, &parts, joined);
    memmove(destination, joined, allocation_size);
    free(joined);
    return AFORC_OK;
}

static AFORC_Status aforc_path_allocate(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    char **output)
{
    AFORC_PathParts parts;
    AFORC_Status status;
    char *path;
    size_t allocation_size;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *output = NULL;
    status = aforc_path_measure(root, relative_path, policy, &parts);
    if (status != AFORC_OK) {
        return status;
    }
    if (!aforc_size_add(parts.total_size, 1u, &allocation_size)) {
        return AFORC_ERROR_LIMIT;
    }
    path = malloc(allocation_size);
    if (path == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    aforc_path_copy(root, relative_path, &parts, path);
    *output = path;
    return AFORC_OK;
}

static AFORC_Status aforc_file_open_status(int error_number)
{
    if (error_number == ENOENT || error_number == ENOTDIR) {
        return AFORC_ERROR_NOT_FOUND;
    }
    return AFORC_ERROR_IO;
}

static AFORC_Status aforc_stream_load(
    FILE *stream,
    size_t max_bytes,
    bool text_mode,
    uint8_t **output_data,
    size_t *output_size)
{
    uint8_t *data = NULL;
    size_t size = 0u;
    size_t capacity = 0u;
    AFORC_Status status = AFORC_OK;

    if (stream == NULL || output_data == NULL || output_size == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *output_data = NULL;
    *output_size = 0u;
    if (text_mode && max_bytes == SIZE_MAX) {
        return AFORC_ERROR_LIMIT;
    }

    for (;;) {
        if (size == capacity) {
            size_t next_capacity;
            size_t allocation_size;
            size_t required;
            uint8_t *resized;

            if (capacity == max_bytes) {
                int extra = fgetc(stream);

                if (extra != EOF) {
                    status = AFORC_ERROR_LIMIT;
                } else if (ferror(stream) != 0) {
                    status = AFORC_ERROR_IO;
                }
                break;
            }
            if (!aforc_size_add(capacity, 1u, &required) ||
                !aforc_assets_growth_capacity(
                    capacity,
                    required,
                    max_bytes,
                    4096u,
                    &next_capacity)) {
                status = AFORC_ERROR_LIMIT;
                break;
            }
            allocation_size = next_capacity;
            if (text_mode &&
                !aforc_size_add(allocation_size, 1u, &allocation_size)) {
                status = AFORC_ERROR_LIMIT;
                break;
            }
            resized = realloc(data, allocation_size);
            if (resized == NULL) {
                status = AFORC_ERROR_OUT_OF_MEMORY;
                break;
            }
            data = resized;
            capacity = next_capacity;
        }

        {
            size_t available = capacity - size;
            size_t count = fread(data + size, 1u, available, stream);

            size += count;
            if (count < available) {
                if (ferror(stream) != 0 || feof(stream) == 0) {
                    status = AFORC_ERROR_IO;
                }
                break;
            }
        }
    }

    /* Empty binary blobs deliberately carry no allocation; empty text still does. */
    if (status == AFORC_OK && !text_mode && size == 0u) {
        free(data);
        data = NULL;
    }
    if (status == AFORC_OK && text_mode) {
        if (data == NULL) {
            data = malloc(1u);
            if (data == NULL) {
                status = AFORC_ERROR_OUT_OF_MEMORY;
            }
        }
        if (status == AFORC_OK && size > 0u &&
            memchr(data, '\0', size) != NULL) {
            status = AFORC_ERROR_FORMAT;
        }
        if (status == AFORC_OK) {
            data[size] = UINT8_C(0);
        }
    }

    if (status != AFORC_OK) {
        free(data);
        return status;
    }
    *output_data = data;
    *output_size = size;
    return AFORC_OK;
}

static AFORC_Status aforc_asset_load_file(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    size_t max_bytes,
    bool text_mode,
    uint8_t **output_data,
    size_t *output_size)
{
    char *path;
    FILE *stream;
    AFORC_Status status;
    int close_result;
    int open_error;

    if (output_data == NULL || output_size == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    *output_data = NULL;
    *output_size = 0u;
    status = aforc_path_allocate(root, relative_path, policy, &path);
    if (status != AFORC_OK) {
        return status;
    }

    errno = 0;
    stream = fopen(path, "rb");
    open_error = errno;
    free(path);
    if (stream == NULL) {
        return aforc_file_open_status(open_error);
    }

    status = aforc_stream_load(
        stream,
        max_bytes,
        text_mode,
        output_data,
        output_size);
    close_result = fclose(stream);
    if (close_result != 0 && status == AFORC_OK) {
        free(*output_data);
        *output_data = NULL;
        *output_size = 0u;
        return AFORC_ERROR_IO;
    }
    return status;
}

AFORC_Status aforc_asset_load_binary(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    size_t max_bytes,
    AFORC_AssetBlob *output)
{
    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->data = NULL;
    output->size = 0u;
    return aforc_asset_load_file(
        root,
        relative_path,
        policy,
        max_bytes,
        false,
        &output->data,
        &output->size);
}

AFORC_Status aforc_asset_load_text(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    size_t max_bytes,
    AFORC_AssetText *output)
{
    uint8_t *data;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->data = NULL;
    output->size = 0u;
    status = aforc_asset_load_file(
        root,
        relative_path,
        policy,
        max_bytes,
        true,
        &data,
        &output->size);
    if (status == AFORC_OK) {
        output->data = (char *)data;
    }
    return status;
}

AFORC_Status aforc_asset_store_binary(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    const void *data,
    size_t size,
    size_t max_bytes)
{
    char *path;
    FILE *stream;
    AFORC_Status status;
    size_t written = 0u;
    int close_result;
    int open_error;

    if ((data == NULL && size != 0u) || size > max_bytes) {
        return size > max_bytes ? AFORC_ERROR_LIMIT : AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_path_allocate(root, relative_path, policy, &path);
    if (status != AFORC_OK) {
        return status;
    }

    /* Direct stream writes preserve the documented non-transactional contract. */
    errno = 0;
    stream = fopen(path, "wb");
    open_error = errno;
    free(path);
    if (stream == NULL) {
        return aforc_file_open_status(open_error);
    }

    status = AFORC_OK;
    while (written < size) {
        const uint8_t *bytes = (const uint8_t *)data;
        size_t count = fwrite(bytes + written, 1u, size - written, stream);

        if (count == 0u) {
            status = AFORC_ERROR_IO;
            break;
        }
        written += count;
    }
    close_result = fclose(stream);
    if (close_result != 0) {
        status = AFORC_ERROR_IO;
    }
    return status;
}
