#include "game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tables_gen.h"

/* ---- per-character constants (02, 03) ------------------------------------ */
static const int k_sprite_h[16] = {0, 156, 162, 147, 149, 144, 190, 160, 160, 150, 155, 155, 165, 155, 160, 165};
static const double k_speed[16] = {0, 8, 8, 8.5, 8, 8, 7.5, 7.5, 8, 8.5, 7.5, 8, 8, 7.5, 7.5, 8};
static const double k_damage_level = 0.9; /* config line 3 default */
static const double k_meter_unit = 2.0;
static const int k_gravity = 4;

/* ---- Delphi System.Random -------------------------------------------------- */
static int osc_random(osc_game *g, int n)
{
    g->rng = g->rng * 0x8088405u + 1u;
    if (n <= 0) return 0;
    return (int)(((uint64_t)(uint32_t)n * (uint64_t)g->rng) >> 32);
}

static int iround(double v) { return (int)floor(v + 0.5); }

static osc_fighter *opp(osc_game *g, osc_fighter *f) { return f == &g->p[1] ? &g->p[2] : &g->p[1]; }

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
static void set_move(osc_game *g, osc_fighter *f, int move)
{
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
    if ((r->flags & OSC_MOVE_ONGROUND) && f->vy != 0) return;
    if ((r->flags & OSC_MOVE_NOT_INAIR) && f->in_air) return;
    if (r->flags & OSC_MOVE_SUPER) {
        if (!(f->health <= 15.0 || f->stocks > 0)) return;
        if (f->stocks == 3) f->meter = 0;
        f->stocks--;
        g->super_pause = 22;
    }
    if (r->flags & OSC_MOVE_WALK) {
        /* handled by the caller (walk keeps state 2 and moves x) */
        if (f->state != 2) { f->state = 2; f->anim_timer = 0; }
        f->anim_dur = 99;
        return;
    }
    f->state = r->state;
    f->anim_timer = 0;
    f->anim_dur = r->dur;
    if (r->meter10 && f->meter < OSC_METER_MAX) f->meter += k_meter_unit * r->meter10 / 10.0;
    if (r->strength) f->hit_level = r->strength;
    if (move == 8 || move == 9) f->attacking = false;
}

/* ---- frame selector (0x47d21c) + boxes ------------------------------------- */
static void select_frame(osc_fighter *f)
{
    bool have_char_rows = false;
    for (int i = 0; i < OSC_N_ANIM; i++)
        if (osc_anim[i].ch == f->char_id && osc_anim[i].state == f->state) { have_char_rows = true; break; }
    int ch = have_char_rows ? f->char_id : 0;
    for (int i = 0; i < OSC_N_ANIM; i++) {
        const osc_anim_row *r = &osc_anim[i];
        if (r->ch != ch || r->state != f->state) continue;
        if (r->timer < 0 || r->timer == f->anim_timer) f->frame = r->frame;
    }
    for (int i = 0; i < OSC_N_HURT; i++) {
        const osc_hurt_row *r = &osc_hurt[i];
        if (r->ch != f->char_id || r->frame != f->frame) continue;
        f->hurt_l = r->l; f->hurt_b = r->b; f->hurt_r = r->r; f->hurt_t = r->t;
        if (f->facing == 2) { f->hurt_l = -r->r; f->hurt_r = -r->l; }
    }
    for (int i = 0; i < OSC_N_HIT; i++) {
        const osc_hit_row *r = &osc_hit[i];
        if (r->ch != f->char_id || r->state != f->state || r->timer != f->anim_timer) continue;
        f->hit_l = r->l; f->hit_t = r->t; f->hit_r = r->r; f->hit_b = r->b;
        if (f->facing == 2) { f->hit_l = -r->r; f->hit_r = -r->l; }
        f->attacking = r->atk;
        if (r->level) f->hit_level = r->level;
        f->damage = r->dmg;
        f->chip = r->p4;
    }
    if (f->state == 21 || f->state == 22) f->blocking = true;
}

/* ---- walking with body push (sub_47648c) ---------------------------------- */
static void move_x(osc_game *g, osc_fighter *f, double dx)
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

