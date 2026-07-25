/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/*
 * Owns versioned save encoding and checked decoding. Writers own mutable
 * payloads; readers borrow untrusted container bytes only after validation.
 */

#include "../../include/aforc/assets.h"

#include "assets_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct AFORC_BoundedReader {
    const uint8_t *data;
    size_t size;
    size_t cursor;
} AFORC_BoundedReader;

typedef struct AFORC_BoundedWriter {
    uint8_t *data;
    size_t size;
    size_t cursor;
} AFORC_BoundedWriter;

static void aforc_bounded_reader_init(
    AFORC_BoundedReader *reader,
    const void *data,
    size_t size)
{
    reader->data = (const uint8_t *)data;
    reader->size = size;
    reader->cursor = 0u;
}

static bool aforc_bounded_reader_take(
    AFORC_BoundedReader *reader,
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

static void aforc_bounded_writer_init(
    AFORC_BoundedWriter *writer,
    void *data,
    size_t size,
    size_t cursor)
{
    writer->data = (uint8_t *)data;
    writer->size = size;
    writer->cursor = cursor;
}

static bool aforc_bounded_writer_take(
    AFORC_BoundedWriter *writer,
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
static void aforc_store_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & UINT16_C(0x00ff));
    destination[1] = (uint8_t)(value >> 8u);
}

static void aforc_store_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & UINT32_C(0x000000ff));
    destination[1] = (uint8_t)((value >> 8u) & UINT32_C(0x000000ff));
    destination[2] = (uint8_t)((value >> 16u) & UINT32_C(0x000000ff));
    destination[3] = (uint8_t)(value >> 24u);
}

static void aforc_store_u64(uint8_t *destination, uint64_t value)
{
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        destination[index] = (uint8_t)(value & UINT64_C(0xff));
        value >>= 8u;
    }
}

static uint16_t aforc_load_u16(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] |
                      (uint16_t)((uint16_t)source[1] << 8u));
}

static uint32_t aforc_load_u32(const uint8_t *source)
{
    return (uint32_t)source[0]          |
           ((uint32_t)source[1] << 8u)  |
           ((uint32_t)source[2] << 16u) |
           ((uint32_t)source[3] << 24u);
}

static uint64_t aforc_load_u64(const uint8_t *source)
{
    uint64_t value = UINT64_C(0);
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        value |= (uint64_t)source[index] << (index * 8u);
    }
    return value;
}

