/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_UI_H
#define AFORC_UI_H

#include "common.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AFORC_UI_NO_INDEX ((size_t)-1)

typedef AFORC_Status (*AFORC_UIPlotFn)(void *context,
                                   AFORC_Point position,
                                   AFORC_Cell cell);

typedef struct AFORC_UICanvas {
    AFORC_Rect clip;
    AFORC_UIPlotFn plot;
    void *context;
} AFORC_UICanvas;

typedef enum AFORC_UIAlign {
    AFORC_UI_ALIGN_START = 0,
    AFORC_UI_ALIGN_CENTER,
    AFORC_UI_ALIGN_END
} AFORC_UIAlign;

typedef enum AFORC_UILayoutAxis {
    AFORC_UI_LAYOUT_ROW = 0,
    AFORC_UI_LAYOUT_COLUMN
} AFORC_UILayoutAxis;

typedef struct AFORC_UILayout {
    AFORC_Rect bounds;
    AFORC_UILayoutAxis axis;
    int32_t gap;
    int64_t cursor;
} AFORC_UILayout;

typedef enum AFORC_UIAnchor {
    AFORC_UI_ANCHOR_TOP_LEFT = 0,
    AFORC_UI_ANCHOR_TOP_CENTER,
    AFORC_UI_ANCHOR_TOP_RIGHT,
    AFORC_UI_ANCHOR_CENTER_LEFT,
    AFORC_UI_ANCHOR_CENTER,
    AFORC_UI_ANCHOR_CENTER_RIGHT,
    AFORC_UI_ANCHOR_BOTTOM_LEFT,
    AFORC_UI_ANCHOR_BOTTOM_CENTER,
    AFORC_UI_ANCHOR_BOTTOM_RIGHT
} AFORC_UIAnchor;

typedef struct AFORC_UIPanelStyle {
    AFORC_Cell top_left;
    AFORC_Cell top_right;
    AFORC_Cell bottom_left;
    AFORC_Cell bottom_right;
    AFORC_Cell horizontal;
    AFORC_Cell vertical;
    AFORC_Cell fill;
    bool fill_interior;
} AFORC_UIPanelStyle;

typedef struct AFORC_UIProgressStyle {
    AFORC_Cell filled;
    AFORC_Cell empty;
} AFORC_UIProgressStyle;

typedef struct AFORC_UIButtonStyle {
    AFORC_UIPanelStyle normal_panel;
    AFORC_UIPanelStyle focused_panel;
    AFORC_UIPanelStyle disabled_panel;
    AFORC_Cell normal_text;
    AFORC_Cell focused_text;
    AFORC_Cell disabled_text;
} AFORC_UIButtonStyle;

typedef struct AFORC_UIMenuItem {
    const char *label;
    size_t label_length;
    uint32_t id;
    bool enabled;
} AFORC_UIMenuItem;

typedef struct AFORC_UIMenuStyle {
    AFORC_Cell normal;
    AFORC_Cell selected;
    AFORC_Cell disabled;
    AFORC_Cell cursor;
} AFORC_UIMenuStyle;

typedef enum AFORC_UIEvent {
    AFORC_UI_EVENT_NONE = 0,
    AFORC_UI_EVENT_FOCUS_NEXT,
    AFORC_UI_EVENT_FOCUS_PREVIOUS,
    AFORC_UI_EVENT_NAV_UP,
    AFORC_UI_EVENT_NAV_DOWN,
    AFORC_UI_EVENT_NAV_HOME,
    AFORC_UI_EVENT_NAV_END,
    AFORC_UI_EVENT_ACTIVATE
} AFORC_UIEvent;

typedef enum AFORC_UIActionType {
    AFORC_UI_ACTION_NONE = 0,
    AFORC_UI_ACTION_FOCUS_CHANGED,
    AFORC_UI_ACTION_SELECTION_CHANGED,
    AFORC_UI_ACTION_ACTIVATED
} AFORC_UIActionType;

typedef struct AFORC_UIAction {
    AFORC_UIActionType type;
    size_t index;
    uint32_t id;
} AFORC_UIAction;

typedef struct AFORC_UIFocusState {
    size_t index;
    size_t count;
    bool wrap;
} AFORC_UIFocusState;

typedef struct AFORC_UIMenuState {
    size_t selected;
    size_t scroll;
    bool wrap;
} AFORC_UIMenuState;

