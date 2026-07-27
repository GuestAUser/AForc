/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include "renderer_diff_cases.h"
#include "renderer_diff_model.h"

#include "../src/platform/terminal_internal.h"
#include "../src/render/renderer_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    RENDERER_DIFF_WIDTH = 80,
    RENDERER_DIFF_HEIGHT = 24
};

typedef void (*RendererDiffPattern)(AFORC_Renderer *renderer);

static AFORC_Cell styled_cell(uint32_t codepoint, uint32_t selector)
{
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = codepoint;
    if ((selector & UINT32_C(1)) == 0U) {
        cell.foreground = aforc_color_indexed((uint8_t)(32U + selector % 192U));
    } else {
        cell.foreground = aforc_color_rgb((uint8_t)(selector * 17U),
                                          (uint8_t)(selector * 29U),
                                          (uint8_t)(selector * 43U));
    }
    if ((selector & UINT32_C(2)) != 0U) {
        cell.background = aforc_color_indexed((uint8_t)(selector % 16U));
    }
    cell.style = (selector & UINT32_C(4)) == 0U
                     ? AFORC_STYLE_BOLD
                     : AFORC_STYLE_UNDERLINE;
    return cell;
}

static void clustered_pattern(AFORC_Renderer *renderer)
{
    const AFORC_Cell cell = styled_cell((uint32_t)'#', 8U);

    for (int32_t row = 7; row < 17; ++row) {
        for (int32_t column = 20; column < 60; ++column) {
            renderer->back[(size_t)row * RENDERER_DIFF_WIDTH +
                           (size_t)column] = cell;
        }
    }
}

static void checker_pattern(AFORC_Renderer *renderer)
{
    AFORC_Cell cell = aforc_cell_default();

    cell.codepoint = (uint32_t)'#';
    for (int32_t row = 0; row < RENDERER_DIFF_HEIGHT; ++row) {
        for (int32_t column = 0; column < RENDERER_DIFF_WIDTH; column += 2) {
            renderer->back[(size_t)row * RENDERER_DIFF_WIDTH +
                           (size_t)column] = cell;
        }
    }
}

static void random_pattern(AFORC_Renderer *renderer)
{
    static const uint32_t codepoints[] = {
        (uint32_t)'@', UINT32_C(0x03bb), UINT32_C(0x4e2d),
        UINT32_C(0x1f642)};
    uint32_t state = UINT32_C(0x8f31a7c5);

    for (size_t index = 0U;
         index < RENDERER_DIFF_WIDTH * RENDERER_DIFF_HEIGHT;
         ++index) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        if (state % 100U < 47U) {
            renderer->back[index] = styled_cell(
                codepoints[state %
                           (sizeof(codepoints) / sizeof(codepoints[0]))],
                state >> 8U);
        }
    }
}

static bool batch_reaches_back(const AFORC_Renderer *renderer,
                               const AFORC_Cell *initial)
{
    const size_t count = RENDERER_DIFF_WIDTH * RENDERER_DIFF_HEIGHT;
    AFORC_Cell *cells = malloc(count * sizeof(*cells));
    bool matches = true;

    if (cells == NULL) {
        return false;
    }
    (void)memcpy(cells, initial, count * sizeof(*initial));
    if (!renderer_diff_apply_ansi(cells, renderer->size, renderer->batch,
                                  renderer->batch_size)) {
        free(cells);
        return false;
    }
    for (size_t index = 0U; index < count; ++index) {
        if (!aforc_renderer_cells_equal(cells[index], renderer->back[index])) {
            matches = false;
            break;
        }
    }
    free(cells);
    return matches;
}

static bool run_pattern(const char *name, RendererDiffPattern pattern)
{
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell poison = styled_cell((uint32_t)'X', 15U);
    AFORC_Cell *cleared = NULL;
    const size_t count = RENDERER_DIFF_WIDTH * RENDERER_DIFF_HEIGHT;
    size_t diff_size;
    size_t dense_size;
    bool passed = false;

    config.size = (AFORC_Size){RENDERER_DIFF_WIDTH, RENDERER_DIFF_HEIGHT};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return false;
    }
    cleared = malloc(count * sizeof(*cleared));
    if (cleared == NULL) {
        aforc_renderer_destroy(renderer);
        return false;
    }
    aforc_renderer_fill_cells(cleared, count, poison);
    renderer->invalidated = false;
    pattern(renderer);
    if (aforc_renderer_build_ansi(renderer) != AFORC_OK ||
        !batch_reaches_back(renderer, renderer->front)) {
        goto cleanup;
    }
    diff_size = renderer->batch_size;
    renderer->invalidated = true;
    if (aforc_renderer_build_ansi(renderer) != AFORC_OK ||
        !batch_reaches_back(renderer, cleared)) {
        goto cleanup;
    }
    dense_size = renderer->batch_size;
    (void)printf("renderer diff %s: diff=%zu dense=%zu\n",
                 name, diff_size, dense_size);
    passed = diff_size <= dense_size;

cleanup:
    free(cleared);
    aforc_renderer_destroy(renderer);
    return passed;
}

static bool invalid_cells_keep_existing_status(void)
{
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell invalid = aforc_cell_default();
    AFORC_Cell changed = aforc_cell_default();
    bool passed = false;

    config.size = (AFORC_Size){8, 1};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return false;
    }
    invalid.codepoint = 1U;
    changed.codepoint = (uint32_t)'#';
    renderer->invalidated = false;
    renderer->back[0] = invalid;
    if (aforc_renderer_build_ansi(renderer) != AFORC_ERROR_FORMAT) {
        goto cleanup;
    }
    renderer->front[0] = invalid;
    renderer->back[7] = changed;
    if (aforc_renderer_build_ansi(renderer) != AFORC_OK) {
        goto cleanup;
    }
    renderer->invalidated = true;
    passed = aforc_renderer_build_ansi(renderer) == AFORC_ERROR_FORMAT;