/* ---- direction helpers (seta) ------------------------------------------------- */
static bool held(const osc_fighter *f, const char *dir)
{
    if (!strcmp(dir, "BAIXO")) return f->in & OSC_IN_DOWN;
    if (!strcmp(dir, "CIMA")) return f->in & OSC_IN_UP;
    bool left = f->in & OSC_IN_LEFT, right = f->in & OSC_IN_RIGHT;
    if (!strcmp(dir, "TRAS")) return f->facing == 1 ? left : right;
    return f->facing == 1 ? right : left; /* FRENTE */
}

static bool dir_bit(const osc_fighter *f, int bit)
{
    static const char *names[4] = {"BAIXO", "FRENTE", "TRAS", "CIMA"};
    return held(f, names[bit]);
}

/* ---- sequenciagolpe / sequencia ---------------------------------------------- */
static void sequences(osc_game *g, osc_fighter *f)
{
    (void)g;
    if (held(f, "BAIXO")) { if (f->down_hold < 18) f->down_hold++; } else if (f->down_hold > 1) f->down_hold--;
    if (held(f, "TRAS")) { if (f->back_hold < 18) f->back_hold++; } else if (f->back_hold > 1) f->back_hold--;
    for (int i = 0; i < OSC_N_SEQS; i++) {
        const osc_seq_row *r = &osc_seqs[i];
        if (r->ch != f->char_id || r->from != f->seq_state) continue;
        bool ok = true;
        for (int b = 0; b < 4 && ok; b++) {
            bool h = dir_bit(f, b);
            if ((r->req & (1 << b)) && !h) ok = false;
            if ((r->forb & (1 << b)) && h) ok = false;
        }
        if (ok && r->down_hold && f->down_hold <= r->down_hold) ok = false;
        if (ok && r->on_ground && f->y != OSC_GROUND_Y) ok = false;
        if (ok) { f->seq_state = r->to; f->seq_timer = 9; }
    }
}

/* ---- golpe: button -> special move ----------------------------------------------- */
static void golpe(osc_game *g, osc_fighter *f, int btn)
{
    if (g->hit_stop) return;
    for (int i = 0; i < OSC_N_GOLPE; i++) {
        const osc_golpe_row *r = &osc_golpe[i];
        if (r->ch != f->char_id || r->btn != btn) continue;
        bool ok = true;
        if (r->seq >= 0 && f->seq_state != r->seq) ok = false;
        if ((r->extra & 1) && f->mash_p <= 5) ok = false;
        if ((r->extra & 2) && f->mash_k <= 5) ok = false;
        if ((r->extra & 4) && f->vy == 0) ok = false;
        if (ok) set_move(g, f, r->move);
    }
}

/* ---- human input dispatch (main loop 4150-4345) -------------------------------- */
static void input_dispatch(osc_game *g, osc_fighter *f)
{
    uint8_t in = f->in, pressed = (uint8_t)(f->in & ~f->in_prev);
    if (!(in & OSC_IN_DOWN)) {
        if (in & OSC_IN_LEFT) { set_move(g, f, 1); if (f->state == 2) move_x(g, f, -f->speed); }
        if (in & OSC_IN_RIGHT) { set_move(g, f, 2); if (f->state == 2) move_x(g, f, f->speed); }
    }
    if (in & OSC_IN_DOWN) set_move(g, f, 7);
    if (!(in & OSC_IN_DOWN) && f->state == 7) f->state = 1;
    if ((in & OSC_IN_UP) && (f->state == 1 || f->state == 2 || f->state == 21 || f->state == 22) &&
        f->vy == 0 && !f->in_air) {
        f->vy = -32; f->in_air = true; f->state = 12; f->anim_timer = 0; f->anim_dur = 99; f->blocking = false;
        f->jump_dir = (in & OSC_IN_LEFT) ? 2 : (in & OSC_IN_RIGHT) ? 3 : 1;
    }
    if (!(in & (OSC_IN_LEFT | OSC_IN_RIGHT)) && f->state == 2) { f->state = 1; f->anim_timer = 0; }
    for (int b = 0; b < 4; b++) {
        if (pressed & (OSC_IN_B1 << b)) {
            golpe(g, f, b + 1);
            set_move(g, f, 3 + b);
        }
    }
    if (pressed & (OSC_IN_B1 | OSC_IN_B3)) f->mash_p += 6;
    if (pressed & (OSC_IN_B2 | OSC_IN_B4)) f->mash_k += 6;
}

