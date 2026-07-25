/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t reference_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t index;

    for (index = 0u; index < size; ++index) {
        unsigned int bit;

        crc ^= (uint32_t)data[index];
        for (bit = 0u; bit < 8u; ++bit) {
            const uint32_t mask = UINT32_C(0) - (crc & UINT32_C(1));

            crc = (crc >> 1u) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static void store_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & UINT16_C(0xff));
    destination[1] = (uint8_t)(value >> 8u);
}

static void store_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & UINT32_C(0xff));
    destination[1] = (uint8_t)((value >> 8u) & UINT32_C(0xff));
    destination[2] = (uint8_t)((value >> 16u) & UINT32_C(0xff));
    destination[3] = (uint8_t)(value >> 24u);
}

static void store_u64(uint8_t *destination, uint64_t value)
{
    size_t index;

    for (index = 0u; index < 8u; ++index) {
        destination[index] = (uint8_t)(value & UINT64_C(0xff));
        value >>= 8u;
    }
}

static void refresh_header_crc(uint8_t *container)
{
    store_u32(container + 24u, reference_crc32(container, 24u));
}

static AFORC_Status make_container(
    uint32_t schema_version,
    const void *payload,
    size_t payload_size,
    AFORC_AssetBlob *output)
{
    AFORC_SaveWriter writer = {0};
    AFORC_Status status =
        aforc_save_writer_init(&writer, schema_version, payload_size);

    if (status == AFORC_OK) {
        status = aforc_save_writer_write_bytes(&writer, payload, payload_size);
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_finish(&writer, output);
    }
    aforc_save_writer_release(&writer);
    return status;
}

static bool reader_init_status(
    const void *container,
    size_t container_size,
    size_t max_payload_bytes,
    uint32_t minimum_schema,
    uint32_t maximum_schema,
    AFORC_Status expected)
{
    AFORC_SaveReader reader = {(const uint8_t *)container, 1u, 1u, 1u, true};
    const AFORC_Status status = aforc_save_reader_init(
        &reader,
        container,
        container_size,
        max_payload_bytes,
        minimum_schema,
        maximum_schema);

    return status == expected &&
           (status == AFORC_OK ||
            (reader.payload == NULL && reader.size == 0u &&
             reader.cursor == 0u && reader.schema_version == 0u &&
             !reader.initialized));
}

static bool test_hostile_headers(void)
{
    static const uint8_t payload[] = {0x10u, 0x20u, 0x30u};
    AFORC_AssetBlob blob = {0};
    uint8_t *mutated;
    bool passed;

    if (make_container(3u, payload, sizeof(payload), &blob) != AFORC_OK) {
        return false;
    }
    mutated = malloc(blob.size);
    if (mutated == NULL) {
        aforc_asset_blob_release(&blob);
        return false;
    }
    (void)memcpy(mutated, blob.data, blob.size);
    mutated[0] = (uint8_t)'X';
    passed = reader_init_status(mutated, blob.size, sizeof(payload), 3u, 3u,
                                AFORC_ERROR_FORMAT);

    (void)memcpy(mutated, blob.data, blob.size);
    store_u16(mutated + 4u, AFORC_SAVE_CONTAINER_VERSION + UINT16_C(1));
    refresh_header_crc(mutated);
    passed = passed && reader_init_status(mutated, blob.size, sizeof(payload),
                                          3u, 3u, AFORC_ERROR_UNSUPPORTED);

    (void)memcpy(mutated, blob.data, blob.size);
    store_u16(mutated + 6u, UINT16_C(1));
    refresh_header_crc(mutated);
    passed = passed && reader_init_status(mutated, blob.size, sizeof(payload),
                                          3u, 3u, AFORC_ERROR_FORMAT);

    (void)memcpy(mutated, blob.data, blob.size);
    store_u64(mutated + 12u, (uint64_t)sizeof(payload) + UINT64_C(1));
    refresh_header_crc(mutated);
    passed = passed && reader_init_status(mutated, blob.size,
                                          sizeof(payload) + 1u, 3u, 3u,
                                          AFORC_ERROR_FORMAT) &&
             reader_init_status(blob.data, blob.size, sizeof(payload) - 1u,
                                3u, 3u, AFORC_ERROR_LIMIT) &&
             reader_init_status(blob.data, blob.size, sizeof(payload),
                                4u, 4u, AFORC_ERROR_UNSUPPORTED) &&
             reader_init_status(NULL, 0u, 0u, 0u, 0u,
                                AFORC_ERROR_INVALID_ARGUMENT);
    free(mutated);
    aforc_asset_blob_release(&blob);
    return passed;
}

