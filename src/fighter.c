/* One fighter: moves (setMove), walking with body push, the per-tick counters and
 * clocks, the physics of jumps and landings, the logic half of desenhalutador. */
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "fight_internal.h"

/* ---- per-character constants (02, 03) ------------------------------------ */
const int k_sprite_h[16] = {0, 156, 162, 147, 149, 144, 190, 160, 160, 150, 155, 155, 165, 155, 160, 165};
const double k_speed[16] = {0, 8, 8, 8.5, 8, 8, 7.5, 7.5, 8, 8.5, 7.5, 8, 8, 7.5, 7.5, 8};
const double k_meter_unit = 2.0;
const int k_gravity = 4;

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
    if (r->needs_fx && !g->settings.unlocks.bruxa) {        /* 0x476454: one projectile of this type per owner */
        int me = (f == &g->fight.p[1]) ? 1 : 2;
        for (int i = 0; i < OSC_MAX_FX; i++)
            if (g->fight.fx[i].type == r->needs_fx && g->fight.fx[i].owner == me) return;
    }
    if ((r->flags & OSC_MOVE_ONGROUND) && f->vy != 0) return;
    if ((r->flags & OSC_MOVE_NOT_INAIR) && f->in_air) return;
    if (r->flags & OSC_MOVE_SUPER) {
        if (!(f->health <= 15.0 || f->stocks > 0)) return;
        if (f->stocks == 3) f->meter = 0;
        f->stocks--;
        g->fight.super_pause = 22;
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
    if (r->snd >= 0) g->fight.snd_pending = r->snd;   /* g_sndIndexA: played at the start of the next tick */
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
    if (overlap && f->state != 23 && f->state != 20 && !g->fight.no_push) {
        if (f->x <= o->x) {
            o->x += 5.0;
            if (o->x > g->fight.cam_x + 370) o->x = g->fight.cam_x + 370;
            f->x = (o->hurt_l + o->x) - f->hurt_r - 5.0;
        } else {
            o->x -= 5.0;
            if (o->x < g->fight.cam_x + 30) o->x = g->fight.cam_x + 30;
            f->x = (o->hurt_r + o->x) - f->hurt_l + 5.0;
        }
    }
    if (f->health > 0.0) {
        if (f->x < g->fight.cam_x + 30) f->x = g->fight.cam_x + 30;
        if (f->x > g->fight.cam_x + 370) f->x = g->fight.cam_x + 370;
    }
}

/* ---- input-side counters (sequences, mash, combo) ----------------------------- */
void counters(osc_game *g, osc_fighter *f)
{
    bool paused = g->fight.hit_stop || g->fight.super_pause;
    if (f->state == 2 && !paused) { f->anim_timer++; if (f->anim_timer > 16) f->anim_timer = 0; }
    if (f->seq_timer > 0 && !paused) { f->seq_timer--; if (f->seq_timer == 0) f->seq_state = 0; }
    if (f->mash_p) f->mash_p--; if (f->mash_p > 15) f->mash_p = 15;
    if (f->mash_k) f->mash_k--; if (f->mash_k > 15) f->mash_k = 15;
    if (f->combo_timer && !paused) { f->combo_timer--; if (!f->combo_timer) f->hits = 0; }
}

/* ---- move clock (0x4a9459): a finished move falls back to standing or falling ---- */
void move_clock(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    if (f->anim_dur != 99 && !g->fight.super_pause && !g->fight.hit_stop) f->anim_timer++;
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
void physics(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    if (f->in_air && !g->fight.super_pause && !g->fight.hit_stop) {
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
                g->fight.snd_pending = 12;
                if (f->char_id == 7 || f->char_id == 10) {   /* the heavy ones shake the street */
                    g->fight.shake = 20;
                    dust(g, f, 20, 30);
                }
                dust(g, f, 5, 20);
            }
        }
        if (f->state == 20) {                       /* bouncing on the ground */
            if (f->facing == 1) move_x(g, f, -f->hitstun);
            if (f->facing == 2) move_x(g, f, f->hitstun);
            if ((g->settings.cfg_blood && osc_random(g, 4) == 0 && f->anim_timer == 5) || g->settings.unlocks.satanas)
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
        if (f->x < g->fight.cam_x + 30) f->x = g->fight.cam_x + 30;
        if (f->x > g->fight.cam_x + 370) f->x = g->fight.cam_x + 370;
    }
}

/* ---- desenhalutador's logic part (0x490bb8): facing, frame, spin ---------------- */
void fighter_frame(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    bool paused = g->fight.hit_stop || g->fight.super_pause;
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

void init_fighter(osc_fighter *f, int ch, int p, const osc_strip *strip) { osc_reset_fighter(f, ch, p, strip); }

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
