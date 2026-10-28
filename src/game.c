#include "game.h"
#include "assets.h"
#include "canvas.h"
#include "config.h"
#include "arcade.h"
#include "unlocks.h"
#include "fight_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exetext.h"
#include "exeimg.h"
#include "font.h"
#include "tables_gen.h"

/* ---- per-character constants (02, 03) ------------------------------------ */
const int k_sprite_h[16] = {0, 156, 162, 147, 149, 144, 190, 160, 160, 150, 155, 155, 165, 155, 160, 165};
const double k_speed[16] = {0, 8, 8, 8.5, 8, 8, 7.5, 7.5, 8, 8.5, 7.5, 8, 8, 7.5, 7.5, 8};
const double k_meter_unit = 2.0;
const int k_gravity = 4;

/* ---- Delphi System.Random -------------------------------------------------- */
int osc_random(osc_game *g, int n)
{
    g->rng = g->rng * 0x8088405u + 1u;
    if (n <= 0) return 0;
    return (int)(((uint64_t)(uint32_t)n * (uint64_t)g->rng) >> 32);
}

static void play_snd(osc_game *g, int idx)
{
    if (idx >= 0 && idx < g->n_sounds && g->sounds[idx].buf) osc_audio_play(&g->sounds[idx]);
}

/* ---- table lookups ---------------------------------------------------------- */
static const osc_move_row *find_move(int ch, int move, int state)
{
    /* character-specific rows win over the common ones */
    for (int pass = 0; pass < 2; pass++) {
        int want = pass == 0 ? ch : 0;
        for (int i = 0; i < OSC_N_MOVES; i++) {
            const osc_move_row *r = &osc_moves[i];
            if (r->ch != want || r->move != move) continue;
            if (r->from_mask == 0 || (r->from_mask & (1u << state))) return r;
        }
    }
    return NULL;
}


/* ---- setMove (0x4775fc) ------------------------------------------------------- */
void set_move(osc_game *g, osc_fighter *f, int move)
{
    f->blocking = false;   /* every setMove drops the guard; states 21/22 raise it again */
    /* Moves with hard-coded behaviour in the original (knockdown, lie down). */
    if (move == 10) {
        f->state = 19; f->anim_timer = 0; f->anim_dur = 50; f->vy = -19;
        f->in_air = true; f->jump_dir = 0; f->invuln = true; f->attacking = false;
        return;
    }
    if (move == 11) {
        f->state = 20; f->anim_timer = 0; f->anim_dur = 25; f->vy = -13;
        f->in_air = true; f->invuln = true;
        return;
    }
    if (move == 12 || move == 13) {
        bool ok = f->state == 1 || f->state == 2 || f->state == 7 || f->state == 21 || f->state == 22 ||
                  (f->state == 20 && f->anim_dur - 3 <= f->anim_timer);
        if (!ok) return;
        f->state = move == 12 ? 21 : 22; f->invuln = false; f->anim_timer = 0; f->anim_dur = 11;
        f->blocking = true; f->frame = move == 12 ? 31 : 32;
        return;
    }
    const osc_move_row *r = find_move(f->char_id, move, f->state);
    if (!r) return;
    if (r->needs_fx && !g->unlock_bruxa) {        /* 0x476454: one projectile of this type per owner */
        int me = (f == &g->p[1]) ? 1 : 2;
        for (int i = 0; i < OSC_MAX_FX; i++)
            if (g->fx[i].type == r->needs_fx && g->fx[i].owner == me) return;
    }
    if ((r->flags & OSC_MOVE_ONGROUND) && f->vy != 0) return;
    if ((r->flags & OSC_MOVE_NOT_INAIR) && f->in_air) return;
    if (r->flags & OSC_MOVE_SUPER) {
        if (!(f->health <= 15.0 || f->stocks > 0)) return;
        if (f->stocks == 3) f->meter = 0;
        f->stocks--;
        g->super_pause = 22;
    }
    if (r->flags & OSC_MOVE_WALK) {
        /* moves 1/2 (0x477634): step by the walk speed, keep the animation clock */
        if (f->state != 1 && f->state != 2) return;
        move_x(g, f, move == 1 ? -f->speed : f->speed);
        f->state = 2; f->anim_dur = 99;
        return;
    }
    f->state = r->state;
    f->anim_timer = 0;
    f->anim_dur = r->dur;
    if (r->meter10 && f->meter < OSC_METER_MAX) f->meter += k_meter_unit * r->meter10 / 10.0;
    if (r->strength) f->hit_level = r->strength;
    if (move == 8 || move == 9) f->attacking = false;
    if (r->snd >= 0) g->snd_pending = r->snd;   /* g_sndIndexA: played at the start of the next tick */
    if (f->char_id == 9 && (move == 20 || move == 21))   /* 0x47b0bb: five dust puffs */
        for (int k = 0; k < 5; k++) {
            double x = f->x - 20 + osc_random(g, 40);
            spawn_fx(g, 2, 0, 0, x, f->y + osc_random(g, 10));
        }
}


/* ---- walking with body push (sub_47648c) ---------------------------------- */
void move_x(osc_game *g, osc_fighter *f, double dx)
{
    osc_fighter *o = opp(g, f);
    f->x += dx;
    bool overlap = f->hurt_l + f->x < o->hurt_r + o->x && o->hurt_l + o->x < f->hurt_r + f->x &&
                   f->hurt_t + (f->y - f->sprite_h) < o->hurt_b + (o->y - o->sprite_h) &&
                   o->hurt_t + (o->y - o->sprite_h) < f->hurt_b + (f->y - f->sprite_h);
    if (overlap && f->state != 23 && f->state != 20 && !g->no_push) {
        if (f->x <= o->x) {
            o->x += 5.0;
            if (o->x > g->cam_x + 370) o->x = g->cam_x + 370;
            f->x = (o->hurt_l + o->x) - f->hurt_r - 5.0;
        } else {
            o->x -= 5.0;
            if (o->x < g->cam_x + 30) o->x = g->cam_x + 30;
            f->x = (o->hurt_r + o->x) - f->hurt_l + 5.0;
        }
    }
    if (f->health > 0.0) {
        if (f->x < g->cam_x + 30) f->x = g->cam_x + 30;
        if (f->x > g->cam_x + 370) f->x = g->cam_x + 370;
    }
}

/* ---- input-side counters (sequences, mash, combo) ----------------------------- */
static void counters(osc_game *g, osc_fighter *f)
{
    bool paused = g->hit_stop || g->super_pause;
    if (f->state == 2 && !paused) { f->anim_timer++; if (f->anim_timer > 16) f->anim_timer = 0; }
    if (f->seq_timer > 0 && !paused) { f->seq_timer--; if (f->seq_timer == 0) f->seq_state = 0; }
    if (f->mash_p) f->mash_p--; if (f->mash_p > 15) f->mash_p = 15;
    if (f->mash_k) f->mash_k--; if (f->mash_k > 15) f->mash_k = 15;
    if (f->combo_timer && !paused) { f->combo_timer--; if (!f->combo_timer) f->hits = 0; }
}

/* ---- move clock (0x4a9459): a finished move falls back to standing or falling ---- */
static void move_clock(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    if (f->anim_dur != 99 && !g->super_pause && !g->hit_stop) f->anim_timer++;
    if (f->anim_timer < f->anim_dur) return;
    f->state = 1; f->attacking = false; f->damage = 0; f->anim_timer = 0;
    f->invuln = false; f->blocking = false; f->knockdown = false; f->hit_level = 0;
    if (f->y < OSC_GROUND_Y) { f->in_air = true; f->state = 12; }
    if (f->x < o->x) f->facing = 1;
    if (f->x >= o->x) f->facing = 2;
    if (f->y > OSC_GROUND_Y) f->y = OSC_GROUND_Y;
    if (f->vy != 0) f->state = 12;
}