static const uint32_t aforc_crc32_table[256] = {
    UINT32_C(0x00000000), UINT32_C(0x77073096), UINT32_C(0xee0e612c), UINT32_C(0x990951ba),
    UINT32_C(0x076dc419), UINT32_C(0x706af48f), UINT32_C(0xe963a535), UINT32_C(0x9e6495a3),
    UINT32_C(0x0edb8832), UINT32_C(0x79dcb8a4), UINT32_C(0xe0d5e91e), UINT32_C(0x97d2d988),
    UINT32_C(0x09b64c2b), UINT32_C(0x7eb17cbd), UINT32_C(0xe7b82d07), UINT32_C(0x90bf1d91),
    UINT32_C(0x1db71064), UINT32_C(0x6ab020f2), UINT32_C(0xf3b97148), UINT32_C(0x84be41de),
    UINT32_C(0x1adad47d), UINT32_C(0x6ddde4eb), UINT32_C(0xf4d4b551), UINT32_C(0x83d385c7),
    UINT32_C(0x136c9856), UINT32_C(0x646ba8c0), UINT32_C(0xfd62f97a), UINT32_C(0x8a65c9ec),
    UINT32_C(0x14015c4f), UINT32_C(0x63066cd9), UINT32_C(0xfa0f3d63), UINT32_C(0x8d080df5),
    UINT32_C(0x3b6e20c8), UINT32_C(0x4c69105e), UINT32_C(0xd56041e4), UINT32_C(0xa2677172),
    UINT32_C(0x3c03e4d1), UINT32_C(0x4b04d447), UINT32_C(0xd20d85fd), UINT32_C(0xa50ab56b),
    UINT32_C(0x35b5a8fa), UINT32_C(0x42b2986c), UINT32_C(0xdbbbc9d6), UINT32_C(0xacbcf940),
    UINT32_C(0x32d86ce3), UINT32_C(0x45df5c75), UINT32_C(0xdcd60dcf), UINT32_C(0xabd13d59),
    UINT32_C(0x26d930ac), UINT32_C(0x51de003a), UINT32_C(0xc8d75180), UINT32_C(0xbfd06116),
    UINT32_C(0x21b4f4b5), UINT32_C(0x56b3c423), UINT32_C(0xcfba9599), UINT32_C(0xb8bda50f),
    UINT32_C(0x2802b89e), UINT32_C(0x5f058808), UINT32_C(0xc60cd9b2), UINT32_C(0xb10be924),
    UINT32_C(0x2f6f7c87), UINT32_C(0x58684c11), UINT32_C(0xc1611dab), UINT32_C(0xb6662d3d),
    UINT32_C(0x76dc4190), UINT32_C(0x01db7106), UINT32_C(0x98d220bc), UINT32_C(0xefd5102a),
    UINT32_C(0x71b18589), UINT32_C(0x06b6b51f), UINT32_C(0x9fbfe4a5), UINT32_C(0xe8b8d433),
    UINT32_C(0x7807c9a2), UINT32_C(0x0f00f934), UINT32_C(0x9609a88e), UINT32_C(0xe10e9818),
    UINT32_C(0x7f6a0dbb), UINT32_C(0x086d3d2d), UINT32_C(0x91646c97), UINT32_C(0xe6635c01),
    UINT32_C(0x6b6b51f4), UINT32_C(0x1c6c6162), UINT32_C(0x856530d8), UINT32_C(0xf262004e),
    UINT32_C(0x6c0695ed), UINT32_C(0x1b01a57b), UINT32_C(0x8208f4c1), UINT32_C(0xf50fc457),
    UINT32_C(0x65b0d9c6), UINT32_C(0x12b7e950), UINT32_C(0x8bbeb8ea), UINT32_C(0xfcb9887c),
    UINT32_C(0x62dd1ddf), UINT32_C(0x15da2d49), UINT32_C(0x8cd37cf3), UINT32_C(0xfbd44c65),
    UINT32_C(0x4db26158), UINT32_C(0x3ab551ce), UINT32_C(0xa3bc0074), UINT32_C(0xd4bb30e2),
    UINT32_C(0x4adfa541), UINT32_C(0x3dd895d7), UINT32_C(0xa4d1c46d), UINT32_C(0xd3d6f4fb),
    UINT32_C(0x4369e96a), UINT32_C(0x346ed9fc), UINT32_C(0xad678846), UINT32_C(0xda60b8d0),
    UINT32_C(0x44042d73), UINT32_C(0x33031de5), UINT32_C(0xaa0a4c5f), UINT32_C(0xdd0d7cc9),
    UINT32_C(0x5005713c), UINT32_C(0x270241aa), UINT32_C(0xbe0b1010), UINT32_C(0xc90c2086),
    UINT32_C(0x5768b525), UINT32_C(0x206f85b3), UINT32_C(0xb966d409), UINT32_C(0xce61e49f),
    UINT32_C(0x5edef90e), UINT32_C(0x29d9c998), UINT32_C(0xb0d09822), UINT32_C(0xc7d7a8b4),
    UINT32_C(0x59b33d17), UINT32_C(0x2eb40d81), UINT32_C(0xb7bd5c3b), UINT32_C(0xc0ba6cad),
    UINT32_C(0xedb88320), UINT32_C(0x9abfb3b6), UINT32_C(0x03b6e20c), UINT32_C(0x74b1d29a),
    UINT32_C(0xead54739), UINT32_C(0x9dd277af), UINT32_C(0x04db2615), UINT32_C(0x73dc1683),
    UINT32_C(0xe3630b12), UINT32_C(0x94643b84), UINT32_C(0x0d6d6a3e), UINT32_C(0x7a6a5aa8),
    UINT32_C(0xe40ecf0b), UINT32_C(0x9309ff9d), UINT32_C(0x0a00ae27), UINT32_C(0x7d079eb1),
    UINT32_C(0xf00f9344), UINT32_C(0x8708a3d2), UINT32_C(0x1e01f268), UINT32_C(0x6906c2fe),
    UINT32_C(0xf762575d), UINT32_C(0x806567cb), UINT32_C(0x196c3671), UINT32_C(0x6e6b06e7),
    UINT32_C(0xfed41b76), UINT32_C(0x89d32be0), UINT32_C(0x10da7a5a), UINT32_C(0x67dd4acc),
    UINT32_C(0xf9b9df6f), UINT32_C(0x8ebeeff9), UINT32_C(0x17b7be43), UINT32_C(0x60b08ed5),
    UINT32_C(0xd6d6a3e8), UINT32_C(0xa1d1937e), UINT32_C(0x38d8c2c4), UINT32_C(0x4fdff252),
    UINT32_C(0xd1bb67f1), UINT32_C(0xa6bc5767), UINT32_C(0x3fb506dd), UINT32_C(0x48b2364b),
    UINT32_C(0xd80d2bda), UINT32_C(0xaf0a1b4c), UINT32_C(0x36034af6), UINT32_C(0x41047a60),
    UINT32_C(0xdf60efc3), UINT32_C(0xa867df55), UINT32_C(0x316e8eef), UINT32_C(0x4669be79),
    UINT32_C(0xcb61b38c), UINT32_C(0xbc66831a), UINT32_C(0x256fd2a0), UINT32_C(0x5268e236),
    UINT32_C(0xcc0c7795), UINT32_C(0xbb0b4703), UINT32_C(0x220216b9), UINT32_C(0x5505262f),
    UINT32_C(0xc5ba3bbe), UINT32_C(0xb2bd0b28), UINT32_C(0x2bb45a92), UINT32_C(0x5cb36a04),
    UINT32_C(0xc2d7ffa7), UINT32_C(0xb5d0cf31), UINT32_C(0x2cd99e8b), UINT32_C(0x5bdeae1d),
    UINT32_C(0x9b64c2b0), UINT32_C(0xec63f226), UINT32_C(0x756aa39c), UINT32_C(0x026d930a),
    UINT32_C(0x9c0906a9), UINT32_C(0xeb0e363f), UINT32_C(0x72076785), UINT32_C(0x05005713),
    UINT32_C(0x95bf4a82), UINT32_C(0xe2b87a14), UINT32_C(0x7bb12bae), UINT32_C(0x0cb61b38),
    UINT32_C(0x92d28e9b), UINT32_C(0xe5d5be0d), UINT32_C(0x7cdcefb7), UINT32_C(0x0bdbdf21),
    UINT32_C(0x86d3d2d4), UINT32_C(0xf1d4e242), UINT32_C(0x68ddb3f8), UINT32_C(0x1fda836e),
    UINT32_C(0x81be16cd), UINT32_C(0xf6b9265b), UINT32_C(0x6fb077e1), UINT32_C(0x18b74777),
    UINT32_C(0x88085ae6), UINT32_C(0xff0f6a70), UINT32_C(0x66063bca), UINT32_C(0x11010b5c),
    UINT32_C(0x8f659eff), UINT32_C(0xf862ae69), UINT32_C(0x616bffd3), UINT32_C(0x166ccf45),
    UINT32_C(0xa00ae278), UINT32_C(0xd70dd2ee), UINT32_C(0x4e048354), UINT32_C(0x3903b3c2),
    UINT32_C(0xa7672661), UINT32_C(0xd06016f7), UINT32_C(0x4969474d), UINT32_C(0x3e6e77db),
    UINT32_C(0xaed16a4a), UINT32_C(0xd9d65adc), UINT32_C(0x40df0b66), UINT32_C(0x37d83bf0),
    UINT32_C(0xa9bcae53), UINT32_C(0xdebb9ec5), UINT32_C(0x47b2cf7f), UINT32_C(0x30b5ffe9),
    UINT32_C(0xbdbdf21c), UINT32_C(0xcabac28a), UINT32_C(0x53b39330), UINT32_C(0x24b4a3a6),
    UINT32_C(0xbad03605), UINT32_C(0xcdd70693), UINT32_C(0x54de5729), UINT32_C(0x23d967bf),
    UINT32_C(0xb3667a2e), UINT32_C(0xc4614ab8), UINT32_C(0x5d681b02), UINT32_C(0x2a6f2b94),
    UINT32_C(0xb40bbe37), UINT32_C(0xc30c8ea1), UINT32_C(0x5a05df1b), UINT32_C(0x2d02ef8d),
};

