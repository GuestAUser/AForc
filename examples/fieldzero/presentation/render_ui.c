#include "fieldzero/presentation.h"

#include "aforc/ui.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum
{
    FIELDZERO_VISUAL_CANVAS = 0,
    FIELDZERO_VISUAL_INK,
    FIELDZERO_VISUAL_FRAME,
    FIELDZERO_VISUAL_SIGNAL,
    FIELDZERO_SPACE_1 = 1,
    FIELDZERO_SPACE_2 = 2,
    FIELDZERO_SPACE_3 = 3,
    FIELDZERO_TITLE_PANEL_WIDTH = 64,
    FIELDZERO_TITLE_PANEL_HEIGHT = 14,
    FIELDZERO_HELP_PANEL_WIDTH = 64,
    FIELDZERO_HELP_PANEL_HEIGHT = 16,
    FIELDZERO_OVERLAY_WIDTH = 48,
    FIELDZERO_OVERLAY_HEIGHT = 7,
    FIELDZERO_HUD_LINE_CAPACITY = 160
};

static AFORC_Cell
fieldzero_ui_cell(uint8_t role, AFORC_CellStyle style, bool no_color)
{
    return fieldzero_visual_cell((uint32_t)' ', role, style, no_color);
}

static AFORC_UIPanelStyle fieldzero_panel_style(bool strong, bool no_color)
{
    return aforc_ui_panel_style_ascii(
        fieldzero_ui_cell(strong ? FIELDZERO_VISUAL_INK
                                 : FIELDZERO_VISUAL_FRAME,
                          strong ? AFORC_STYLE_BOLD : AFORC_STYLE_NONE,
                          no_color),
        fieldzero_ui_cell(FIELDZERO_VISUAL_CANVAS, AFORC_STYLE_NONE, no_color),
        true);
}

static AFORC_Status fieldzero_label(const AFORC_UICanvas *canvas,
                                    AFORC_Rect rect,
                                    const char *text,
                                    AFORC_UIAlign align,
                                    uint8_t role,
                                    AFORC_CellStyle style,
                                    bool no_color)
{
    return aforc_ui_draw_label(canvas,
                               rect,
                               text,
                               strlen(text),
                               align,
                               fieldzero_ui_cell(role, style, no_color));
}

