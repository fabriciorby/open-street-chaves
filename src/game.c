#include "game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exetext.h"
#include "exeimg.h"
#include "font.h"
#include "tables_gen.h"

/* ---- per-character constants (02, 03) ------------------------------------ */
static const int k_sprite_h[16] = {0, 156, 162, 147, 149, 144, 190, 160, 160, 150, 155, 155, 165, 155, 160, 165};
static const double k_speed[16] = {0, 8, 8, 8.5, 8, 8, 7.5, 7.5, 8, 8.5, 7.5, 8, 8, 7.5, 7.5, 8};
static const double k_meter_unit = 2.0;
static const int k_gravity = 4;

/* ---- Delphi System.Random -------------------------------------------------- */
int osc_random(osc_game *g, int n)
{
    g->rng = g->rng * 0x8088405u + 1u;
    if (n <= 0) return 0;
    return (int)(((uint64_t)(uint32_t)n * (uint64_t)g->rng) >> 32);
}

static int iround(double v) { return (int)floor(v + 0.5); }

static osc_fighter *opp(osc_game *g, osc_fighter *f) { return f == &g->p[1] ? &g->p[2] : &g->p[1]; }

void osc_play_snd(osc_game *g, int idx) ;
static void play_snd(osc_game *g, int idx)
{
    if (idx >= 0 && idx < g->n_sounds && g->sounds[idx].buf) osc_audio_play(&g->sounds[idx], false);
}

/* AI probability helper: Random(round(C * k)) == 0 (05 "AI difficulty"). */
static bool chance(osc_game *g, double c, double k)
{
    int n = iround(c * k);
    if (n < 1) n = 1;
    return osc_random(g, n) == 0;
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
    if (r->snd >= 0) play_snd(g, r->snd);
}

/* ---- CPU opponent (cpu 0x4931ac / defendergolpe CPU part), generic rules ----------
 * The per-character special-move choices of the original are approximated with
 * the same distance bands (see 04 and 99); the walk/normals/block logic is exact. */
static void cpu_think(osc_game *g, osc_fighter *f)
{
    /* TForm1.cpu, transcribed from the x86 code (docs/spec/05d-cpu-table.txt). */
    osc_fighter *o = opp(g, f);
    int me = (f == &g->p[1]) ? 1 : 2;
    double k = 6.1 - 0.7 * g->fight_no;      /* cpu() */
    double kb = 8.4 - 1.0 * g->fight_no;     /* defendergolpe() */
    int dist = abs(iround(f->x) - iround(o->x));
    f->in = 0;

    /* a hostile effect with a hit box is live (projectile incoming) */
    bool proj = false;
    int pd = 0;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_effect *e = &g->fx[i];
        if (e->type && e->owner != me && e->has_hitbox) {
            proj = true;
            pd = abs(iround(e->x) - iround(f->x));
        }
    }

    /* preferred distance re-rolled with chance 1/13 */
    static const int pref[6] = {100, 150, 200, 250, 300, 350};
    if (osc_random(g, 13) == 0) f->ai_pref = pref[osc_random(g, 6)];
    if (abs(dist - f->ai_pref) < 51) f->ai_pref = dist;

    bool busy = !(f->state == 1 || f->state == 2 || f->state == 7);
    if (!f->blocking && !proj && !busy) {
        bool toward = dist >= f->ai_pref;   /* aiPrefDist <= dist -> walk toward */
        bool right = (f->x < o->x) == toward;
        f->in |= right ? OSC_IN_RIGHT : OSC_IN_LEFT;
    }
    /* defendergolpe(): opponent attacking within range */
    if (o->attacking && !busy && dist <= 161) {
        if (chance(g, 0.5, kb)) {
            f->in &= (uint8_t)~(OSC_IN_LEFT | OSC_IN_RIGHT);
            f->in |= (f->x < o->x) ? OSC_IN_LEFT : OSC_IN_RIGHT;   /* hold back */
            if (o->state == 9 || o->state == 11 || o->y > OSC_GROUND_Y - 20) f->in |= OSC_IN_DOWN;
        } else if (chance(g, 2.5, kb) && f->state != 12) {
            f->in |= OSC_IN_UP;                                      /* jump away */
        }
    }
    /* close range: normals, crouch, crouching normals */
    if (!f->blocking && !proj && dist <= 80) {
        if (chance(g, 8, k)) set_move(g, f, 3);
        if (chance(g, 8, k)) set_move(g, f, 4);
        if (chance(g, 9, k)) set_move(g, f, 5);
        if (chance(g, 9, k)) set_move(g, f, 6);
        if (chance(g, 4, k)) {
            set_move(g, f, 7);
            if (chance(g, 2, k)) set_move(g, f, 3);
            if (chance(g, 2, k)) set_move(g, f, 4);
            if (chance(g, 3, k)) set_move(g, f, 5);
            if (chance(g, 3, k)) set_move(g, f, 6);
        }
    }
    /* per-character specials and supers */
    if (!f->blocking) {
        for (int i = 0; i < OSC_N_CPU; i++) {
            const osc_cpu_row *r = &osc_cpu[i];
            if (r->ch != f->char_id) continue;
            if ((r->cond & 1) && proj) continue;
            if ((r->cond & 2) && !proj) continue;
            if ((r->cond & 4) && !(o->y < OSC_GROUND_Y)) continue;
            if ((r->cond & 8) && !(f->y < OSC_GROUND_Y - 100)) continue;
            if ((r->cond & 16) && !(f->y < OSC_GROUND_Y - 50)) continue;
            int v = (r->cond & 2) ? pd : dist;
            if (r->greater ? !(v > r->thr) : !(v < r->thr)) continue;
            if (!chance(g, r->c, k)) continue;
            set_move(g, f, r->move + (r->n > 1 ? osc_random(g, r->n) : 0));
        }
    }
}

