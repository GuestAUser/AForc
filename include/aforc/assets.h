/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_ASSETS_H
#define AFORC_ASSETS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AFORC_ASSET_DEFAULT_MAX_PATH_BYTES ((size_t)4096u)
#define AFORC_ASSET_DEFAULT_MAX_COMPONENT_BYTES ((size_t)255u)
#define AFORC_ASSET_DEFAULT_MAX_DEPTH ((size_t)32u)
#define AFORC_ASSET_DEFAULT_MAX_FILE_BYTES ((size_t)16777216u)

#define AFORC_CONFIG_DEFAULT_MAX_INPUT_BYTES ((size_t)1048576u)
#define AFORC_CONFIG_DEFAULT_MAX_LINE_BYTES ((size_t)8192u)
#define AFORC_CONFIG_DEFAULT_MAX_ENTRIES ((size_t)4096u)
#define AFORC_CONFIG_DEFAULT_MAX_SECTION_BYTES ((size_t)128u)
#define AFORC_CONFIG_DEFAULT_MAX_KEY_BYTES ((size_t)128u)
#define AFORC_CONFIG_DEFAULT_MAX_VALUE_BYTES ((size_t)4096u)

#define AFORC_SAVE_CONTAINER_VERSION ((uint16_t)1u)
#define AFORC_SAVE_HEADER_SIZE ((size_t)28u)

/*
 * Path limits count bytes, not characters, and exclude the terminating NUL.
 * `relative_path` and `root` are C strings, so each terminator must occur at
 * or before max_path_bytes; max_path_bytes itself must be less than SIZE_MAX.
 * A relative asset path uses '/' separators and must contain one or more
 * non-empty components.  Absolute paths, '.', '..', '\\', ':', ASCII control
 * bytes, and (unless enabled) components beginning with '.' are rejected.
 * max_path_bytes bounds each input path and the final root/path join;
 * max_component_bytes and max_depth bound each component and their count.
 *
 * This policy is deliberately lexical.  The root is trusted and is not
 * normalized.  The standard C file API cannot prevent symlink traversal,
 * mount-point escape, path replacement races, special-file access, or blocking
 * I/O.  Callers requiring filesystem confinement must provide it in a
 * platform-specific layer.  The load functions bound allocation and consume
 * at most max_bytes plus one probe byte.
 */
typedef struct AFORC_AssetPathPolicy {
    size_t max_path_bytes;
    size_t max_component_bytes;
    size_t max_depth;
    bool allow_hidden_components;
} AFORC_AssetPathPolicy;

typedef struct AFORC_AssetBlob {
    uint8_t *data;
    size_t size;
} AFORC_AssetBlob;

typedef struct AFORC_AssetText {
    char *data;
    size_t size;
} AFORC_AssetText;

/* Blob and text outputs must be zero-initialized or released before reuse.
 * Successful binary loads of an empty file return {NULL, 0}. Successful text
 * loads always return a NUL-terminated allocation, including for an empty
 * file, and reject embedded NUL bytes. max_bytes bounds file content bytes;
 * text loads additionally require max_bytes < SIZE_MAX for the terminator.
 */
AFORC_API AFORC_AssetPathPolicy aforc_asset_path_policy_default(void);
AFORC_API AFORC_Status aforc_asset_path_validate(
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy);
AFORC_API AFORC_Status aforc_asset_path_join(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    char *destination,
    size_t destination_capacity);
AFORC_API AFORC_Status aforc_asset_load_binary(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    size_t max_bytes,
    AFORC_AssetBlob *output);
AFORC_API AFORC_Status aforc_asset_load_text(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    size_t max_bytes,
    AFORC_AssetText *output);
/* The store operation is neither atomic nor durable and may leave a partial
 * file after an I/O failure. */
AFORC_API AFORC_Status aforc_asset_store_binary(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    const void *data,
    size_t size,
    size_t max_bytes);
