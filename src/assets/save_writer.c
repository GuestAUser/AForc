/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns bounded growth and primitive writes for caller-owned save payloads. */

#include "save_internal.h"

#include "assets_internal.h"

#include <stdlib.h>
#include <string.h>

bool aforc_save_writer_valid(const AFORC_SaveWriter *writer)
{
    return writer != NULL && writer->initialized &&
           writer->size <= writer->capacity &&
           writer->capacity <= writer->max_payload_bytes &&
           ((writer->capacity == 0u && writer->payload == NULL) ||
            (writer->capacity != 0u && writer->payload != NULL));
}

static bool aforc_save_writer_source_offset(const AFORC_SaveWriter *writer,
                                            const void *source,
                                            size_t source_size,
                                            size_t *output_offset)
{
    uintptr_t payload_address;
    uintptr_t source_address;
    uintptr_t offset;

    if (source == NULL || source_size == 0u || writer->payload == NULL)
    {
        return false;
    }
    payload_address = (uintptr_t)writer->payload;
    source_address = (uintptr_t)source;
    if (source_address < payload_address)
    {
        return false;
    }
    offset = source_address - payload_address;
    if (offset > (uintptr_t)writer->size ||
        source_size > writer->size - (size_t)offset)
    {
        return false;
    }
    *output_offset = (size_t)offset;
    return true;
}

static AFORC_Status aforc_save_writer_reserve(AFORC_SaveWriter *writer,
                                              size_t additional_size)
{
    size_t required;
    size_t next_capacity;
    uint8_t *resized;

    if (writer == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer))
    {
        return AFORC_ERROR_STATE;
    }
    if (additional_size > writer->max_payload_bytes - writer->size ||
        !aforc_size_add(writer->size, additional_size, &required))
    {
        return AFORC_ERROR_LIMIT;
    }
    if (required <= writer->capacity)
    {
        return AFORC_OK;
    }
    if (!aforc_assets_growth_capacity(writer->capacity,
                                      required,
                                      writer->max_payload_bytes,
                                      64u,
                                      &next_capacity))
    {
        return AFORC_ERROR_LIMIT;
    }

    resized = realloc(writer->payload, next_capacity);
    if (resized == NULL)
    {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    writer->payload = resized;
    writer->capacity = next_capacity;
    return AFORC_OK;
}

static AFORC_Status
aforc_save_writer_take(AFORC_SaveWriter *writer, size_t size, uint8_t **output)
{
    AFORC_SaveBoundedWriter bytes;
    AFORC_Status status;

    status = aforc_save_writer_reserve(writer, size);
    if (status != AFORC_OK)
    {
        return status;
    }
    aforc_save_bounded_writer_init(
        &bytes, writer->payload, writer->capacity, writer->size);
    if (!aforc_save_bounded_writer_take(&bytes, size, output))
    {
        return AFORC_ERROR_STATE;
    }
    writer->size = bytes.cursor;
    return AFORC_OK;
}

AFORC_Status aforc_save_writer_init(AFORC_SaveWriter *writer,
                                    uint32_t schema_version,
                                    size_t max_payload_bytes)
{
    if (writer == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    writer->payload = NULL;
    writer->size = 0u;
    writer->capacity = 0u;
    writer->max_payload_bytes = max_payload_bytes;
    writer->schema_version = schema_version;
    writer->initialized = true;
    return AFORC_OK;
}

void aforc_save_writer_release(AFORC_SaveWriter *writer)
{
    if (writer == NULL)
    {
        return;
    }
    free(writer->payload);
    writer->payload = NULL;
    writer->size = 0u;
    writer->capacity = 0u;
    writer->max_payload_bytes = 0u;
    writer->schema_version = 0u;
    writer->initialized = false;
}

AFORC_Status aforc_save_writer_write_u8(AFORC_SaveWriter *writer, uint8_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 1u, &destination);

    if (status == AFORC_OK)
    {
        destination[0] = value;
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u16(AFORC_SaveWriter *writer,
                                         uint16_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 2u, &destination);

    if (status == AFORC_OK)
    {
        aforc_save_store_u16(destination, value);
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u32(AFORC_SaveWriter *writer,
                                         uint32_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 4u, &destination);

    if (status == AFORC_OK)
    {
        aforc_save_store_u32(destination, value);
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u64(AFORC_SaveWriter *writer,
                                         uint64_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 8u, &destination);

    if (status == AFORC_OK)
    {
        aforc_save_store_u64(destination, value);
    }
    return status;
}

AFORC_Status aforc_save_writer_write_i32(AFORC_SaveWriter *writer,
                                         int32_t value)
{
    return aforc_save_writer_write_u32(writer,
                                       aforc_save_signed_i32_bits(value));
}

AFORC_Status aforc_save_writer_write_i64(AFORC_SaveWriter *writer,
                                         int64_t value)
{
    return aforc_save_writer_write_u64(writer,
                                       aforc_save_signed_i64_bits(value));
}

AFORC_Status aforc_save_writer_write_bytes(AFORC_SaveWriter *writer,
                                           const void *data,
                                           size_t size)
{
    AFORC_Status status;
    size_t source_offset = 0u;
    bool source_is_payload;
    uint8_t *destination;

    if (data == NULL && size != 0u)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (writer == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer))
    {
        return AFORC_ERROR_STATE;
    }
    if (size == 0u)
    {
        return AFORC_OK;
    }
    source_is_payload =
        aforc_save_writer_source_offset(writer, data, size, &source_offset);
    status = aforc_save_writer_take(writer, size, &destination);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (source_is_payload)
    {
        data = writer->payload + source_offset;
    }
    memmove(destination, data, size);
    return AFORC_OK;
}

AFORC_Status aforc_save_writer_write_string(AFORC_SaveWriter *writer,
                                            const char *text,
                                            size_t size)
{
    size_t total_size;
    AFORC_Status status;
    size_t source_offset = 0u;
    bool source_is_payload;
    uint8_t *destination;

    if (text == NULL && size != 0u)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (size > UINT32_MAX)
    {
        return AFORC_ERROR_LIMIT;
    }
    if (!aforc_size_add(size, 4u, &total_size))
    {
        return AFORC_ERROR_OVERFLOW;
    }
    if (writer == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer))
    {
        return AFORC_ERROR_STATE;
    }
    source_is_payload =
        aforc_save_writer_source_offset(writer, text, size, &source_offset);
    status = aforc_save_writer_take(writer, total_size, &destination);
    if (status != AFORC_OK)
    {
        return status;
    }
    aforc_save_store_u32(destination, (uint32_t)size);
    if (size > 0u)
    {
        const char *source =
            source_is_payload ? (const char *)(writer->payload + source_offset)
                              : text;

        memmove(destination + 4u, source, size);
    }
    return AFORC_OK;
}