/* ---- effects (06) -------------------------------------------------------------- */
static const osc_fx_type *fx_type(int t)
{
    for (int i = 0; i < OSC_N_FX_TYPES; i++)
        if (osc_fx_types[i].type == t) return &osc_fx_types[i];
    return NULL;
}

/* FUN_00476a04: take the first free slot and apply the per-type defaults. */
static osc_effect *spawn_fx(osc_game *g, int type, int variant, int owner, double x, double y)
{
    const osc_fx_type *t = fx_type(type);
    if (!t) return NULL;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        osc_effect *e = &g->fx[i];
        if (e->type) continue;
        memset(e, 0, sizeof *e);
        e->type = type; e->x = x; e->y = y; e->owner = owner;
        e->dir = owner ? g->p[owner].facing : 1;
        e->speed = variant == 2 ? t->speedB : t->speedA;
        e->damage = t->damage;
        e->life = t->life;
        if (type == 24) e->life = 15 + osc_random(g, 15);
        e->has_hitbox = t->flags & OSC_FX_HITBOX;
        e->top_anchor = t->flags & OSC_FX_TOP;
        e->gravity = t->flags & OSC_FX_GRAVITY;
        e->persist = t->flags & OSC_FX_PERSIST;
        e->vy = t->vy;
        e->c5 = t->c5;
        e->loop_at = t->loop_at;
        e->hb_l = t->l; e->hb_t = t->t; e->hb_r = t->r; e->hb_b = t->b;
        if (type == 21) { e->dir = 1 + osc_random(g, 2); e->speed = osc_random(g, 10); }
        if (SDL_getenv("OSC_DEBUG")) SDL_Log("spawn fx type %d var %d owner %d at %.0f,%.0f speed %d dmg %d box %d,%d,%d,%d tick %u", type, variant, owner, x, y, e->speed, e->damage, e->hb_l, e->hb_t, e->hb_r, e->hb_b, g->tick);
        return e;
    }
    return NULL;
}

static int fx_sprite(const osc_effect *e)
{
    int s = e->sprite;
    for (int i = 0; i < OSC_N_FX_SPRITES; i++) {
        const osc_fx_sprite_row *r = &osc_fx_sprites[i];
        if (r->type != e->type) continue;
        if (r->phase < 0 || r->phase == e->phase) s = r->sprite;
    }
    return s;
}