static AFORC_Status fieldzero_draw_title(const AFORC_UICanvas *canvas,
                                         const FieldzeroGame *game,
                                         bool no_color,
                                         AFORC_Rect arena)
{
    AFORC_Rect panel;
    AFORC_UIPanelStyle panel_style = fieldzero_panel_style(false, no_color);
    char seed_text[64];
    AFORC_Status status = aforc_ui_layout_anchor(
        arena,
        (AFORC_Size){FIELDZERO_TITLE_PANEL_WIDTH, FIELDZERO_TITLE_PANEL_HEIGHT},
        AFORC_UI_ANCHOR_CENTER,
        &panel);

    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(canvas, panel, &panel_style);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                                         panel.y + FIELDZERO_SPACE_1,
                                         panel.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            "FIELD ZERO // SURVEY RECOVERY",
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
    }
    const int32_t origin_x = panel.x + 13;
    const int32_t origin_y = panel.y + 6;

    for (int32_t offset = -5; status == AFORC_OK && offset <= 5; ++offset)
    {
        status =
            aforc_ui_canvas_plot(canvas,
                                 (AFORC_Point){origin_x + offset, origin_y},
                                 fieldzero_visual_cell((uint32_t)'-',
                                                       FIELDZERO_VISUAL_FRAME,
                                                       AFORC_STYLE_DIM,
                                                       no_color));
    }
    for (int32_t offset = -3; status == AFORC_OK && offset <= 3; ++offset)
    {
        status =
            aforc_ui_canvas_plot(canvas,
                                 (AFORC_Point){origin_x, origin_y + offset},
                                 fieldzero_visual_cell((uint32_t)'|',
                                                       FIELDZERO_VISUAL_FRAME,
                                                       AFORC_STYLE_DIM,
                                                       no_color));
    }
    if (status == AFORC_OK)
    {
        status = aforc_ui_canvas_plot(
            canvas,
            (AFORC_Point){origin_x, origin_y},
            fieldzero_visual_cell((uint32_t)'+',
                                  FIELDZERO_VISUAL_SIGNAL,
                                  AFORC_STYLE_BOLD | AFORC_STYLE_REVERSE,
                                  no_color));
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + 24, panel.y + 4, panel.width - 27, 1},
            "REFERENCE DATUM: UNRESOLVED",
            AFORC_UI_ALIGN_START,
            FIELDZERO_VISUAL_INK,
            AFORC_STYLE_BOLD,
            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + 24, panel.y + 6, panel.width - 27, 1},
            "TOUCH EACH + IN SEQUENCE.",
            AFORC_UI_ALIGN_START,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_NONE,
            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + 24, panel.y + 7, panel.width - 27, 1},
            "REACH > WHEN ALL MARKS ARE x.",
            AFORC_UI_ALIGN_START,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_NONE,
            no_color);
    }
    (void)snprintf(seed_text, sizeof(seed_text), "SEED %" PRIu64, game->seed);
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + 24, panel.y + 9, panel.width - 27, 1},
            seed_text,
            AFORC_UI_ALIGN_START,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_DIM,
            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                         panel.y + panel.height - FIELDZERO_SPACE_2,
                         panel.width - 2 * FIELDZERO_SPACE_2,
                         1},
            "ENTER BEGIN   ? HELP   Q QUIT",
            AFORC_UI_ALIGN_CENTER,
            FIELDZERO_VISUAL_INK,
            AFORC_STYLE_BOLD,
            no_color);
    }
    return status;
}

static AFORC_Status fieldzero_draw_overview_map(const AFORC_UICanvas *canvas,
                                                AFORC_Rect panel,
                                                bool no_color)
{
    static const char *const names[FIELDZERO_SECTOR_COUNT] = {
        "ORIGIN", "SPAN", "WELL", "SHEAR", "HORIZON"};
    static const char *const maps[FIELDZERO_SECTOR_COUNT][3] = {
        {"+--:--", "|..:..", "---:--"},
        {"------", "..==..", "------"},
        {"|..|..", "|==|..", "|..|.."},
        {"__..__", "..==..", "__..__"},
        {":..:..", "--==--", ":..:.."}};
    const AFORC_Rect bounds = {panel.x + FIELDZERO_SPACE_2,
                               panel.y + 4,
                               panel.width - 2 * FIELDZERO_SPACE_2,
                               5};

    for (size_t sector = 0U; sector < FIELDZERO_SECTOR_COUNT; ++sector)
    {
        AFORC_Rect column;
        AFORC_Status status = aforc_ui_layout_split(bounds,
                                                    AFORC_UI_LAYOUT_ROW,
                                                    FIELDZERO_SECTOR_COUNT,
                                                    FIELDZERO_SPACE_1,
                                                    sector,
                                                    &column);

        if (status != AFORC_OK)
        {
            return status;
        }
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){column.x, column.y, column.width, 1},
                            names[sector],
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
        for (size_t row = 0U; status == AFORC_OK && row < 3U; ++row)
        {
            status = fieldzero_label(
                canvas,
                (AFORC_Rect){
                    column.x, column.y + 2 + (int32_t)row, column.width, 1},
                maps[sector][row],
                AFORC_UI_ALIGN_CENTER,
                FIELDZERO_VISUAL_FRAME,
                AFORC_STYLE_DIM,
                no_color);
        }
        if (status != AFORC_OK)
        {
            return status;
        }
    }
    return aforc_ui_canvas_plot(
        canvas,
        (AFORC_Point){bounds.x +
                          (bounds.width / FIELDZERO_SECTOR_COUNT - 6) / 2,
                      bounds.y + 2},
        fieldzero_visual_cell((uint32_t)'+',
                              FIELDZERO_VISUAL_SIGNAL,
                              AFORC_STYLE_BOLD | AFORC_STYLE_REVERSE,
                              no_color));
}

