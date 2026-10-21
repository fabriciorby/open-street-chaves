/* Game state for the current milestone: one stage, one fighter, idle/walk.
 *
 * Everything here follows docs/spec: 28 ms tick, 400x300 logical screen,
 * world coordinates with the feet on groundY = 300, camera subtracted when
 * drawing, frames and offsets from 05a/05c.
 */
#ifndef OSC_GAME_H
#define OSC_GAME_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "strip.h"
#include "wav.h"

#define OSC_SCREEN_W 400
#define OSC_SCREEN_H 300
#define OSC_TICK_MS 28          /* default g_speedDelay (DXTimer interval) */
#define OSC_GROUND_Y 300        /* g_groundY */

typedef struct {
    /* players[p] subset (02-player-record.md) */
    double x, y;
    int char_id;
    int state;      /* 1 idle, 2 walk (05-fight-engine.md) */
    int frame;      /* strip frame index, 1-based */
    int facing;     /* 1 right, 2 left */
    int anim_timer;
    int sprite_h;   /* +0x7a */
    double speed;   /* +0x80 */
} osc_fighter;

typedef struct {
    SDL_Renderer *ren;
    char data_dir[1024];
    SDL_Texture *stage;
    int stage_w, stage_h;
    osc_strip strip;
    osc_fighter p1;
    int cam_x, cam_y;
    unsigned tick;
    osc_wav music;
    bool has_music;
    bool paused;
} osc_game;

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir);
void osc_game_tick(osc_game *g, const bool *keys);
void osc_game_render(osc_game *g);
void osc_game_shutdown(osc_game *g);

#endif
