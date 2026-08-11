#include "fieldzero/content.h"

#include <stdint.h>
#include <string.h>

typedef const FieldzeroRoomDefinition *(*FieldzeroRoomProvider)(size_t *);

static const FieldzeroRoomProvider fieldzero_room_providers[] = {
    fieldzero_origin_rooms,
    fieldzero_span_rooms,
    fieldzero_well_rooms,
    fieldzero_shear_rooms,
    fieldzero_horizon_rooms,
};

static const size_t fieldzero_sector_room_counts[] = {2U, 2U, 3U, 3U, 2U};

static bool fieldzero_text_valid(const char *text, bool allow_newline)
{
    bool saw_newline = false;
    bool line_has_text = false;

    if (text == NULL || text[0] == '\0')
    {
        return false;
    }
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor != '\0';
         ++cursor)
    {
        if (*cursor == (unsigned char)'\n')
        {
            if (!allow_newline || saw_newline || !line_has_text)
            {
                return false;
            }
            saw_newline = true;
            line_has_text = false;
        }
        else
        {
            if (*cursor < 32U || *cursor > 126U)
            {
                return false;
            }
            line_has_text = true;
        }
    }
    return line_has_text;
}

static bool fieldzero_point_in_arena(AFORC_Point point)
{
    return point.x >= 0 && point.x < FIELDZERO_ARENA_WIDTH && point.y >= 0 &&
           point.y < FIELDZERO_ARENA_HEIGHT;
}

static bool fieldzero_rect_in_arena(AFORC_Rect rectangle, AFORC_Point offset)
{
    const int64_t left = (int64_t)rectangle.x + offset.x;
    const int64_t top = (int64_t)rectangle.y + offset.y;
    const int64_t right = left + rectangle.width;
    const int64_t bottom = top + rectangle.height;

    return rectangle.width > 0 && rectangle.height > 0 && left >= 0 &&
           top >= 0 && right <= FIELDZERO_ARENA_WIDTH &&
           bottom <= FIELDZERO_ARENA_HEIGHT;
}

static bool fieldzero_rect_contains(AFORC_Rect rectangle,
                                    AFORC_Point offset,
                                    AFORC_Point point)
{
    const int64_t left = (int64_t)rectangle.x + offset.x;
    const int64_t top = (int64_t)rectangle.y + offset.y;

    return point.x >= left && point.y >= top &&
           (int64_t)point.x < left + rectangle.width &&
           (int64_t)point.y < top + rectangle.height;
}

static bool fieldzero_room_cell_blocked(const FieldzeroRoomDefinition *room,
                                        uint8_t state,
                                        AFORC_Point point)
{
    for (size_t index = 0U; index < room->static_rectangle_count; ++index)
    {
        if (fieldzero_rect_contains(
                room->static_rectangles[index], (AFORC_Point){0, 0}, point))
        {
            return true;
        }
    }
    for (size_t band_index = 0U; band_index < room->band_count; ++band_index)
    {
        const FieldzeroBand *band = &room->bands[band_index];

        for (size_t rectangle_index = 0U;
             rectangle_index < band->rectangle_count;
             ++rectangle_index)
        {
            if (fieldzero_rect_contains(band->rectangles[rectangle_index],
                                        band->offsets[state],
                                        point))
            {
                return true;
            }
        }
    }
    return false;
}

static bool fieldzero_room_static_support(const FieldzeroRoomDefinition *room,
                                          AFORC_Point point)
{
    const AFORC_Point support = {point.x, point.y + 1};

    if (!fieldzero_point_in_arena(support))
    {
        return false;
    }
    for (size_t index = 0U; index < room->static_rectangle_count; ++index)
    {
        if (fieldzero_rect_contains(
                room->static_rectangles[index], (AFORC_Point){0, 0}, support))
        {
            return true;
        }
    }
    return false;
}

static bool fieldzero_room_release_safe(const FieldzeroRoomDefinition *room,
                                        AFORC_Point point)
{
    if (!fieldzero_point_in_arena(point) ||
        !fieldzero_room_static_support(room, point))
    {
        return false;
    }
    for (uint8_t state = 0U; state < room->state_count; ++state)
    {
        if (fieldzero_room_cell_blocked(room, state, point))
        {
            return false;
        }
    }
    return true;
}

static bool fieldzero_band_valid(const FieldzeroBand *band, uint8_t state_count)
{
    bool moved_horizontally = false;
    bool moved_vertically = false;

    if (band->rectangle_count == 0U ||
        band->rectangle_count > FIELDZERO_MAX_BAND_RECTS ||
        (unsigned char)band->glyph < 32U || (unsigned char)band->glyph > 126U ||
        band->offsets[0].x != 0 || band->offsets[0].y != 0)
    {
        return false;
    }
    for (uint8_t state = 0U; state < state_count; ++state)
    {
        const AFORC_Point offset = band->offsets[state];

        if (offset.x % 4 != 0 || offset.y % 4 != 0)
        {
            return false;
        }
        moved_horizontally = moved_horizontally || offset.x != 0;
        moved_vertically = moved_vertically || offset.y != 0;
        for (size_t index = 0U; index < band->rectangle_count; ++index)
        {
            if (!fieldzero_rect_in_arena(band->rectangles[index], offset))
            {
                return false;
            }
        }
    }
    return moved_horizontally != moved_vertically;
}

