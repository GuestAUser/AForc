#include "fieldzero/content.h"

typedef const FieldzeroRoomDefinition *(*FieldzeroRoomProvider)(size_t *);

static const FieldzeroRoomProvider room_providers[FIELDZERO_SECTOR_COUNT] = {
    fieldzero_origin_rooms,
    fieldzero_span_rooms,
    fieldzero_well_rooms,
    fieldzero_shear_rooms,
    fieldzero_horizon_rooms,
};

const FieldzeroRoomDefinition *fieldzero_content_room(size_t room_index)
{
    if (room_index >= FIELDZERO_ROOM_COUNT)
    {
        return NULL;
    }
    for (size_t sector = 0U; sector < FIELDZERO_SECTOR_COUNT; ++sector)
    {
        size_t count = 0U;
        const FieldzeroRoomDefinition *rooms = room_providers[sector](&count);

        if (rooms == NULL)
        {
            return NULL;
        }
        if (room_index < count)
        {
            return &rooms[room_index];
        }
        room_index -= count;
    }
    return NULL;
}

const char *fieldzero_sector_name(FieldzeroSector sector)
{
    static const char *const names[FIELDZERO_SECTOR_COUNT] = {
        "ORIGIN",
        "SPAN",
        "WELL",
        "SHEAR",
        "HORIZON",
    };

    if (sector < FIELDZERO_SECTOR_ORIGIN || sector > FIELDZERO_SECTOR_HORIZON)
    {
        return "UNKNOWN";
    }
    return names[(size_t)sector];
}