static uint32_t aforc_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t index;

    for (index = 0u; index < size; ++index) {
        const uint8_t table_index = (uint8_t)(crc ^ (uint32_t)data[index]);

        crc = aforc_crc32_table[table_index] ^ (crc >> 8u);
    }
    return ~crc;
}

static bool aforc_bounded_writer_write_bytes(
    AFORC_BoundedWriter *writer,
    const void *source,
    size_t size)
{
    uint8_t *destination;

    if (!aforc_bounded_writer_take(writer, size, &destination)) {
        return false;
    }
    if (size > 0u) {
        memcpy(destination, source, size);
    }
    return true;
}

static bool aforc_bounded_writer_write_u16(
    AFORC_BoundedWriter *writer,
    uint16_t value)
{
    uint8_t *destination;

    if (!aforc_bounded_writer_take(writer, 2u, &destination)) {
        return false;
    }
    aforc_store_u16(destination, value);
    return true;
}

static bool aforc_bounded_writer_write_u32(
    AFORC_BoundedWriter *writer,
    uint32_t value)
{
    uint8_t *destination;

    if (!aforc_bounded_writer_take(writer, 4u, &destination)) {
        return false;
    }
    aforc_store_u32(destination, value);
    return true;
}

static bool aforc_bounded_writer_write_u64(
    AFORC_BoundedWriter *writer,
    uint64_t value)
{
    uint8_t *destination;

    if (!aforc_bounded_writer_take(writer, 8u, &destination)) {
        return false;
    }
    aforc_store_u64(destination, value);
    return true;
}