static bool test_reader_transactions(void)
{
    static const uint8_t truncated_string[] = {
        0x05u, 0x00u, 0x00u, 0x00u, (uint8_t)'a', (uint8_t)'b'
    };
    AFORC_AssetBlob blob = {0};
    AFORC_SaveReader reader;
    AFORC_AssetView view = {(const uint8_t *)"sentinel", 8u};
    uint64_t value = UINT64_C(0xfeedface);
    bool passed;

    if (make_container(1u, truncated_string, sizeof(truncated_string), &blob) !=
        AFORC_OK) {
        return false;
    }
    passed = aforc_save_reader_init(&reader, blob.data, blob.size,
                                    sizeof(truncated_string), 1u, 1u) ==
                 AFORC_OK &&
             aforc_save_reader_read_string(&reader, &view) ==
                 AFORC_ERROR_END_OF_STREAM &&
             reader.cursor == 0u && view.data == NULL && view.size == 0u &&
             aforc_save_reader_read_u64(&reader, &value) ==
                 AFORC_ERROR_END_OF_STREAM &&
             reader.cursor == 0u && value == UINT64_C(0xfeedface) &&
             aforc_save_reader_remaining(&reader) == sizeof(truncated_string) &&
             !aforc_save_reader_finished(&reader);
    aforc_asset_blob_release(&blob);
    return passed;
}

static bool test_writer_alias_and_limits(void)
{
    uint8_t initial[64];
    AFORC_SaveWriter writer = {0};
    AFORC_SaveWriter invalid = {0};
    AFORC_AssetBlob blob = {0};
    AFORC_Status status;
    size_t index;
    bool passed;

    for (index = 0u; index < sizeof(initial); ++index) {
        initial[index] = (uint8_t)index;
    }
    status = aforc_save_writer_init(&writer, 9u, 128u);
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_bytes(&writer, initial, sizeof(initial));
    }
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_string(
            &writer,
            (const char *)(writer.payload + 1u),
            3u);
    }
    passed = status == AFORC_OK && writer.size == sizeof(initial) + 7u &&
             writer.payload[64u] == 3u && writer.payload[65u] == 0u &&
             writer.payload[66u] == 0u && writer.payload[67u] == 0u &&
             writer.payload[68u] == 1u && writer.payload[69u] == 2u &&
             writer.payload[70u] == 3u;
    aforc_save_writer_release(&writer);

    status = aforc_save_writer_init(&writer, 2u, 4u);
    if (status == AFORC_OK) {
        status = aforc_save_writer_write_u32(&writer, UINT32_C(0x12345678));
    }
    passed = passed && status == AFORC_OK &&
             aforc_save_writer_write_u8(&writer, UINT8_C(1)) ==
                 AFORC_ERROR_LIMIT &&
             writer.size == 4u &&
             aforc_save_writer_finish(&writer, &blob) == AFORC_OK &&
             blob.size == AFORC_SAVE_HEADER_SIZE + 4u &&
             aforc_save_writer_write_u8(&invalid, UINT8_C(1)) ==
                 AFORC_ERROR_STATE;
#if SIZE_MAX > UINT32_MAX
    passed = passed &&
             aforc_save_writer_write_string(
                 &writer,
                 "x",
                 (size_t)UINT32_MAX + 1u) == AFORC_ERROR_LIMIT;
#endif
    aforc_asset_blob_release(&blob);
    aforc_save_writer_release(&writer);
    return passed;
}

int main(void)
{
    if (!test_hostile_headers() || !test_reader_transactions() ||
        !test_writer_alias_and_limits()) {
        (void)fputs("save boundary regression failed\n", stderr);
        return 1;
    }
    (void)puts("save boundaries: ok");
    return 0;
}