static AFORC_Status fieldzero_draw_completion(const AFORC_UICanvas *canvas,
                                              const FieldzeroGame *game,
                                              bool no_color,
                                              AFORC_Rect arena)
{
    AFORC_UIPanelStyle panel_style = fieldzero_panel_style(true, no_color);
    char stats[FIELDZERO_HUD_LINE_CAPACITY];
    char seed[64];
    AFORC_Status status = aforc_ui_draw_panel(canvas, arena, &panel_style);

    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){arena.x + FIELDZERO_SPACE_2,
                                         arena.y + FIELDZERO_SPACE_1,
                                         arena.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            "FIELD ZERO // RUN COMPLETE",
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_draw_overview_map(canvas, arena, no_color);
    }
    (void)snprintf(stats,
                   sizeof(stats),
                   "ROOMS %u/%u   MEMORIES %u/%u   FALLS %" PRIu32,
                   fieldzero_popcount_u16(game->completed_rooms),
                   FIELDZERO_ROOM_COUNT,
                   fieldzero_popcount_u16(game->collected_memories),
                   FIELDZERO_MEMORY_COUNT,
                   game->falls);
    (void)snprintf(seed, sizeof(seed), "SEED %" PRIu64, game->seed);
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){arena.x + FIELDZERO_SPACE_2,
                                         arena.y + 11,
                                         arena.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            stats,
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_NONE,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){arena.x + FIELDZERO_SPACE_2,
                                         arena.y + 12,
                                         arena.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            seed,
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_FRAME,
                            AFORC_STYLE_DIM,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){arena.x + FIELDZERO_SPACE_2,
                         arena.y + 14,
                         arena.width - 2 * FIELDZERO_SPACE_2,
                         1},
            "SURVEY REFERENCE: RESTORED // ORIGIN: OPERATOR POSITION",
            AFORC_UI_ALIGN_CENTER,
            FIELDZERO_VISUAL_INK,
            AFORC_STYLE_BOLD,
            no_color);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){arena.x + FIELDZERO_SPACE_2,
                                         arena.y + 16,
                                         arena.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            "R START NEW RUN   Q QUIT",
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
    }
    return status;
}

static void fieldzero_format_phase_status(const FieldzeroGame *game,
                                          char *text,
                                          size_t capacity)
{
    switch (game->phase)
    {
        case FIELDZERO_PHASE_REGISTERING:
            (void)snprintf(text,
                           capacity,
                           "REGISTERING + %u/%zu // FOUR-CELL SHIFT",
                           (unsigned int)game->registration_target_state,
                           fieldzero_room_mark_count(game->room));
            break;
        case FIELDZERO_PHASE_DISSOLVING:
            (void)snprintf(
                text, capacity, "POSITION UNRESOLVED // RESTORING CHECKPOINT");
            break;
        case FIELDZERO_PHASE_ROOM_TRANSITION:
            (void)snprintf(text,
                           capacity,
                           game->transition_direction < 0
                               ? "RETURNING // ALIGNMENTS PRESERVED"
                               : "ROOM RECORDED // UPDATING SURVEY");
            break;
        case FIELDZERO_PHASE_SECTOR_TRANSITION:
            (void)snprintf(text, capacity, "SECTOR MAP // REFERENCE UPDATED");
            break;
        case FIELDZERO_PHASE_COMPLETE:
            (void)snprintf(text, capacity, "SURVEY REFERENCE RESTORED");
            break;
        case FIELDZERO_PHASE_ACTIVE:
        default:
            if (game->room_state < fieldzero_room_mark_count(game->room))
            {
                (void)snprintf(text,
                               capacity,
                               "OBJECTIVE // TOUCH + %u/%zu TO ALIGN",
                               (unsigned int)game->room_state + 1U,
                               fieldzero_room_mark_count(game->room));
            }
            else
            {
                (void)snprintf(
                    text, capacity, "OBJECTIVE // REACH > // ROOM ALIGNED");
            }
            break;
    }
}