static bool aforc_save_writer_valid(const AFORC_SaveWriter *writer)
{
    return writer != NULL && writer->initialized &&
           writer->size <= writer->capacity &&
           writer->capacity <= writer->max_payload_bytes &&
           ((writer->capacity == 0u && writer->payload == NULL) ||
            (writer->capacity != 0u && writer->payload != NULL));
}

static bool aforc_save_writer_source_offset(
    const AFORC_SaveWriter *writer,
    const void *source,
    size_t source_size,
    size_t *output_offset)
{
    uintptr_t payload_address;
    uintptr_t source_address;
    uintptr_t offset;

    if (source == NULL || source_size == 0u || writer->payload == NULL) {
        return false;
    }
    payload_address = (uintptr_t)writer->payload;
    source_address = (uintptr_t)source;
    if (source_address < payload_address) {
        return false;
    }
    offset = source_address - payload_address;
    if (offset > (uintptr_t)writer->size ||
        source_size > writer->size - (size_t)offset) {
        return false;
    }
    *output_offset = (size_t)offset;
    return true;
}

static AFORC_Status aforc_save_writer_reserve(
    AFORC_SaveWriter *writer,
    size_t additional_size)
{
    size_t required;
    size_t next_capacity;
    uint8_t *resized;

    if (writer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer)) {
        return AFORC_ERROR_STATE;
    }
    if (additional_size > writer->max_payload_bytes - writer->size ||
        !aforc_size_add(writer->size, additional_size, &required)) {
        return AFORC_ERROR_LIMIT;
    }
    if (required <= writer->capacity) {
        return AFORC_OK;
    }
    if (!aforc_assets_growth_capacity(
            writer->capacity,
            required,
            writer->max_payload_bytes,
            64u,
            &next_capacity)) {
        return AFORC_ERROR_LIMIT;
    }

    resized = realloc(writer->payload, next_capacity);
    if (resized == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    writer->payload = resized;
    writer->capacity = next_capacity;
    return AFORC_OK;
}

static AFORC_Status aforc_save_writer_take(
    AFORC_SaveWriter *writer,
    size_t size,
    uint8_t **output)
{
    AFORC_BoundedWriter bytes;
    AFORC_Status status;

    status = aforc_save_writer_reserve(writer, size);
    if (status != AFORC_OK) {
        return status;
    }
    aforc_bounded_writer_init(
        &bytes,
        writer->payload,
        writer->capacity,
        writer->size);
    if (!aforc_bounded_writer_take(&bytes, size, output)) {
        return AFORC_ERROR_STATE;
    }
    writer->size = bytes.cursor;
    return AFORC_OK;
}