/* five puffs of dust at the feet */
static void dust(osc_game *g, osc_fighter *f, int n, int spread)
{
    for (int k = 0; k < n; k++) {
        int rx = osc_random(g, 2 * spread), ry = osc_random(g, 10);
        spawn_fx(g, 2, 0, 0, f->x - spread + rx, f->y + ry);
    }
}

/* ---- jumps, knock-ups and landings (0x4a954c..0x4a99d8) ----------------------- */
static void physics(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    if (f->in_air && !g->super_pause && !g->hit_stop) {
        f->y += f->vy;
        f->vy += k_gravity;
        if (f->jump_dir == 2) move_x(g, f, -8.0);
        if (f->jump_dir == 3) move_x(g, f, 8.0);
        if (f->state == 19) {                       /* knocked into the air */
            if (f->facing == 1) f->x -= f->hitstun;
            if (f->facing == 2) f->x += f->hitstun;
            if (f->y >= OSC_GROUND_Y) {
                set_move(g, f, 11);
                f->y = OSC_GROUND_Y - 10;
                f->rot = 0; f->rot_step = 0;
                g->snd_pending = 12;
                if (f->char_id == 7 || f->char_id == 10) {   /* the heavy ones shake the street */
                    g->shake = 20;
                    dust(g, f, 20, 30);
                }
                dust(g, f, 5, 20);
            }
        }
        if (f->state == 20) {                       /* bouncing on the ground */
            if (f->facing == 1) move_x(g, f, -f->hitstun);
            if (f->facing == 2) move_x(g, f, f->hitstun);
            if ((g->cfg_blood && osc_random(g, 4) == 0 && f->anim_timer == 5) || g->unlock_satanas)
                spawn_fx(g, 10, 0, 0, f->facing == 1 ? f->x - 30 : f->x + 30, f->y - 50);
            if (f->y > OSC_GROUND_Y) {
                f->y = OSC_GROUND_Y; f->in_air = false; f->rot = 0; f->rot_step = 0; f->vy = 0; f->hitstun = 0;
            }
        }
        if (f->y > OSC_GROUND_Y) {                  /* landing */
            f->vy = 0; f->in_air = false; f->y = OSC_GROUND_Y; f->rot = 0; f->rot_step = 0;
            f->state = 1; f->anim_timer = 0; f->attacking = false; f->damage = 0; f->knockdown = false;
            f->jump_dir = 0;
            if (f->x < o->x) f->facing = 1;
            if (f->x >= o->x) f->facing = 2;
            dust(g, f, 5, 20);
        }
    }
    if (f->health > 0.0) {
        if (f->x < g->cam_x + 30) f->x = g->cam_x + 30;
        if (f->x > g->cam_x + 370) f->x = g->cam_x + 370;
    }
}

/* ---- stage objects (0x4a618e..0x4a6622) ------------------------------------------- */
static void stage_objects(osc_game *g)
{
    bool paused = g->hit_stop || g->super_pause;
    /* something crossing the street on stages 1 and 2 */
    if ((g->stage_id == 1 || (g->stage_id == 2 && !g->no_push)) && !g->mover.active &&
        g->cam_x > 40 && g->cam_x < 160 && osc_random(g, g->mover.freq) == 0) {
        g->mover.x = g->cam_x - 80; g->mover.y = 146; g->mover.speed = 7;
        g->mover.kind = osc_random(g, 2) + 1;
        g->mover.active = true;
    }
    if (g->mover.active) {
        if (!paused) g->mover.x += g->mover.speed;
        if (g->mover.x > g->cam_x + 420) g->mover.active = false;
        /* ... and dropping something on the way */
        if (g->mover.kind != 0 && g->mover.x > g->cam_x + 150 && g->mover.x < g->cam_x + 250 &&
            osc_random(g, 5) == 0 && !g->super_pause && !g->hit_stop && !g->no_push) {
            spawn_fx(g, g->mover.kind == 1 ? 3 : 4, 0, 0, g->mover.x + 13, g->mover.y - 5);
            g->mover.kind = 0;
        }
    }
    /* stage 2: a fighter knocked into the breakable thing smashes it */
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        int l = iround(f->x) + f->hurt_l, t = iround(f->y) - f->sprite_h + f->hurt_t;
        int r = iround(f->x) + f->hurt_r, b = iround(f->y) - f->sprite_h + f->hurt_b;
        if (f->state != 19) continue;
        if (g->crate.x + g->crate.r1 >= l && g->crate.y + g->crate.b1 >= t &&
            g->crate.x + g->crate.r0 <= r && g->crate.y + g->crate.b0 <= b && g->crate.icon) {
            g->crate.icon = 0;
            g->snd_pending = 9;
            for (int k = 0; k < 30; k++) {
                int rx = osc_random(g, 40), ry = osc_random(g, 20);
                spawn_fx(g, 21, 0, 0, g->crate.x + 20 + rx, g->crate.y + 20 + ry);
            }
        }
    }
}

/* ---- desenhalutador's logic part (0x490bb8): facing, frame, spin ---------------- */
static void fighter_frame(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    bool paused = g->hit_stop || g->super_pause;
    if ((f->state == 1 || f->state == 2 || f->state == 7) && f->vy == 0) {
        if (f->x < o->x) f->facing = 1;
        if (f->x >= o->x) f->facing = 2;
    }
    if (!paused) {
        select_frame(g, f);
        f->rot += f->rot_step;
    }
    if (f->state != 12 && f->state != 19) { f->rot = 0; f->rot_step = 0; }
}

void osc_reset_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip);
static void init_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip) { osc_reset_fighter(f, ch, p, strip); }

/* zerardados: per-round reset of one fighter. */
void osc_reset_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip)
{
    memset(f, 0, sizeof *f);
    f->char_id = ch;
    f->x = p == 1 ? 220.0 : 380.0;
    f->y = OSC_GROUND_Y;
    f->health = f->health_shown = OSC_HEALTH_MAX;
    f->state = 1; f->frame = 1; f->anim_dur = 99;
    f->facing = p == 1 ? 1 : 2;
    f->sprite_h = k_sprite_h[ch];
    f->speed = k_speed[ch];
    f->strip = strip;
    f->human = true;
    f->hurt_l = -20; f->hurt_r = 20; f->hurt_t = 10; f->hurt_b = f->sprite_h; /* fallback box */
}

/* FormCreate: the common assets, the default records, the settings and the unlock files. */
bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir)
{
    bool ai_p2 = g->ai_p2;
    int lang = g->lang;
    memset(g, 0, sizeof *g);
    g->ai_p2 = ai_p2;
    g->lang = lang;
    g->speed_delay = OSC_TICK_MS;   /* FormCreate: g_speedDelay = 28 */
    g->ren = ren;
    g->rng = (uint32_t)SDL_GetTicksNS();
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);
    if (!osc_assets_load(g)) return false;   /* no data folder */
    /* default high-score table (FormCreate: criarecorde 1..10) */
    {
        static const char *names[10] = {"CHV", "SMD", "CHQ", "QUI", "DFL", "PGR", "SBR", "B71", "PTY", "NHO"};
        for (int i = 0; i < 10; i++) {
            memcpy(g->records[i].name, names[i], 4);
            g->records[i].char_id = i + 1;
            g->records[i].score = i == 0 ? 10000 : 8500 - 500 * i;
        }
    }
    osc_font_init(ren);
    osc_config_load(g);
    g->screen = OSC_SCREEN_ATTRACT;
    g->screen_tick = 0;
    g->sel[1] = 1; g->sel[2] = 2;
    osc_unlocks_load(g);
    g->p1_in = false;   /* nobody has joined until a start button */
    g->p2_in = false;
    return true;
}

