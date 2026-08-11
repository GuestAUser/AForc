#ifndef FIELDZERO_GAME_H
#define FIELDZERO_GAME_H

#include "aforc/world.h"
#include "fieldzero/content.h"

typedef struct FieldzeroGame
{
    AFORC_TileMap *static_map;
    AFORC_TileMap *active_map;
    AFORC_TileMap *staging_map;
    const FieldzeroRoomDefinition *room;
    FieldzeroPlayer player;
    FieldzeroCheckpoint checkpoint;
    FieldzeroActions actions;
    FieldzeroGamePhase phase;
    uint64_t seed;
    uint32_t falls;
    uint16_t completed_rooms;
    uint16_t collected_memories;
    uint8_t room_states[FIELDZERO_ROOM_COUNT];
    uint8_t room_index;
    uint8_t room_state;
    uint8_t registration_target_state;
    uint8_t phase_ticks;
    int8_t transition_direction;
    bool memory_collected_here;
} FieldzeroGame;

AFORC_Status fieldzero_game_init(FieldzeroGame *game, uint64_t seed);
void fieldzero_game_dispose(FieldzeroGame *game);
AFORC_Status fieldzero_game_restart_run(FieldzeroGame *game);
AFORC_Status fieldzero_game_enter_room(FieldzeroGame *game,
                                       size_t room_index,
                                       bool from_left);
AFORC_Status fieldzero_game_tick(FieldzeroGame *game);
AFORC_Status fieldzero_game_tick_progression(FieldzeroGame *game);
AFORC_Status fieldzero_game_restart_room(FieldzeroGame *game);
void fieldzero_game_clear_actions(FieldzeroGame *game);

void fieldzero_game_set_move(FieldzeroGame *game, int direction, bool held);
void fieldzero_game_press_jump(FieldzeroGame *game);
void fieldzero_game_release_jump(FieldzeroGame *game);
void fieldzero_game_press_dash(FieldzeroGame *game);

bool fieldzero_game_cell_blocked(const FieldzeroGame *game,
                                 int32_t x,
                                 int32_t y);
AFORC_Status fieldzero_game_rebuild_maps(FieldzeroGame *game);
AFORC_Status fieldzero_game_begin_registration(FieldzeroGame *game);
AFORC_Status fieldzero_game_tick_registration(FieldzeroGame *game);
AFORC_Status fieldzero_game_restore_checkpoint(FieldzeroGame *game);
uint64_t fieldzero_game_state_digest(const FieldzeroGame *game);
uint64_t fieldzero_game_collision_digest(const FieldzeroGame *game);

#endif