/* ---- defendergolpe (human part) -------------------------------------------------- */
static void defend(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    if (!f->human) return;
    if (held(f, "TRAS") && o->attacking) set_move(g, f, held(f, "BAIXO") ? 13 : 12);
}

/* ---- collision + hit resolution (main loop 5175-5470) ---------------------------- */
static void resolve_hits(osc_game *g, osc_fighter *a)
{
    osc_fighter *v = opp(g, a);
    if (!a->attacking || v->invuln || g->hit_stop || g->super_pause) return;
    double al = a->x + a->hit_l, at = a->y - a->sprite_h + a->hit_t, ar = a->x + a->hit_r, ab = a->y - a->sprite_h + a->hit_b;
    double vl = v->x + v->hurt_l, vt = v->y - v->sprite_h + v->hurt_t, vr = v->x + v->hurt_r, vb = v->y - v->sprite_h + v->hurt_b;
    if (!(al < vr && vl < ar && at < vb && vt < ab)) return;

    /* low attacks break stand block, jump attacks break crouch block */
    if ((a->state == 9 || a->state == 11) && v->state == 21) v->blocking = false;
    if (a->state >= 13 && a->state <= 16 && v->state == 22) v->blocking = false;

    if (v->blocking) {
        v->anim_timer = 1;
        if (v->facing == 1) move_x(g, v, -4.0); else move_x(g, v, 4.0);
        if (!g->no_push) v->health -= a->chip * k_damage_level;
        if (v->health <= 0.0) set_move(g, v, 10);
        a->attacking = false;
        if (v->health > 0.0) g->hit_stop = osc_random(g, 4) + (a->hit_level == 2 ? 4 : 2);
        return;
    }
    v->facing = v->x < a->x ? 1 : 2;
    if (a->human && v->health > 0.0) a->score += 5;
    if (a->meter < OSC_METER_MAX) a->meter += k_meter_unit * 2.0;
    if (v->meter < OSC_METER_MAX) v->meter += k_meter_unit;
    if (a->combo_timer < 36) a->hits = 0;
    a->hits++;
    a->combo_timer = 44;
    if (a->hits > a->max_combo) a->max_combo = a->hits;
    if (!g->no_push) v->health -= a->damage * k_damage_level;
    if (v->health <= 0.0) set_move(g, v, 10);
    bool heavy = a->state == 4 || a->state == 6 || a->state == 8 || a->state == 9 || a->state == 10;
    set_move(g, v, heavy ? 9 : 8);
    if (a->knockdown || v->vy != 0 || v->y < OSC_GROUND_Y) set_move(g, v, 10);
    v->hitstun = 4;
    v->blocking = false;
    a->attacking = false;
    if (v->health > 0.0) g->hit_stop = osc_random(g, 4) + (a->hit_level == 2 ? 7 : 5);
    select_frame(v);
}

/* ---- per-tick timers, physics ---------------------------------------------------- */
static void advance(osc_game *g, osc_fighter *f)
{
    bool paused = g->hit_stop || g->super_pause;
    if (!paused && f->anim_dur != 99) f->anim_timer++;
    if (f->anim_dur != 99 && f->anim_timer >= f->anim_dur) {
        f->state = 1; f->attacking = false; f->knockdown = false; f->anim_timer = 0;
        f->invuln = false; f->blocking = false; f->hit_level = 0; f->anim_dur = 99;
        if (f->y < OSC_GROUND_Y || f->vy != 0) { f->in_air = true; f->state = 12; }
    }
    if (f->state == 2 && !paused) { f->anim_timer++; if (f->anim_timer > 16) f->anim_timer = 0; }
    if (f->seq_timer > 0 && !paused) { f->seq_timer--; if (f->seq_timer == 0) f->seq_state = 0; }
    if (f->mash_p) f->mash_p--; if (f->mash_p > 15) f->mash_p = 15;
    if (f->mash_k) f->mash_k--; if (f->mash_k > 15) f->mash_k = 15;
    if (f->combo_timer && !paused) { f->combo_timer--; if (!f->combo_timer) f->hits = 0; }

    if (paused) return;
    /* jump / knock-up */
    if (f->in_air) {
        f->y += f->vy;
        f->vy += k_gravity;
        if (f->jump_dir == 2) move_x(g, f, -f->speed);
        if (f->jump_dir == 3) move_x(g, f, f->speed);
        if (f->state == 19) {
            osc_fighter *o = opp(g, f);
            move_x(g, f, f->x < o->x ? -f->hitstun : f->hitstun);
            if (f->y >= OSC_GROUND_Y) {
                set_move(g, f, 11);
                f->y = OSC_GROUND_Y - 10;
                return;
            }
        }
        if (f->y >= OSC_GROUND_Y) {
            f->y = OSC_GROUND_Y; f->vy = 0; f->in_air = false; f->rot = 0; f->rot_step = 0;
            if (f->state == 20) { f->state = 1; f->invuln = false; }
            else { f->state = 1; }
            f->anim_timer = 0; f->anim_dur = 99; f->attacking = false; f->knockdown = false;
        }
    }
    if (f->state == 12 || f->state == 19) f->rot += f->rot_step; else { f->rot = 0; f->rot_step = 0; }
    /* facing follows the opponent while free */
    if ((f->state == 1 || f->state == 2 || f->state == 7) && !f->in_air) {
        osc_fighter *o = opp(g, f);
        f->facing = f->x < o->x ? 1 : 2;
    }
}