static AFORC_Status fieldzero_draw_hud(const AFORC_UICanvas *canvas,
                                       const FieldzeroGame *game,
                                       bool no_color,
                                       AFORC_Rect arena)
{
    char header[FIELDZERO_HUD_LINE_CAPACITY];
    char status_text[FIELDZERO_HUD_LINE_CAPACITY];
    char phase_text[FIELDZERO_HUD_LINE_CAPACITY];
    char memory_text[FIELDZERO_HUD_LINE_CAPACITY];
    const char *sector = fieldzero_sector_name(game->room->sector);
    const int32_t hud_y = arena.y + arena.height + FIELDZERO_SPACE_1;

    (void)snprintf(header,
                   sizeof(header),
                   "FIELD ZERO // %u/%u %s // %02u %s",
                   (unsigned int)game->room->sector + 1U,
                   FIELDZERO_SECTOR_COUNT,
                   sector,
                   (unsigned int)game->room_index + 1U,
                   game->room->name);
    (void)snprintf(
        status_text,
        sizeof(status_text),
        "SURVEY %02u/%u   ALIGN %u/%zu   MEMORY %02u/%u   FALLS %" PRIu32,
        fieldzero_popcount_u16(game->completed_rooms),
        FIELDZERO_ROOM_COUNT,
        (unsigned int)game->room_state,
        fieldzero_room_mark_count(game->room),
        fieldzero_popcount_u16(game->collected_memories),
        FIELDZERO_MEMORY_COUNT,
        game->falls);
    fieldzero_format_phase_status(game, phase_text, sizeof(phase_text));
    AFORC_Status result = fieldzero_label(
        canvas,
        (AFORC_Rect){arena.x, arena.y - FIELDZERO_SPACE_1, arena.width, 1},
        header,
        AFORC_UI_ALIGN_START,
        FIELDZERO_VISUAL_FRAME,
        AFORC_STYLE_BOLD,
        no_color);

    if (result == AFORC_OK)
    {
        result = fieldzero_label(canvas,
                                 (AFORC_Rect){arena.x, hud_y, arena.width, 1},
                                 status_text,
                                 AFORC_UI_ALIGN_START,
                                 FIELDZERO_VISUAL_INK,
                                 AFORC_STYLE_NONE,
                                 no_color);
    }
    if (result == AFORC_OK)
    {
        result = fieldzero_label(
            canvas,
            (AFORC_Rect){arena.x, hud_y + 1, arena.width, 1},
            phase_text,
            AFORC_UI_ALIGN_START,
            game->phase == FIELDZERO_PHASE_ACTIVE ? FIELDZERO_VISUAL_FRAME
                                                  : FIELDZERO_VISUAL_INK,
            game->phase == FIELDZERO_PHASE_ACTIVE ? AFORC_STYLE_NONE
                                                  : AFORC_STYLE_BOLD,
            no_color);
    }
    if (result != AFORC_OK)
    {
        return result;
    }
    if (game->memory_collected_here && game->room->memory_text != NULL)
    {
        const char *line = game->room->memory_text;
        const char *newline = strchr(line, '\n');
        const size_t first_length =
            newline == NULL ? strlen(line) : (size_t)(newline - line);

        (void)snprintf(memory_text,
                       sizeof(memory_text),
                       "MEMORY %02u/%u // %.*s",
                       fieldzero_popcount_u16(game->collected_memories),
                       FIELDZERO_MEMORY_COUNT,
                       (int)first_length,
                       line);
        result =
            fieldzero_label(canvas,
                            (AFORC_Rect){arena.x, hud_y + 2, arena.width, 1},
                            memory_text,
                            AFORC_UI_ALIGN_START,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
        if (result == AFORC_OK && newline != NULL && newline[1] != '\0')
        {
            result = fieldzero_label(
                canvas,
                (AFORC_Rect){arena.x, hud_y + 3, arena.width, 1},
                newline + 1,
                AFORC_UI_ALIGN_START,
                FIELDZERO_VISUAL_INK,
                AFORC_STYLE_BOLD,
                no_color);
        }
    }
    else
    {
        result = fieldzero_label(
            canvas,
            (AFORC_Rect){arena.x, hud_y + 2, arena.width, 1},
            "A/D MOVE   SPACE/Z JUMP   X DASH   ? HELP   P PAUSE",
            AFORC_UI_ALIGN_START,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_DIM,
            no_color);
    }
    return result;
}

static AFORC_Status fieldzero_draw_overlay(const AFORC_UICanvas *canvas,
                                           AFORC_Rect arena,
                                           const char *title,
                                           const char *line_one,
                                           const char *line_two,
                                           bool no_color)
{
    AFORC_Rect panel;
    AFORC_UIPanelStyle panel_style = fieldzero_panel_style(true, no_color);
    AFORC_Status status = aforc_ui_layout_anchor(
        arena,
        (AFORC_Size){FIELDZERO_OVERLAY_WIDTH, FIELDZERO_OVERLAY_HEIGHT},
        AFORC_UI_ANCHOR_CENTER,
        &panel);

    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(canvas, panel, &panel_style);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                                         panel.y + FIELDZERO_SPACE_1,
                                         panel.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            title,
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                                         panel.y + FIELDZERO_SPACE_3,
                                         panel.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            line_one,
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_NONE,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                         panel.y + panel.height - FIELDZERO_SPACE_2,
                         panel.width - 2 * FIELDZERO_SPACE_2,
                         1},
            line_two,
            AFORC_UI_ALIGN_CENTER,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_DIM,
            no_color);
    }
    return status;
}