/* Round/fight init (state 6 -> 7). */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage)
{
    char rel[64];
    osc_image img;
    char path[1200];
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    memset(g->fx, 0, sizeof g->fx);
    memset(g->fx_draw, 0, sizeof g->fx_draw);

    int chars[2] = {char1, char2};
    for (int i = 0; i < 2; i++) {
        snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", chars[i]);
        load_path(path, sizeof path, g->data_dir, rel);
        if (!osc_bmp_load(path, true, &img)) return false;
        bool ok = osc_strip_build(g->ren, &img, &g->strips[i]);
        osc_image_free(&img);
        if (!ok) return false;
        bool human = g->p[i + 1].human, had = g->p[i + 1].char_id != 0;
        int wins = g->p[i + 1].wins, score = g->p[i + 1].score;
        init_fighter(&g->p[i + 1], chars[i], i + 1, &g->strips[i]);
        g->p[i + 1].human = had ? human : (i == 0 || !g->ai_p2);
        g->p[i + 1].wins = wins; g->p[i + 1].score = score;
    }
    if (stage < 1 || stage > 12) {
        if (g->p[1].human && g->p[2].human) {                       /* 2P: Round(Random(120)/10)+1 */
            stage = iround(osc_random(g, 120) / 10.0) + 1;
            if (stage > 12) stage = 12;
            if (g->unlock_sc) stage = g->unlock_sc;
        } else stage = osc_stage_for(g, char2);
    }
    if (!osc_load_stage(g, stage)) return false;

    /* music: Chaves 4, Quico 2, Madruga 3 (CPU side), stage 12 -> 6, else Round(Random(60)/10)+1 */
    int music = 0;
    for (int i = 1; i <= 2; i++) {
        if (g->p[i].human) continue;
        if (g->p[i].char_id == 1) music = 4;
        if (g->p[i].char_id == 4) music = 2;
        if (g->p[i].char_id == 2) music = 3;
    }
    if (stage == 12) music = 6;
    if (!music) music = iround(osc_random(g, 6 * 10) / 10.0) + 1;   /* 6 = Musicas loaded */
    if (music > 6) music = 6;
    if (music == 6 && stage != 12) music = 1;
    if (g->demo == 0) osc_play_music(g, music);   /* the attract demo starts no music (0x49fdb1) */

    g->cam_x = 100; g->cam_y = 20;
    g->timer_sec = osc_round_time(g); g->timer_sub = 25;
    g->round_no = 1;
    if (g->fight_no < 1) g->fight_no = 1;
    g->round_start = 0; g->round_end = 0; g->no_push = true; g->hit_stop = 0; g->super_pause = 0;
    g->match_over = 0;
    g->p[1].wins = g->p[2].wins = 0;
    g->p[1].on_top = true; g->p[2].on_top = false;
    g->mover.active = false; g->mover.freq = 330;
    g->first_hit = false; g->dialogo = 0; g->shake = 0; g->flash = 0; g->flash_on = false;
    g->snd_pending = 0; g->sp_draw = false;
    g->p[1].ai_pref = g->p[2].ai_pref = 200;
    g->screen = OSC_SCREEN_FIGHT;
    g->screen_tick = 0;
    SDL_Log("fight %d: %s vs %s at %s", g->fight_no, osc_text(OSC_MEMO_NOMES, char1, g->lang),
            osc_text(OSC_MEMO_NOMES, char2, g->lang), osc_text(OSC_MEMO_CENARIOS, stage - 1, g->lang));
    return true;
}

