/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_SAVE_INTERNAL_H
#define AFORC_SAVE_INTERNAL_H

#include "assets_internal.h"

typedef struct AFORC_SaveBoundedReader
{
    const uint8_t *data;
    size_t size;
    size_t cursor;
} AFORC_SaveBoundedReader;

typedef struct AFORC_SaveBoundedWriter
{
    uint8_t *data;
    size_t size;
    size_t cursor;
} AFORC_SaveBoundedWriter;

AFORC_ASSETS_INTERNAL void aforc_save_bounded_reader_init(
    AFORC_SaveBoundedReader *reader, const void *data, size_t size);
AFORC_ASSETS_INTERNAL bool aforc_save_bounded_reader_take(
    AFORC_SaveBoundedReader *reader, size_t size, const uint8_t **output);
AFORC_ASSETS_INTERNAL void aforc_save_bounded_writer_init(
    AFORC_SaveBoundedWriter *writer, void *data, size_t size, size_t cursor);
AFORC_ASSETS_INTERNAL bool aforc_save_bounded_writer_take(
    AFORC_SaveBoundedWriter *writer, size_t size, uint8_t **output);

AFORC_ASSETS_INTERNAL void aforc_save_store_u16(uint8_t *destination,
                                                uint16_t value);
AFORC_ASSETS_INTERNAL void aforc_save_store_u32(uint8_t *destination,
                                                uint32_t value);
AFORC_ASSETS_INTERNAL void aforc_save_store_u64(uint8_t *destination,
                                                uint64_t value);
AFORC_ASSETS_INTERNAL uint16_t aforc_save_load_u16(const uint8_t *source);
AFORC_ASSETS_INTERNAL uint32_t aforc_save_load_u32(const uint8_t *source);
AFORC_ASSETS_INTERNAL uint64_t aforc_save_load_u64(const uint8_t *source);

AFORC_ASSETS_INTERNAL bool aforc_save_bounded_writer_write_bytes(
    AFORC_SaveBoundedWriter *writer, const void *source, size_t size);
AFORC_ASSETS_INTERNAL bool
aforc_save_bounded_writer_write_u16(AFORC_SaveBoundedWriter *writer,
                                    uint16_t value);
AFORC_ASSETS_INTERNAL bool
aforc_save_bounded_writer_write_u32(AFORC_SaveBoundedWriter *writer,
                                    uint32_t value);
AFORC_ASSETS_INTERNAL bool
aforc_save_bounded_writer_write_u64(AFORC_SaveBoundedWriter *writer,
                                    uint64_t value);

AFORC_ASSETS_INTERNAL uint32_t aforc_save_signed_i32_bits(int32_t value);
AFORC_ASSETS_INTERNAL uint64_t aforc_save_signed_i64_bits(int64_t value);
AFORC_ASSETS_INTERNAL int32_t aforc_save_signed_i32_value(uint32_t bits);
AFORC_ASSETS_INTERNAL int64_t aforc_save_signed_i64_value(uint64_t bits);

AFORC_ASSETS_INTERNAL uint32_t aforc_save_crc32(const uint8_t *data,
                                                size_t size);
AFORC_ASSETS_INTERNAL bool
aforc_save_writer_valid(const AFORC_SaveWriter *writer);

#endif