/*
 * POSIX atomic replacement writes through an exclusively created, owner-only
 * temporary file in the destination directory, flushes and synchronizes its
 * complete contents, closes it, then renames it over the destination and
 * synchronizes the containing directory. A failure before rename leaves an
 * existing destination unchanged; a failure after rename may report I/O while
 * leaving the complete new file installed. Temporary-file cleanup is attempted
 * on failure. Absent cleanup failures or external filesystem races, the
 * destination is always the previous complete file or the new complete file.
 *
 * The operation does not preserve destination metadata. It retains the path
 * policy's lexical-only caveats: trusted roots, symlink traversal, path and
 * mount replacement races, special files, and blocking I/O remain platform
 * concerns.
 */
AFORC_API AFORC_Status aforc_asset_store_binary_atomic(
    const char *root,
    const char *relative_path,
    const AFORC_AssetPathPolicy *policy,
    const void *data,
    size_t size,
    size_t max_bytes);
AFORC_API void aforc_asset_blob_release(AFORC_AssetBlob *blob);
AFORC_API void aforc_asset_text_release(AFORC_AssetText *text);

typedef struct AFORC_Rng {
    uint64_t state;
    uint64_t increment;
} AFORC_Rng;

/* Canonical PCG-XSH-RR 64/32.  Seed before use.  Equal seed/stream pairs have
 * identical output on every supported platform.  stream is encoded as the
 * canonical odd increment, so its most-significant bit is not significant. */
AFORC_API AFORC_Status aforc_rng_seed(
    AFORC_Rng *rng,
    uint64_t seed,
    uint64_t stream);
AFORC_API AFORC_Status aforc_rng_next_u32(AFORC_Rng *rng, uint32_t *output);
AFORC_API AFORC_Status aforc_rng_bounded_u32(
    AFORC_Rng *rng,
    uint32_t exclusive_bound,
    uint32_t *output);

typedef struct AFORC_ConfigLimits {
    size_t max_input_bytes;
    size_t max_line_bytes;
    size_t max_entries;
    size_t max_section_bytes;
    size_t max_key_bytes;
    size_t max_value_bytes;
} AFORC_ConfigLimits;

typedef struct AFORC_ConfigEntry {
    char *section;
    char *key;
    char *value;
} AFORC_ConfigEntry;

typedef struct AFORC_Config {
    AFORC_ConfigEntry *entries;
    size_t count;
} AFORC_Config;

/*
 * The parser accepts LF and CRLF lines, trims ASCII space and tab around each
 * line, section, key, and value, and recognizes full-line '#' or ';' comments.
 * Section lines are [name]; entries are key=value; entries before a section use
 * the empty section name.  Section and key names are case-sensitive and may
 * contain only ASCII letters, digits, '_', '-', and '.'.  A repeated
 * section/key is rejected with AFORC_ERROR_EXISTS.  Empty values are valid;
 * empty section declarations and keys are not.
 *
 * There are no quoted strings, escapes, inline comments, continuations,
 * interpolation, or UTF-8 validation. Input need not be NUL-terminated.
 * Non-ASCII value bytes are retained as opaque bytes. Embedded NUL and ASCII
 * control bytes other than tab and line endings are invalid. max_line_bytes
 * excludes LF and an optional preceding CR. All other text limits count
 * trimmed bytes and exclude allocated NUL terminators. output must be
 * zero-initialized or released before parsing.
 */
AFORC_API AFORC_ConfigLimits aforc_config_limits_default(void);
AFORC_API AFORC_Status aforc_config_parse(
    const char *input,
    size_t input_size,
    const AFORC_ConfigLimits *limits,
    AFORC_Config *output);
AFORC_API const char *aforc_config_get(
    const AFORC_Config *config,
    const char *section,
    const char *key);
AFORC_API void aforc_config_release(AFORC_Config *config);

typedef struct AFORC_SaveWriter {
    uint8_t *payload;
    size_t size;
    size_t capacity;
    size_t max_payload_bytes;
    uint32_t schema_version;
    bool initialized;
} AFORC_SaveWriter;

typedef struct AFORC_SaveReader {
    const uint8_t *payload;
    size_t size;
    size_t cursor;
    uint32_t schema_version;
    bool initialized;
} AFORC_SaveReader;