static AFORC_Status fieldzero_draw_help(const AFORC_UICanvas *canvas,
                                        AFORC_Rect arena,
                                        bool no_color)
{
    static const char *const lines[] = {
        "MOVE       LEFT/RIGHT OR A/D",
        "JUMP       SPACE OR Z   RELEASE FOR SHORT JUMP",
        "WALL KICK  JUMP WHILE TOUCHING A WALL",
        "AIR DASH   X   ONE DASH UNTIL LANDING",
        "REGISTER   TOUCH + IN ORDER   BANDS SHIFT 4 CELLS",
        "EXIT       REACH > AFTER EVERY + BECOMES x",
        "RESTART    R   RETURN TO ROOM ENTRY",
        "PAUSE      P",
        "QUIT       Q OR ESCAPE",
        "GLYPHS     @ PLAYER   o MEMORY   > EXIT"};
    AFORC_Rect panel;
    AFORC_UIPanelStyle panel_style = fieldzero_panel_style(true, no_color);
    AFORC_Status status = aforc_ui_layout_anchor(
        arena,
        (AFORC_Size){FIELDZERO_HELP_PANEL_WIDTH, FIELDZERO_HELP_PANEL_HEIGHT},
        AFORC_UI_ANCHOR_CENTER,
        &panel);

    if (status == AFORC_OK)
    {
        status = aforc_ui_draw_panel(canvas, panel, &panel_style);
    }
    if (status == AFORC_OK)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                                         panel.y + FIELDZERO_SPACE_1,
                                         panel.width - 2 * FIELDZERO_SPACE_2,
                                         1},
                            "FIELD ZERO // CONTROLS",
                            AFORC_UI_ALIGN_CENTER,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_BOLD,
                            no_color);
    }
    for (size_t index = 0U;
         status == AFORC_OK && index < sizeof(lines) / sizeof(lines[0]);
         ++index)
    {
        status =
            fieldzero_label(canvas,
                            (AFORC_Rect){panel.x + FIELDZERO_SPACE_3,
                                         panel.y + 3 + (int32_t)index,
                                         panel.width - 2 * FIELDZERO_SPACE_3,
                                         1},
                            lines[index],
                            AFORC_UI_ALIGN_START,
                            FIELDZERO_VISUAL_INK,
                            AFORC_STYLE_NONE,
                            no_color);
    }
    if (status == AFORC_OK)
    {
        status = fieldzero_label(
            canvas,
            (AFORC_Rect){panel.x + FIELDZERO_SPACE_2,
                         panel.y + panel.height - FIELDZERO_SPACE_2,
                         panel.width - 2 * FIELDZERO_SPACE_2,
                         1},
            "? OR Q CLOSE",
            AFORC_UI_ALIGN_CENTER,
            FIELDZERO_VISUAL_FRAME,
            AFORC_STYLE_BOLD,
            no_color);
    }
    return status;
}

