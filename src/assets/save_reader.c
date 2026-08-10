/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns transactional reads from validated, borrowed save payloads. */

#include "save_internal.h"

#include <string.h>

static bool aforc_save_reader_valid(const AFORC_SaveReader *reader)
{
    return reader != NULL && reader->initialized &&
           reader->cursor <= reader->size && reader->payload != NULL;
}

size_t aforc_save_reader_remaining(const AFORC_SaveReader *reader)
{
    if (!aforc_save_reader_valid(reader))
    {
        return 0u;
    }
    return reader->size - reader->cursor;
}

bool aforc_save_reader_finished(const AFORC_SaveReader *reader)
{
    return aforc_save_reader_valid(reader) && reader->cursor == reader->size;
}

static AFORC_Status aforc_save_reader_take(AFORC_SaveReader *reader,
                                           size_t size,
                                           const uint8_t **output)
{
    AFORC_SaveBoundedReader bytes;

    if (reader == NULL || output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_reader_valid(reader))
    {
        return AFORC_ERROR_STATE;
    }
    aforc_save_bounded_reader_init(&bytes, reader->payload, reader->size);
    bytes.cursor = reader->cursor;
    if (!aforc_save_bounded_reader_take(&bytes, size, output))
    {
        return AFORC_ERROR_END_OF_STREAM;
    }
    reader->cursor = bytes.cursor;
    return AFORC_OK;
}

AFORC_Status aforc_save_reader_read_u8(AFORC_SaveReader *reader,
                                       uint8_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 1u, &data);
    if (status == AFORC_OK)
    {
        *output = data[0];
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u16(AFORC_SaveReader *reader,
                                        uint16_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 2u, &data);
    if (status == AFORC_OK)
    {
        *output = aforc_save_load_u16(data);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u32(AFORC_SaveReader *reader,
                                        uint32_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 4u, &data);
    if (status == AFORC_OK)
    {
        *output = aforc_save_load_u32(data);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u64(AFORC_SaveReader *reader,
                                        uint64_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 8u, &data);
    if (status == AFORC_OK)
    {
        *output = aforc_save_load_u64(data);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_i32(AFORC_SaveReader *reader,
                                        int32_t *output)
{
    uint32_t bits;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_read_u32(reader, &bits);
    if (status == AFORC_OK)
    {
        *output = aforc_save_signed_i32_value(bits);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_i64(AFORC_SaveReader *reader,
                                        int64_t *output)
{
    uint64_t bits;
    AFORC_Status status;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_read_u64(reader, &bits);
    if (status == AFORC_OK)
    {
        *output = aforc_save_signed_i64_value(bits);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_bytes(AFORC_SaveReader *reader,
                                          void *destination,
                                          size_t size)
{
    const uint8_t *data;
    AFORC_Status status;

    if (destination == NULL && size != 0u)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, size, &data);
    if (status == AFORC_OK && size > 0u)
    {
        memmove(destination, data, size);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_string(AFORC_SaveReader *reader,
                                           AFORC_AssetView *output)
{
    size_t initial_cursor;
    uint32_t encoded_size;
    size_t string_size;
    const uint8_t *data;
    AFORC_Status status;

    if (reader == NULL || output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_reader_valid(reader))
    {
        return AFORC_ERROR_STATE;
    }
    output->data = NULL;
    output->size = 0u;
    initial_cursor = reader->cursor;
    status = aforc_save_reader_read_u32(reader, &encoded_size);
    if (status != AFORC_OK)
    {
        return status;
    }
#if SIZE_MAX < UINT32_MAX
    if (encoded_size > (uint32_t)SIZE_MAX)
    {
        reader->cursor = initial_cursor;
        return AFORC_ERROR_LIMIT;
    }
#endif
    string_size = (size_t)encoded_size;
    status = aforc_save_reader_take(reader, string_size, &data);
    if (status != AFORC_OK)
    {
        reader->cursor = initial_cursor;
        return status;
    }
    output->data = data;
    output->size = string_size;
    return AFORC_OK;
}
