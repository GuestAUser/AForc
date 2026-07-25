/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "../include/aforc/assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool test_path_policy(void)
{
    AFORC_AssetPathPolicy policy = aforc_asset_path_policy_default();
    char joined[64];
    char aliased_root[64] = "root";
    char aliased_relative[64] = "file.bin";

    if (aforc_asset_path_validate("sprites/player.txt", &policy) != AFORC_OK ||
        aforc_asset_path_validate("", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("/absolute", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("a//b", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("a/./b", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("a/../b", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("a\\b", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate("drive:name", &policy) != AFORC_ERROR_FORMAT ||
        aforc_asset_path_validate(".hidden", &policy) != AFORC_ERROR_FORMAT) {
        return false;
    }
    policy.allow_hidden_components = true;
    if (aforc_asset_path_validate(".hidden/save", &policy) != AFORC_OK) {
        return false;
    }
    policy.max_component_bytes = 3u;
    if (aforc_asset_path_validate("four", &policy) != AFORC_ERROR_LIMIT) {
        return false;
    }
    policy = aforc_asset_path_policy_default();
    policy.max_depth = 1u;
    if (aforc_asset_path_validate("a/b", &policy) != AFORC_ERROR_LIMIT) {
        return false;
    }
    policy = aforc_asset_path_policy_default();
    policy.max_path_bytes = 3u;
    if (aforc_asset_path_validate("abc", &policy) != AFORC_OK ||
        aforc_asset_path_validate("abcd", &policy) != AFORC_ERROR_LIMIT) {
        return false;
    }
    policy.max_path_bytes = SIZE_MAX;
    if (aforc_asset_path_validate("a", &policy) !=
        AFORC_ERROR_INVALID_ARGUMENT) {
        return false;
    }

    policy = aforc_asset_path_policy_default();
    if (aforc_asset_path_join("root", "dir/file", &policy,
                              joined, sizeof(joined)) != AFORC_OK ||
        strcmp(joined, "root/dir/file") != 0 ||
        aforc_asset_path_join("root", "file", &policy,
                              joined, 9u) != AFORC_ERROR_LIMIT ||
        joined[0] != '\0') {
        return false;
    }
    if (aforc_asset_path_join(aliased_root, "file.bin", &policy,
                              aliased_root, sizeof(aliased_root)) != AFORC_OK ||
        strcmp(aliased_root, "root/file.bin") != 0 ||
        aforc_asset_path_join("root", aliased_relative, &policy,
                              aliased_relative,
                              sizeof(aliased_relative)) != AFORC_OK ||
        strcmp(aliased_relative, "root/file.bin") != 0) {
        return false;
    }
    return true;
}

static bool make_file_path(
    char *destination,
    size_t capacity,
    const char *directory,
    const char *name)
{
    const int written = snprintf(destination, capacity, "%s/%s", directory, name);

    return written >= 0 && (size_t)written < capacity;
}

static void cleanup_test_directory(const char *directory)
{
    static const char *const names[] = {
        "data.bin", "text.txt", "empty.bin", "nul.txt"
    };
    char path[256];
    size_t index;

    for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (make_file_path(path, sizeof(path), directory, names[index])) {
            (void)unlink(path);
        }
    }
    (void)rmdir(directory);
}

static bool test_file_io(void)
{
    static const uint8_t binary[] = {0x10u, 0x20u, 0x30u, 0x40u};
    static const char text[] = "bounded text";
    static const char nul_text[] = {'a', '\0', 'b'};
    AFORC_AssetPathPolicy policy = aforc_asset_path_policy_default();
    AFORC_AssetBlob blob = {0};
    AFORC_AssetText loaded_text = {0};
    char directory_template[] = "/tmp/aforc-assets-XXXXXX";
    char *directory = mkdtemp(directory_template);
    bool passed = directory != NULL;

    if (!passed) {
        return false;
    }
    passed = aforc_asset_store_binary(directory, "data.bin", &policy,
                                      binary, sizeof(binary), sizeof(binary)) ==
                 AFORC_OK &&
             aforc_asset_load_binary(directory, "data.bin", &policy,
                                     sizeof(binary), &blob) == AFORC_OK &&
             blob.size == sizeof(binary) &&
             memcmp(blob.data, binary, sizeof(binary)) == 0;
    aforc_asset_blob_release(&blob);
    if (passed) {
        passed = aforc_asset_load_binary(directory, "data.bin", &policy,
                                         sizeof(binary) - 1u, &blob) ==
                     AFORC_ERROR_LIMIT &&
                 blob.data == NULL && blob.size == 0u;
    }
    if (passed) {
        passed = aforc_asset_store_binary(directory, "text.txt", &policy,
                                          text, sizeof(text) - 1u,
                                          sizeof(text) - 1u) == AFORC_OK &&
                 aforc_asset_load_text(directory, "text.txt", &policy,
                                       sizeof(text) - 1u,
                                       &loaded_text) == AFORC_OK &&
                 loaded_text.size == sizeof(text) - 1u &&
                 strcmp(loaded_text.data, text) == 0;
    }
    aforc_asset_text_release(&loaded_text);
    if (passed) {
        passed = aforc_asset_store_binary(directory, "empty.bin", &policy,
                                          NULL, 0u, 0u) == AFORC_OK &&
                 aforc_asset_load_binary(directory, "empty.bin", &policy,
                                         0u, &blob) == AFORC_OK &&
                 blob.data == NULL && blob.size == 0u &&
                 aforc_asset_load_text(directory, "empty.bin", &policy,
                                       0u, &loaded_text) == AFORC_OK &&
                 loaded_text.data != NULL && loaded_text.size == 0u &&
                 loaded_text.data[0] == '\0';
    }
    aforc_asset_blob_release(&blob);
    aforc_asset_text_release(&loaded_text);
    if (passed) {
        passed = aforc_asset_store_binary(directory, "nul.txt", &policy,
                                          nul_text, sizeof(nul_text),
                                          sizeof(nul_text)) == AFORC_OK &&
                 aforc_asset_load_text(directory, "nul.txt", &policy,
                                       sizeof(nul_text), &loaded_text) ==
                     AFORC_ERROR_FORMAT &&
                 loaded_text.data == NULL && loaded_text.size == 0u &&
                 aforc_asset_load_binary(directory, "missing.bin", &policy,
                                         1u, &blob) == AFORC_ERROR_NOT_FOUND;
    }
    aforc_asset_blob_release(&blob);
    aforc_asset_text_release(&loaded_text);
    cleanup_test_directory(directory);
    return passed;
}

static bool test_rng_vectors(void)
{
    static const uint32_t expected[] = {
        UINT32_C(0xa15c02b7), UINT32_C(0x7b47f409),
        UINT32_C(0xba1d3330), UINT32_C(0x83d2f293),
        UINT32_C(0xbfa4784b), UINT32_C(0xcbed606e),
    };
    AFORC_Rng rng;
    AFORC_Rng equivalent_stream;
    AFORC_Rng invalid = {0};
    uint32_t value;
    uint32_t equivalent;
    size_t index;

    if (aforc_rng_seed(&rng, UINT64_C(42), UINT64_C(54)) != AFORC_OK) {
        return false;
    }
    for (index = 0u; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        if (aforc_rng_next_u32(&rng, &value) != AFORC_OK ||
            value != expected[index]) {
            return false;
        }
    }
    if (aforc_rng_seed(&rng, UINT64_C(9), UINT64_C(7)) != AFORC_OK ||
        aforc_rng_seed(&equivalent_stream, UINT64_C(9),
                       UINT64_C(7) | (UINT64_C(1) << 63u)) != AFORC_OK ||
        aforc_rng_next_u32(&rng, &value) != AFORC_OK ||
        aforc_rng_next_u32(&equivalent_stream, &equivalent) != AFORC_OK ||
        value != equivalent ||
        aforc_rng_bounded_u32(&rng, 1u, &value) != AFORC_OK || value != 0u ||
        aforc_rng_next_u32(&invalid, &value) != AFORC_ERROR_INVALID_ARGUMENT ||
        aforc_rng_bounded_u32(&rng, 0u, &value) !=
            AFORC_ERROR_INVALID_ARGUMENT) {
        return false;
    }
    return true;
}

int main(void)
{
    if (!test_path_policy() || !test_file_io() || !test_rng_vectors()) {
        (void)fputs("asset contract regression failed\n", stderr);
        return 1;
    }
    (void)puts("asset contracts: ok");
    return 0;
}