void osc_fight_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->tick++;
    if (g->unlock_mentecapto) { g->hud_y = -90; g->hud_y2 = 40; }   /* top of DXTimer1Timer */
    if (g->demo > 0) { g->timer_sec = g->cfg_time_mode; g->timer_sub = 25; }   /* attract demo: g_timerA := g_cfg1_mode every tick (0x4aa026) */
    if (g->challenge > 0) {                                        /* "ALGUÉM ENTROU NO COMBATE !" */
        g->no_push = true;
        if (--g->challenge == 0) {
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
            g->sel_locked[1] = g->sel_locked[2] = 0; g->timer_sec = 20; g->timer_sub = 25;
            g->p[1].human = g->p1_in; g->p[2].human = g->p2_in; g->ai_p2 = !g->p2_in;
            g->sel[1] = g->p[1].char_id; g->sel[2] = g->p[2].char_id;
            g->no_push = false;
        }
        return;
    }
    g->p[1].in = in1; g->p[2].in = in2;
    /* round 1: both fighters jump in from the stage edges (0x4a44cb) */
    if (g->round_no == 1 && g->round_start == 0) {
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            f->x = i == 1 ? 0.0 : 600.0;
            f->jump_dir = i == 1 ? 3 : 2;
            f->y = OSC_GROUND_Y; f->in_air = true; f->vy = -25; f->frame = 21;
            f->state = 12; f->anim_timer = 0; f->anim_dur = 99;
        }
        g->no_push = true;
        g->cam_y = iround((g->p[1].y + g->p[2].y) / 8.0) - 55;
    }
    /* the last fight against the CPU opens with a dialogue (0x4a4591) */
    if (g->fight_no == 8 && g->round_no == 1 && g->round_start == 1 && (!g->p[1].human || !g->p[2].human)) {
        g->dialogo = 600; g->round_start = 2; g->hud_y = -120; g->hud_y2 = 60;
    }
    if (g->dialogo > 0) g->dialogo--;
    /* the sound asked for last tick (g_sndIndexA) */
    if (g->snd_pending) { play_snd(g, g->snd_pending); g->snd_pending = 0; }
    /* super pause: the stage gives way to a scrolling pattern (0x4a4682) */
    g->sp_draw = g->super_pause > 0;
    if (g->super_pause > 0) {
        g->super_pause--;
        int sp = g->super_pause;
        g->sp_off = 0;
        if (sp % 2 == 0) g->sp_off = 20;
        if (sp % 3 == 0) g->sp_off = 40;
        if (sp % 4 == 0) g->sp_off = 60;
        if (sp % 5 == 0) g->sp_off = 80;
    }
    /* camera: midpoint of the fighters, the shake, then the stage limits (0x4a47f1) */
    if (!g->no_push) {
        g->cam_x = iround((g->p[1].x + g->p[2].x) / 2.0) - 200;
        g->cam_y = iround((g->p[1].y + g->p[2].y) / 8.0) - 55;
    }
    if (g->shake > 0) {
        g->shake--;
        g->cam_x = g->cam_x - 10 + osc_random(g, 20);
        g->cam_y = g->cam_y - 10 + osc_random(g, 20);
    }
    /* fixed limits of the original (0x4a47f1): x 0..200, y 0..20 */
    if (g->cam_x < 0) g->cam_x = 0; if (g->cam_x > 200) g->cam_x = 200;
    if (g->cam_y < 0) g->cam_y = 0; if (g->cam_y > 20) g->cam_y = 20;
    bool frozen = g->hit_stop || g->super_pause;
    if (g->demo > 0) {                                                     /* nobody falls in the demo (0x4a4cfe) */
        g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;
        g->p[1].stocks = g->p[2].stocks = 3;
    }
    if (g->cfg_power == 2 || g->cfg_power == 4) g->p[1].stocks = 3;        /* "Poder" 1P MAX / MAX */
    if (g->cfg_power == 3 || g->cfg_power == 4) g->p[2].stocks = 3;
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        /* meter 102 -> stock (main loop 4572) */
        if (f->stocks < 0) f->stocks = 0;
        if (f->meter >= OSC_METER_MAX && f->stocks < 3) { f->stocks++; f->meter = 0; play_snd(g, 13); }
        if (f->stocks >= 3) { f->meter = OSC_METER_MAX; f->stocks = 3; }
        /* health bars (HUD loop 0x4a4cbe): the red bar waits 20 ticks after a hit, then
         * falls 3 per tick to the health */
        if (g->unlock_energeticos && g->p[1].health > 0.0 && g->p[2].health > 0.0) f->health += 0.15;   /* g_energeticos */
        if (f->health < 0.0) f->health = 0.0;
        if (f->health > OSC_HEALTH_MAX) f->health = OSC_HEALTH_MAX;
        if (f->health_shown > OSC_HEALTH_MAX) f->health_shown = OSC_HEALTH_MAX;
        if (!g->super_pause && !g->hit_stop && f->trail_delay == 0) {
            if (f->health_shown > f->health) f->health_shown -= 3.0;
            if (f->health_shown < f->health) f->health_shown = f->health;
        }
        if (f->trail_delay > 0) f->trail_delay--;
        if (f->msg_timer > 0) f->msg_timer--;
        if (f->combo_timer > 15 && f->hits > 1) f->combo_jitter = osc_random(g, 10);
    }

    /* the clock (0x4a4c66) stops during pauses and while nobody may move */
    if (!frozen && !g->no_push) g->timer_sub--;
    if (g->timer_sub <= 0) { g->timer_sub = 25; if (g->timer_sec > 0) g->timer_sec--; }

    /* input (0x4a5a61): humans, then the sequences, then the CPU */
    if (!frozen && !g->challenge && !g->no_push)
        for (int i = 1; i <= 2; i++)
            if (g->p[i].human) input_dispatch(g, &g->p[i]);
    for (int i = 1; i <= 2; i++) sequences(g, &g->p[i]);
    if (!frozen && !g->challenge)
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (f->human || g->no_push) continue;
            cpu_think(g, f);
        }
    for (int i = 1; i <= 2; i++) counters(g, &g->p[i]);
    stage_objects(g);
    /* fighters in drawing order: the one who hit last goes on top */
    if (g->p[1].on_top) { fighter_frame(g, &g->p[2]); fighter_frame(g, &g->p[1]); }
    else { fighter_frame(g, &g->p[1]); fighter_frame(g, &g->p[2]); }
    if (!frozen && !g->challenge && !g->no_push)
        for (int i = 1; i <= 2; i++) defend(g, &g->p[i]);
    update_fx(g);

    /* round intro (DXTimer1Timer 0x4a71d0): g_roundStart counts to 71, the bars
     * fill from tick 6, no input until tick 55 */
    if (g->round_start <= 70 && g->dialogo == 0) {   /* the dialogue holds the intro */
        g->round_start++;
        if (g->round_start < 10) g->no_push = true;
        g->hud_y = g->hud_y2 = 0;
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (g->round_start < 5) f->health = f->health_shown = 1.0;
            if (g->round_start > 5 && g->round_start < 55 && f->health < OSC_HEALTH_MAX) f->health += 3.0;
        }
        if (g->round_start == 55) {
            g->no_push = false;
            g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;
        }
    }

    /* time over (0x4a7005): the lower health drops to 0, a draw empties both */
    if (g->timer_sec == 0) {
        if (g->round_end < 30) { g->round_end = 30; play_snd(g, 4); }
        if (g->hud_y > -120) g->hud_y -= 10;
        if (g->hud_y2 < 490) g->hud_y2 += 10;
        g->no_push = true;
        double h1 = g->p[1].health, h2 = g->p[2].health;
        if (h1 < h2) g->p[1].health = 0.0;
        if (h2 < h1) g->p[2].health = 0.0;
        if (h1 == h2) g->p[1].health = g->p[2].health = 0.0;
    }

    /* round end (0x4a7496): runs once a fighter is down */
    if (g->p[1].health <= 0.0 || (g->p[2].health <= 0.0 && g->round_end < 230)) {
        bool calm = !g->hit_stop && !g->super_pause;
        if (calm) g->round_end++;
        if (g->round_end > 40) g->no_push = true;
        if (g->round_end == 1) { g->hit_stop = 30; g->round_end = 2; }
        if (g->p[1].health <= 0.0) { g->p[2].invuln = true; g->p[1].invuln = false; }
        if (g->p[2].health <= 0.0) { g->p[1].invuln = true; g->p[2].invuln = false; }
        if (g->round_end > 0 && g->round_end < 30) { g->hud_y -= 5; g->hud_y2 += 5; }
        /* the round's winner: win pose and bonuses */
        if (g->round_end > 30 && g->round_end < 90)
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i], *o = opp(g, f);
                if (f->health <= 0.0) continue;
                if (g->no_push) set_move(g, f, 14);
                calm = !g->hit_stop && !g->super_pause;
                if (f->human && f->health < OSC_HEALTH_MAX && g->round_end == 35 && calm) {
                    f->score += iround(f->health) * 5;   /* VITALIDADE x5 */
                    g->round_end++;
                }
                if (f->human && f->health == OSC_HEALTH_MAX && g->round_end == 45 && calm) {
                    f->score += 1000;                     /* PERFEITO */
                    g->round_end++;
                }
                if (f->human && f->wins == g->rounds_to_win && o->wins == 0 && g->round_end == 45 && calm)
                    f->score += 1000;                     /* COMPLETO */
            }
        if (g->round_end == 40) {
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i];
                if (f->health > 0.0 && !g->hit_stop && !g->super_pause) { f->wins++; g->round_end++; }
                g->transition = iround(osc_random(g, 40) / 10.0) + 1;
                if (g->transition > 4) g->transition = 4;
                f->quote = iround(osc_random(g, 30) / 10.0) + 1;
                if (f->quote > 3) f->quote = 3;
            }
        }
        /* the match winner: pose, big portrait and quote (0x4a7b5d) */
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (f->wins < g->rounds_to_win || g->round_end < 90 || g->round_end > 210) continue;
            if (g->no_push) set_move(g, f, 14);
            /* holding the first button hurries the scene along */
            if ((f->in & OSC_IN_B1) && (g->round_end < 90 || g->round_end > 100)) g->round_end += 2;
            if (g->round_end == 95 && !g->hit_stop && !g->super_pause && f->char_id >= 1 && f->char_id <= 15) {
                /* the victory line, a or b: Round(Random(200) / 200) with banker's rounding */
                const osc_wav *v = &g->voices[f->char_id][osc_random(g, 200) > 100];
                if (v->buf) osc_audio_play(v);
            }
        }
        bool decided = g->p[1].wins >= g->rounds_to_win || g->p[2].wins >= g->rounds_to_win;
        if (!decided && g->round_end > 130) {
            /* next round (zerardados for both) */
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i];
                int wins = f->wins, ch = f->char_id, st = f->stocks, score = f->score, mc = f->max_combo;
                double meter = f->meter; bool human = f->human;
                const osc_strip *st_ = f->strip;
                extern void osc_reset_fighter(osc_fighter *, int, int, const osc_strip *);
                osc_reset_fighter(f, ch, i, st_);
                f->wins = wins; f->stocks = st; f->score = score; f->human = human; f->meter = meter; f->max_combo = mc;
            }
            g->round_no++;
            g->round_end = 0; g->round_start = 0; g->hit_stop = 0; g->super_pause = 0;
            g->hud_y = g->hud_y2 = 0; g->first_hit = false;
            g->p[1].on_top = true; g->p[2].on_top = false; g->mover.active = false;
            g->timer_sec = osc_round_time(g); g->timer_sub = 25; g->cam_x = 100;
        }
        if (decided && g->round_end > 220) g->match_over = g->p[1].wins >= g->rounds_to_win ? 1 : 2;
    }

    /* body hits, then the clocks and the physics (0x4a85ec..0x4a99d8) */
    for (int i = 1; i <= 2; i++) melee_step(g, &g->p[i]);
    if (g->hit_stop > 0) g->hit_stop--;
    for (int i = 1; i <= 2; i++) move_clock(g, &g->p[i]);
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        if (f->y > OSC_GROUND_Y) {
            f->y = OSC_GROUND_Y; f->rot = 0; f->rot_step = 0; f->vy = 0; f->in_air = false;
            f->state = 1; f->anim_timer = 0; f->attacking = false; f->knockdown = false;
        }
    }
    for (int i = 1; i <= 2; i++) physics(g, &g->p[i]);
    /* dialogue skip: the human's first button hurries it (0x4a9f83) */
    if (g->dialogo > 0)
        for (int i = 1; i <= 2; i++)
            if (g->p[i].human && (g->p[i].in & OSC_IN_B1) && g->dialogo > 10) g->dialogo -= 5;
    for (int i = 1; i <= 2; i++) g->p[i].in_prev = g->p[i].in;
    if (!g->super_pause && !g->hit_stop) g->ciclo++;
    if (g->ciclo > 30) g->ciclo = 0;   /* state 7 wraps the blink counter at 31 */
    /* red flash on strong hits (0x4aa29a), shown on even ticks */
    g->flash_on = false;
    if (g->flash > 0) { g->flash_on = (g->ciclo % 2) == 0; g->flash--; }
    osc_audio_update();
}