AFORC_Status aforc_save_writer_init(
    AFORC_SaveWriter *writer,
    uint32_t schema_version,
    size_t max_payload_bytes)
{
    if (writer == NULL) {
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
    if (writer == NULL) {
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

    if (status == AFORC_OK) {
        destination[0] = value;
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u16(AFORC_SaveWriter *writer, uint16_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 2u, &destination);

    if (status == AFORC_OK) {
        aforc_store_u16(destination, value);
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u32(AFORC_SaveWriter *writer, uint32_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 4u, &destination);

    if (status == AFORC_OK) {
        aforc_store_u32(destination, value);
    }
    return status;
}

AFORC_Status aforc_save_writer_write_u64(AFORC_SaveWriter *writer, uint64_t value)
{
    uint8_t *destination;
    AFORC_Status status = aforc_save_writer_take(writer, 8u, &destination);

    if (status == AFORC_OK) {
        aforc_store_u64(destination, value);
    }
    return status;
}

static uint32_t aforc_signed_i32_bits(int32_t value)
{
    if (value >= 0) {
        return (uint32_t)value;
    }
    {
        uint32_t magnitude = (uint32_t)(-(int64_t)value);
        return UINT32_MAX - magnitude + UINT32_C(1);
    }
}

static uint64_t aforc_signed_i64_bits(int64_t value)
{
    uint64_t magnitude;

    if (value >= 0) {
        return (uint64_t)value;
    }
    magnitude = (uint64_t)(-(value + INT64_C(1)));
    ++magnitude;
    return UINT64_MAX - magnitude + UINT64_C(1);
}

AFORC_Status aforc_save_writer_write_i32(AFORC_SaveWriter *writer, int32_t value)
{
    return aforc_save_writer_write_u32(writer, aforc_signed_i32_bits(value));
}

AFORC_Status aforc_save_writer_write_i64(AFORC_SaveWriter *writer, int64_t value)
{
    return aforc_save_writer_write_u64(writer, aforc_signed_i64_bits(value));
}

AFORC_Status aforc_save_writer_write_bytes(
    AFORC_SaveWriter *writer,
    const void *data,
    size_t size)
{
    AFORC_Status status;
    size_t source_offset = 0u;
    bool source_is_payload;
    uint8_t *destination;

    if (data == NULL && size != 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (writer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer)) {
        return AFORC_ERROR_STATE;
    }
    if (size == 0u) {
        return AFORC_OK;
    }
    source_is_payload = aforc_save_writer_source_offset(
        writer,
        data,
        size,
        &source_offset);
    status = aforc_save_writer_take(writer, size, &destination);
    if (status != AFORC_OK) {
        return status;
    }
    if (source_is_payload) {
        data = writer->payload + source_offset;
    }
    memmove(destination, data, size);
    return AFORC_OK;
}

AFORC_Status aforc_save_writer_write_string(
    AFORC_SaveWriter *writer,
    const char *text,
    size_t size)
{
    size_t total_size;
    AFORC_Status status;
    size_t source_offset = 0u;
    bool source_is_payload;
    uint8_t *destination;

    if (text == NULL && size != 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (size > UINT32_MAX) {
        return AFORC_ERROR_LIMIT;
    }
    if (!aforc_size_add(size, 4u, &total_size)) {
        return AFORC_ERROR_OVERFLOW;
    }
    if (writer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer)) {
        return AFORC_ERROR_STATE;
    }
    source_is_payload = aforc_save_writer_source_offset(
        writer,
        text,
        size,
        &source_offset);
    status = aforc_save_writer_take(writer, total_size, &destination);
    if (status != AFORC_OK) {
        return status;
    }
    aforc_store_u32(destination, (uint32_t)size);
    if (size > 0u) {
        const char *source = source_is_payload
                                 ? (const char *)(writer->payload + source_offset)
                                 : text;

        memmove(destination + 4u, source, size);
    }
    return AFORC_OK;
}

AFORC_Status aforc_save_writer_finish(
    const AFORC_SaveWriter *writer,
    AFORC_AssetBlob *output)
{
    static const uint8_t magic[4] = {'A', '2', 'D', 'S'};
    size_t container_size;
    uint8_t *container;
    uint32_t payload_checksum;
    uint32_t header_checksum;
    AFORC_BoundedWriter bytes;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->data = NULL;
    output->size = 0u;
    if (writer == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer)) {
        return AFORC_ERROR_STATE;
    }
    if (!aforc_size_add(AFORC_SAVE_HEADER_SIZE, writer->size, &container_size)) {
        return AFORC_ERROR_OVERFLOW;
    }
#if SIZE_MAX > UINT64_MAX
    if (writer->size > (size_t)UINT64_MAX) {
        return AFORC_ERROR_LIMIT;
    }
#endif

    container = malloc(container_size);
    if (container == NULL) {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    aforc_bounded_writer_init(&bytes, container, container_size, 0u);
    payload_checksum = aforc_crc32(writer->payload, writer->size);

    /* Header and payload CRCs cover distinct trust boundaries in the format. */
    if (!aforc_bounded_writer_write_bytes(&bytes, magic, sizeof(magic)) ||
        !aforc_bounded_writer_write_u16(
            &bytes,
            AFORC_SAVE_CONTAINER_VERSION) ||
        !aforc_bounded_writer_write_u16(&bytes, UINT16_C(0)) ||
        !aforc_bounded_writer_write_u32(&bytes, writer->schema_version) ||
        !aforc_bounded_writer_write_u64(&bytes, (uint64_t)writer->size) ||
        !aforc_bounded_writer_write_u32(&bytes, payload_checksum)) {
        free(container);
        return AFORC_ERROR_OVERFLOW;
    }
    header_checksum = aforc_crc32(container, 24u);
    if (!aforc_bounded_writer_write_u32(&bytes, header_checksum) ||
        !aforc_bounded_writer_write_bytes(
            &bytes,
            writer->payload,
            writer->size)) {
        free(container);
        return AFORC_ERROR_OVERFLOW;
    }

    output->data = container;
    output->size = container_size;
    return AFORC_OK;
}

static bool aforc_save_reader_valid(const AFORC_SaveReader *reader)
{
    return reader != NULL && reader->initialized &&
           reader->cursor <= reader->size &&
           reader->payload != NULL;
}

AFORC_Status aforc_save_reader_init(
    AFORC_SaveReader *reader,
    const void *container,
    size_t container_size,
    size_t max_payload_bytes,
    uint32_t minimum_schema_version,
    uint32_t maximum_schema_version)
{
    static const uint8_t magic[4] = {'A', '2', 'D', 'S'};
    const uint8_t *bytes = (const uint8_t *)container;
    uint16_t container_version;
    uint16_t flags;
    uint32_t schema_version;
    uint64_t encoded_payload_size;
    size_t payload_size;
    size_t expected_size;
    uint32_t payload_checksum;
    uint32_t header_checksum;

    if (reader == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    reader->payload = NULL;
    reader->size = 0u;
    reader->cursor = 0u;
    reader->schema_version = 0u;
    reader->initialized = false;
    if (container == NULL || minimum_schema_version > maximum_schema_version) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    if (container_size < AFORC_SAVE_HEADER_SIZE ||
        memcmp(bytes, magic, sizeof(magic)) != 0) {
        return AFORC_ERROR_FORMAT;
    }
    header_checksum = aforc_load_u32(bytes + 24u);
    if (aforc_crc32(bytes, 24u) != header_checksum) {
        return AFORC_ERROR_CHECKSUM;
    }
    container_version = aforc_load_u16(bytes + 4u);
    if (container_version != AFORC_SAVE_CONTAINER_VERSION) {
        return AFORC_ERROR_UNSUPPORTED;
    }
    flags = aforc_load_u16(bytes + 6u);
    if (flags != 0u) {
        return AFORC_ERROR_FORMAT;
    }

    schema_version = aforc_load_u32(bytes + 8u);
    if (schema_version < minimum_schema_version ||
        schema_version > maximum_schema_version) {
        return AFORC_ERROR_UNSUPPORTED;
    }
    encoded_payload_size = aforc_load_u64(bytes + 12u);
#if SIZE_MAX < UINT64_MAX
    if (encoded_payload_size > (uint64_t)SIZE_MAX) {
        return AFORC_ERROR_LIMIT;
    }
#endif
    payload_size = (size_t)encoded_payload_size;
    if (payload_size > max_payload_bytes ||
        !aforc_size_add(AFORC_SAVE_HEADER_SIZE, payload_size, &expected_size)) {
        return AFORC_ERROR_LIMIT;
    }
    if (container_size != expected_size) {
        return AFORC_ERROR_FORMAT;
    }
    payload_checksum = aforc_load_u32(bytes + 20u);
    if (aforc_crc32(bytes + AFORC_SAVE_HEADER_SIZE, payload_size) !=
        payload_checksum) {
        return AFORC_ERROR_CHECKSUM;
    }

    reader->payload = bytes + AFORC_SAVE_HEADER_SIZE;
    reader->size = payload_size;
    reader->cursor = 0u;
    reader->schema_version = schema_version;
    reader->initialized = true;
    return AFORC_OK;
}

size_t aforc_save_reader_remaining(const AFORC_SaveReader *reader)
{
    if (!aforc_save_reader_valid(reader)) {
        return 0u;
    }
    return reader->size - reader->cursor;
}

bool aforc_save_reader_finished(const AFORC_SaveReader *reader)
{
    return aforc_save_reader_valid(reader) && reader->cursor == reader->size;
}

static AFORC_Status aforc_save_reader_take(
    AFORC_SaveReader *reader,
    size_t size,
    const uint8_t **output)
{
    AFORC_BoundedReader bytes;

    if (reader == NULL || output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_reader_valid(reader)) {
        return AFORC_ERROR_STATE;
    }
    aforc_bounded_reader_init(&bytes, reader->payload, reader->size);
    bytes.cursor = reader->cursor;
    if (!aforc_bounded_reader_take(&bytes, size, output)) {
        return AFORC_ERROR_END_OF_STREAM;
    }
    reader->cursor = bytes.cursor;
    return AFORC_OK;
}

AFORC_Status aforc_save_reader_read_u8(AFORC_SaveReader *reader, uint8_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 1u, &data);
    if (status == AFORC_OK) {
        *output = data[0];
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u16(AFORC_SaveReader *reader, uint16_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 2u, &data);
    if (status == AFORC_OK) {
        *output = aforc_load_u16(data);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u32(AFORC_SaveReader *reader, uint32_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 4u, &data);
    if (status == AFORC_OK) {
        *output = aforc_load_u32(data);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_u64(AFORC_SaveReader *reader, uint64_t *output)
{
    const uint8_t *data;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, 8u, &data);
    if (status == AFORC_OK) {
        *output = aforc_load_u64(data);
    }
    return status;
}

static int32_t aforc_signed_i32_value(uint32_t bits)
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

static int64_t aforc_signed_i64_value(uint64_t bits)
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

AFORC_Status aforc_save_reader_read_i32(AFORC_SaveReader *reader, int32_t *output)
{
    uint32_t bits;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_read_u32(reader, &bits);
    if (status == AFORC_OK) {
        *output = aforc_signed_i32_value(bits);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_i64(AFORC_SaveReader *reader, int64_t *output)
{
    uint64_t bits;
    AFORC_Status status;

    if (output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_read_u64(reader, &bits);
    if (status == AFORC_OK) {
        *output = aforc_signed_i64_value(bits);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_bytes(
    AFORC_SaveReader *reader,
    void *destination,
    size_t size)
{
    const uint8_t *data;
    AFORC_Status status;

    if (destination == NULL && size != 0u) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status = aforc_save_reader_take(reader, size, &data);
    if (status == AFORC_OK && size > 0u) {
        memmove(destination, data, size);
    }
    return status;
}

AFORC_Status aforc_save_reader_read_string(
    AFORC_SaveReader *reader,
    AFORC_AssetView *output)
{
    size_t initial_cursor;
    uint32_t encoded_size;
    size_t string_size;
    const uint8_t *data;
    AFORC_Status status;

    if (reader == NULL || output == NULL) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_reader_valid(reader)) {
        return AFORC_ERROR_STATE;
    }
    output->data = NULL;
    output->size = 0u;
    initial_cursor = reader->cursor;
    status = aforc_save_reader_read_u32(reader, &encoded_size);
    if (status != AFORC_OK) {
        return status;
    }
#if SIZE_MAX < UINT32_MAX
    if (encoded_size > (uint32_t)SIZE_MAX) {
        reader->cursor = initial_cursor;
        return AFORC_ERROR_LIMIT;
    }
#endif
    string_size = (size_t)encoded_size;
    status = aforc_save_reader_take(reader, string_size, &data);
    if (status != AFORC_OK) {
        reader->cursor = initial_cursor;
        return status;
    }
    output->data = data;
    output->size = string_size;
    return AFORC_OK;
}