/* spawn the effects the frame selector produces for (char, state, timer) */
static void fx_spawns_for(osc_game *g, osc_fighter *f, int pidx)
{
    for (int i = 0; i < OSC_N_FX_SPAWNS; i++) {
        const osc_fx_spawn_row *r = &osc_fx_spawns[i];
        if (r->ch != f->char_id || r->state != f->state) continue;
        if (r->timer >= 0 ? r->timer != f->anim_timer : f->anim_timer != 0) continue;
        if (r->variant && f->hit_level && r->variant != f->hit_level) continue;
        if (r->variant == 2 && !f->hit_level) continue;
        /* the original's per-move offsets could not be recovered textually (99):
         * projectiles start a body width ahead at chest height, dust at the feet */
        double dx = f->facing == 1 ? 40.0 : -40.0;
        double y = r->type == 2 ? f->y : f->y - f->sprite_h * 0.55;
        spawn_fx(g, r->type, r->variant, pidx, f->x + dx, y);
    }
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

/* ---- hit application shared by body attacks and projectiles --------------------------- */
typedef struct { int damage, chip, level; bool heavy, knockdown; double x, y; int attacker; } osc_hitinfo;

static void apply_hit(osc_game *g, osc_fighter *a, osc_fighter *v, const osc_hitinfo *h)
{
    if (v->blocking) {
        v->anim_timer = 1;
        play_snd(g, 10);
        spawn_fx(g, 14, 0, 0, h->x, h->y);
        if (v->facing == 1) move_x(g, v, -4.0); else move_x(g, v, 4.0);
        if (!g->no_push) v->health -= h->chip * g->damage_level;
        if (v->health <= 0.0) { play_snd(g, 9); set_move(g, v, 10); }
        if (v->health > 0.0) g->hit_stop = h->attacker ? osc_random(g, 4) + (h->level == 2 ? 4 : 2) : 2;
        return;
    }
    play_snd(g, h->level >= 3 ? 9 : h->level == 2 ? 8 : 7);
    spawn_fx(g, 1, 0, 0, h->x, h->y);
    v->facing = v->x < a->x ? 1 : 2;
    if (a->human && v->health > 0.0) a->score += h->attacker ? 5 : 10;
    if (a->meter < OSC_METER_MAX) a->meter += k_meter_unit * (h->attacker ? 2.0 : 1.0);
    if (v->meter < OSC_METER_MAX && h->attacker) v->meter += k_meter_unit;
    if (a->combo_timer < 36) a->hits = 0;
    a->hits++;
    a->combo_timer = h->attacker ? 44 : 50;
    if (a->hits > a->max_combo) a->max_combo = a->hits;
    if (!g->no_push) v->health -= h->damage * g->damage_level;
    if (v->health <= 0.0) { play_snd(g, 9); set_move(g, v, 10); }
    set_move(g, v, h->heavy ? 9 : 8);
    if (h->knockdown || v->vy != 0 || v->y < OSC_GROUND_Y) set_move(g, v, 10);
    v->hitstun = h->attacker ? 4 : 8;
    v->blocking = false;
    if (v->health > 0.0) g->hit_stop = h->attacker ? osc_random(g, 4) + (h->level == 2 ? 7 : 5) : 5;
    select_frame(v);
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

    osc_hitinfo h = {a->damage, a->chip, a->hit_level,
                 a->state == 4 || a->state == 6 || a->state == 8 || a->state == 9 || a->state == 10,
                 a->knockdown, a->facing == 1 ? ar : al, at, a == &g->p[1] ? 1 : 2};
    a->attacking = false;
    apply_hit(g, a, v, &h);
}

/* ---- effects update: motion, collisions, drawing data (desenhaefeitos) ------------- */
static void update_fx(osc_game *g)
{
    bool frozen = g->hit_stop || g->super_pause;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        osc_effect *e = &g->fx[i];
        if (!e->type) continue;
        e->sprite = fx_sprite(e);
        if (!frozen) {
            /* motion rules (06) */
            if (e->dir == 1) e->x += e->speed; else if (e->dir == 2) e->x -= e->speed;
            if (e->type == 19) e->y += e->speed;
            if (e->type == 20) e->y -= 7 + osc_random(g, 13);
            if (e->type == 2) e->y -= 1.0;
            if (e->type == 5) e->y -= 5.0;
            if (e->gravity) {
                e->y += e->vy; e->vy += 4;
                if (e->y > OSC_GROUND_Y - 5) {
                    e->y = OSC_GROUND_Y - 5; e->speed = 0; e->vy = 0; e->gravity = false;
                    if (e->type == 22) { e->done = true; e->has_hitbox = false; }
                }
            }
            if (e->type == 30 && g->p[e->owner].state != 26) e->type = 0;
            if ((e->type == 31 || e->type == 32) && g->p[e->owner].state != 29) e->type = 0;
            if (e->type == 34 && e->phase > 13) e->has_hitbox = false;
            if (!e->type) continue;
        }
        /* effect vs effect: hostile projectiles from different owners cancel */
        if (e->has_hitbox && e->damage > 0 && e->owner) {
            for (int j = 0; j < OSC_MAX_FX; j++) {
                osc_effect *o = &g->fx[j];
                if (j == i || !o->type || !o->has_hitbox || o->damage <= 0 || !o->owner || o->owner == e->owner) continue;
                if (e->x + e->hb_l < o->x + o->hb_r && o->x + o->hb_l < e->x + e->hb_r &&
                    e->y + e->hb_t < o->y + o->hb_b && o->y + o->hb_t < e->y + e->hb_b) {
                    spawn_fx(g, 1, 0, 0, e->x, e->y);
                    if (!e->persist) e->type = 0;
                    if (!o->persist) o->type = 0;
                    break;
                }
            }
            if (!e->type) continue;
        }
        /* effect vs player */
        if (e->has_hitbox && e->damage > 0 && e->owner && !frozen) {
            for (int p = 1; p <= 2; p++) {
                if (p == e->owner) continue;
                osc_fighter *v = &g->p[p], *a = &g->p[e->owner];
                double vl = v->x + v->hurt_l, vt = v->y - v->sprite_h + v->hurt_t, vr = v->x + v->hurt_r, vb = v->y - v->sprite_h + v->hurt_b;
                if (!(e->x + e->hb_l < vr && vl < e->x + e->hb_r && e->y + e->hb_t < vb && vt < e->y + e->hb_b)) continue;
                if (v->invuln && !v->blocking) continue;
                osc_hitinfo h = {e->damage, (int)(e->damage / 4), e->damage >= 20 ? 2 : 1, true,
                             e->type == 8 || e->type == 20, e->x, e->y, 0};
                apply_hit(g, a, v, &h);
                if (!e->persist) e->type = 0;
                e->has_hitbox = false;
                break;
            }
            if (!e->type) continue;
        }
        if (!frozen) {
            if (e->life != 99) e->phase++;
            if (e->loop_at && e->phase >= e->loop_at) e->phase = 0;
            if (e->c5 && ++e->c4 >= e->c5) e->has_hitbox = true;
            if (e->life != 99 && e->phase > e->life) e->type = 0;
            if (e->x < g->cam_x - 200 || e->x > g->cam_x + 600) e->type = 0;
        }
    }
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
                play_snd(g, 12);
                f->y = OSC_GROUND_Y - 10;
                for (int k = 0; k < 5; k++)
                    spawn_fx(g, 2, 0, 0, f->x - 20 + osc_random(g, 40), f->y + osc_random(g, 10));
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

void osc_reset_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip);
static void init_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip) { osc_reset_fighter(f, ch, p, strip); }

