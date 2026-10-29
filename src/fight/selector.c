/* Frame selector (0x47d21c): which frame, hit boxes, throws and special-move
 * motion each (character, state, timer) shows. */
#include <stdlib.h>
#include <string.h>

#include "render/canvas.h"
#include "fight/fight_internal.h"

/* ---- frame selector (0x47d21c) -------------------------------------------------------------
 * One function in the original: the frame stores (06f), the field stores (06g and the code below),
 * the effect spawns (06e), the sounds (06c), then the hurt and hit boxes (05a). */
static bool sel_cmp(int op, double a, double b)
{
    switch (op) {
    case 1: return a == b;
    case 2: return a >= b;
    case 3: return a > b;
    case 4: return a < b;
    case 5: return a <= b;
    }
    return true;
}

static bool sel_extra(const osc_fighter *f, const osc_fighter *o, int xf, int xo, int xv)
{
    if (!xf) return true;
    double a = xf == 1 ? f->hits : xf == 2 ? f->combo_timer : xf == 3 ? o->state : f->y;
    return sel_cmp(xo, a, xv);
}

static void sel_apply(osc_fighter *t, int field, int val)
{
    switch (field) {
    case OSC_SF_KNOCKDOWN: t->knockdown = val != 0; break;
    case OSC_SF_INAIR: t->in_air = val != 0; break;
    case OSC_SF_JUMPDIR: t->jump_dir = val; break;
    case OSC_SF_HITSTUN: t->hitstun = val; break;
    case OSC_SF_TIMER: t->anim_timer = val; break;
    case OSC_SF_VY: t->vy = val; break;
    case OSC_SF_INVULN: t->invuln = val != 0; break;
    }
}

/* the victim of a throw super, flailing and pinned in front of the thrower (0x47f270..0x47f3b6; the same
 * code in the four throws, only the distance differs: dist + Random(rn) * mul) */
static void throw_hold(osc_game *g, osc_fighter *f, osc_fighter *o, double dist, int rn, int mul)
{
    o->frame = 26 + osc_random(g, 2);
    o->anim_timer = 0; o->anim_dur = 5; o->vy = 0;
    o->y += 10.0;
    if (o->y > OSC_GROUND_Y) o->y = OSC_GROUND_Y;
    if (f->facing == 1) o->x = f->x + dist + osc_random(g, rn) * mul;
    if (f->facing == 2) o->x = f->x - dist - osc_random(g, rn) * mul;
}

/* the victim spins away at the end of a throw super: rotStep -spin / +spin by the victim's side */
static void throw_spin(osc_fighter *o, int spin)
{
    if (o->facing == 1) o->rot_step = -spin;
    if (o->facing == 2) o->rot_step = spin;
}

/* flash of the screen (g_shakeHit 0x4b449b) with config line 7 on */
static void throw_flash(osc_game *g, int chance)
{
    if (g->settings.cfg_flash && osc_random(g, chance) == 0) g->fight.flash = 3;
}

/* Seu Madruga 28/29, 31, Chiquinha 30 and Sr. Barriga 30: the thrower grabs and shakes the opponent,
 * the last tick (timer > dur - 2) deals the damage and throws him (0x47f13c..0x47fa57, 0x480890..0x480dc0,
 * 0x484dd7..0x48522d). The hit sounds and sparks are in 06c / 06e. */