/* ---- rendering ------------------------------------------------------------------------ */
static void draw_fighter(osc_game *g, const osc_fighter *f)
{
    const osc_strip *s = f->strip;
    /* shadow (Efeitos[0]), rising a little with the jump */
    if (g->n_fx_tex > 0) {
        int sy = OSC_GROUND_Y - 12 - iround((OSC_GROUND_Y - iround(f->y)) / 12.0) - g->cam_y;
        draw_tex(g, g->fx_tex[0], (float)(iround(f->x) - 35 - g->cam_x), (float)sy, 255, false);
    }
    if (f->frame < 1 || f->frame > s->count || !s->tex[f->frame]) return;
    int w = s->w[f->frame];
    int off = (f->frame < 64) ? osc_frame_offset[f->char_id][f->frame] : 0;
    float dx = f->facing == 1 ? (float)(f->x - off - g->cam_x) : (float)(f->x + off - w - g->cam_x);
    float dy = (float)(f->y - f->sprite_h - g->cam_y);
    SDL_FRect dst = {dx, dy, (float)w, (float)s->h};
    SDL_RenderTextureRotated(g->ren, s->tex[f->frame], NULL, &dst, (double)f->rot, NULL,
                             f->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    if (g->debug_boxes) {
        SDL_FRect hb = {(float)(f->x + f->hurt_l - g->cam_x), (float)(f->y - f->sprite_h + f->hurt_t - g->cam_y),
                        (float)(f->hurt_r - f->hurt_l), (float)(f->hurt_b - f->hurt_t)};
        SDL_SetRenderDrawColor(g->ren, 50, 50, 255, 255);
        SDL_RenderRect(g->ren, &hb);
        if (f->attacking) {
            SDL_FRect ab = {(float)(f->x + f->hit_l - g->cam_x), (float)(f->y - f->sprite_h + f->hit_t - g->cam_y),
                            (float)(f->hit_r - f->hit_l), (float)(f->hit_b - f->hit_t)};
            SDL_SetRenderDrawColor(g->ren, 255, 50, 50, 255);
            SDL_RenderRect(g->ren, &ab);
        }
    }
}

static const char *tx(osc_game *g, int get) { return osc_text(OSC_MEMO_TEXTO, get - 1, g->lang); }   /* MemoTexto.Lines.Get(n) */
static bool even(int v) { return (v & 1) == 0; }

/* Top HUD, drawn before the fighters (DXTimer1Timer 0x4a49ac..0x4a5a61). */
static void draw_hud_top(osc_game *g)
{
    int hy = g->hud_y;
    char buf[96];
    snprintf(buf, sizeof buf, "%s %d", tx(g, 29), g->fight_no);                        /* BATALHA n */
    texto(g, 200, hy + 3, buf, 'c', 7, k_white);
    barraredonda(g, 170, hy + 15, 230, hy + 54, 15, k_white, k_white);
    barraredonda(g, 172, hy + 17, 228, hy + 52, 15, k_grey, k_grey);
    barraredonda(g, 174, hy + 19, 226, hy + 50, 15, k_black, k_black);
    if (g->cfg_time_mode != 4) {
        osc_rgb c = g->timer_sub <= 2 ? rgb(255, 180, 180) : rgb(255, 50, 50);
        if (g->timer_sec >= 10) {
            snprintf(buf, sizeof buf, "%d", g->timer_sec);
            texto(g, 200, hy + 10, buf, 'c', 25, c);
        } else {
            snprintf(buf, sizeof buf, "0%d", g->timer_sec);
            texto(g, 200, hy + 10, buf, 'c', 25, c);
            if (even(g->timer_sub)) texto(g, 200, hy + 10, buf, 'c', 25, rgb(255, 180, 180));
        }
    }
    for (int p = 1; p <= 2; p++) {
        const osc_fighter *f = &g->p[p];
        int s = p == 1 ? -1 : 1, c = f->char_id;
        int pulse = iround(cos(g->ciclo / 4.75) * 30.0);
        osc_rgb plate = p == 1 ? rgb(pulse + 50, pulse + 50, 255) : rgb(255, pulse + 30, pulse + 30);
        barra(g, 200 + s * 198, hy + 40, 200 + s * 130, hy + 68, plate);
        retangulo(g, 200 + s * 198, hy + 40, 200 + s * 130, hy + 68, k_white);
        barraredonda(g, 200 + s * 157, hy + 30, 200 + s * 17, hy + 50, 8, k_black, k_white);
        barraredonda(g, 200 + s * 155, hy + 32, 200 + s * 19, hy + 48, 8, k_grey, k_grey);
        barraredonda(g, 200 + s * 153, hy + 34, 200 + s * 21, hy + 46, 8, k_black, k_black);
        if (f->health <= 15.0 && even(g->ciclo))
            barraredonda(g, 200 + s * 153, hy + 34, 200 + s * 21, hy + 46, 8, k_black, rgb(200, 0, 0));
        /* face: Rxx-a, or Rxx-b shaking while the red bar falls; P2's faces are mirrored */
        int shake = 0;
        if (f->health_shown > f->health) shake = even(g->ciclo) ? -1 : 1;
        if (c >= 1 && c <= 15) {
            SDL_Texture *t = shake ? g->face_hurt[c] : g->face_small[c];
            float w, h;
            if (t && SDL_GetTextureSize(t, &w, &h)) {
                SDL_FRect d = {(float)((p == 1 ? 3 : 347) - shake), (float)(hy + 7), w, h};
                SDL_RenderTextureRotated(g->ren, t, NULL, &d, 0.0, NULL, p == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
            }
        }
        /* tick marks (0x4a527b..0x4a5323): drawn on the real health, under the bars, so only the empty part shows them */
        for (int i = 0; i < 7; i++) {
            int tx_ = 200 + s * (40 + 16 * i);
            retangulo(g, tx_, hy + 35, tx_, hy + 45, f->health > 15.0 ? rgb(80, 80, 80) : rgb(150, 30, 30));
        }
        barra(g, 200 + iround(s * (25.0 + f->health_shown)), hy + 35, 200 + s * 25, hy + 45, rgb(170, 0, 0));
        int hx = 200 + iround(s * (25.0 + f->health));
        barra(g, hx, hy + 35, 200 + s * 25, hy + 45, rgb(150, 150, 0));
        barra(g, hx, hy + 36, 200 + s * 25, hy + 44, rgb(220, 220, 70));
        barra(g, hx, hy + 38, 200 + s * 25, hy + 42, rgb(250, 250, 100));
        barra(g, hx, hy + 39, 200 + s * 25, hy + 40, k_white);
        /* round lamps */
        int pulse2 = iround(cos(g->ciclo / 2.735) * 60.0);
        for (int i = 1; i <= g->rounds_to_win; i++) {
            int xa = 200 + s * (195 - 17 * (i - 1)), xb = 200 + s * (180 - 17 * (i - 1));
            barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, k_black);
            if (f->wins >= i) {
                barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, rgb(160 - pulse2, 0, 0));
                if (g->ciclo < 10) barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, rgb(255, 0, 0));
            }
        }
        /* an empty side invites a second player */
        if (!f->human && g->demo == 0 && g->ciclo < 15) {
            if (g->credits == 0) texto(g, 200 + s * 100, 0, tx(g, 6), 'c', 10, k_white);          /* INSIRA FICHAS */
            else if (!g->no_push) texto(g, 200 + s * 100, 0, tx(g, 7), 'c', 10, k_white);      /* ENTRE AGORA */
        }
        const char *name = osc_text(OSC_MEMO_NOMES, c, g->lang);
        if (p == 1) texto(g, 200 + s * 145, hy + 45, name, '-', 12, rgb(120, 120, 255));
        else texto(g, 200 + s * 145, hy + 45, name, 'e', 12, rgb(255, 110, 110));
        if (f->human) {
            int k = iround(cos(g->ciclo / 4.75) * 20.0);
            snprintf(buf, sizeof buf, "%d", f->score);
            if (p == 1) {
                texto(g, 200 + s * 145, hy + 10, "1P", '-', 12, rgb(k + 120, k + 120, 255));
                texto(g, 200 + s * 50, hy + 10, buf, 'e', 12, rgb(k + 120, k + 120, 255));
            } else {
                texto(g, 200 + s * 145, hy + 10, "2P", 'e', 12, rgb(255, k + 110, k + 110));
                texto(g, 200 + s * 50, hy + 10, buf, '-', 12, rgb(255, k + 110, k + 110));
            }
        }
    }
}

