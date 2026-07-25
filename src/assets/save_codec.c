/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns checked cursor movement, endian primitives, and signed bit mappings. */

#include "save_internal.h"

#include <limits.h>
#include <string.h>

void aforc_save_bounded_reader_init(
    AFORC_SaveBoundedReader *reader,
    const void *data,
    size_t size)
{
    reader->data = (const uint8_t *)data;
    reader->size = size;
    reader->cursor = 0u;
}

bool aforc_save_bounded_reader_take(
    AFORC_SaveBoundedReader *reader,
    size_t size,
    const uint8_t **output)
{
    if (reader == NULL || output == NULL || reader->cursor > reader->size ||
        size > reader->size - reader->cursor) {
        return false;
    }
    *output = reader->data + reader->cursor;
    reader->cursor += size;
    return true;
}

void aforc_save_bounded_writer_init(
    AFORC_SaveBoundedWriter *writer,
    void *data,
    size_t size,
    size_t cursor)
{
    writer->data = (uint8_t *)data;
    writer->size = size;
    writer->cursor = cursor;
}

bool aforc_save_bounded_writer_take(
    AFORC_SaveBoundedWriter *writer,
    size_t size,
    uint8_t **output)
{
    if (writer == NULL || output == NULL || writer->cursor > writer->size ||
        size > writer->size - writer->cursor) {
        return false;
    }
    *output = writer->data + writer->cursor;
    writer->cursor += size;
    return true;
}

/* Save bytes are explicitly little-endian, independent of host byte order. */
void aforc_save_store_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & UINT16_C(0x00ff));
    destination[1] = (uint8_t)(value >> 8u);
}

void aforc_save_store_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & UINT32_C(0x000000ff));
    destination[1] = (uint8_t)((value >> 8u) & UINT32_C(0x000000ff));
    destination[2] = (uint8_t)((value >> 16u) & UINT32_C(0x000000ff));
    destination[3] = (uint8_t)(value >> 24u);
}

void aforc_save_store_u64(uint8_t *destination, uint64_t value)
{
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        destination[index] = (uint8_t)(value & UINT64_C(0xff));
        value >>= 8u;
    }
}

uint16_t aforc_save_load_u16(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] |
                      (uint16_t)((uint16_t)source[1] << 8u));
}

uint32_t aforc_save_load_u32(const uint8_t *source)
{
    return (uint32_t)source[0]          |
           ((uint32_t)source[1] << 8u)  |
           ((uint32_t)source[2] << 16u) |
           ((uint32_t)source[3] << 24u);
}

uint64_t aforc_save_load_u64(const uint8_t *source)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        value |= (uint64_t)source[index] << (index * 8u);
    }
    return value;
}

bool aforc_save_bounded_writer_write_bytes(
    AFORC_SaveBoundedWriter *writer,
    const void *source,
    size_t size)
{
    uint8_t *destination;

    if (!aforc_save_bounded_writer_take(writer, size, &destination)) {
        return false;
    }
    if (size > 0u) {
        memcpy(destination, source, size);
    }
    return true;
}

bool aforc_save_bounded_writer_write_u16(
    AFORC_SaveBoundedWriter *writer,
    uint16_t value)
{
    uint8_t *destination;

    if (!aforc_save_bounded_writer_take(writer, 2u, &destination)) {
        return false;
    }
    aforc_save_store_u16(destination, value);
    return true;
}

bool aforc_save_bounded_writer_write_u32(
    AFORC_SaveBoundedWriter *writer,
    uint32_t value)
{
    uint8_t *destination;

    if (!aforc_save_bounded_writer_take(writer, 4u, &destination)) {
        return false;
    }
    aforc_save_store_u32(destination, value);
    return true;
}

bool aforc_save_bounded_writer_write_u64(
    AFORC_SaveBoundedWriter *writer,
    uint64_t value)
{
    uint8_t *destination;

    if (!aforc_save_bounded_writer_take(writer, 8u, &destination)) {
        return false;
    }
    aforc_save_store_u64(destination, value);
    return true;
}

uint32_t aforc_save_signed_i32_bits(int32_t value)
{
    if (value >= 0) {
        return (uint32_t)value;
    }
    {
        uint32_t magnitude = (uint32_t)(-(int64_t)value);
        return UINT32_MAX - magnitude + UINT32_C(1);
    }
}

uint64_t aforc_save_signed_i64_bits(int64_t value)
{
    uint64_t magnitude;

    if (value >= 0) {
        return (uint64_t)value;
    }
    magnitude = (uint64_t)(-(value + INT64_C(1)));
    ++magnitude;
    return UINT64_MAX - magnitude + UINT64_C(1);
}

int32_t aforc_save_signed_i32_value(uint32_t bits)
{
    if (bits <= (uint32_t)INT32_MAX) {
        return (int32_t)bits;
    }
    {
        uint32_t magnitude = UINT32_MAX - bits + UINT32_C(1);

        if (magnitude == (uint32_t)INT32_MAX + UINT32_C(1)) {
            return INT32_MIN;
        }
        return -(int32_t)magnitude;
    }
}

int64_t aforc_save_signed_i64_value(uint64_t bits)
{
    if (bits <= (uint64_t)INT64_MAX) {
        return (int64_t)bits;
    }
    {
        uint64_t magnitude = UINT64_MAX - bits + UINT64_C(1);

        if (magnitude == (uint64_t)INT64_MAX + UINT64_C(1)) {
            return INT64_MIN;
        }
        return -(int64_t)magnitude;
    }
}
