/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

/* Owns version-1 container framing, checksums, and hostile-header validation.
 */

#include "save_internal.h"

#include <stdlib.h>
#include <string.h>

AFORC_Status aforc_save_writer_finish(const AFORC_SaveWriter *writer,
                                      AFORC_AssetBlob *output)
{
    static const uint8_t magic[4] = {'A', '2', 'D', 'S'};
    size_t container_size;
    uint8_t *container;
    uint32_t payload_checksum;
    uint32_t header_checksum;

    if (output == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    output->data = NULL;
    output->size = 0u;
    if (writer == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (!aforc_save_writer_valid(writer))
    {
        return AFORC_ERROR_STATE;
    }
    if (!aforc_size_add(AFORC_SAVE_HEADER_SIZE, writer->size, &container_size))
    {
        return AFORC_ERROR_OVERFLOW;
    }
#if SIZE_MAX > UINT64_MAX
    if (writer->size > (size_t)UINT64_MAX)
    {
        return AFORC_ERROR_LIMIT;
    }
#endif

    container = malloc(container_size);
    if (container == NULL)
    {
        return AFORC_ERROR_OUT_OF_MEMORY;
    }
    payload_checksum = aforc_save_crc32(writer->payload, writer->size);

    /* Header and payload CRCs cover distinct trust boundaries in the format. */
    memcpy(container, magic, sizeof(magic));
    aforc_save_store_u16(container + 4u, AFORC_SAVE_CONTAINER_VERSION);
    aforc_save_store_u16(container + 6u, UINT16_C(0));
    aforc_save_store_u32(container + 8u, writer->schema_version);
    aforc_save_store_u64(container + 12u, (uint64_t)writer->size);
    aforc_save_store_u32(container + 20u, payload_checksum);
    header_checksum = aforc_save_crc32(container, 24u);
    aforc_save_store_u32(container + 24u, header_checksum);
    if (writer->size > 0u)
    {
        memcpy(
            container + AFORC_SAVE_HEADER_SIZE, writer->payload, writer->size);
    }
    output->data = container;
    output->size = container_size;
    return AFORC_OK;
}

AFORC_Status aforc_save_reader_init(AFORC_SaveReader *reader,
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

    if (reader == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    reader->payload = NULL;
    reader->size = 0u;
    reader->cursor = 0u;
    reader->schema_version = 0u;
    reader->initialized = false;
    if (container == NULL || minimum_schema_version > maximum_schema_version)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }

    if (container_size < AFORC_SAVE_HEADER_SIZE ||
        memcmp(bytes, magic, sizeof(magic)) != 0)
    {
        return AFORC_ERROR_FORMAT;
    }
    header_checksum = aforc_save_load_u32(bytes + 24u);
    if (aforc_save_crc32(bytes, 24u) != header_checksum)
    {
        return AFORC_ERROR_CHECKSUM;
    }
    container_version = aforc_save_load_u16(bytes + 4u);
    if (container_version != AFORC_SAVE_CONTAINER_VERSION)
    {
        return AFORC_ERROR_UNSUPPORTED;
    }
    flags = aforc_save_load_u16(bytes + 6u);
    if (flags != 0u)
    {
        return AFORC_ERROR_FORMAT;
    }

    schema_version = aforc_save_load_u32(bytes + 8u);
    if (schema_version < minimum_schema_version ||
        schema_version > maximum_schema_version)
    {
        return AFORC_ERROR_UNSUPPORTED;
    }
    encoded_payload_size = aforc_save_load_u64(bytes + 12u);
#if SIZE_MAX < UINT64_MAX
    if (encoded_payload_size > (uint64_t)SIZE_MAX)
    {
        return AFORC_ERROR_LIMIT;
    }
#endif
    payload_size = (size_t)encoded_payload_size;
    if (payload_size > max_payload_bytes ||
        !aforc_size_add(AFORC_SAVE_HEADER_SIZE, payload_size, &expected_size))
    {
        return AFORC_ERROR_LIMIT;
    }
    if (container_size != expected_size)
    {
        return AFORC_ERROR_FORMAT;
    }
    payload_checksum = aforc_save_load_u32(bytes + 20u);
    if (aforc_save_crc32(bytes + AFORC_SAVE_HEADER_SIZE, payload_size) !=
        payload_checksum)
    {
        return AFORC_ERROR_CHECKSUM;
    }

    reader->payload = bytes + AFORC_SAVE_HEADER_SIZE;
    reader->size = payload_size;
    reader->cursor = 0u;
    reader->schema_version = schema_version;
    reader->initialized = true;
    return AFORC_OK;
}
