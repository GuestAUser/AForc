#ifndef FIELDZERO_CONTENT_H
#define FIELDZERO_CONTENT_H

#include "fieldzero/types.h"

const FieldzeroRoomDefinition *fieldzero_content_room(size_t room_index);
const char *fieldzero_sector_name(FieldzeroSector sector);
bool fieldzero_content_validate_all(void);

const FieldzeroRoomDefinition *fieldzero_origin_rooms(size_t *out_count);
const FieldzeroRoomDefinition *fieldzero_span_rooms(size_t *out_count);
const FieldzeroRoomDefinition *fieldzero_well_rooms(size_t *out_count);
const FieldzeroRoomDefinition *fieldzero_shear_rooms(size_t *out_count);
const FieldzeroRoomDefinition *fieldzero_horizon_rooms(size_t *out_count);

#endif