/* Bottom HUD, after the fighters and effects (0x4a678e..0x4a6fff). */
static void draw_hud_bottom(osc_game *g)
{
    int hy2 = g->hud_y2;
    char buf[32];
    for (int p = 1; p <= 2; p++) {
        const osc_fighter *f = &g->p[p];
        int s = p == 1 ? -1 : 1;
        if (f->msg_timer > 0) {
            const char *m = tx(g, f->msg_index);
            if (strcmp(m, "-") != 0) texto(g, 200 + s * 100, 80, m, 'c', 15, rgb(80, 255, 140));
            if (f->msg_index == 1 && f->human) texto(g, 200 + s * 80, 110, tx(g, 2), 'c', 10, k_white);   /* Primeiro Ataque */
        }
        if (f->combo_timer > 0 && f->hits > 1) {
            snprintf(buf, sizeof buf, "%d", f->hits);
            if (f->combo_timer > 15) texto(g, 195 + f->combo_jitter + s * 170, 110, buf, 'c', 25, k_white);
            else texto(g, 200 + s * 170, 110, buf, 'c', 25, k_white);
            texto(g, 200 + s * 150, 145, tx(g, 3), 'c', 12, rgb(80, 255, 140));                       /* Acertos. */
        }
        /* super stocks */
        barraredonda(g, 200 + s * 65, hy2 + 280, 200 + s * 22, hy2 + 288, 6, rgb(10, 20, 30), k_white);
        int pv = even(g->ciclo) && f->stocks > 0 ? 100 : 0;
        for (int i = 1; i <= f->stocks; i++)
            circulopintado(g, 200 + s * (30 + 10 * (i - 1)), hy2 + 286, 200 + s * (35 + 10 * (i - 1)), hy2 + 281,
                           rgb(255, 20, 20), rgb(255, pv + 150, pv + 150));
        int pw = even(g->ciclo) && f->stocks > 0 ? 50 : 0;
        snprintf(buf, sizeof buf, "%d", f->stocks);
        texto(g, 200 + s * 18, hy2 + 270, buf, 'c', 12, rgb(255 - pw, 255 - pw, 255));
        /* special meter */
        barraredonda(g, 200 + s * 170, hy2 + 275, 200 + s * 60, hy2 + 285, 5, k_white, k_white);
        barraredonda(g, 200 + s * 169, hy2 + 276, 200 + s * 61, hy2 + 284, 5, k_grey, k_grey);
        barraredonda(g, 200 + s * 168, hy2 + 277, 200 + s * 62, hy2 + 283, 5, rgb(60, 30, 0), k_black);
        int mx = 200 + iround(s * (64.0 + f->meter));
        if (f->meter < OSC_METER_MAX) {
            barra(g, mx, hy2 + 278, 200 + s * 64, hy2 + 282, rgb(210, 160, 30));
            barra(g, mx, hy2 + 279, 200 + s * 64, hy2 + 281, rgb(250, 220, 140));
        } else {
            int pm = even(g->ciclo) ? 50 : 0, k = f->stocks + 1;
            barra(g, mx, hy2 + 278, 200 + s * 64, hy2 + 282, rgb(240, pm + 190, pm + 60));
            barra(g, mx, hy2 + 279, 200 + s * 64, hy2 + 281, rgb(255, 250, pm + 170));
            osc_rgb c = even(g->ciclo) ? rgb(k + 140, k + 140, 255) : rgb(k + 170, k + 170, 255);
            texto(g, 200 + iround(s * 115.0), hy2 + 262, tx(g, 4), 'c', 15, c);                       /* M A X */
        }
    }
}

