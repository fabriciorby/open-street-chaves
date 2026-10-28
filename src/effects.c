/* Effects table (06): spawning, the per-tick update and the draw snapshot. */
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "effects.h"
#include "fight_internal.h"

/* ---- effects (06) -------------------------------------------------------------- */
static const osc_fx_type *fx_type(int t)
{
    for (int i = 0; i < OSC_N_FX_TYPES; i++)
        if (osc_fx_types[i].type == t) return &osc_fx_types[i];
    return NULL;
}

/* FUN_00476a04: take the first free slot and apply the per-type defaults. */
osc_effect *spawn_fx(osc_game *g, int type, int variant, int owner, double x, double y)
{
    const osc_fx_type *t = fx_type(type);
    if (!t) return NULL;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        osc_effect *e = &g->fx[i];
        if (e->type) continue;
        int stale = e->sprite;                              /* FUN_00476a04 does not write the sprite byte */
        memset(e, 0, sizeof *e);
        e->sprite = stale;
        e->type = type; e->x = x; e->y = y; e->owner = owner;
        e->dir = owner ? g->p[owner].facing : 1;
        e->speed = variant == 2 ? t->speedB : t->speedA;
        e->damage = t->damage;
        e->life = t->life;
        if (type == 24) e->life = 15 + osc_random(g, 15);
        /* FUN_00476a04 extras the table does not carry */
        switch (type) {
        case 7: case 9: case 12: case 13: case 15: case 18: case 19: case 22: case 26: case 29: case 33:
            e->knock = osc_random(g, 5) == 0; break;       /* 1 in 5 knocks down */
        case 31: case 32:
            e->knock = true; osc_random(g, 5); break;
        case 8: case 23: case 30: case 34: e->knock = true; break;
        case 20: e->knock = true; e->speed = 10 + osc_random(g, 2); break;
        default: break;
        }
        if (type == 30) e->reflect = true;                  /* bats projectiles back */
        if (type == 22) { e->vy = variant == 2 ? -17 : -13; e->sprite = 56; }
        /* chip on guard (+0x23): Round(damage / 4), / 15 for the big ones */
        e->chip = iround(e->damage / ((type == 8 || type == 31 || type == 32) ? 15.0 : 4.0));
        e->has_hitbox = t->flags & OSC_FX_HITBOX;
        e->bottom_anchor = t->flags & OSC_FX_TOP;
        e->gravity = t->flags & OSC_FX_GRAVITY;
        e->persist = t->flags & OSC_FX_PERSIST;
        e->vy = t->vy;
        e->c5 = t->c5;
        e->loop_at = t->loop_at;
        e->hb_l = t->l; e->hb_t = t->t; e->hb_r = t->r; e->hb_b = t->b;
        if (type == 21) {                                   /* debris */
            int r = osc_random(g, 5);
            e->vy = -23 - 5 * r;
            e->dir = 1 + osc_random(g, 2);
            e->speed = osc_random(g, 10);
            e->sprite = 53 + osc_random(g, 3);
        }
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
        if (r->phase < 0) {
            /* "any phase" rows: types 24, 25 and 30 are picked by the conditions below, type 32 only below phase 2 */
            if (e->type == 24 || e->type == 25 || e->type == 30) continue;
            if (e->type == 32 && e->phase >= 2) continue;
            s = r->sprite;
        } else if (r->phase == e->phase) s = r->sprite;
    }
    switch (e->type) {   /* the range conditions of desenhaefeitos 0x48f50e.. */
    case 24: s = e->phase < e->life - 10 ? 61 : 62; break;
    case 25: s = e->phase < 12 ? 63 : e->phase < 24 ? 64 : 65; break;
    case 30:   /* by the countdown c4 of c5 = 25, the later tests win */
        if (e->c5 - 10 <= e->c4) s = 77;
        if (e->c5 - 7 <= e->c4) s = 78;
        if (e->c5 - 4 <= e->c4) s = 79;
        break;
    default: break;
    }
    return s;
}

/* the sounds the frame selector assigns per (char, state, timer); table order = evaluation order (06c) */
void fx_sounds_for(osc_game *g, osc_fighter *f)
{
    for (int i = 0; i < OSC_N_FX_SOUNDS; i++) {
        const osc_fx_sound_row *r = &osc_fx_sounds[i];
        if (r->ch != f->char_id || r->state != f->state) continue;
        if (r->timer >= 0) { if (r->timer == f->anim_timer) g->snd_pending = r->snd; }
        else if (osc_random(g, r->chance) == 0) g->snd_pending = r->snd;
    }
}

/* Spawn the effects the frame selector produces for (char, state, timer): every call of
 * FUN_00476a04 in 0x47d21c..0x48dcbf with its guard (06e). */
