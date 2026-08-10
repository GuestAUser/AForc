/*
 * AForc
 * Author: GuestAUser
 * SPDX-License-Identifier: MIT
 */

#ifndef AFORC_EXAMPLES_ROGUELIKE_INCLUDE_ROGUELIKE_INTERNAL_H
#define AFORC_EXAMPLES_ROGUELIKE_INCLUDE_ROGUELIKE_INTERNAL_H

#include "aforc/assets.h"
#include "aforc/ecs.h"
#include "aforc/effects.h"
#include "aforc/engine.h"
#include "aforc/input.h"
#include "aforc/renderer.h"
#include "aforc/terminal.h"
#include "aforc/ui.h"
#include "aforc/world.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    GAME_MAX_ROOMS = 24,
    GAME_MAX_ENEMIES = 48,
    GAME_PARTICLE_CAPACITY = 160,
    GAME_HUD_ROWS = 5,
    GAME_SAVE_LEGACY_SCHEMA = 1,
    GAME_SAVE_SCHEMA = 2,
    GAME_SAVE_MAX_BYTES = 16384,
    GAME_SAVE_MAX_FILE_BYTES =
        GAME_SAVE_MAX_BYTES + (int)AFORC_SAVE_HEADER_SIZE,
    GAME_MESSAGE_CAPACITY = 160,
    GAME_MIN_COLUMNS = 40,
    GAME_MIN_ROWS = 14
};

enum
{
    TILE_WALL = 0,
    TILE_FLOOR = 1,
    TILE_EXIT = 2
};

typedef enum GameRunState
{
    GAME_PLAYING = 0,
    GAME_DEFEATED,
    GAME_VICTORIOUS
} GameRunState;

typedef struct GameRules
{
    int32_t map_width;
    int32_t map_height;
    uint32_t room_count;
    uint32_t enemy_count;
    uint32_t fov_radius;
    int32_t player_health;
    int32_t player_attack;
    int32_t enemy_health;
    int32_t enemy_attack;
    uint32_t final_floor;
} GameRules;

typedef struct GameRoom
{
    AFORC_Rect bounds;
    AFORC_Point center;
} GameRoom;

typedef struct GamePosition
{
    AFORC_Point point;
} GamePosition;

typedef struct GameActor
{
    int32_t health;
    int32_t maximum_health;
    int32_t attack;
    uint32_t glyph;
    AFORC_Color color;
    bool hostile;
} GameActor;

typedef struct Game
{
    AFORC_Allocator allocator;
    AFORC_Terminal *terminal;
    AFORC_Renderer *renderer;
    AFORC_Input *input;
    AFORC_TileMap *map;
    AFORC_PathWorkspace *path_workspace;
    AFORC_Ecs *ecs;
    AFORC_EcsView *render_view;
    AFORC_ComponentType position_type;
    AFORC_ComponentType actor_type;
    AFORC_Entity player;
    AFORC_Camera camera;
    uint8_t *visibility;
    uint8_t *explored;
    size_t cell_count;
    AFORC_Point visibility_origin;
    bool visibility_valid;
    AFORC_Particle particles[GAME_PARTICLE_CAPACITY];
    AFORC_ParticlePool particle_pool;
    double particle_millisecond_remainder;
    AFORC_Tween exit_tween;
    double tween_millisecond_remainder;
    AFORC_Scene scene;
    GameRules rules;
    uint64_t seed;
    const char *save_path;
    uint32_t floor;
    uint32_t score;
    uint32_t turn;
    AFORC_Point exit_position;
    GameRunState run_state;
    bool help_visible;
    char message[GAME_MESSAGE_CAPACITY];
} Game;

extern const AFORC_SceneVTable game_scene_vtable;

AFORC_Status game_parse_seed(const char *text, uint64_t *out_seed);
AFORC_Status game_error(AFORC_Error *error,
                        AFORC_Status status,
                        const char *subsystem,
                        const char *message);
void game_set_message(Game *game, const char *format, ...);

bool game_tile_blocks(AFORC_Tile tile,
                      uint32_t layer,
                      AFORC_Point position,
                      void *context);
AFORC_Status game_create_actor(Game *game,
                               AFORC_Point position,
                               GameActor actor,
                               AFORC_Entity *out_entity);
AFORC_Status game_actor_components(Game *game,
                                   AFORC_Entity entity,
                                   GamePosition **out_position,
                                   GameActor **out_actor);
AFORC_Status game_entity_at(Game *game,
                            AFORC_Point point,
                            AFORC_Entity *out_entity,
                            bool *out_found);

AFORC_Status game_load_rules(GameRules *rules);
AFORC_Status game_populate_floor(Game *game,
                                 AFORC_Rng *rng,
                                 const GameRoom *rooms,
                                 size_t room_count,
                                 int32_t player_health);
AFORC_Status
game_generate_floor(Game *game, uint32_t floor, int32_t player_health);
AFORC_Status game_new_run(Game *game);

AFORC_Status game_enemy_turns(Game *game);
AFORC_Status game_move_player(Game *game, AFORC_Point delta);
AFORC_Status game_wait_turn(Game *game);
AFORC_Status game_descend(Game *game);
AFORC_Status game_emit_burst(Game *game, AFORC_Point point, bool strong);
AFORC_Status game_scene_event(AFORC_Scene *scene,
                              AFORC_Engine *engine,
                              const void *event_data,
                              bool *consumed,
                              AFORC_Error *error);

AFORC_Status game_encode_save(const Game *game, AFORC_AssetBlob *out_blob);
AFORC_Status game_decode_save(Game *game, const void *data, size_t size);
AFORC_Status game_save(Game *game);
AFORC_Status game_load(Game *game);

AFORC_Cell
game_cell(uint32_t codepoint, AFORC_Color foreground, AFORC_CellStyle style);
AFORC_Status
game_plot_cell(void *context, AFORC_Point position, AFORC_Cell cell);
AFORC_Status game_render_world(Game *game, AFORC_Size screen, int32_t map_rows);
AFORC_Status game_render_hud(Game *game, AFORC_Size screen, int32_t map_rows);
AFORC_Status game_scene_fixed_update(AFORC_Scene *scene,
                                     AFORC_Engine *engine,
                                     double seconds,
                                     AFORC_Error *error);
AFORC_Status game_scene_update(AFORC_Scene *scene,
                               AFORC_Engine *engine,
                               double seconds,
                               AFORC_Error *error);
AFORC_Status game_scene_render(AFORC_Scene *scene,
                               AFORC_Engine *engine,
                               double interpolation,
                               AFORC_Error *error);

AFORC_Status
game_dispatch_input_queue(Game *game, AFORC_Engine *engine, AFORC_Error *error);
AFORC_Status
game_poll_events(void *context, AFORC_Engine *engine, AFORC_Error *error);
AFORC_Status
game_begin_frame(void *context, AFORC_Engine *engine, AFORC_Error *error);
AFORC_Status
game_present(void *context, AFORC_Engine *engine, AFORC_Error *error);
AFORC_Status
game_smoke_checks(Game *game, AFORC_Engine *engine, AFORC_Error *error);
AFORC_Status
game_runtime_smoke_checks(Game *game, AFORC_Engine *engine, AFORC_Error *error);

void game_dispose(Game *game);
AFORC_Status game_initialize(Game *game,
                             AFORC_Renderer *renderer,
                             AFORC_Input *input,
                             AFORC_Terminal *terminal,
                             uint64_t seed);

int game_run_smoke(uint64_t seed);
int game_run_interactive(uint64_t seed);

#endif