typedef struct AFORC_AssetView {
    const uint8_t *data;
    size_t size;
} AFORC_AssetView;

/*
 * Save container version 1 is exactly:
 *   0..3   "A2DS" (legacy version-1 wire magic retained for compatibility)
 *   4..5   container version, little-endian u16
 *   6..7   flags (zero), little-endian u16
 *   8..11  caller schema version, little-endian u32
 *   12..19 payload size, little-endian u64
 *   20..23 payload CRC-32/ISO-HDLC, little-endian u32
 *   24..27 CRC-32/ISO-HDLC of header bytes 0..23, little-endian u32
 *   28..   payload
 * Readers reject trailing bytes, nonzero flags, unsupported container versions,
 * schemas outside the caller's inclusive range, oversized payloads, and either
 * checksum mismatch.  The reader borrows the container memory.
 *
 * Writers and readers must be initialized before use. A writer passed to init
 * must be uninitialized or released first. Releasing a writer invalidates it;
 * readers own no memory and need no release operation. finish outputs must be
 * zero-initialized or released before reuse.
 *
 * Integer primitives use fixed-width little-endian representations.  Strings
 * use a little-endian u32 byte count followed by unmodified, non-NUL-terminated
 * bytes.  A returned AFORC_AssetView remains valid only while the borrowed
 * container remains alive and unchanged.
 */
AFORC_API AFORC_Status aforc_save_writer_init(
    AFORC_SaveWriter *writer,
    uint32_t schema_version,
    size_t max_payload_bytes);
AFORC_API void aforc_save_writer_release(AFORC_SaveWriter *writer);
AFORC_API AFORC_Status aforc_save_writer_write_u8(
    AFORC_SaveWriter *writer,
    uint8_t value);
AFORC_API AFORC_Status aforc_save_writer_write_u16(
    AFORC_SaveWriter *writer,
    uint16_t value);
AFORC_API AFORC_Status aforc_save_writer_write_u32(
    AFORC_SaveWriter *writer,
    uint32_t value);
AFORC_API AFORC_Status aforc_save_writer_write_u64(
    AFORC_SaveWriter *writer,
    uint64_t value);
AFORC_API AFORC_Status aforc_save_writer_write_i32(
    AFORC_SaveWriter *writer,
    int32_t value);
AFORC_API AFORC_Status aforc_save_writer_write_i64(
    AFORC_SaveWriter *writer,
    int64_t value);
AFORC_API AFORC_Status aforc_save_writer_write_bytes(
    AFORC_SaveWriter *writer,
    const void *data,
    size_t size);
AFORC_API AFORC_Status aforc_save_writer_write_string(
    AFORC_SaveWriter *writer,
    const char *text,
    size_t size);
AFORC_API AFORC_Status aforc_save_writer_finish(
    const AFORC_SaveWriter *writer,
    AFORC_AssetBlob *output);

AFORC_API AFORC_Status aforc_save_reader_init(
    AFORC_SaveReader *reader,
    const void *container,
    size_t container_size,
    size_t max_payload_bytes,
    uint32_t minimum_schema_version,
    uint32_t maximum_schema_version);
AFORC_API size_t aforc_save_reader_remaining(const AFORC_SaveReader *reader);
AFORC_API bool aforc_save_reader_finished(const AFORC_SaveReader *reader);
AFORC_API AFORC_Status aforc_save_reader_read_u8(
    AFORC_SaveReader *reader,
    uint8_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_u16(
    AFORC_SaveReader *reader,
    uint16_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_u32(
    AFORC_SaveReader *reader,
    uint32_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_u64(
    AFORC_SaveReader *reader,
    uint64_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_i32(
    AFORC_SaveReader *reader,
    int32_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_i64(
    AFORC_SaveReader *reader,
    int64_t *output);
AFORC_API AFORC_Status aforc_save_reader_read_bytes(
    AFORC_SaveReader *reader,
    void *destination,
    size_t size);
AFORC_API AFORC_Status aforc_save_reader_read_string(
    AFORC_SaveReader *reader,
    AFORC_AssetView *output);

#ifdef __cplusplus
}
#endif

#endif
