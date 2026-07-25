/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#include "../include/aforc/ui.h"

#include <stdio.h>

enum {
    UI_TEST_WIDTH = 6,
    UI_TEST_HEIGHT = 4
};

typedef struct TestCanvas {
    AFORC_Cell cells[UI_TEST_WIDTH * UI_TEST_HEIGHT];
    size_t plots;
} TestCanvas;

static AFORC_Status test_plot(void *context,
                              AFORC_Point position,
                              AFORC_Cell cell)
{
    TestCanvas *canvas = context;

    if (position.x < 0 || position.y < 0 || position.x >= UI_TEST_WIDTH ||
        position.y >= UI_TEST_HEIGHT) {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    canvas->cells[(size_t)position.y * UI_TEST_WIDTH + (size_t)position.x] =
        cell;
    ++canvas->plots;
    return AFORC_OK;
}

static void test_canvas_clear(TestCanvas *canvas)
{
    for (size_t index = 0U;
         index < sizeof(canvas->cells) / sizeof(canvas->cells[0]);
         ++index) {
        canvas->cells[index] = aforc_cell_default();
    }
    canvas->plots = 0U;
}

static uint32_t test_codepoint(const TestCanvas *canvas, int32_t x, int32_t y)
{
    return canvas->cells[(size_t)y * UI_TEST_WIDTH + (size_t)x].codepoint;
}

static bool canvas_and_widgets_are_clipped(void)
{
    TestCanvas target;
    AFORC_UICanvas canvas;
    AFORC_Cell mark = aforc_cell_default();
    AFORC_UIProgressStyle progress;
    static const char label[] = "ABCD";

    test_canvas_clear(&target);
    mark.codepoint = (uint32_t)'#';
    if (aforc_ui_canvas_init(&canvas, (AFORC_Rect){1, 1, 3, 2}, test_plot,
                             &target) != AFORC_OK ||
        aforc_ui_canvas_fill(&canvas, (AFORC_Rect){0, 0, 6, 4}, mark) !=
            AFORC_OK ||
        target.plots != 6U) {
        return false;
    }
    for (int32_t y = 0; y < UI_TEST_HEIGHT; ++y) {
        for (int32_t x = 0; x < UI_TEST_WIDTH; ++x) {
            const bool inside = x >= 1 && x < 4 && y >= 1 && y < 3;

            if (test_codepoint(&target, x, y) !=
                (inside ? (uint32_t)'#' : (uint32_t)' ')) {
                return false;
            }
        }
    }

    test_canvas_clear(&target);
    if (aforc_ui_canvas_init(&canvas, (AFORC_Rect){0, 0, 6, 1}, test_plot,
                             &target) != AFORC_OK ||
        aforc_ui_draw_label(&canvas, (AFORC_Rect){0, 0, 6, 1}, label,
                            sizeof(label) - 1U, AFORC_UI_ALIGN_END,
                            aforc_cell_default()) != AFORC_OK ||
        target.plots != 4U || test_codepoint(&target, 0, 0) != (uint32_t)' ' ||
        test_codepoint(&target, 1, 0) != (uint32_t)' ' ||
        test_codepoint(&target, 2, 0) != (uint32_t)'A' ||
        test_codepoint(&target, 5, 0) != (uint32_t)'D') {
        return false;
    }

    test_canvas_clear(&target);
    progress.filled = aforc_cell_default();
    progress.filled.codepoint = (uint32_t)'=';
    progress.empty = aforc_cell_default();
    progress.empty.codepoint = (uint32_t)'-';
    if (aforc_ui_draw_progress(&canvas, (AFORC_Rect){0, 0, 6, 1},
                               UINT64_MAX - 1U, UINT64_MAX,
                               &progress) != AFORC_OK ||
        target.plots != 6U) {
        return false;
    }
    for (int32_t x = 0; x < UI_TEST_WIDTH; ++x) {
        const uint32_t expected = x < 5 ? (uint32_t)'=' : (uint32_t)'-';

        if (test_codepoint(&target, x, 0) != expected) {
            return false;
        }
    }
    return true;
}

static bool focus_and_menu_state_is_stable(void)
{
    AFORC_UIFocusState focus;
    AFORC_UIMenuState menu;
    AFORC_UIAction action;
    AFORC_UIMenuItem items[] = {
        {"one", 3U, 11U, true},
        {"two", 3U, 22U, false},
        {"three", 5U, 33U, true},
    };

    if (aforc_ui_focus_init(&focus, 3U, 2U, false) != AFORC_OK ||
        aforc_ui_focus_handle(&focus, AFORC_UI_EVENT_FOCUS_NEXT, &action) !=
            AFORC_OK ||
        focus.index != 2U || action.type != AFORC_UI_ACTION_NONE ||
        action.index != AFORC_UI_NO_INDEX) {
        return false;
    }
    if (aforc_ui_menu_init(&menu, items, 3U, 1U, true) != AFORC_OK ||
        menu.selected != 2U ||
        aforc_ui_menu_handle(&menu, items, 3U, 1U, true,
                             AFORC_UI_EVENT_NAV_DOWN, &action) != AFORC_OK ||
        menu.selected != 0U || menu.scroll != 0U ||
        action.type != AFORC_UI_ACTION_SELECTION_CHANGED ||
        action.index != 0U || action.id != 11U) {
        return false;
    }
    if (aforc_ui_menu_handle(&menu, items, 3U, 1U, true,
                             AFORC_UI_EVENT_ACTIVATE, &action) != AFORC_OK ||
        action.type != AFORC_UI_ACTION_ACTIVATED || action.index != 0U ||
        action.id != 11U) {
        return false;
    }
    items[0].enabled = false;
    if (aforc_ui_menu_handle(&menu, items, 3U, 1U, true,
                             AFORC_UI_EVENT_NONE, &action) != AFORC_OK ||
        menu.selected != 2U || menu.scroll != 2U ||
        action.type != AFORC_UI_ACTION_NONE) {
        return false;
    }
    return true;
}

int main(void)
{
    if (!canvas_and_widgets_are_clipped()) {
        (void)fputs("UI canvas/widget regression failed\n", stderr);
        return 1;
    }
    if (!focus_and_menu_state_is_stable()) {
        (void)fputs("UI state regression failed\n", stderr);
        return 2;
    }
    (void)puts("UI regression: ok");
    return 0;
}