/* ---- public ------------------------------------------------------------------------- */
static bool load_path(char *dst, size_t n, const char *dir, const char *rel)
{
    int r = snprintf(dst, n, "%s/%s", dir, rel);
    return r > 0 && (size_t)r < n;
}

static void init_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip)
{
    memset(f, 0, sizeof *f);
    f->char_id = ch;
    f->x = p == 1 ? 220.0 : 380.0;
    f->y = OSC_GROUND_Y;
    f->health = OSC_HEALTH_MAX;
    f->state = 1; f->frame = 1; f->anim_dur = 99;
    f->facing = p == 1 ? 1 : 2;
    f->sprite_h = k_sprite_h[ch];
    f->speed = k_speed[ch];
    f->strip = strip;
    f->human = true;
    f->hurt_l = -20; f->hurt_r = 20; f->hurt_t = 10; f->hurt_b = f->sprite_h; /* fallback box */
}

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir, int char1, int char2)
{
    memset(g, 0, sizeof *g);
    g->ren = ren;
    g->rng = (uint32_t)SDL_GetTicksNS();
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);
    char path[1200];
    osc_image img;

    load_path(path, sizeof path, data_dir, "Cenarios/Cenario-01.bmp");
    if (!osc_bmp_load(path, false, &img)) return false;
    g->stage = osc_texture_from_image(ren, &img);
    g->stage_w = img.w; g->stage_h = img.h;
    osc_image_free(&img);
    if (!g->stage) return false;

    int chars[2] = {char1, char2};
    for (int i = 0; i < 2; i++) {
        char rel[64];
        snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", chars[i]);
        load_path(path, sizeof path, data_dir, rel);
        if (!osc_bmp_load(path, true, &img)) return false;
        bool ok = osc_strip_build(ren, &img, &g->strips[i]);
        osc_image_free(&img);
        if (!ok) return false;
        SDL_Log("%s: %d frames, height %d", rel, g->strips[i].count, g->strips[i].h);
        init_fighter(&g->p[i + 1], chars[i], i + 1, &g->strips[i]);
    }
    g->cam_x = 100;
    g->timer_sec = 99; g->timer_sub = 25;

    load_path(path, sizeof path, data_dir, "Musicas/Musica-01.wav");
    if (osc_audio_open() && osc_wav_load(path, &g->music)) {
        g->has_music = true;
        osc_audio_play(&g->music, true);
    }
    return true;
}