static const char *fieldzero_sector_record(FieldzeroSector sector)
{
    switch (sector)
    {
        case FIELDZERO_SECTOR_ORIGIN:
            return "DATUM CONTROL";
        case FIELDZERO_SECTOR_SPAN:
            return "BASELINE SURVEY";
        case FIELDZERO_SECTOR_WELL:
            return "DEPTH SOUNDING";
        case FIELDZERO_SECTOR_SHEAR:
            return "GRID CORRECTION";
        case FIELDZERO_SECTOR_HORIZON:
            return "CONTROL NETWORK";
        default:
            return "SURVEY SECTOR";
    }
}

static AFORC_Status fieldzero_draw_room_transition(const AFORC_UICanvas *canvas,
                                                   const FieldzeroGame *game,
                                                   AFORC_Rect arena,
                                                   bool no_color)
{
    char title[FIELDZERO_HUD_LINE_CAPACITY];
    char line_one[FIELDZERO_HUD_LINE_CAPACITY];
    char line_two[FIELDZERO_HUD_LINE_CAPACITY];

    if (game->transition_direction < 0)
    {
        const size_t destination_index = (size_t)game->room_index - 1U;
        const FieldzeroRoomDefinition *destination =
            fieldzero_content_room(destination_index);

        (void)snprintf(title,
                       sizeof(title),
                       "RETURNING // ROOM %02u/%u",
                       (unsigned int)destination_index + 1U,
                       FIELDZERO_ROOM_COUNT);
        (void)snprintf(line_one,
                       sizeof(line_one),
                       "%s / %s",
                       fieldzero_sector_name(destination->sector),
                       destination->name);
        (void)snprintf(
            line_two, sizeof(line_two), "CURRENT ALIGNMENTS PRESERVED");
    }
    else
    {
        const size_t destination_index = (size_t)game->room_index + 1U;
        const FieldzeroRoomDefinition *destination =
            fieldzero_content_room(destination_index);

        (void)snprintf(title,
                       sizeof(title),
                       "ROOM %02u/%u RECORDED",
                       (unsigned int)game->room_index + 1U,
                       FIELDZERO_ROOM_COUNT);
        (void)snprintf(line_one,
                       sizeof(line_one),
                       "%u/%u SURVEY ROOMS ALIGNED",
                       fieldzero_popcount_u16(game->completed_rooms),
                       FIELDZERO_ROOM_COUNT);
        if (destination != NULL)
        {
            (void)snprintf(line_two,
                           sizeof(line_two),
                           "NEXT // %s / %s",
                           fieldzero_sector_name(destination->sector),
                           destination->name);
        }
        else
        {
            (void)snprintf(
                line_two, sizeof(line_two), "RECONSTRUCTING SURVEY MAP");
        }
    }
    return fieldzero_draw_overlay(
        canvas, arena, title, line_one, line_two, no_color);
}

static AFORC_Status
fieldzero_draw_sector_transition(const AFORC_UICanvas *canvas,
                                 const FieldzeroGame *game,
                                 AFORC_Rect arena,
                                 bool no_color)
{
    char title[FIELDZERO_HUD_LINE_CAPACITY];
    char room[FIELDZERO_HUD_LINE_CAPACITY];

    (void)snprintf(title,
                   sizeof(title),
                   "SECTOR %u/%u // %s",
                   (unsigned int)game->room->sector + 1U,
                   FIELDZERO_SECTOR_COUNT,
                   fieldzero_sector_name(game->room->sector));
    (void)snprintf(room,
                   sizeof(room),
                   "ROOM %02u/%u // %s",
                   (unsigned int)game->room_index + 1U,
                   FIELDZERO_ROOM_COUNT,
                   game->room->name);
    return fieldzero_draw_overlay(canvas,
                                  arena,
                                  title,
                                  fieldzero_sector_record(game->room->sector),
                                  room,
                                  no_color);
}

