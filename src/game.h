/* Fight engine state (docs/spec/02, 05). Two fighters, one stage.
 *
 * World: x in px (double), y = feet (double), groundY = 300, camera
 * subtracted when drawing. One tick = 28 ms. All numeric tables come from
 * tables_gen.c (generated from docs/spec by tools/gen_tables.py).
 */
#ifndef OSC_GAME_H
#define OSC_GAME_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "strip.h"
#include "wav.h"

#define OSC_SCREEN_W 400
#define OSC_SCREEN_H 300
#define OSC_TICK_MS 28
#define OSC_GROUND_Y 300
#define OSC_HEALTH_MAX 125.0
#define OSC_METER_MAX 102.0

/* DelphiX input state bits (TDXInputState order). */
enum { OSC_IN_UP = 1, OSC_IN_DOWN = 2, OSC_IN_LEFT = 4, OSC_IN_RIGHT = 8,
       OSC_IN_B1 = 16, OSC_IN_B2 = 32, OSC_IN_B3 = 64, OSC_IN_B4 = 128 };

typedef struct {
    /* players[p] (02-player-record.md) */
    double x, y;
    int char_id;
    int16_t rot, rot_step;
    int hurt_l, hurt_t, hurt_r, hurt_b;
    int hit_l, hit_t, hit_r, hit_b;
    double health;
    int stocks;
    double meter;
    bool attacking;
    bool knockdown;      /* +0x69 current attack knocks down */
    bool in_air;         /* +0x6a */
    int state;           /* +0x6b */
    int frame;           /* +0x6c */
    int facing;          /* +0x6d 1 right, 2 left */
    int jump_dir;        /* +0x6e 1 neutral 2 left 3 right */
    int hit_level;       /* +0x6f */
    int damage;          /* +0x70 */
    int chip;            /* +0x71 */
    int hitstun;         /* +0x72 */
    int anim_timer;      /* +0x74 */
    int anim_dur;        /* +0x76 */
    int vy;              /* +0x78 */
    int sprite_h;        /* +0x7a */
    double speed;        /* +0x80 */
    int score;
    bool invuln;         /* +0x92 */
    bool blocking;       /* +0x93 */
    int hits, combo_timer, max_combo;
    int seq_state, seq_timer, down_hold, back_hold, mash_p, mash_k;
    bool human;
    uint8_t in, in_prev; /* input bits this tick / last tick */
    int wins;
    const osc_strip *strip;
    int slot_base;       /* 0 for P1, 50 for P2 in the original; unused here */
} osc_fighter;

typedef struct {
    SDL_Renderer *ren;
    char data_dir[1024];
    SDL_Texture *stage;
    int stage_w, stage_h;
    osc_strip strips[2];
    osc_fighter p[3];    /* 1-based like the original */
    int cam_x, cam_y;
    unsigned tick;
    int hit_stop, super_pause;
    int timer_sec, timer_sub;
    bool paused, no_push, debug_boxes;
    uint32_t rng;        /* Delphi RandSeed */
    osc_wav music;
    bool has_music;
    int round_start, round_end;
} osc_game;

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir, int char1, int char2);
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_game_render(osc_game *g);
void osc_game_shutdown(osc_game *g);

#endif
