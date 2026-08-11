#include "fieldzero/content.h"

typedef const FieldzeroRoomDefinition *(*FieldzeroRoomProvider)(size_t *);

static const FieldzeroRoomProvider room_providers[FIELDZERO_SECTOR_COUNT] = {
    fieldzero_origin_rooms,
    fieldzero_span_rooms,
    fieldzero_well_rooms,
    fieldzero_shear_rooms,
    fieldzero_horizon_rooms,
};

const FieldzeroRoomDefinition *
fieldzero_content_sector_rooms(FieldzeroSector sector, size_t *out_count)
{
    if (sector < FIELDZERO_SECTOR_ORIGIN || sector > FIELDZERO_SECTOR_HORIZON)
    {
        if (out_count != NULL)
        {
            *out_count = 0U;
        }
        return NULL;
    }
    return room_providers[(size_t)sector](out_count);
}

const FieldzeroRoomDefinition *fieldzero_content_room(size_t room_index)
{
    if (room_index >= FIELDZERO_ROOM_COUNT)
    {
        return NULL;
    }
    for (size_t sector = 0U; sector < FIELDZERO_SECTOR_COUNT; ++sector)
    {
        size_t count = 0U;
        const FieldzeroRoomDefinition *rooms =
            fieldzero_content_sector_rooms((FieldzeroSector)sector, &count);

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
