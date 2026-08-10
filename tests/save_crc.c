/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "aforc/assets.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AFORC_Status make_container(uint32_t schema_version,
                                   const void *payload,
                                   size_t payload_size,
                                   AFORC_AssetBlob *out_blob)
{
    AFORC_SaveWriter writer = {0};
    AFORC_Status status =
        aforc_save_writer_init(&writer, schema_version, payload_size);

    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_bytes(&writer, payload, payload_size);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_finish(&writer, out_blob);
    }
    aforc_save_writer_release(&writer);
    return status;
}

static bool test_empty_vector(void)
{
    static const uint8_t expected[] = {
        0x41u, 0x32u, 0x44u, 0x53u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0xf0u, 0x5au, 0xaau, 0xddu,
    };
    AFORC_AssetBlob blob = {0};
    const bool created = make_container(0u, NULL, 0u, &blob) == AFORC_OK;
    const bool matches = created && blob.size == sizeof(expected) &&
                         memcmp(blob.data, expected, sizeof(expected)) == 0;

    aforc_asset_blob_release(&blob);
    return matches;
}

static bool test_known_vector(void)
{
    static const uint8_t payload[] = "123456789";
    static const uint8_t expected[] = {
        0x41u, 0x32u, 0x44u, 0x53u, 0x01u, 0x00u, 0x00u, 0x00u, 0x04u, 0x03u,
        0x02u, 0x01u, 0x09u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x26u, 0x39u, 0xf4u, 0xcbu, 0xcbu, 0x4au, 0x32u, 0x6bu, 0x31u, 0x32u,
        0x33u, 0x34u, 0x35u, 0x36u, 0x37u, 0x38u, 0x39u,
    };
    AFORC_AssetBlob blob = {0};
    const bool created = make_container(UINT32_C(0x01020304),
                                        payload,
                                        sizeof(payload) - 1u,
                                        &blob) == AFORC_OK;
    const bool matches = created && blob.size == sizeof(expected) &&
                         memcmp(blob.data, expected, sizeof(expected)) == 0;

    aforc_asset_blob_release(&blob);
    return matches;
}

static bool test_roundtrip(void)
{
    static const char text[] = "AFORC";
    AFORC_SaveWriter writer = {0};
    AFORC_SaveReader reader;
    AFORC_AssetBlob blob = {0};
    AFORC_AssetView view;
    uint8_t value_u8;
    uint16_t value_u16;
    uint32_t value_u32;
    uint64_t value_u64;
    int32_t value_i32;
    int64_t value_i64;
    AFORC_Status status = aforc_save_writer_init(&writer, 7u, 64u);

    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u8(&writer, UINT8_C(0xa5));
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u16(&writer, UINT16_C(0x1234));
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_u32(&writer, UINT32_C(0x89abcdef));
    }
    if (status == AFORC_OK)
    {
        status =
            aforc_save_writer_write_u64(&writer, UINT64_C(0x0123456789abcdef));
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_i32(&writer, INT32_MIN);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_write_i64(&writer, INT64_MIN);
    }
    if (status == AFORC_OK)
    {
        status =
            aforc_save_writer_write_string(&writer, text, sizeof(text) - 1u);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_writer_finish(&writer, &blob);
    }
    aforc_save_writer_release(&writer);
    if (status == AFORC_OK)
    {
        status =
            aforc_save_reader_init(&reader, blob.data, blob.size, 64u, 7u, 7u);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_u8(&reader, &value_u8);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_u16(&reader, &value_u16);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_u32(&reader, &value_u32);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_u64(&reader, &value_u64);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_i32(&reader, &value_i32);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_i64(&reader, &value_i64);
    }
    if (status == AFORC_OK)
    {
        status = aforc_save_reader_read_string(&reader, &view);
    }
    if (status == AFORC_OK &&
        (value_u8 != UINT8_C(0xa5) || value_u16 != UINT16_C(0x1234) ||
         value_u32 != UINT32_C(0x89abcdef) ||
         value_u64 != UINT64_C(0x0123456789abcdef) || value_i32 != INT32_MIN ||
         value_i64 != INT64_MIN || view.size != sizeof(text) - 1u ||
         memcmp(view.data, text, sizeof(text) - 1u) != 0 ||
         !aforc_save_reader_finished(&reader)))
    {
        status = AFORC_ERROR_STATE;
    }
    aforc_asset_blob_release(&blob);
    return status == AFORC_OK;
}

static bool test_corruption_and_truncation(void)
{
    static const uint8_t payload[] = "123456789";
    AFORC_AssetBlob blob = {0};
    AFORC_SaveReader reader;
    uint8_t *mutated;
    bool passed =
        make_container(3u, payload, sizeof(payload) - 1u, &blob) == AFORC_OK;

    if (!passed)
    {
        return false;
    }
    mutated = malloc(blob.size + 1u);
    if (mutated == NULL)
    {
        aforc_asset_blob_release(&blob);
        return false;
    }
    (void)memcpy(mutated, blob.data, blob.size);
    mutated[8] ^= UINT8_C(1);
    passed = aforc_save_reader_init(
                 &reader, mutated, blob.size, blob.size, 0u, UINT32_MAX) ==
             AFORC_ERROR_CHECKSUM;
    (void)memcpy(mutated, blob.data, blob.size);
    mutated[AFORC_SAVE_HEADER_SIZE] ^= UINT8_C(1);
    passed =
        passed && aforc_save_reader_init(
                      &reader, mutated, blob.size, blob.size, 0u, UINT32_MAX) ==
                      AFORC_ERROR_CHECKSUM;
    for (size_t size = 0u; passed && size < blob.size; ++size)
    {
        passed = aforc_save_reader_init(
                     &reader, blob.data, size, blob.size, 0u, UINT32_MAX) !=
                 AFORC_OK;
    }
    (void)memcpy(mutated, blob.data, blob.size);
    mutated[blob.size] = 0u;
    passed = passed &&
             aforc_save_reader_init(
                 &reader, mutated, blob.size + 1u, blob.size, 0u, UINT32_MAX) ==
                 AFORC_ERROR_FORMAT;
    free(mutated);
    aforc_asset_blob_release(&blob);
    return passed;
}

static bool test_deterministic_output(void)
{
    static const uint8_t payload[] = {0x00u, 0x7fu, 0x80u, 0xffu};
    AFORC_AssetBlob first = {0};
    AFORC_AssetBlob second = {0};
    const bool created =
        make_container(11u, payload, sizeof(payload), &first) == AFORC_OK &&
        make_container(11u, payload, sizeof(payload), &second) == AFORC_OK;
    const bool matches = created && first.size == second.size &&
                         memcmp(first.data, second.data, first.size) == 0;

    aforc_asset_blob_release(&first);
    aforc_asset_blob_release(&second);
    return matches;
}

int main(void)
{
    if (!test_empty_vector() || !test_known_vector() || !test_roundtrip() ||
        !test_corruption_and_truncation() || !test_deterministic_output())
    {
        (void)fprintf(stderr, "save CRC regression failed\n");
        return 1;
    }
    (void)puts("save CRC regressions: ok");
    return 0;
}
