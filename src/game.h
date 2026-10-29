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
    double health_shown; /* +0x48 the red bar that trails the health */
    int trail_delay;     /* +0x50 ticks before the red bar starts to fall (20 per hit) */
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
    int msg_index, msg_timer; /* +0x8f/+0x90 MemoTexto line shown over the player ("Primeiro Ataque") */
    int quote;           /* +0x8d winner's line 1..3 from MemoFrases */
    bool invuln;         /* +0x92 */
    bool blocking;       /* +0x93 */
    int hits, combo_timer, max_combo;
    int combo_jitter;    /* Random(10) shake of the hit counter, drawn by the HUD */
    int seq_state, seq_timer, down_hold, back_hold, mash_p, mash_k;
    int ai_pref;         /* +0x1c preferred distance */
    bool human;
    bool on_top;         /* +0x91 drawn over the opponent (the last one who hit) */
    uint8_t in, in_prev; /* input bits this tick / last tick */
    int wins;
    int strip_slot;      /* 1-based index into osc_assets.strips (the fighter holds no texture), 0 = none */
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
    bool has_hitbox, done, persist, bottom_anchor, gravity;
    bool knock;          /* +0x29 the hit knocks down */
    bool reflect;        /* +0x32 turns hostile effects it touches into its owner's */
    int chip;            /* +0x23 damage on guard */
    int loop_at;
} osc_effect;

/* what desenhaefeitos drew for a slot in its last pass (see fx_snapshot): kind 0 nothing, 1 sprite, 2 popup text */
typedef struct {
    uint8_t kind;
    int sprite, sx, sy, dir;   /* sx, sy: Round(x) - camX, Round(y) - camY at the moment of drawing */
    bool bottom, box;
    int bx, by, bw, bh;        /* debug box */
} osc_fx_draw;

typedef struct { char name[4]; int char_id; int score; } osc_record;   /* g_records (10 x 12 bytes) */

/* Everything loaded from the game folder (textures, sounds): owned by assets.c. */
typedef struct {
    SDL_Texture *stage;
    int stage_w, stage_h;
    osc_strip strips[2];
    SDL_Texture *fx_tex[OSC_MAX_FX_SPRITES];
    int fx_w[OSC_MAX_FX_SPRITES], fx_h[OSC_MAX_FX_SPRITES], n_fx_tex;
    SDL_Texture *anim_tex[OSC_MAX_FX_SPRITES];   /* Animacoes/Anim-NN.bmp (stage animations) */
    int anim_w[OSC_MAX_FX_SPRITES], anim_h[OSC_MAX_FX_SPRITES], n_anim_tex;
    osc_wav music;
    bool has_music;
    osc_wav sounds[80];   /* DXWaveListSnd[k] = Sons/Som-(k+1).wav */
    int n_sounds;
    osc_wav voices[16][2]; /* DXWaveListVitoria[2(N-1) + {a, b}] = Falas/Fala-NN-a/b.wav */
    SDL_Texture *face_small[16], *face_big[16];
    SDL_Texture *face_hurt[16];  /* Rxx-b: the HUD face while the red bar falls */
    int face_big_w[16], face_big_h[16];
    SDL_Texture *abertura[16]; int abertura_w[16], abertura_h[16], n_abertura;   /* DXImageListAbertura (from the exe) */
    SDL_Texture *icone[14];    int icone_w[14], icone_h[14], n_icone;            /* DXImageListIcone */
    SDL_Texture *final_tex[11]; int final_w[11], final_h[11], n_final;          /* DXImageListFinal */
} osc_assets;

/* The *.dat marker files next to Chaves.exe (unlocks.c). */
typedef struct {
    bool churros;     /* Churros.dat: the VS ladder shows every face */
    bool gentalha;    /* Gentalha.dat: a new game starts at fight 8 */
    bool energeticos; /* Energeticos.dat (0x4b5898): +0.15 health per tick while both are alive */
    bool satanas;     /* Satanas.dat (0x4b58ad): blood on every hit */
    bool fadiga;      /* Fadiga.dat (0x4b58ae): the VS screen jumps to the ending */
    bool mentecapto;  /* Mentecapto.dat (0x4b58af): the HUD stays off screen */
    bool bruxa;       /* Bruxa.dat (0x4b58b0): no "one projectile at a time" rule */
    int sc;           /* SC01..SC12.dat (0x4b58b1): the highest NN found forces that stage, 0 = none */
} osc_unlocks;

/* Options chosen by the player or on the command line / read from Chaves.dat. */
typedef struct {
    int lang;                /* 0 Portuguese, 1 Spanish */
    bool ai_p2;              /* P2 is the CPU (set by main before osc_game_init) */
    int rounds_to_win;
    int level;                /* g_cfg0 "Nível do Jogo" (display only) */
    /* F1 settings (01 "Config file") and F2 key map */
    int cfg_time_mode;        /* 1..4 = 30/60/90/99 s */
    int cfg_power;            /* 1 normal, 2 P1 max, 3 P2 max, 4 both */
    double damage_level;      /* 0.3..2.0 */
    bool cfg_blood, cfg_flash, show_level, show_credits;
    SDL_Scancode keymap[2][8];
    osc_unlocks unlocks;
} osc_settings;

/* Screen flow of the arcade cabinet: which screen is up, who joined, menus, records, plus the host's
 * debug / presentation toggles. Nothing here is needed to simulate a fight. */