static bool fieldzero_room_valid(const FieldzeroRoomDefinition *room,
                                 FieldzeroSector sector)
{
    int64_t reverse_x;
    int64_t reverse_y;
    AFORC_Point reverse_spawn;

    if (room == NULL || !fieldzero_text_valid(room->name, false) ||
        room->sector != sector || room->state_count < 2U ||
        room->state_count > FIELDZERO_MAX_ROOM_STATES ||
        room->mark_count != (size_t)room->state_count - 1U ||
        room->mark_count > FIELDZERO_MAX_MARKS ||
        room->static_rectangle_count == 0U ||
        room->static_rectangle_count > FIELDZERO_MAX_STATIC_RECTS ||
        room->band_count == 0U || room->band_count > FIELDZERO_MAX_BANDS ||
        (unsigned char)room->terrain_glyph < 32U ||
        (unsigned char)room->terrain_glyph > 126U ||
        (unsigned char)room->detail_glyph < 32U ||
        (unsigned char)room->detail_glyph > 126U ||
        room->has_memory != (room->memory_text != NULL) ||
        (room->has_memory && !fieldzero_text_valid(room->memory_text, true)))
    {
        return false;
    }
    for (size_t index = 0U; index < room->static_rectangle_count; ++index)
    {
        if (!fieldzero_rect_in_arena(room->static_rectangles[index],
                                     (AFORC_Point){0, 0}))
        {
            return false;
        }
    }
    for (size_t index = 0U; index < room->band_count; ++index)
    {
        if (!fieldzero_band_valid(&room->bands[index], room->state_count))
        {
            return false;
        }
    }
    if (!fieldzero_room_release_safe(room, room->entry) ||
        !fieldzero_room_release_safe(room, room->spawn) ||
        !fieldzero_room_release_safe(room, room->exit))
    {
        return false;
    }
    for (size_t index = 0U; index < room->mark_count; ++index)
    {
        if (!fieldzero_room_release_safe(room, room->marks[index]))
        {
            return false;
        }
    }
    if (room->has_memory && !fieldzero_room_release_safe(room, room->memory))
    {
        return false;
    }
    reverse_x = (int64_t)room->exit.x - room->spawn.x + room->entry.x;
    reverse_y = (int64_t)room->exit.y - room->spawn.y + room->entry.y;
    if (reverse_x < 0 || reverse_x >= FIELDZERO_ARENA_WIDTH || reverse_y < 0 ||
        reverse_y >= FIELDZERO_ARENA_HEIGHT)
    {
        return false;
    }
    reverse_spawn = (AFORC_Point){(int32_t)reverse_x, (int32_t)reverse_y};
    return fieldzero_room_release_safe(room, reverse_spawn);
}

static bool
fieldzero_validator_rejects_broken(const FieldzeroRoomDefinition *room)
{
    FieldzeroRoomDefinition broken = *room;

    broken.state_count = 0U;
    if (fieldzero_room_valid(&broken, room->sector))
    {
        return false;
    }
    broken = *room;
    broken.bands[0].offsets[1].x = 1;
    if (fieldzero_room_valid(&broken, room->sector))
    {
        return false;
    }
    broken = *room;
    broken.bands[0].offsets[1] = (AFORC_Point){4, 4};
    if (fieldzero_room_valid(&broken, room->sector))
    {
        return false;
    }
    broken = *room;
    broken.spawn = (AFORC_Point){-1, 0};
    if (fieldzero_room_valid(&broken, room->sector))
    {
        return false;
    }
    broken = *room;
    broken.has_memory = !broken.has_memory;
    return !fieldzero_room_valid(&broken, room->sector);
}

bool fieldzero_content_validate_all(void)
{
    const FieldzeroRoomDefinition *seen[FIELDZERO_ROOM_COUNT] = {0};
    size_t total_rooms = 0U;
    size_t total_memories = 0U;

    if (sizeof(fieldzero_room_providers) /
                sizeof(fieldzero_room_providers[0]) !=
            FIELDZERO_SECTOR_COUNT ||
        sizeof(fieldzero_sector_room_counts) /
                sizeof(fieldzero_sector_room_counts[0]) !=
            FIELDZERO_SECTOR_COUNT)
    {
        return false;
    }
    for (size_t sector = 0U; sector < FIELDZERO_SECTOR_COUNT; ++sector)
    {
        size_t count = 0U;
        const FieldzeroRoomDefinition *rooms =
            fieldzero_room_providers[sector](&count);

        if (rooms == NULL || count != fieldzero_sector_room_counts[sector] ||
            strcmp(fieldzero_sector_name((FieldzeroSector)sector), "UNKNOWN") ==
                0)
        {
            return false;
        }
        for (size_t room_index = 0U; room_index < count; ++room_index)
        {
            const FieldzeroRoomDefinition *room = &rooms[room_index];

            if (total_rooms >= FIELDZERO_ROOM_COUNT ||
                !fieldzero_room_valid(room, (FieldzeroSector)sector) ||
                fieldzero_content_room(total_rooms) != room)
            {
                return false;
            }
            for (size_t prior = 0U; prior < total_rooms; ++prior)
            {
                if (strcmp(seen[prior]->name, room->name) == 0)
                {
                    return false;
                }
            }
            seen[total_rooms] = room;
            ++total_rooms;
            total_memories += room->has_memory ? 1U : 0U;
        }
    }
    return total_rooms == FIELDZERO_ROOM_COUNT &&
           total_memories == FIELDZERO_MEMORY_COUNT &&
           fieldzero_content_room(FIELDZERO_ROOM_COUNT) == NULL &&
           fieldzero_validator_rejects_broken(seen[0]);
}