static void sel_throws(osc_game *g, osc_fighter *f, osc_fighter *o)
{
    int ch = f->char_id, st = f->state;
    bool last = f->anim_timer > f->anim_dur - 2;
    if (ch == 2 && (st == 28 || st == 29)) {
        /* a new random frame 41..43 every tick, never the same twice */
        int fr;
        do fr = 41 + osc_random(g, 3); while (fr == f->frame);
        f->frame = fr;
    }
    if (ch == 2 && st == 29) {
        throw_flash(g, 6);
        f->hitstun = 0;
        throw_hold(g, f, o, 40.0, 2, 8);
        f->hits++; f->combo_timer = 20;
        if (last) {
            f->damage = 20;
            o->health -= f->damage * g->settings.damage_level;
            set_move(g, o, 10);
            o->hitstun = 30;
        }
    }
    if (ch == 2 && st == 31) {
        f->hitstun = 0;
        f->frame = 1 + osc_random(g, 45);
        throw_flash(g, 5);
        throw_hold(g, f, o, 40.0, 2, 8);
        f->hits += 2; f->combo_timer = 43;
        if (osc_random(g, 30) == 0 && o->health > 0.0) g->fight.hit_stop = 5;
        if (last) {
            f->damage = 47;
            o->health -= f->damage * g->settings.damage_level;
            f->state = 1; f->frame = 38;
            g->fight.hit_stop = 5;
            set_move(g, f, 19);
            set_move(g, o, 10);
            f->attacking = false;
            o->hitstun = 28; o->vy = -30;
            throw_spin(o, 25);
        }
    }
    if (ch == 3 && st == 30) {
        f->hitstun = 0;
        throw_flash(g, 5);
        throw_hold(g, f, o, 65.0, 3, 5);
        f->hits++; f->combo_timer = 43;
        if (osc_random(g, 20) == 0 && o->health > 0.0) g->fight.hit_stop = 5;
        if (last) {
            f->damage = 36;
            o->health -= f->damage * g->settings.damage_level;
            f->vy = -28; f->in_air = true;
            f->state = 12; f->anim_timer = 0; f->anim_dur = 99;
            set_move(g, o, 10);
            o->hitstun = 30;
            throw_spin(o, 15);
        }
    }
    if (ch == 7 && st == 30) {
        f->hitstun = 0;
        f->frame = 1 + osc_random(g, 46);
        throw_flash(g, 5);
        if (osc_random(g, 3) == 0) g->fight.hit_stop = 3 + osc_random(g, 3);
        throw_hold(g, f, o, 40.0, 2, 8);
        f->hits++; f->combo_timer = 43;
        if (last) {
            f->damage = 36;
            o->health -= f->damage * g->settings.damage_level;
            f->state = 1; f->frame = 12;
            if (o->health > 0.0) g->fight.hit_stop = 5;
            f->anim_timer = 0; f->vy = -48; f->in_air = true;
            set_move(g, f, 21);
            set_move(g, o, 10);
            o->hitstun = 30; o->vy = -35;
            throw_spin(o, 25);
        }
    }
}

/* The other stores of the selector that are not table rows (the motion of the special moves,
 * x / y / vy / inAir, is special_motion, 06d). */
static void sel_code(osc_game *g, osc_fighter *f, osc_fighter *o)
{
    int ch = f->char_id, st = f->state, t = f->anim_timer;
    /* 0x47db6a: a knocked-out fighter lies still */
    if (st == 20 && f->vy == 0 && f->health <= 0.0) { f->anim_timer = 0; f->y = OSC_GROUND_Y; }
    /* 0x47e540: Chaves' state 29 shakes the street once */
    if (ch == 1 && st == 29 && t == 9) g->fight.shake = 20;
    sel_throws(g, f, o);
}

static void sel_frames(osc_game *g, osc_fighter *f)
{
    for (int i = 0; i < OSC_N_ANIM; i++) {
        const osc_anim_row *r = &osc_anim[i];
        if (r->state != f->state || (r->ch && r->ch != f->char_id) || (r->ch_ne && r->ch_ne == f->char_id)) continue;
        if (f->anim_timer < r->t_lo || f->anim_timer > r->t_hi) continue;
        if (r->lvl && f->hit_level != r->lvl) continue;
        if (r->mod && (((r->psrc ? g->fight.ciclo : f->anim_timer) % r->mod == 0) == (r->meq != 0))) continue;
        if (r->cur > 0 && f->frame != r->cur) continue;
        if (r->cur < 0 && f->frame == -r->cur) continue;
        if (r->chance && osc_random(g, r->chance) != 0) continue;
        f->frame = r->frame + (r->kind ? osc_random(g, r->rnd) : 0);
    }
}

static void sel_stores(osc_game *g, osc_fighter *f, osc_fighter *o)
{
    for (int i = 0; i < OSC_N_SEL_STORE; i++) {
        const osc_sel_store_row *r = &osc_sel_store[i];
        if (r->ch && r->ch != f->char_id) continue;
        if (r->state != f->state || f->anim_timer < r->t_lo || f->anim_timer > r->t_hi) continue;
        if (r->face && f->facing != r->face) continue;
        if (r->lvl && f->hit_level != r->lvl) continue;
        if (r->cur && f->frame != r->cur) continue;
        if (!sel_extra(f, o, r->xf1, r->xo1, r->xv1) || !sel_extra(f, o, r->xf2, r->xo2, r->xv2) ||
            !sel_extra(f, o, r->xf3, r->xo3, r->xv3)) continue;
        sel_apply(r->who ? o : f, r->field, r->val);
    }
    sel_code(g, f, o);
}