/* zerardados: per-round reset of one fighter. */
void osc_reset_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip)
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

static bool load_image_tex(osc_game *g, const char *rel, bool key, SDL_Texture **tex, int *w, int *h)
{
    char path[1200];
    osc_image img;
    load_path(path, sizeof path, g->data_dir, rel);
    if (!osc_bmp_load(path, key, &img)) return false;
    *tex = osc_texture_from_image(g->ren, &img);
    if (w) *w = img.w;
    if (h) *h = img.h;
    osc_image_free(&img);
    return *tex != NULL;
}

/* Common assets: effects, stage animations, faces, sounds, text (FormCreate). */
bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir)
{
    bool ai_p2 = g->ai_p2;
    int lang = g->lang;
    memset(g, 0, sizeof *g);
    g->ai_p2 = ai_p2;
    g->lang = lang;
    g->ren = ren;
    g->rng = (uint32_t)SDL_GetTicksNS();
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);
    char path[1200], rel[64];

    for (int n = 1; n < OSC_MAX_FX_SPRITES; n++) {
        snprintf(rel, sizeof rel, "Efeitos/Efeito-%03d.bmp", n);
        if (!load_image_tex(g, rel, true, &g->fx_tex[n - 1], &g->fx_w[n - 1], &g->fx_h[n - 1])) break;
        g->n_fx_tex = n;
    }
    if (g->n_fx_tex == 0) return false;   /* no data folder */
    for (int n = 1; n < OSC_MAX_FX_SPRITES; n++) {
        snprintf(rel, sizeof rel, "Animacoes/Anim-%02d.bmp", n);
        if (!load_image_tex(g, rel, true, &g->anim_tex[n - 1], &g->anim_w[n - 1], &g->anim_h[n - 1])) break;
        g->n_anim_tex = n;
    }
    for (int c = 1; c <= 15; c++) {
        snprintf(rel, sizeof rel, "Rostos/R%02d-a.bmp", c);
        load_image_tex(g, rel, true, &g->face_small[c], NULL, NULL);
        snprintf(rel, sizeof rel, "Rostos/R%02d-c.bmp", c);
        load_image_tex(g, rel, true, &g->face_big[c], &g->face_big_w[c], &g->face_big_h[c]);
    }
    SDL_Log("%d effect sprites, %d stage animation sprites loaded", g->n_fx_tex, g->n_anim_tex);

    if (osc_audio_open()) {
        for (int n = 1; n <= 80; n++) {
            snprintf(rel, sizeof rel, "Sons/Som-%02d.wav", n);
            load_path(path, sizeof path, data_dir, rel);
            if (!osc_wav_load(path, &g->sounds[g->n_sounds])) break;
            g->n_sounds++;
        }
        SDL_Log("%d sounds loaded", g->n_sounds);
    }
    osc_text_load(data_dir);
    /* intro pictures and icons live inside Chaves.exe (03-assets.md) */
    if (osc_exeimg_load(data_dir)) {
        g->n_abertura = osc_exeimg_count("Abertura"); if (g->n_abertura > 16) g->n_abertura = 16;
        for (int i = 0; i < g->n_abertura; i++) {
            osc_image img;
            if (!osc_exeimg_get("Abertura", i, &img)) continue;
            g->abertura[i] = osc_texture_from_image(ren, &img); g->abertura_w[i] = img.w; g->abertura_h[i] = img.h;
            osc_image_free(&img);
        }
        g->n_icone = osc_exeimg_count("Icone"); if (g->n_icone > 14) g->n_icone = 14;
        for (int i = 0; i < g->n_icone; i++) {
            osc_image img;
            if (!osc_exeimg_get("Icone", i, &img)) continue;
            g->icone[i] = osc_texture_from_image(ren, &img); g->icone_w[i] = img.w; g->icone_h[i] = img.h;
            osc_image_free(&img);
        }
        g->n_final = osc_exeimg_count("Final"); if (g->n_final > 11) g->n_final = 11;
        for (int i = 0; i < g->n_final; i++) {
            osc_image img;
            if (!osc_exeimg_get("Final", i, &img)) continue;
            g->final_tex[i] = osc_texture_from_image(ren, &img); g->final_w[i] = img.w; g->final_h[i] = img.h;
            osc_image_free(&img);
        }
        osc_exeimg_free();
    }
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
    g->sel_timer = 30 * 25;
    g->p1_in = true;
    g->p2_in = !g->ai_p2;
    return true;
}