/* Round intro and outro texts, the winner's scene and the wipe (0x4a7005..0x4a83bf). */
static void draw_round_texts(osc_game *g)
{
    char buf[96];
    int re = g->round_end;
    if (g->timer_sec == 0) {
        int p = even(g->ciclo) ? 80 : 0;
        if (re < 80) texto(g, 200, 40, tx(g, 27), 'c', 28, rgb(255, p, p));                       /* FIM DO TEMPO */
        if (g->p[1].health == g->p[2].health && re < 90) texto(g, 200, 120, tx(g, 28), 'c', 29, rgb(255, 2 * p, 2 * p));   /* EMPATE */
    }
    if (g->round_start <= 71) {
        if (g->round_start > 5 && g->round_start < 40) {
            int p = even(g->ciclo) ? 30 : 0;
            snprintf(buf, sizeof buf, "%d", g->round_no);
            texto(g, 260, 85, buf, 'c', 35, rgb(255 - p, 255 - p, 255 - p));
            texto(g, 200, 100, "ROUND   ", 'c', 20, k_white);
        }
        if (g->round_start > 42 && g->round_start < 60) {
            int p = even(g->ciclo) ? 120 : 0;
            texto(g, 200, 120, tx(g, 15), 'c', 25, rgb(255, p + 30, p));                            /* LUTEM */
        }
    }
    if (!(g->p[1].health <= 0.0 || (g->p[2].health <= 0.0 && re < 230))) return;
    if (re > 0 && re < 30) {
        int p = even(g->ciclo) ? 100 : 0;
        texto(g, 200, 100, "KO", 'c', 70, rgb(255, p, 0));
    }
    if (re > 30 && re < 90)
        for (int i = 1; i <= 2; i++) {
            const osc_fighter *f = &g->p[i], *o = &g->p[3 - i];
            if (f->health <= 0.0) continue;
            int p = even(g->ciclo) ? 80 : 0;
            texto(g, 200, 110, osc_text(OSC_MEMO_NOMES, f->char_id, g->lang), 'c', 20, rgb(255, p, p));
            texto(g, 200, 140, tx(g, 14), 'c', 14, k_white);                                        /* VENCEU */
            if (f->human && f->health < OSC_HEALTH_MAX) {
                snprintf(buf, sizeof buf, "%sx5=%d PTS.", tx(g, 5), iround(f->health) * 5);         /* VITALIDADE */
                texto(g, 200, 210, buf, 'c', 12, k_white);
            }
            int p3 = even(g->ciclo) ? 120 : 0;
            if (f->health == OSC_HEALTH_MAX && re > 35) texto(g, 200, 170, tx(g, 30), 'c', 28, rgb(255, p3 + 120, p3 + 30));   /* PERFEITO */
            if (f->human && f->health == OSC_HEALTH_MAX) texto(g, 200, 210, "BONUS 1000 PTS.", 'c', 15, k_white);
            if (f->human && f->wins == g->rounds_to_win && o->wins == 0) {
                int p4 = even(g->ciclo) ? 20 : 0;
                snprintf(buf, sizeof buf, "%s  1000 PTS.", tx(g, 32));                              /* COMPLETO */
                texto(g, 200, 240, buf, 'c', 20, rgb(p4 + 170, p4 + 170, p4 + 235));
            }
        }
    for (int i = 1; i <= 2; i++) {
        const osc_fighter *f = &g->p[i];
        if (f->wins < g->rounds_to_win || re < 90 || re > 210) continue;
        int p5 = even(g->ciclo) ? 30 : 0, c = f->char_id;
        /* the winner's big face (Rxx-c) slides in, holds, slides out */
        if (c >= 1 && c <= 15 && g->face_big[c]) {
            int l = re < 100 ? 728 - (re - 90) * 50 : re < 180 ? 128 : 128 - (re - 180) * 50;
            SDL_FRect d = {(float)l, 29.f, 144.f, 189.f};
            SDL_RenderTexture(g->ren, g->face_big[c], NULL, &d);
        }
        if (f->human) {
            texto(g, 10, 20, tx(g, 16), '-', 12, rgb(p5 + 30, p5 + 220, p5 + 30));                    /* MÁXIMO DE ACERTOS */
            snprintf(buf, sizeof buf, "%d", f->max_combo);
            texto(g, 10, 35, buf, '-', 14, rgb(255 - p5, 255 - p5, 255 - p5));
            texto(g, 10, 60, tx(g, 17), '-', 12, rgb(p5 + 30, p5 + 220, p5 + 30));                    /* PONTOS */
            snprintf(buf, sizeof buf, "%d", f->score);
            texto(g, 10, 75, buf, '-', 14, rgb(255 - p5, 255 - p5, 255 - p5));
        }
        barraredonda(g, 5, 220, 395, 280, 5, k_white, k_black);
        texto(g, 200, 237, osc_text(OSC_MEMO_FRASES, (c - 1) * 4 + f->quote, g->lang), 'c', 12, k_white);
    }
    /* wipe to the next round */
    if (g->p[1].wins < g->rounds_to_win && g->p[2].wins < g->rounds_to_win && re > 95) {
        switch (g->transition) {
        case 1:
            barra(g, 0, 0, 400, re * 10 - 950, k_black);
            barra(g, 0, 1250 - re * 10, 400, 300, k_black);
            break;
        case 2:
            barra(g, 0, 0, 200, re * 12 - 1150, k_black);
            barra(g, 200, 1450 - re * 12, 400, 300, k_black);
            break;
        case 3:
            for (int i = 0; i < 4; i++) {
                barra(g, 0, 80 * i, re * 17 - 1670, 80 * i + 40, k_black);
                barra(g, 2070 - re * 17, 80 * i + 40, 400, 80 * i + 80, k_black);
            }
            break;
        case 4:
            barra(g, 0, 0, 400, re * 10 - 1000, k_black);
            barra(g, 0, 1300 - re * 10, 400, 300, k_black);
            barra(g, 0, 0, re * 10 - 1000, 300, k_black);
            barra(g, 1400 - re * 10, 0, 400, 300, k_black);
            break;
        }
    }
}

