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
    int ai_pref;         /* +0x1c preferred distance */
    bool human;
    uint8_t in, in_prev; /* input bits this tick / last tick */
    int wins;
    const osc_strip *strip;
    int slot_base;       /* 0 for P1, 50 for P2 in the original; unused here */
} osc_fighter;

#define OSC_MAX_FX 80
#define OSC_MAX_FX_SPRITES 128

/* one slot of the effects table (06) */
typedef struct {
    int type;            /* 0 = free */
    double x, y;
    int dir;             /* 1 right, 2 left */
    int speed, damage, owner;
    int phase, life, sprite;
    int c4, c5;
    int vy;              /* gravity-driven effects */
    int hb_l, hb_t, hb_r, hb_b;
    bool has_hitbox, done, persist, top_anchor, gravity;
    int loop_at;
} osc_effect;

typedef struct { char name[4]; int char_id; int score; } osc_record;   /* g_records (10 x 12 bytes) */

typedef struct {
    SDL_Renderer *ren;
    char data_dir[1024];
    SDL_Texture *stage;
    int stage_w, stage_h;
    osc_strip strips[2];
    SDL_Texture *fx_tex[OSC_MAX_FX_SPRITES];
    int fx_w[OSC_MAX_FX_SPRITES], fx_h[OSC_MAX_FX_SPRITES], n_fx_tex;
    SDL_Texture *anim_tex[OSC_MAX_FX_SPRITES];   /* Animacoes/Anim-NN.bmp (stage animations) */
    int anim_w[OSC_MAX_FX_SPRITES], anim_h[OSC_MAX_FX_SPRITES], n_anim_tex;
    int stage_id;
    int ciclo;           /* g_ciclo blink counter, 0..30 in the fight */
    osc_effect fx[OSC_MAX_FX];
    osc_fighter p[3];    /* 1-based like the original */
    int cam_x, cam_y;
    unsigned tick;
    int hit_stop, super_pause;
    int timer_sec, timer_sub;
    bool paused, no_push, debug_boxes;
    uint32_t rng;        /* Delphi RandSeed */
    osc_wav music;
    bool has_music;
    osc_wav sounds[80];   /* DXWaveListSnd[k] = Sons/Som-(k+1).wav */
    int n_sounds;
    int round_start, round_end;
    int round_no, fight_no;  /* fight_no 1..8 drives the AI probabilities (05) */
    int rounds_to_win;
    bool ai_p2;
    int lang;                /* 0 Portuguese, 1 Spanish */

    /* screens (07) */
    int screen;              /* osc_screen */
    int screen_tick;
    int sel[3], sel_locked[3], sel_timer, sel_sub;
    int ladder[9];           /* ladder[1..7] opponents, ladder[8] rival */
    int match_over;          /* 0 running, 1 P1 won, 2 P2 won */
    SDL_Texture *face_small[16], *face_big[16];
    int face_big_w[16], face_big_h[16];
    bool p1_in, p2_in;       /* humans who joined */
    int first_hit_given;

    /* coin-op flow (01, 07): attract, demo fight, records */
    int credits;
    int demo;                 /* g_demo countdown while the attract demo fight runs */
    int menu_cam;             /* g_camY as the attract / records scroll offset */
    bool key_coin, key_start1, key_start2;   /* '5' '1' '2' edge events from main.c, consumed each tick */
    SDL_Texture *abertura[16]; int abertura_w[16], abertura_h[16], n_abertura;   /* DXImageListAbertura (from the exe) */
    SDL_Texture *icone[14];    int icone_w[14], icone_h[14], n_icone;            /* DXImageListIcone */
    osc_record records[10];
    /* name entry (state 12) */
    char name[3][4];          /* players' 3-letter names, 'AAA' on entry */
    int name_pos[3];          /* 1..3, 4 = done */
    bool name_active[3];
    int name_after;           /* screen after the names: continue, records (ending) */
    int level;                /* g_cfg0 "Nível do Jogo" (display only) */
    /* F1 settings (01 "Config file") and F2 key map */
    int cfg_time_mode;        /* 1..4 = 30/60/90/99 s */
    int cfg_power;            /* 1 normal, 2 P1 max, 3 P2 max, 4 both */
    double damage_level;      /* 0.3..2.0 */
    bool cfg_blood, cfg_flash, show_level, show_credits;
    int snd_test, mus_test, menu_cursor;
    SDL_Scancode keymap[2][8];
    SDL_Scancode last_key;    /* last key pressed (F2 menu assigns it) */
    bool key_f1, key_f2;
} osc_game;

enum { OSC_SCREEN_ATTRACT = 1, OSC_SCREEN_MENU_F1 = 2, OSC_SCREEN_MENU_F2 = 3, OSC_SCREEN_SELECT = 4, OSC_SCREEN_VS = 5, OSC_SCREEN_FIGHT = 7,
       OSC_SCREEN_RECORDS = 8, OSC_SCREEN_CONTINUE = 9, OSC_SCREEN_NAME = 12 };

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir);
/* Start a fight: characters 1..15, stage 1..12 (0 = by opponent), round 1. */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage);
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_game_render(osc_game *g);
void osc_game_shutdown(osc_game *g);
/* fight-screen internals used by flow.c */
void osc_fight_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_fight_render(osc_game *g);
void osc_text_at(osc_game *g, float x, float y, const char *s, bool centered);
/* TForm1.texto: align 'c' centred, 'e' right-aligned, else left; size in points of the original font. */
void osc_text_ex(osc_game *g, float x, float y, const char *s, char align, int size, Uint8 r, Uint8 gg, Uint8 b);
int osc_random(osc_game *g, int n);      /* Delphi Random(n) */
void osc_play_snd(osc_game *g, int idx);
void osc_go_attract(osc_game *g, int tick);
void osc_play_music(osc_game *g, int n);   /* Musicas/Musica-0n.wav */
/* menu.c */
void osc_config_load(osc_game *g);
void osc_config_save(osc_game *g);
int osc_round_time(const osc_game *g);
void osc_menu_enter(osc_game *g, int which);   /* 2 = F1, 3 = F2 */
void osc_menu_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_menu_render(osc_game *g);
void osc_make_ladder(osc_game *g, int char_id);
int osc_stage_for(osc_game *g, int opponent);

#endif