/* Stage chosen by the CPU opponent's character (07, state 5). */
int osc_stage_for(osc_game *g, int opponent)
{
    static const int base[16] = {0, 1, 2, 2, 3, 3, 4, 11, 1, 1, 11, 6, 1, 1, 2, 1};
    static const int alt[16] = {0, 0, 7, 10, 10, 8, 8, 5, 9, 10, 10, 5, 9, 5, 1, 6};
    int st = base[opponent];
    if (osc_random(g, 5) < 2 && g->fight_no < 8 && alt[opponent]) st = alt[opponent];
    if (osc_random(g, 6) < 2 && g->fight_no < 8 &&
        (opponent == 3 || opponent == 4 || opponent == 9 || opponent == 10 || opponent == 11 || opponent == 14)) st = 12;
    return st;
}

/* criaadversario: 7 random distinct opponents plus the rival (02). */
void osc_make_ladder(osc_game *g, int ch)
{
    static const int rival[16] = {0, 2, 5, 9, 1, 2, 7, 2, 5, 3, 1, 3, 1, 5, 5, 3};
    static const int excl[16][6] = {{0}, {3, 9}, {3, 8, 9, 14, 15}, {1, 2, 14}, {5, 6}, {4, 6}, {4, 5, 15},
                                    {8, 10}, {2, 7}, {1, 2, 15}, {7, 9}, {1, 9}, {5, 8}, {1, 8, 14}, {2, 3, 13}, {1, 2, 9}};
    bool avail[16];
    for (int i = 1; i <= 15; i++) avail[i] = true;
    avail[ch] = false;
    avail[rival[ch]] = false;
    for (int i = 0; i < 6 && excl[ch][i]; i++) avail[excl[ch][i]] = false;
    for (int slot = 1; slot <= 7; slot++) {
        int c;
        do { c = osc_random(g, 15) + 1; } while (!avail[c]);
        g->ladder[slot] = c;
        avail[c] = false;
    }
    g->ladder[8] = rival[ch];
}