void osc_fx_render(osc_game *g)
{
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_fx_draw *d = &g->fx_draw[i];
        if (d->kind == 2) {        /* pickup popup: MemoTexto.Get(5), centred, size 10, green (0x48fc96) */
            texto(g, d->sx, d->sy, tx(g, 5), 'c', 10, rgb(80, 255, 140));
        } else if (d->kind == 1 && g->fx_tex[d->sprite]) {
            float w = (float)g->fx_w[d->sprite], h = (float)g->fx_h[d->sprite];
            /* desenhaefeitos 0x48fb0e: x - Round(w / 2), y - Round(h / 2), or y - h with the f8 flag */
            float ox = (float)iround(w / 2.0), oy = d->bottom ? h : (float)iround(h / 2.0);
            SDL_FRect r = {(float)d->sx - ox, (float)d->sy - oy, w, h};
            SDL_RenderTextureRotated(g->ren, g->fx_tex[d->sprite], NULL, &r, 0.0, NULL,
                                     d->dir == 2 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (d->box) {
            SDL_FRect hb = {(float)d->bx, (float)d->by, (float)d->bw, (float)d->bh};
            SDL_SetRenderDrawColor(g->ren, 255, 200, 0, 255);
            SDL_RenderRect(g->ren, &hb);
        }
    }
}

/* ---- the last fight's dialogue (0x4a99de) ------------------------------------------
 * A is the human, B the CPU; four exchanges of 150 ticks, three lines each, from the
 * memo the port's text index calls Finais (the form field at 0x300, MemoDialogo). */
static void draw_dialogue(osc_game *g)
{
    int d = g->dialogo;
    if (d <= 0) return;
    int a = g->p[1].human ? 1 : 2, b = g->p[1].human ? 2 : 1;
    int ca = g->p[a].char_id, cb = g->p[b].char_id;
    barraredonda(g, 5, 165, 290, 215, 20, k_white, k_black);
    if (ca >= 1 && ca <= 15 && g->face_big[ca])
        draw_tex(g, g->face_big[ca], (float)(d > 580 ? 15 - (d - 580) * 10 : 15), 85.f, 255, false);
    barraredonda(g, 110, 220, 395, 270, 20, k_white, k_black);
    if (cb >= 1 && cb <= 15 && g->face_big[cb])
        draw_tex(g, g->face_big[cb], (float)(d > 580 ? 288 + (d - 580) * 10 : 288), 140.f, 255, false);
    /* on the exact boundaries the line and speaker keep their last values */
    if (d > 450 && d < 600) { g->dialog_line = (ca - 1) * 12; g->dialog_speaker = 1; }
    if (d > 300 && d < 450) { g->dialog_line = (ca - 1) * 12 + 3; g->dialog_speaker = 2; }
    if (d > 150 && d < 300) { g->dialog_line = (ca - 1) * 12 + 6; g->dialog_speaker = 1; }
    if (d > 0 && d < 150) { g->dialog_line = (ca - 1) * 12 + 9; g->dialog_speaker = 2; }
    for (int k = 0; k < 3; k++) {
        const char *s = osc_text(OSC_MEMO_FINAIS, g->dialog_line + k, g->lang);
        if (g->dialog_speaker == 1) texto(g, 115, 168 + 14 * k, s, '-', 10, k_white);
        if (g->dialog_speaker == 2) texto(g, 120, 223 + 14 * k, s, '-', 10, k_white);
    }
}

/* hand-coded draws of desenhaanimacoes that are not table rows (world coords, minus camera) */
static void stage_anim_at(osc_game *g, int item, int x, int y)
{
    if (item >= g->n_anim_tex || !g->anim_tex[item]) return;
    SDL_FRect d = {(float)(x - g->cam_x), (float)(y - g->cam_y), (float)g->anim_w[item], (float)g->anim_h[item]};
    SDL_RenderTexture(g->ren, g->anim_tex[item], NULL, &d);
}

/* called after each table row of the stage (whether or not that row was drawn): the rows
 * missing from 07c sit in the original right after these ones */
static void stage_anim_extra(osc_game *g, const osc_stage_anim_row *r)
{
    int c1 = g->p[1].char_id, c2 = g->p[2].char_id;
    if (r->stage_mask == 1024 && r->item == 38) {            /* 0x49c44c: Anim 39 */
        if (c1 != 11 && c2 != 11 && c1 != 2 && c2 != 2)
            stage_anim_at(g, 39, 150 + osc_random(g, 2), 138);
    } else if (r->stage_mask == 2048 && r->item == 75) {     /* 0x49c520: Anim 76, before Anim 77 */
        if (c1 != 14 && c2 != 14)
            stage_anim_at(g, 76, 120 + osc_random(g, 2), 82);
    } else if (r->stage_mask == 256 && r->item == 26) {      /* 0x49bf04: five flickering lights, last of stage 8 */
        for (int i = 1; i <= 5; i++) {
            int r3 = osc_random(g, 3), r2 = osc_random(g, 2);
            int base = iround((g->p[1].x + g->p[2].x) / 2.0) - 60;
            stage_anim_at(g, 18, base + 15 * i + r2, 269 - 4 * r3);
        }
    } else if (r->stage_mask == 4096 && r->item == 40 && r->ciclo_lo == 23) {   /* 0x49c7fa: Icone[13], every tick */
        if (13 < g->n_icone && g->icone[13])
            draw_tex(g, g->icone[13], (float)(225 - g->cam_x),
                     (float)(134 - iround(-cos((g->ciclo - 11) / 5.0) * 70.0) - g->cam_y), 255, false);
    }
}

void osc_fight_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->sp_draw) {
        /* super pause: Icone[7] tiled and sliding instead of the stage (0x4a47a0) */
        if (7 < g->n_icone && g->icone[7])
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 5; j++)
                    draw_tex(g, g->icone[7], (float)(100 * i - 80 + g->sp_off), (float)(100 * j), 255, false);
    } else {
        if (g->stage) {
            SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->stage_w, (float)g->stage_h};
            SDL_RenderTexture(g->ren, g->stage, NULL, &dst);
        }
        /* stage animations (desenhaanimacoes, table 07c) */
        for (int i = 0; i < OSC_N_STAGE_ANIM; i++) {
            const osc_stage_anim_row *r = &osc_stage_anim[i];
            if (!(r->stage_mask & (1u << g->stage_id))) continue;
            if (g->ciclo >= r->ciclo_lo && g->ciclo <= r->ciclo_hi &&
                !(r->not_char && (g->p[1].char_id == r->not_char || g->p[2].char_id == r->not_char))) {
                SDL_Texture *t = r->is_effect ? (r->item < g->n_fx_tex ? g->fx_tex[r->item] : NULL)
                                              : (r->item < g->n_anim_tex ? g->anim_tex[r->item] : NULL);
                if (t) {
                    int w = r->is_effect ? g->fx_w[r->item] : g->anim_w[r->item];
                    int h = r->is_effect ? g->fx_h[r->item] : g->anim_h[r->item];
                    int ax = r->x + r->xmul * g->ciclo, ay = r->y - (r->yjit ? r->yjit * osc_random(g, 3) : 0);
                    SDL_FRect d = {(float)(ax - g->cam_x), (float)(ay - g->cam_y), (float)w, (float)h};
                    SDL_RenderTexture(g->ren, t, NULL, &d);
                }
            }
            stage_anim_extra(g, r);
        }
    }
    draw_hud_top(g);
    /* something crossing the street (Efeitos[0] shadow under Efeitos[9]) */
    if (g->mover.active && g->n_fx_tex > 9) {
        draw_tex(g, g->fx_tex[0], (float)(g->mover.x + 10 - g->cam_x), (float)(g->mover.y + 132 - g->cam_y), 255, false);
        draw_tex(g, g->fx_tex[9], (float)(g->mover.x - g->cam_x), (float)(g->mover.y - g->cam_y), 255, false);
    }
    if (g->crate.icon && !g->super_pause && g->crate.icon < g->n_icone && g->icone[g->crate.icon])
        draw_tex(g, g->icone[g->crate.icon], (float)(g->crate.x - g->cam_x), (float)(g->crate.y - g->cam_y), 255, false);
    /* the one who hit last is drawn on top */
    if (g->p[1].on_top) { draw_fighter(g, &g->p[2]); draw_fighter(g, &g->p[1]); }
    else { draw_fighter(g, &g->p[1]); draw_fighter(g, &g->p[2]); }
    osc_fx_render(g);
    draw_hud_bottom(g);
    draw_round_texts(g);
    draw_dialogue(g);
    if (g->flash_on) barra(g, 0, 0, 400, 300, rgb(255, 0, 0));
    if (g->show_moves) osc_movelist_render(g);   /* 0x4aa31c, state 7 only */
    if (g->challenge > 0) {
        osc_rgb pink = rgb(250, 140, 140);
        barraredonda(g, 80, 100, 320, 170, 10, k_white, k_black);                   /* 0x4ae384 */
        texto(g, 200, 100, osc_text(OSC_MEMO_TEXTO, 9, g->lang), 'c', 15, pink);    /* ESPERE ... */
        texto(g, 200, 120, osc_text(OSC_MEMO_TEXTO, 10, g->lang), 'c', 15, pink);   /* ALGUÉM ENTROU */
        texto(g, 200, 140, osc_text(OSC_MEMO_TEXTO, 11, g->lang), 'c', 15, pink);   /* NO COMBATE ! */
    }
}

void osc_game_shutdown(osc_game *g)
{
    osc_assets_free(g);
    memset(g, 0, sizeof *g);
}

void osc_play_snd(osc_game *g, int idx) { play_snd(g, idx); }

void osc_draw_fighter_at(osc_game *g, int slot, int x, int y, int state, int timer)
{
    osc_fighter *f = &g->p[slot];
    if (!f->strip || !f->strip->count) return;
    f->x = x; f->y = y; f->state = state; f->anim_timer = timer; f->in_air = false; f->rot = 0;
    f->sprite_h = k_sprite_h[f->char_id];
    select_pose(g, f);
    draw_fighter(g, f);
}