AFORC_API AFORC_Status aforc_ui_canvas_init(AFORC_UICanvas *canvas,
                                      AFORC_Rect clip,
                                      AFORC_UIPlotFn plot,
                                      void *context);
AFORC_API AFORC_Status aforc_ui_canvas_child(const AFORC_UICanvas *parent,
                                       AFORC_Rect clip,
                                       AFORC_UICanvas *out_canvas);
AFORC_API AFORC_Status aforc_ui_canvas_plot(const AFORC_UICanvas *canvas,
                                      AFORC_Point position,
                                      AFORC_Cell cell);
AFORC_API AFORC_Status aforc_ui_canvas_fill(const AFORC_UICanvas *canvas,
                                      AFORC_Rect rect,
                                      AFORC_Cell cell);

AFORC_API AFORC_Status aforc_ui_layout_init(AFORC_UILayout *layout,
                                      AFORC_Rect bounds,
                                      AFORC_UILayoutAxis axis,
                                      int32_t gap);
AFORC_API AFORC_Status aforc_ui_layout_next(AFORC_UILayout *layout,
                                      int32_t extent,
                                      AFORC_Rect *out_rect);
AFORC_API AFORC_Status aforc_ui_layout_split(AFORC_Rect bounds,
                                       AFORC_UILayoutAxis axis,
                                       size_t count,
                                       int32_t gap,
                                       size_t index,
                                       AFORC_Rect *out_rect);
AFORC_API AFORC_Status aforc_ui_layout_anchor(AFORC_Rect bounds,
                                        AFORC_Size size,
                                        AFORC_UIAnchor anchor,
                                        AFORC_Rect *out_rect);

AFORC_API AFORC_UIPanelStyle aforc_ui_panel_style_ascii(AFORC_Cell border,
                                                   AFORC_Cell fill,
                                                   bool fill_interior);
AFORC_API AFORC_Status aforc_ui_draw_panel(const AFORC_UICanvas *canvas,
                                     AFORC_Rect rect,
                                     const AFORC_UIPanelStyle *style);
AFORC_API AFORC_Status aforc_ui_draw_label(const AFORC_UICanvas *canvas,
                                     AFORC_Rect rect,
                                     const char *text,
                                     size_t text_length,
                                     AFORC_UIAlign align,
                                     AFORC_Cell cell);
AFORC_API AFORC_Status aforc_ui_draw_progress(
    const AFORC_UICanvas *canvas,
    AFORC_Rect rect,
    uint64_t value,
    uint64_t maximum,
    const AFORC_UIProgressStyle *style);
AFORC_API AFORC_Status aforc_ui_draw_button(
    const AFORC_UICanvas *canvas,
    AFORC_Rect rect,
    const char *label,
    size_t label_length,
    bool focused,
    bool enabled,
    const AFORC_UIButtonStyle *style);
AFORC_API AFORC_Status aforc_ui_draw_menu(const AFORC_UICanvas *canvas,
                                    AFORC_Rect rect,
                                    const AFORC_UIMenuItem *items,
                                    size_t item_count,
                                    const AFORC_UIMenuState *state,
                                    const AFORC_UIMenuStyle *style);

AFORC_API AFORC_Status aforc_ui_focus_init(AFORC_UIFocusState *state,
                                     size_t count,
                                     size_t initial_index,
                                     bool wrap);
AFORC_API AFORC_Status aforc_ui_focus_handle(AFORC_UIFocusState *state,
                                       AFORC_UIEvent event,
                                       AFORC_UIAction *out_action);
AFORC_API AFORC_Status aforc_ui_button_handle(uint32_t id,
                                        bool focused,
                                        bool enabled,
                                        AFORC_UIEvent event,
                                        AFORC_UIAction *out_action);
AFORC_API AFORC_Status aforc_ui_menu_init(AFORC_UIMenuState *state,
                                    const AFORC_UIMenuItem *items,
                                    size_t item_count,
                                    size_t initial_index,
                                    bool wrap);
AFORC_API AFORC_Status aforc_ui_menu_handle(AFORC_UIMenuState *state,
                                      const AFORC_UIMenuItem *items,
                                      size_t item_count,
                                      size_t visible_rows,
                                      bool focused,
                                      AFORC_UIEvent event,
                                      AFORC_UIAction *out_action);

#ifdef __cplusplus
}
#endif

#endif