/* Round/fight init (state 6 -> 7). */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage)
{
    char rel[64];
    osc_image img;
    char path[1200];
    if (g->stage) { SDL_DestroyTexture(g->stage); g->stage = NULL; }
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    memset(g->fx, 0, sizeof g->fx);

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
    if (stage < 1 || stage > 12) stage = osc_stage_for(g, char2);
    g->stage_id = stage;
    snprintf(rel, sizeof rel, "Cenarios/Cenario-%02d.bmp", stage);
    if (!load_image_tex(g, rel, false, &g->stage, &g->stage_w, &g->stage_h)) return false;

    /* music: Chaves 4, Quico 2, Madruga 3 (CPU side), stage 12 -> 6, else random 1..5 */
    int music = 0;
    for (int i = 1; i <= 2; i++) {
        if (g->p[i].human) continue;
        if (g->p[i].char_id == 1) music = 4;
        if (g->p[i].char_id == 4) music = 2;
        if (g->p[i].char_id == 2) music = 3;
    }
    if (stage == 12) music = 6;
    if (!music) music = osc_random(g, 5) + 1;
    osc_play_music(g, music);

    g->cam_x = 100; g->cam_y = 20;
    g->timer_sec = osc_round_time(g); g->timer_sub = 25;
    g->round_no = 1;
    if (g->fight_no < 1) g->fight_no = 1;
    g->round_start = 0; g->round_end = 0; g->no_push = true; g->hit_stop = 0; g->super_pause = 0;
    g->match_over = 0;
    g->p[1].wins = g->p[2].wins = 0;
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
    if (g->paused) return;
    if (g->demo > 0) { g->timer_sec = 99; g->timer_sub = 25; }   /* attract demo: clock frozen */
    g->p[1].in = in1; g->p[2].in = in2;
    bool frozen = g->hit_stop || g->super_pause;

    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        if (!f->human && !frozen && !g->no_push) cpu_think(g, f);
        if (!frozen && !g->no_push) input_dispatch(g, f);
        sequences(g, f);
        if (!frozen && !g->no_push && f->human) defend(g, f);
    }
    for (int i = 1; i <= 2; i++) {
        advance(g, &g->p[i]);
        select_frame(&g->p[i]);
        if (!frozen) fx_spawns_for(g, &g->p[i], i);
    }
    for (int i = 1; i <= 2; i++) resolve_hits(g, &g->p[i]);
    update_fx(g);
    if (g->demo > 0) g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;   /* nobody falls in the demo */
    if (g->cfg_power == 2 || g->cfg_power == 4) g->p[1].stocks = 3;        /* "Poder" 1P MAX / MAX */
    if (g->cfg_power == 3 || g->cfg_power == 4) g->p[2].stocks = 3;
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        f->in_prev = f->in;
        /* meter 102 -> stock (main loop 4572) */
        if (f->meter >= OSC_METER_MAX && f->stocks < 3) { f->stocks++; f->meter = 0; play_snd(g, 13); }
        if (f->meter > OSC_METER_MAX) f->meter = OSC_METER_MAX;
    }

    /* round intro (g_roundStart 0..72): bars fill, no input until tick 55 */
    if (g->round_start < 72 && g->round_end == 0) {
        g->round_start++;
        g->no_push = g->round_start < 55;
        if (g->round_start < 5) { g->p[1].health = g->p[2].health = 1.0; }
        else if (g->round_start < 55) {
            for (int i = 1; i <= 2; i++) if (g->p[i].health < OSC_HEALTH_MAX) g->p[i].health += 3.0;
        }
        if (g->round_start == 55) g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;
        if (g->round_start < 55) return;
    }

    /* round end: KO or time over (simplified sequence, 05 "Round flow") */
    bool ko = g->p[1].health <= 0.0 || g->p[2].health <= 0.0;
    if ((ko || g->timer_sec == 0) && g->round_end == 0) {
        g->round_end = 1;
        g->hit_stop = 30;
        if (!ko) play_snd(g, 4);
    }
    if (g->round_end > 0) {
        g->no_push = true;
        g->round_end++;
        if (g->round_end == 2) {
            osc_fighter *w = NULL;
            if (g->p[1].health > g->p[2].health) w = &g->p[1];
            else if (g->p[2].health > g->p[1].health) w = &g->p[2];
            if (w) { w->wins++; if (w->human) w->score += 1000; }
        }
        if (g->round_end > 230) {
            /* next round (state 6 -> 7 init) */
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i];
                int wins = f->wins, ch = f->char_id, st = f->stocks, score = f->score; bool human = f->human;
                const osc_strip *s = f->strip;
                extern void osc_reset_fighter(osc_fighter *, int, int, const osc_strip *);
                osc_reset_fighter(f, ch, i, s);
                f->wins = wins; f->stocks = st; f->score = score; f->human = human;
            }
            if (g->p[1].wins >= g->rounds_to_win) g->match_over = 1;
            else if (g->p[2].wins >= g->rounds_to_win) g->match_over = 2;
            g->round_no++;
            g->round_end = 0; g->round_start = 0; g->no_push = true; g->hit_stop = 0; g->super_pause = 0;
            g->timer_sec = osc_round_time(g); g->timer_sub = 25; g->cam_x = 100;
        }
    }

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
    if (++g->ciclo > 30) g->ciclo = 0;   /* state 7 wraps the blink counter at 31 */
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