void fx_spawns_for(osc_game *g, osc_fighter *f, int pidx)
{
    for (int i = 0; i < OSC_N_FX_SPAWNS; i++) {
        const osc_fx_spawn_row *r = &osc_fx_spawns[i];
        if (r->ch != f->char_id || r->state != f->state) continue;
        if (f->anim_timer < r->t_lo || f->anim_timer > r->t_hi) continue;
        if (r->frame >= 0 && f->frame != r->frame) continue;
        if (r->face && f->facing != r->face) continue;
        if (r->lvl && f->hit_level != r->lvl) continue;
        if (r->mod && f->anim_timer % r->mod) continue;
        for (int n = 0; n < r->count; n++) {
            double bx = (r->flags & OSC_SPAWN_CAM_X) ? g->cam_x : (r->flags & OSC_SPAWN_ROUND) ? iround(f->x) : f->x;
            double by = (r->flags & OSC_SPAWN_CAM_Y) ? g->cam_y : (r->flags & OSC_SPAWN_ROUND) ? iround(f->y) : f->y;
            double x = bx + r->dx, y = by + r->dy;
            if (r->rx) x += r->rx > 0 ? osc_random(g, r->rx) : -osc_random(g, -r->rx);
            if (r->ry) y += r->ry > 0 ? osc_random(g, r->ry) : -osc_random(g, -r->ry);
            spawn_fx(g, r->type, r->variant, r->owner ? pidx : 0, x, y);
        }
    }
}

/* Dona Neves' Phoenix Arrow / Shining Crystal Bit (types 31 / 32, 0x48f76f..0x48f8f1): the position is
 * recomputed from the owner every tick, x = owner.x +/- Round(70 * Cos(c4 / 3)) and
 * y = owner.y - 80 + Round(70 * Cos(c4 / 3)) (floats 0x49056c = 3.0, 0x490570 = 70.0, 0x490574 = 80.0).
 * Whenever c4 is a multiple of 10 the box is re-armed, c4 steps once more and the slot dies if the owner
 * is no longer in state 29 (0x1d). Not gated by the pauses. */
static void fx_orbit(osc_game *g, osc_effect *e)
{
    const osc_fighter *ow = &g->p[e->owner];
    int r = iround(70.0 * cos(e->c4 / 3.0));
    e->x = e->type == 31 ? r + ow->x : ow->x - r;
    e->y = (ow->y - 80.0) + r;
    if (e->c4 % 10 == 0) {
        e->has_hitbox = true;
        e->c4++;
        if (ow->state != 29) e->type = 0;
    }
}

/* ---- effects update: motion, collisions, drawing data (desenhaefeitos) ------------- */
/* The draw data of a slot, taken at the point of the slot loop where desenhaefeitos draws (0x48fb0e..): the
 * original draws the sprite right there, so an effect that is freed later in the same tick is still seen. */
static void fx_snapshot(osc_game *g, int i, const osc_effect *e)
{
    osc_fx_draw *d = &g->fx_draw[i];
    memset(d, 0, sizeof *d);
    d->sx = iround(e->x) - g->cam_x;
    d->sy = iround(e->y) - g->cam_y;
    if (g->flow.debug_boxes) {   /* 0x48fa5b: every live slot, whether armed or not */
        d->box = true;
        d->bx = (int)(e->x + e->hb_l - g->cam_x); d->by = (int)(e->y + e->hb_t - g->cam_y);
        d->bw = e->hb_r - e->hb_l; d->bh = e->hb_b - e->hb_t;
    }
    if (e->done && (g->ciclo & 1)) return;           /* blinking: drawn on even g_ciclo only */
    if (e->type != 5 && e->sprite < g->assets.n_fx_tex) { d->kind = 1; d->sprite = e->sprite; d->dir = e->dir; d->bottom = e->bottom_anchor; }
    if (e->type == 5) d->kind = 2;                   /* the floating "VITALIDADE" text of a pickup */
}