cleanup:
    aforc_renderer_destroy(renderer);
    return passed;
}

static bool batch_contains(const AFORC_Renderer *renderer,
                           const char *needle,
                           size_t needle_size)
{
    if (needle_size > renderer->batch_size) {
        return false;
    }
    for (size_t offset = 0U;
         offset <= renderer->batch_size - needle_size;
         ++offset) {
        if (memcmp(renderer->batch + offset, needle, needle_size) == 0) {
            return true;
        }
    }
    return false;
}

static bool invalidation_skips_cleared_cells(void)
{
    static const char clear_screen[] = "\x1b[0m\x1b[2J\x1b[H";
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    bool passed;

    config.size = (AFORC_Size){8, 2};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return false;
    }
    passed = aforc_renderer_build_ansi(renderer) == AFORC_OK &&
             renderer->batch_size == sizeof(clear_screen) - 1U &&
             memcmp(renderer->batch,
                    clear_screen,
                    sizeof(clear_screen) - 1U) == 0;
    aforc_renderer_destroy(renderer);
    return passed;
}

static bool non_ascii_repositions_following_cell(void)
{
    static const char second_column[] = "\x1b[1;2H";
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Cell wide = aforc_cell_default();
    AFORC_Cell following = aforc_cell_default();
    bool passed;

    config.size = (AFORC_Size){3, 1};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return false;
    }
    wide.codepoint = UINT32_C(0x4e2d);
    following.codepoint = (uint32_t)'X';
    renderer->invalidated = false;
    renderer->back[0] = wide;
    renderer->back[1] = following;
    passed = aforc_renderer_build_ansi(renderer) == AFORC_OK &&
             batch_contains(renderer,
                            second_column,
                            sizeof(second_column) - 1U);
    aforc_renderer_destroy(renderer);
    return passed;
}

static bool empty_present_skips_front_copy(void)
{
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Terminal terminal = {0};
    bool passed;

    config.size = (AFORC_Size){8, 2};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return false;
    }
    renderer->invalidated = false;
    renderer->back[0].foreground.red = UINT8_C(0x5a);
    terminal.active = true;
    terminal.size = config.size;

    passed = aforc_renderer_present(renderer, &terminal) == AFORC_OK &&
             renderer->batch_size == 0U &&
             renderer->front[0].foreground.red == 0U &&
             renderer->back[0].foreground.red == UINT8_C(0x5a) &&
             !renderer->invalidated;
    aforc_renderer_destroy(renderer);
    return passed;
}

int renderer_diff_run_benchmark(void)
{
    enum {
        BENCHMARK_ITERATIONS = 2000
    };
    AFORC_RendererConfig config = aforc_renderer_config_default();
    AFORC_Renderer *renderer = NULL;
    AFORC_Terminal terminal = {0};
    struct timespec started;
    struct timespec finished;
    uint64_t elapsed_ns;
    int result = 1;

    config.size = (AFORC_Size){320, 120};
    if (aforc_renderer_create(&renderer, &config) != AFORC_OK) {
        return result;
    }
    renderer->invalidated = false;
    terminal.active = true;
    terminal.size = config.size;
    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) {
        goto cleanup;
    }
    for (size_t iteration = 0U;
         iteration < (size_t)BENCHMARK_ITERATIONS;
         ++iteration) {
        if (aforc_renderer_present(renderer, &terminal) != AFORC_OK ||
            renderer->batch_size != 0U) {
            goto cleanup;
        }
    }
    if (clock_gettime(CLOCK_MONOTONIC, &finished) != 0) {
        goto cleanup;
    }
    elapsed_ns = (uint64_t)(finished.tv_sec - started.tv_sec) *
                 UINT64_C(1000000000);
    if (finished.tv_nsec >= started.tv_nsec) {
        elapsed_ns += (uint64_t)(finished.tv_nsec - started.tv_nsec);
    } else {
        elapsed_ns -= (uint64_t)(started.tv_nsec - finished.tv_nsec);
    }
    (void)printf("renderer empty-present benchmark: iterations=%d "
                 "cells=%d elapsed_ns=%" PRIu64 " ns_per_present=%" PRIu64
                 "\n",
                 BENCHMARK_ITERATIONS,
                 config.size.width * config.size.height,
                 elapsed_ns,
                 elapsed_ns / (uint64_t)BENCHMARK_ITERATIONS);
    result = 0;

cleanup:
    aforc_renderer_destroy(renderer);
    return result;
}

int renderer_diff_run_cases(void)
{
    int result = 0;

    if (!run_pattern("clustered", clustered_pattern)) {
        (void)fprintf(stderr, "clustered renderer diff failed\n");
        result = 1;
    }
    if (!run_pattern("checker", checker_pattern)) {
        (void)fprintf(stderr, "checker renderer diff exceeded dense redraw\n");
        result = 2;
    }
    if (!run_pattern("random", random_pattern)) {
        (void)fprintf(stderr, "random renderer diff failed\n");
        result = 3;
    }
    if (!invalid_cells_keep_existing_status()) {
        (void)fprintf(stderr, "renderer diff error semantics changed\n");
        result = 4;
    }
    if (!invalidation_skips_cleared_cells()) {
        (void)fprintf(stderr, "renderer invalidation emitted cleared cells\n");
        result = 5;
    }
    if (!non_ascii_repositions_following_cell()) {
        (void)fprintf(stderr, "renderer Unicode cursor recovery failed\n");
        result = 6;
    }
    if (!empty_present_skips_front_copy()) {
        (void)fprintf(stderr, "empty renderer diff copied the front buffer\n");
        result = 7;
    }
    return result;
}
