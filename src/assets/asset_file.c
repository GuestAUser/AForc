/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

/* Owns bounded stdio blob/text I/O over paths produced by the lexical policy. */

#include "../../include/aforc/assets.h"

#include "assets_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static AFORC_Status aforc_file_open_status(int error_number)
{
    if (error_number == ENOENT || error_number == ENOTDIR) {
        return AFORC_ERROR_NOT_FOUND;
    }
    return AFORC_ERROR_IO;
}

static AFORC_Status aforc_stream_write_all(
    FILE *stream,
    const void *data,
    size_t size)
{
    const uint8_t *bytes = data;
    size_t written = 0u;

    while (written < size) {
        size_t count = fwrite(bytes + written, 1u, size - written, stream);

        if (count == 0u) {
            return AFORC_ERROR_IO;
        }
        written += count;
    }
    return AFORC_OK;
}

static AFORC_Status aforc_sync_parent_directory(const char *path)
{
    const char *separator = strrchr(path, '/');
    const char current_directory[] = ".";
    char *allocated_directory = NULL;
    const char *directory = current_directory;
    size_t directory_size;
    int descriptor;
    int open_error;
    AFORC_Status status = AFORC_OK;

    if (separator != NULL) {
        directory_size = separator == path ? 1u : (size_t)(separator - path);
        allocated_directory = malloc(directory_size + 1u);
        if (allocated_directory == NULL) {
            return AFORC_ERROR_OUT_OF_MEMORY;
        }
        memcpy(allocated_directory, path, directory_size);
        allocated_directory[directory_size] = '\0';
        directory = allocated_directory;
    }

    errno = 0;
    descriptor = open(directory, O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    open_error = errno;
    free(allocated_directory);
    if (descriptor < 0) {
        return aforc_file_open_status(open_error);
    }
    if (fsync(descriptor) != 0) {
        status = AFORC_ERROR_IO;
    }
    if (close(descriptor) != 0) {
        status = AFORC_ERROR_IO;
    }
    return status;
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
    status = aforc_asset_path_allocate(root, relative_path, policy, &path);
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
    int close_result;
    int open_error;

    if ((data == NULL && size != 0u) || size > max_bytes) {
        return size > max_bytes ? AFORC_ERROR_LIMIT : AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_asset_path_allocate(root, relative_path, policy, &path);
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

    status = aforc_stream_write_all(stream, data, size);
    close_result = fclose(stream);
    if (close_result != 0) {
        status = AFORC_ERROR_IO;
    }
    return status;
}

AFORC_Status aforc_asset_store_binary_atomic(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    const void *data,
    size_t size,
    size_t max_bytes)
{
    static const char temporary_name[] = ".aforc-asset-XXXXXX";
    char *path = NULL;
    char *temporary_path = NULL;
    FILE *stream = NULL;
    AFORC_Status status;
    const char *separator;
    size_t directory_prefix_size;
    size_t temporary_size;
    int descriptor = -1;
    int open_error;
    bool temporary_created = false;
    bool renamed = false;

    if ((data == NULL && size != 0u) || size > max_bytes) {
        return size > max_bytes ? AFORC_ERROR_LIMIT : AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_asset_path_allocate(root, relative_path, policy, &path);
    if (status != AFORC_OK) {
        return status;
    }
    separator = strrchr(path, '/');
    directory_prefix_size =
        separator == NULL ? 0u : (size_t)(separator - path) + 1u;
    if (!aforc_size_add(directory_prefix_size,
                        sizeof(temporary_name),
                        &temporary_size)) {
        free(path);
        return AFORC_ERROR_LIMIT;
    }
    temporary_path = malloc(temporary_size);
    if (temporary_path == NULL) {
        free(path);
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    if (directory_prefix_size > 0u) {
        memcpy(temporary_path, path, directory_prefix_size);
    }
    memcpy(temporary_path + directory_prefix_size,
           temporary_name,
           sizeof(temporary_name));

    errno = 0;
    descriptor = mkstemp(temporary_path);
    open_error = errno;
    if (descriptor < 0) {
        status = aforc_file_open_status(open_error);
        goto cleanup;
    }
    temporary_created = true;
    if (fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        status = AFORC_ERROR_IO;
        goto cleanup;
    }
    stream = fdopen(descriptor, "wb");
    if (stream == NULL) {
        status = AFORC_ERROR_IO;
        goto cleanup;
    }
    descriptor = -1;

    status = aforc_stream_write_all(stream, data, size);
    if (status == AFORC_OK && fflush(stream) != 0) {
        status = AFORC_ERROR_IO;
    }
    if (status == AFORC_OK) {
        int stream_descriptor = fileno(stream);

        if (stream_descriptor < 0 || fsync(stream_descriptor) != 0) {
            status = AFORC_ERROR_IO;
        }
    }
    if (fclose(stream) != 0) {
        status = AFORC_ERROR_IO;
    }
    stream = NULL;
    if (status != AFORC_OK) {
        goto cleanup;
    }
    if (rename(temporary_path, path) != 0) {
        status = AFORC_ERROR_IO;
        goto cleanup;
    }
    renamed = true;
    status = aforc_sync_parent_directory(path);

cleanup:
    if (stream != NULL) {
        if (fclose(stream) != 0) {
            status = AFORC_ERROR_IO;
        }
    } else if (descriptor >= 0 && close(descriptor) != 0) {
        status = AFORC_ERROR_IO;
    }
    if (temporary_created && !renamed) {
        (void)unlink(temporary_path);
    }
    free(temporary_path);
    free(path);
    return status;
}