static void sel_boxes(osc_fighter *f)
{
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

/* 0x489db8: Furtado's run turns at random after the edge test (the walk itself is special_motion) */
static void sel_run_turn(osc_game *g, osc_fighter *f)
{
    if (f->char_id == 12 && f->state == 23 && f->anim_timer % 8 == 0 && osc_random(g, 2) == 0) {
        if (f->facing == 1) f->facing = 2;
        else if (f->facing == 2) f->facing = 1;
    }
}

void select_frame(osc_game *g, osc_fighter *f)
{
    sel_frames(g, f);
    sel_stores(g, f, opp(g, f));
    special_motion(g, f);
    sel_run_turn(g, f);
    fx_spawns_for(g, f, f == &g->fight.p[1] ? 1 : 2);
    fx_sounds_for(g, f);
    sel_boxes(f);
}

/* the pose alone (select and ending screens): frame and boxes, no side effects on the fight */
void select_pose(osc_game *g, osc_fighter *f)
{
    sel_frames(g, f);
    sel_boxes(f);
}

/* ---- motion of the special moves (06d-special-motion) -------------------------------
 * The frame selector (0x47d21c) walks the fighter with sub_47648c and writes x / y / vy / inAir
 * directly in the special states. The walks are the osc_motion table; the rest is hand-coded
 * here. Phase 0 runs before the walks (rules whose y change precedes the walk calls in the
 * original), phase 1 after them. */
static void jump_off(osc_fighter *f, int vy) { f->in_air = true; f->vy = vy; }

static void motion_rules(osc_game *g, osc_fighter *f, int phase)
{
    const int T = f->anim_timer, S = f->hit_level, fr = f->frame, st = f->state;
    const int floor_y = OSC_GROUND_Y;
    if (st < 23) return;
    switch (f->char_id) {
    case 1:
        if (st == 27 && phase == 0) f->y -= 10;
        if (st == 27 && phase == 0 && T == 20) f->in_air = true;
        break;
    case 2:
        if (st == 26 && phase == 0) {
            if ((T == 11 && S == 1) || (T == 15 && S == 2)) jump_off(f, -6);
            if (fr == 39) f->y -= S == 1 ? 11 : S == 2 ? 12 : 0;
            if (fr == 40) f->in_air = true;
        }
        if (st == 27) {
            if (phase == 0) {
                if (T == 0) { f->vy = 0; f->jump_dir = 0; }
                if (T > 2) f->y += S == 1 ? 20 : S == 2 ? 13 : 0;
            } else if (T > 2 && f->y > 270) f->vy = 2;
        }
        break;
    case 3:
        if (st == 30 && phase == 0) f->y -= 3;
        break;
    case 6:
        if (phase == 0) break;
        if (st == 27) {
            if (T > 2 && T < 13) f->y -= S == 1 ? 10 : S == 2 ? 13 : 0;
            if (T == 13) jump_off(f, S == 1 ? -3 : -6);
        }
        if (st == 28) {
            if (T > 1 && T < 6) f->y -= 15;
            if (T == 6) f->in_air = true;
        }
        if (st == 29) {
            if (T == 8) { f->in_air = false; f->y = floor_y; }
            if (T > 8 && T < 26) f->y -= 10;
            if (T == 26) jump_off(f, -5);
        }
        break;
    case 8:
        if (phase == 1) break;
        if (st == 23) {
            f->y += (T % 2 == 0) ? -4 : 4;
            if (f->y > floor_y) f->y = floor_y;
        }
        if (st == 26 && (T == 0 || T == 8)) { f->vy = 0; f->in_air = false; }
        if (st == 27 && T == 2) { f->vy = S == 1 ? -25 : S == 2 ? -32 : f->vy; f->jump_dir = 0; f->in_air = true; }
        if (st == 28 && T == 0) { f->vy = 0; f->in_air = false; }
        if (st == 28 && T == 8) {
            double d = S == 1 ? 80.0 : S == 2 ? 150.0 : 0.0;
            f->x += f->facing == 1 ? d : -d;
            f->y = 50.0; f->vy = 0; f->in_air = true; f->jump_dir = 0;
        }
        break;
    case 9:
        if (st == 26) {
            if (phase == 1 && T < 8) f->y -= S == 1 ? 9 : S == 2 ? 15 : 0;
            if (phase == 1 && T == 8) jump_off(f, -10);
        }
        if (st == 28) {
            if (phase == 0 && T == 0) f->vy = 0;
            if (phase == 1 && T > 2) {
                f->y += 15;
                if (f->y >= floor_y) { f->state = 1; f->anim_timer = 0; f->attacking = false; f->y = floor_y; }
            }
        }
        if (st == 29 && phase == 1) {
            if (T > 11 && T < 19) f->y -= 18;
            if (T == 22) jump_off(f, -14);
        }
        break;
    case 10:
        if (st == 29 && phase == 0) {
            if (T == 15) jump_off(f, -35);
            if (T == 20) f->vy = 10;
        }
        break;
    case 11:
        if (st == 28 && phase == 1) {
            f->y -= S == 1 ? 6 : S == 2 ? 8 : 0;
            if (T == 11) f->vy = -10;
        }
        break;
    case 12:
        if (st == 23 && phase == 0) {
            f->x += f->facing == 1 ? 12.0 : f->facing == 2 ? -12.0 : 0.0;
            if (f->x - g->fight.cam_x <= 100.0) f->facing = 1;
            if (f->x - g->fight.cam_x >= 300.0) f->facing = 2;
        }
        if (st == 26 && phase == 0) {
            if (((T == 8 && S == 1) || (T == 10 && S == 2))) jump_off(f, -6);
            if (fr == 39) f->y -= S == 1 ? 15 : S == 2 ? 17 : 0;
            if (fr == 40) f->in_air = true;
        }
        if (st == 27 && phase == 0) { f->y -= 11; f->y += T; }
        if (st == 28 && phase == 0) {
            if (T > 12 && T < 25) f->y -= 12;
            if (T == 25) { f->in_air = true; f->jump_dir = 0; f->vy = -8; }
        }
        break;
    case 13:
        if (st == 23 && phase == 0) {
            f->y -= iround(cos(T / 3.0) * 6.0);
            if (f->y > floor_y - 10) f->y = floor_y - 10;
        }
        if (st == 28 && phase == 0 && T == 10) {
            static const int cx[5] = {0, 0x50, 0xa0, 0xf0, 0x140};
            if (S >= 1 && S <= 4) f->x = g->fight.cam_x + cx[S];
            f->y = g->fight.cam_y + 0x96;
            f->in_air = true; f->vy = 0;
        }
        break;
    case 14:
        if (phase == 0) break;
        if (st == 27) f->y += 8;
        if (st == 28) {
            if (T > 3 && T < 16) f->y -= S == 1 ? 6 : S == 2 ? 9 : 0;
            if (T == 17) { f->in_air = true; f->jump_dir = 0; f->vy = -2; }
        }
        break;
    case 15:
        if (st == 27 && phase == 0) {
            if (T >= 0 && T < 9) f->y -= S == 1 ? 6 : S == 2 ? 10 : 0;
            if (T == 9) jump_off(f, -5);
        }
        if (st == 28 && phase == 1) f->y += (signed char)(T - 8);
        if (st == 29 && phase == 1) f->y += (signed char)(T - 5);
        break;
    }
}

void special_motion(osc_game *g, osc_fighter *f)
{
    if (f->state < 23 && !(f->char_id == 13 && f->state == 11)) return;
    motion_rules(g, f, 0);
    for (int i = 0; i < OSC_N_MOTION; i++) {
        const osc_motion_row *r = &osc_motion[i];
        if (r->ch != f->char_id || r->state != f->state) continue;
        if (r->variant && f->hit_level != r->variant) continue;
        if (f->anim_timer < r->tmin || f->anim_timer > r->tmax) continue;
        if (r->frame >= 0 && f->frame != r->frame) continue;
        move_x(g, f, f->facing == 1 ? r->dx : -r->dx);
    }
    if (f->char_id == 15 && (f->state == 28 || f->state == 29)) {   /* computed walks (signed byte k) */
        int k = f->state == 28 ? (signed char)(18 - f->anim_timer) : (signed char)(22 - f->anim_timer);
        if (f->state == 29 && k < 1) k = 1;
        double dx = k;
        if (f->state == 28 && f->hit_level == 2) dx = k * 1.3;
        if (f->state == 28 && f->hit_level != 1 && f->hit_level != 2) dx = 0;
        if (dx != 0.0) move_x(g, f, f->facing == 1 ? dx : -dx);
    }
    motion_rules(g, f, 1);
}