typedef struct {
    /* screens (07) */
    int screen;              /* osc_screen */
    int screen_tick;
    int sel[3], sel_locked[3], sel_sub;
    int ladder[9];           /* ladder[1..7] opponents, ladder[8] rival */
    int match_over;          /* 0 running, 1 P1 won, 2 P2 won */
    bool p1_in, p2_in;       /* humans who joined */

    /* coin-op flow (01, 07): attract, demo fight, records */
    int credits;
    int demo;                 /* g_demo countdown while the attract demo fight runs */
    int menu_cam;             /* g_camY as the attract / records scroll offset */
    bool key_coin, key_start1, key_start2;   /* '5' '1' '2' edge events from main.c, consumed each tick */
    osc_record records[10];
    /* name entry (state 12) */
    char name[3][4];          /* players' 3-letter names, 'AAA' on entry */
    int name_pos[3];          /* 1..3, 4 = done */
    bool name_active[3];
    int name_after;           /* screen after the names: continue, records (ending) */
    int snd_test, mus_test, menu_cursor;
    bool key_f1, key_f2;
    int ending_char;          /* state 10: whose ending */
    /* host toggles (not part of the original's fight state) */
    bool debug_boxes;
    bool show_moves;         /* g_showMoves: G drew the move list */
    int speed_delay;         /* g_speedDelay = DXTimer1.Interval in ms, 0..80 (- / = keys) */
    int speed_overlay;       /* g_speedOverlayTimer: ticks left of the VELOCIDADE bar */
    bool speed_overlay_draw; /* the bar was drawn in the last tick */
    int view_dx;             /* DXDraw1.Left, -40..40 (, / . keys) */
} osc_flow;

/* One fight and nothing else: the 28 ms tick simulation (fight.c, fighter.c, combat.c, cpu.c, effects.c, stage.c) and
 * the state the HUD / fight_draw.c read back. Holds no SDL type; textures and sounds live in osc_assets. */
typedef struct {
    int stage_id;
    int ciclo;           /* g_ciclo blink counter, 0..30 in the fight */
    osc_effect fx[OSC_MAX_FX];
    osc_fx_draw fx_draw[OSC_MAX_FX];
    osc_fighter p[3];    /* 1-based like the original */
    int cam_x, cam_y;
    unsigned tick;
    int hit_stop, super_pause;
    int timer_sec, timer_sub;
    bool paused, no_push;
    bool timer_off;          /* DXTimer1.Enabled = false: set after the tick that ends with g_paused == 1 */
    uint32_t rng;        /* Delphi RandSeed */
    int round_start, round_end;
    int round_no, fight_no;  /* fight_no 1..8 drives the AI probabilities (05) */
    bool first_hit;          /* 0x4b57f4: the round's first attack was landed */
    int hud_y, hud_y2;       /* 0x4b44d2 / 0x4b44d4: top and bottom HUD offsets (slide out at round end) */
    int transition;          /* 0x4b449d: 1..4, wipe between rounds */
    int challenge;            /* g_challengeTimer: a second player joined a 1P fight */
    /* stage objects (05 "Stage objects") */
    struct { int x, y, speed, kind, freq; bool active; } mover;   /* 0x4b57c8: crosses stages 1 and 2 */
    struct { int x, y, r0, b0, r1, b1, icon; } crate;              /* 0x4b57d8: breakable thing on stage 2 */
    int shake;                /* 0x4b449a: camera shake ticks */
    int flash;                /* 0x4b449b: red flash ticks */
    bool flash_on;            /* this frame shows the flash */
    int snd_pending;          /* g_sndIndexA: the last sound asked for this tick wins */
    int dialogo;              /* g_dialogo: the last fight's dialogue, 600 ticks */
    int dialog_speaker, dialog_line;   /* 0x4b4494 and the memo line on screen */
    bool sp_draw; int sp_off; /* super pause: pattern instead of the stage, scrolled by sp_off */
} osc_fight;

typedef struct {
    SDL_Renderer *ren;
    char data_dir[1024];
    osc_assets assets;
    osc_settings settings;
    osc_flow flow;
    osc_fight fight;
} osc_game;

enum { OSC_SCREEN_ATTRACT = 1, OSC_SCREEN_MENU_F1 = 2, OSC_SCREEN_MENU_F2 = 3, OSC_SCREEN_SELECT = 4, OSC_SCREEN_VS = 5, OSC_SCREEN_FIGHT = 7,
       OSC_SCREEN_RECORDS = 8, OSC_SCREEN_CONTINUE = 9, OSC_SCREEN_ENDING = 10, OSC_SCREEN_CREDITS = 11,
       OSC_SCREEN_NAME = 12 };

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir);
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_game_render(osc_game *g);
void osc_game_shutdown(osc_game *g);
void osc_movelist_render(osc_game *g);   /* G: mostragolpe */
void osc_game_key(osc_game *g, SDL_Keycode key);   /* FormKeyDown: - = , . P G */
void osc_go_attract(osc_game *g, int tick);
/* screen_endings.c */
void osc_ending_start(osc_game *g, int ch);
void osc_ending_seek(osc_game *g, int tick);
void osc_ending_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_ending_render(osc_game *g);
/* screen_menu.c */
void osc_menu_enter(osc_game *g, int which);   /* 2 = F1, 3 = F2 */
void osc_menu_tick(osc_game *g, uint8_t in1, uint8_t in2);
void osc_menu_key(osc_game *g, SDL_Scancode k);   /* FormKeyDown in state 3: assigns the key */
void osc_menu_render(osc_game *g);

#endif