void osc_fx_render(osc_game *g)
{
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_effect *e = &g->fx[i];
        if (!e->type || e->sprite < 0 || e->sprite >= g->n_fx_tex || !g->fx_tex[e->sprite]) continue;
        if (e->done && (g->tick & 1)) continue;
        float w = (float)g->fx_w[e->sprite], h = (float)g->fx_h[e->sprite];
        SDL_FRect d = {(float)(e->x - w / 2 - g->cam_x), (float)((e->top_anchor ? e->y : e->y - h) - g->cam_y), w, h};
        SDL_RenderTextureRotated(g->ren, g->fx_tex[e->sprite], NULL, &d, 0.0, NULL,
                                 e->dir == 2 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        if (g->debug_boxes && e->has_hitbox) {
            SDL_FRect hb = {(float)(e->x + e->hb_l - g->cam_x), (float)(e->y + e->hb_t - g->cam_y), (float)(e->hb_r - e->hb_l), (float)(e->hb_b - e->hb_t)};
            SDL_SetRenderDrawColor(g->ren, 255, 200, 0, 255);
            SDL_RenderRect(g->ren, &hb);
        }
    }
}

void osc_fight_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->stage) {
        SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->stage_w, (float)g->stage_h};
        SDL_RenderTexture(g->ren, g->stage, NULL, &dst);
    }
    /* stage animations (desenhaanimacoes, table 07c) */
    for (int i = 0; i < OSC_N_STAGE_ANIM; i++) {
        const osc_stage_anim_row *r = &osc_stage_anim[i];
        if (!(r->stage_mask & (1u << g->stage_id))) continue;
        if (g->ciclo < r->ciclo_lo || g->ciclo > r->ciclo_hi) continue;
        if (r->not_char && (g->p[1].char_id == r->not_char || g->p[2].char_id == r->not_char)) continue;
        SDL_Texture *t = r->is_effect ? (r->item < g->n_fx_tex ? g->fx_tex[r->item] : NULL)
                                      : (r->item < g->n_anim_tex ? g->anim_tex[r->item] : NULL);
        if (!t) continue;
        int w = r->is_effect ? g->fx_w[r->item] : g->anim_w[r->item];
        int h = r->is_effect ? g->fx_h[r->item] : g->anim_h[r->item];
        SDL_FRect d = {(float)(r->x - g->cam_x), (float)(r->y - g->cam_y), (float)w, (float)h};
        SDL_RenderTexture(g->ren, t, NULL, &d);
    }
    /* fighter shadows: Efeito-001 under the feet (desenhalutador) */
    for (int i = 1; i <= 2; i++) {
        if (g->n_fx_tex < 1) break;
        const osc_fighter *f = &g->p[i];
        SDL_FRect d = {(float)(f->x - 35 - g->cam_x), (float)(OSC_GROUND_Y - 12 - g->cam_y), (float)g->fx_w[0], (float)g->fx_h[0]};
        SDL_RenderTexture(g->ren, g->fx_tex[0], NULL, &d);
    }
    /* the player who attacked last is drawn on top; simple rule: P2 then P1 */
    draw_fighter(g, &g->p[2]);
    draw_fighter(g, &g->p[1]);
    osc_fx_render(g);
    draw_hud(g);
}