static AFORC_Status fieldzero_draw_state_overlay(const AFORC_UICanvas *canvas,
                                                 const FieldzeroGame *game,
                                                 const FieldzeroViewState *view,
                                                 AFORC_Rect arena,
                                                 bool no_color)
{
    if (view->quit_confirmation)
    {
        return fieldzero_draw_overlay(canvas,
                                      arena,
                                      "QUIT CURRENT RUN?",
                                      "Q CONFIRM   ESC CONTINUE",
                                      "EXIT ERASES THIS RUN",
                                      no_color);
    }
    if (view->focus_paused)
    {
        return fieldzero_draw_overlay(canvas,
                                      arena,
                                      "INPUT FOCUS LOST",
                                      "SIMULATION PAUSED",
                                      "RETURN TO THE TERMINAL TO RESUME",
                                      no_color);
    }
    if (view->help_visible)
    {
        return fieldzero_draw_help(canvas, arena, no_color);
    }
    if (view->paused)
    {
        return fieldzero_draw_overlay(canvas,
                                      arena,
                                      "PAUSED",
                                      "P RESUME   R RESTART ROOM",
                                      "? HELP   Q RESUME",
                                      no_color);
    }
    if (game->phase == FIELDZERO_PHASE_DISSOLVING)
    {
        return fieldzero_draw_overlay(canvas,
                                      arena,
                                      "POSITION UNRESOLVED",
                                      "RESTORING CHECKPOINT",
                                      "ROOM ALIGNMENT PRESERVED",
                                      no_color);
    }
    if (game->phase == FIELDZERO_PHASE_ROOM_TRANSITION)
    {
        return fieldzero_draw_room_transition(canvas, game, arena, no_color);
    }
    if (game->phase == FIELDZERO_PHASE_SECTOR_TRANSITION)
    {
        return fieldzero_draw_sector_transition(canvas, game, arena, no_color);
    }
    return AFORC_OK;
}

AFORC_Status fieldzero_render_ui(AFORC_Renderer *renderer,
                                 const FieldzeroGame *game,
                                 const FieldzeroPresentation *presentation,
                                 const FieldzeroViewState *view,
                                 AFORC_Rect arena)
{
    AFORC_UICanvas canvas;
    const AFORC_Size screen = aforc_renderer_size(renderer);
    AFORC_Status status;

    if (renderer == NULL || game == NULL || presentation == NULL ||
        view == NULL)
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    status =
        aforc_ui_canvas_init(&canvas,
                             (AFORC_Rect){0, 0, screen.width, screen.height},
                             fieldzero_visual_plot,
                             renderer);
    if (status != AFORC_OK)
    {
        return status;
    }
    if (view->screen == FIELDZERO_SCREEN_TITLE)
    {
        status =
            fieldzero_draw_title(&canvas, game, presentation->no_color, arena);
    }
    else if (view->screen == FIELDZERO_SCREEN_COMPLETE ||
             game->phase == FIELDZERO_PHASE_COMPLETE)
    {
        status = fieldzero_draw_completion(
            &canvas, game, presentation->no_color, arena);
    }
    else if (view->screen == FIELDZERO_SCREEN_PLAY && game->room != NULL)
    {
        status =
            fieldzero_draw_hud(&canvas, game, presentation->no_color, arena);
    }
    else
    {
        return AFORC_ERROR_INVALID_ARGUMENT;
    }
    if (status != AFORC_OK)
    {
        return status;
    }
    return fieldzero_draw_state_overlay(
        &canvas, game, view, arena, presentation->no_color);
}