void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->tick++;
    if (g->paused) return;
    g->p[1].in = in1; g->p[2].in = in2;
    bool frozen = g->hit_stop || g->super_pause;

    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        if (!frozen && f->human && !g->no_push) input_dispatch(g, f);
        sequences(g, f);
        if (!frozen && !g->no_push) defend(g, f);
    }
    for (int i = 1; i <= 2; i++) { advance(g, &g->p[i]); select_frame(&g->p[i]); }
    for (int i = 1; i <= 2; i++) resolve_hits(g, &g->p[i]);
    for (int i = 1; i <= 2; i++) g->p[i].in_prev = g->p[i].in;

    /* camera: midpoint rule, clamped to the stage */
    if (!g->no_push) {
        g->cam_x = iround((g->p[1].x + g->p[2].x) / 2.0) - 200;
        g->cam_y = iround((g->p[1].y + g->p[2].y) / 8.0) - 55;
    }
    int max_cam = g->stage_w - OSC_SCREEN_W; if (max_cam < 0) max_cam = 0;
    if (g->cam_x < 0) g->cam_x = 0; if (g->cam_x > max_cam) g->cam_x = max_cam;
    int max_cy = g->stage_h - OSC_SCREEN_H; if (max_cy < 0) max_cy = 0;
    if (g->cam_y < 0) g->cam_y = 0; if (g->cam_y > max_cy) g->cam_y = max_cy;

    if (g->hit_stop) g->hit_stop--;
    if (g->super_pause) g->super_pause--;
    if (!frozen && g->timer_sec > 0 && --g->timer_sub == 0) { g->timer_sec--; g->timer_sub = 25; }
    osc_audio_update();
}

/* ---- rendering ------------------------------------------------------------------------ */
static void draw_fighter(osc_game *g, const osc_fighter *f)
{
    const osc_strip *s = f->strip;
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

static void fill(osc_game *g, float x1, float y1, float x2, float y2, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_FRect rc = {SDL_min(x1, x2), SDL_min(y1, y2), SDL_fabsf(x2 - x1), SDL_fabsf(y2 - y1)};
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderFillRect(g->ren, &rc);
}

static void draw_hud(osc_game *g)
{
    /* timer frame: (170,15)-(230,54) white / grey / black (07b) */
    fill(g, 170, 15, 230, 54, 255, 255, 255);
    fill(g, 172, 17, 228, 52, 170, 170, 170);
    fill(g, 174, 19, 226, 50, 0, 0, 0);
    char buf[16];
    snprintf(buf, sizeof buf, "%2d", g->timer_sec);
    SDL_SetRenderDrawColor(g->ren, 255, 255, 255, 255);
    SDL_RenderDebugText(g->ren, 192, 30, buf);
    for (int i = 1; i <= 2; i++) {
        const osc_fighter *f = &g->p[i];
        float s = i == 1 ? -1.f : 1.f;
        /* health frame */
        fill(g, 200 + s * 157, 30, 200 + s * 17, 50, 255, 255, 255);
        fill(g, 200 + s * 155, 32, 200 + s * 19, 48, 170, 170, 170);
        fill(g, 200 + s * 153, 34, 200 + s * 21, 46, 0, 0, 0);
        double hp = f->health < 0 ? 0 : f->health;
        float len = (float)(hp / OSC_HEALTH_MAX * 132.0);
        fill(g, 200 + s * 21, 35, 200 + s * (21 + len), 45, hp <= 15 ? 220 : 240, hp <= 15 ? 40 : 200, 40);
        /* super meter at the bottom */
        float ml = (float)(f->meter / OSC_METER_MAX * 100.0);
        fill(g, 200 + s * 60, 285, 200 + s * 160, 291, 60, 60, 60);
        fill(g, 200 + s * 60, 286, 200 + s * (60 + ml), 290, 250, 220, 60);
        for (int k = 0; k < 3; k++)
            fill(g, 200 + s * (30 + 10 * k) - 3, 283, 200 + s * (30 + 10 * k) + 3, 289, k < f->stocks ? 255 : 80, k < f->stocks ? 200 : 80, 40);
        if (f->hits > 1) {
            snprintf(buf, sizeof buf, "%d HITS", f->hits);
            SDL_RenderDebugText(g->ren, i == 1 ? 30 : 320, 60, buf);
        }
    }
}

void osc_game_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->stage) {
        SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->stage_w, (float)g->stage_h};
        SDL_RenderTexture(g->ren, g->stage, NULL, &dst);
    }
    /* the player who attacked last is drawn on top; simple rule: P2 then P1 */
    draw_fighter(g, &g->p[2]);
    draw_fighter(g, &g->p[1]);
    draw_hud(g);
}

void osc_game_shutdown(osc_game *g)
{
    if (g->has_music) osc_wav_free(&g->music);
    osc_audio_close();
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    if (g->stage) SDL_DestroyTexture(g->stage);
    memset(g, 0, sizeof *g);
}