void osc_game_shutdown(osc_game *g)
{
    if (g->has_music) osc_wav_free(&g->music);
    for (int i = 0; i < g->n_sounds; i++) osc_wav_free(&g->sounds[i]);
    osc_text_free();
    osc_font_quit();
    osc_audio_close();
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    for (int i = 0; i < g->n_fx_tex; i++) if (g->fx_tex[i]) SDL_DestroyTexture(g->fx_tex[i]);
    if (g->stage) SDL_DestroyTexture(g->stage);
    for (int i = 0; i < 16; i++) if (g->abertura[i]) SDL_DestroyTexture(g->abertura[i]);
    for (int i = 0; i < 14; i++) if (g->icone[i]) SDL_DestroyTexture(g->icone[i]);
    for (int i = 0; i < 11; i++) if (g->final_tex[i]) SDL_DestroyTexture(g->final_tex[i]);
    memset(g, 0, sizeof *g);
}

void osc_play_snd(osc_game *g, int idx) { play_snd(g, idx); }

void osc_play_music(osc_game *g, int n)
{
    char rel[64], path[1200];
    if (g->has_music) { osc_wav_free(&g->music); g->has_music = false; }
    snprintf(rel, sizeof rel, "Musicas/Musica-%02d.wav", n);
    load_path(path, sizeof path, g->data_dir, rel);
    if (osc_audio_open() && osc_wav_load(path, &g->music)) { g->has_music = true; osc_audio_play(&g->music, true); }
}

/* ---- helpers for the ending scripts (endings.c) ---------------------------- */
bool osc_load_fighter(osc_game *g, int slot, int ch)
{
    char rel[64], path[1200];
    osc_image img;
    osc_strip_free(&g->strips[slot - 1]);
    snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", ch);
    load_path(path, sizeof path, g->data_dir, rel);
    if (!osc_bmp_load(path, true, &img)) return false;
    bool ok = osc_strip_build(g->ren, &img, &g->strips[slot - 1]);
    osc_image_free(&img);
    if (!ok) return false;
    osc_fighter *f = &g->p[slot];
    f->char_id = ch; f->strip = &g->strips[slot - 1];
    f->facing = slot == 1 ? 1 : 2; f->rot = 0; f->state = 1; f->anim_timer = 0;
    return true;
}

bool osc_load_stage(osc_game *g, int stage)
{
    char rel[64];
    if (g->stage) { SDL_DestroyTexture(g->stage); g->stage = NULL; }
    snprintf(rel, sizeof rel, "Cenarios/Cenario-%02d.bmp", stage);
    g->stage_id = stage;
    return load_image_tex(g, rel, false, &g->stage, &g->stage_w, &g->stage_h);
}

void osc_draw_fighter_at(osc_game *g, int slot, int x, int y, int state, int timer)
{
    osc_fighter *f = &g->p[slot];
    if (!f->strip || !f->strip->count) return;
    f->x = x; f->y = y; f->state = state; f->anim_timer = timer; f->in_air = false; f->rot = 0;
    f->sprite_h = k_sprite_h[f->char_id];
    select_frame(f);
    draw_fighter(g, f);
}

void osc_spawn_fx(osc_game *g, int type, int x, int y) { spawn_fx(g, type, 0, 1, x, y); }
void osc_fx_tick(osc_game *g) { update_fx(g); }