/* ---- effects update: desenhaefeitos 0x48edfc, one pass over the 80 slots in table order ------------ */
void update_fx(osc_game *g)
{
    bool frozen = g->hit_stop || g->super_pause;       /* DAT_004b57f9 / DAT_004b57f8 */
    memset(g->fx_draw, 0, sizeof g->fx_draw);
    for (int i = 0; i < OSC_MAX_FX; i++) {
        osc_effect *e = &g->fx[i];
        if (!e->type) continue;
        /* (phase) -> sprite table, 0x48ee4e..0x48f6fb: runs first, not gated by the pauses */
        e->sprite = fx_sprite(e);
        /* blink flags: pickups from life - 20, the popup text from life - 10, the geyser at phase 5 */
        if ((e->type == 3 || e->type == 4) && e->life - 20 <= e->phase) e->done = true;
        if (e->type == 5 && e->life - 10 <= e->phase) e->done = true;
        if (e->type == 8 && e->phase == 5) e->done = true;
        if (e->type == 2 && !frozen) e->y -= 1.0;
        /* the looping types rewind when they reach their end phase (the sprite keeps the last one for this tick) */
        if (e->loop_at && e->phase == e->loop_at) e->phase = 0;
        /* Power Geyser: the pillar's box grows with the frames (FUN_004769c4 sets the box l, t, r, b), and at
         * phases 6 and 12 the box is re-armed and the phase steps once more (0x48f6fb..0x48f754, not gated) */
        if (e->type == 8) {
            if (e->phase == 2) { e->hb_l = -45; e->hb_t = -150; e->hb_r = 45; e->hb_b = 0; }
            if (e->phase == 5) { e->hb_l = -45; e->hb_t = -220; e->hb_r = 45; e->hb_b = 0; }
            if (e->phase == 6 || e->phase == 12) { e->has_hitbox = true; e->phase++; }
        }
        /* Psycho Field burst: the box is re-armed at c4 = 3, 6, 9 and c4 steps once more */
        if (e->type == 28 && (e->c4 == 3 || e->c4 == 6 || e->c4 == 9)) { e->has_hitbox = true; e->c4++; }
        if (e->type == 31 || e->type == 32) fx_orbit(g, e);                  /* 0x48f76f, not gated */
        if (e->type == 30 && g->p[e->owner].state != 26) e->type = 0;       /* 0x48f8f4, not gated */
        if (e->type == 34 && e->phase > 13) e->has_hitbox = false;                          /* 0x48f911, not gated */
        if (e->type == 24) { e->x += iround(2.0 * cos((double)e->phase)); e->y -= 1.0; }   /* 0x48f920, not gated; 0x49057c = 2.0 */
        if (e->gravity && !frozen) {                                                         /* 0x48f95d */
            e->y += e->vy; e->vy += 4;
            if (e->y > OSC_GROUND_Y - 5) {
                e->y = OSC_GROUND_Y - 5; e->speed = 0; e->vy = 0;
                if (e->type == 22) { e->done = true; e->has_hitbox = false; }
            }
        }
        if (!frozen) {                                                                       /* 0x48f9cb */
            if (e->dir == 1) e->x += e->speed;
            if (e->dir == 2) e->x -= e->speed;
            if (e->type == 19) e->y += e->speed;
            if (e->type == 20) e->y -= 7 + osc_random(g, 13);
        }
        fx_snapshot(g, i, e);                                                                /* 0x48fa5b..0x48fc8d */
        if (e->type == 5 && !g->super_pause) e->y -= 5.0;                                    /* 0x48fd90: not by the hit stop */
        /* effect vs effect (0x48fdb2): a reflector takes the other one over, otherwise
         * effects of different owners cancel, each leaving a spark unless it persists */
        for (int j = 0; j < OSC_MAX_FX && e->type; j++) {
            osc_effect *o = &g->fx[j];
            if (!(e->x + e->hb_r > o->x + o->hb_l && e->x + e->hb_l < o->x + o->hb_r &&
                  e->y + e->hb_b > o->y + o->hb_t && e->y + e->hb_t < o->y + o->hb_b)) continue;
            if (!o->has_hitbox || !e->has_hitbox || !o->type || o->damage <= 0 || e->damage <= 0 || j == i) continue;
            if (e->reflect) { o->owner = e->owner; o->dir = g->p[e->owner].facing; }
            if (o->reflect) { e->owner = o->owner; e->dir = g->p[o->owner].facing; }
            if (o->owner == e->owner) continue;
            if (!e->persist) { spawn_fx(g, 1, 0, 0, e->x, e->y); e->type = 0; }
            if (!o->persist) { spawn_fx(g, 1, 0, 0, o->x, o->y); o->type = 0; }
        }
        /* effect vs player (0x48fee7): the slot is processed to its end even when it was freed above */
        for (int p = 1; p <= 2; p++) {
            osc_fighter *v = &g->p[p], *a = opp(g, v);
            if (!(e->x + e->hb_r > v->x + v->hurt_l && e->x + e->hb_l < v->x + v->hurt_r &&
                  e->y + e->hb_b > v->y - v->sprite_h + v->hurt_t && e->y + e->hb_t < v->y - v->sprite_h + v->hurt_b))
                continue;
            bool live = !g->challenge && !g->no_push;
            if (v->blocking && e->has_hitbox && e->damage > 0 && e->owner != p) {
                v->anim_timer = 1;
                g->snd_pending = 10;
                spawn_fx(g, 14, 0, 0, e->x, e->y);
                if (v->facing == 1) move_x(g, v, -10.0);
                if (v->facing == 2) move_x(g, v, 10.0);
                if (live) { v->health -= e->chip * g->settings.damage_level; v->trail_delay = 20; }
                if (v->health <= 0.0) { g->snd_pending = 9; set_move(g, v, 10); }
                if (!e->persist) e->type = 0;
                e->has_hitbox = false;
                if (v->health > 0.0) g->hit_stop = 2;
            }
            if (e->has_hitbox && e->damage > 0 && !v->invuln && e->owner != p && !v->blocking) {
                if (v->x < a->x) v->facing = 1;
                if (v->x >= a->x) v->facing = 2;
                g->snd_pending = 9;
                spawn_fx(g, 1, 0, 0, e->x, e->y);
                if (g->settings.cfg_flash && osc_random(g, 3) == 0) g->flash = 4;
                if ((g->settings.cfg_blood && osc_random(g, 3) == 0) || g->settings.unlocks.satanas) spawn_fx(g, 10, 0, 0, v->x, v->y - (v->sprite_h - 40));
                if (a->human && v->health > 0.0) a->score += 10;
                if (a->combo_timer <= 35) a->hits = 0;
                a->hits++;
                a->combo_timer = 50;
                if (a->hits > a->max_combo) a->max_combo = a->hits;
                if (live) {
                    v->health -= e->damage * g->settings.damage_level;
                    if (v->meter < OSC_METER_MAX) v->meter += 1.0 * k_meter_unit;   /* the one hit gains */
                    v->trail_delay = 20;
                }
                if (e->knock || v->vy != 0 || v->health <= 0.0) set_move(g, v, 10);
                set_move(g, v, 9);
                v->hitstun = 8;
                if (!g->first_hit) {
                    a->msg_index = 1; a->msg_timer = 50;   /* "Primeiro Ataque" */
                    if (a->human) a->score += 300;
                    g->first_hit = true;
                }
                a->on_top = true; v->on_top = false;
                if (v->health > 0.0) g->hit_stop = 5;
                if (e->type == 8) { v->hitstun = 10; v->vy = -60; }
                if (e->type == 20) { v->vy = -20; v->invuln = false; v->hitstun = 6; }
                if (e->type == 8 || e->type == 20) {
                    v->state = 19;
                    if (v->facing == 1) v->rot_step = -20;
                    if (v->facing == 2) v->rot_step = 20;
                }
                if (!e->persist) e->type = 0;
                e->has_hitbox = false;
                select_frame(g, v);
            }
            /* pickups dropped by the passer-by: 20 or 30 health (0x490308) */
            if ((e->type == 3 || e->type == 4) && e->phase > 13 && v->health > 0.0 && !g->no_push) {
                if (!g->challenge) v->health += (e->type == 3 ? 20.0 : 30.0) * g->settings.damage_level;
                g->snd_pending = 11;
                spawn_fx(g, 5, 0, 0, e->x, e->y);
                e->type = 0;
            }
        }
        if (!frozen) {
            if (e->life != 99) e->phase++;
            if (e->life != 99 && e->phase > e->life) e->type = 0;
            /* countdown (desenhaefeitos 0x48edfc, decomp 889-915): the slot is freed at c4 >= c5;
             * type 27 arms its box 2 ticks early and bursts into three type-28 effects ("A") */
            if (e->c5) {
                e->c4++;
                if (e->type == 27) {
                    if (e->c4 >= e->c5 - 2) e->has_hitbox = true;
                    if (e->c4 >= e->c5) {
                        double bx = e->x, by = e->y;
                        spawn_fx(g, 28, 1, e->owner, bx - 30, by + 20);
                        spawn_fx(g, 28, 1, e->owner, bx + 30, by + 20);
                        spawn_fx(g, 28, 1, e->owner, bx, by - 40);
                    }
                }
                if (e->c4 >= e->c5) e->type = 0;
            }
        }
        /* the cull at the end of the slot (desenhaefeitos): a screen margin of 50 px (not gated by the
         * pauses); the Chaos Flare (19) also bursts when its box reaches the floor */
        if (e->x < g->cam_x - 50) e->type = 0;
        if (e->x > g->cam_x + 450) e->type = 0;
        if (e->y < g->cam_y - 50) e->type = 0;
        if (e->type == 19 && e->hb_b + e->y > OSC_GROUND_Y) {
            e->type = 0;
            spawn_fx(g, 1, 0, 0, e->x, e->y);
        }
    }
}

void osc_spawn_fx(osc_game *g, int type, int x, int y) { spawn_fx(g, type, 0, 0, x, y); }   /* endings: owner 0 */
void osc_fx_tick(osc_game *g) { update_fx(g); }
