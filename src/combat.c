/* Guarding and body hits (defendergolpe 0x494bd8, DXTimer1Timer 0x4a85ec..0x4a9442). */
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "fight_internal.h"

/* ---- defendergolpe (0x494bd8): guard for humans, guard and jump-over for the CPU -- */
void defend(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    int me = (f == &g->fight.p[1]) ? 1 : 2;
    if (f->human) {
        /* holding back guards against a body attack or any hostile projectile */
        if (held(f, "TRAS") && !held(f, "BAIXO") && o->attacking) set_move(g, f, 12);
        if (held(f, "TRAS") && held(f, "BAIXO") && o->attacking) set_move(g, f, 13);
        for (int i = 0; i < OSC_MAX_FX; i++) {
            const osc_effect *e = &g->fight.fx[i];
            if (!e->type || !e->has_hitbox || e->owner == me) continue;
            if (held(f, "TRAS") && !held(f, "BAIXO")) set_move(g, f, 12);
            if (held(f, "TRAS") && held(f, "BAIXO")) set_move(g, f, 13);
        }
        return;
    }
    int dist = abs(iround(g->fight.p[1].x) - iround(g->fight.p[2].x));
    bool proj = false, pers = false;
    int pd = 0;
    double ey = 0;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_effect *e = &g->fight.fx[i];
        if (!e->has_hitbox || e->owner == me || !e->type) continue;
        proj = true;
        pd = abs(iround(f->x) - iround(e->x));
        if (e->persist) pers = true;
        ey = e->y;
    }
    double kb = (8 - g->settings.level) * 0.9 + 1.2 - 0.1 * g->fight.fight_no;
    if (kb < 0.0) kb = 0.0;

    if (o->attacking && dist <= 160) {
        if (o->y <= OSC_GROUND_Y - 30 && chance(g, 0.5, kb)) set_move(g, f, 12);
        if (o->y >= OSC_GROUND_Y - 30 && chance(g, 0.5, kb)) set_move(g, f, 13);
        if (f->state == 21) set_move(g, f, 12);   /* keep the guard up */
        if (f->state == 22) set_move(g, f, 13);
    }
    if (f->state == 1 || f->state == 2) {
        bool jump = false;
        if (proj && pd <= 130 && f->y - 120.0 < ey) {
            int n = iround(5.0 * kb);
            jump = osc_random(g, n < 1 ? 1 : n) <= 3;
        }
        if (!jump && !proj && chance(g, 45, kb) && !f->in_air && !pers) jump = true;
        if (jump) {
            f->vy = -32; f->in_air = true; f->state = 12;
            f->anim_timer = 0; f->anim_dur = 99; f->blocking = false;
            if (f->facing == 1) f->jump_dir = 3;
            if (f->facing == 2) f->jump_dir = 2;
            if (osc_random(g, 8) == 0) f->jump_dir = 1;
        }
    }
    if (proj && pd <= 120 && f->y - 120.0 < ey && chance(g, 0.4, kb)) set_move(g, f, 12);
    if (proj && pd <= 100 && f->y - 120.0 >= ey && chance(g, 2.5, kb)) set_move(g, f, 13);
}

/* ---- body hits (DXTimer1Timer 0x4a85ec..0x4a9442), attacker a = P1 then P2 ---- */
void melee_step(osc_game *g, osc_fighter *a)
{
    osc_fighter *v = opp(g, a);
    /* states 17/18 slide forward with a trail of dust */
    if ((a->state == 17 || a->state == 18) && !g->fight.super_pause && !g->fight.hit_stop) {
        if (a->facing == 1) move_x(g, a, -a->hitstun);
        if (a->facing == 2) move_x(g, a, a->hitstun);
        if (osc_random(g, 2) == 0) {
            int rx = osc_random(g, 40), ry = osc_random(g, 10);
            spawn_fx(g, 2, 0, 0, a->x - 20 + rx, a->y + ry);
        }
    }
    int al = iround(a->x) + a->hit_l, at = iround(a->y) - a->sprite_h + a->hit_t;
    int ar = iround(a->x) + a->hit_r, ab = iround(a->y) - a->sprite_h + a->hit_b;
    int vl = iround(v->x) + v->hurt_l, vt = iround(v->y) - v->sprite_h + v->hurt_t;
    int vr = iround(v->x) + v->hurt_r, vb = iround(v->y) - v->sprite_h + v->hurt_b;
    if (!a->attacking || v->invuln || !(al < vr && ar > vl && at < vb && ab > vt) || g->fight.hit_stop || g->fight.super_pause)
        return;

    /* low attacks open a standing guard, jump attacks a crouching one */
    if ((a->state == 9 || a->state == 11) && v->state == 21) v->blocking = false;
    if (a->state >= 13 && a->state <= 16 && v->state == 22) v->blocking = false;
    double cx = a->x + iround((a->hit_l + a->hit_r) / 2.0);
    double cy = a->y - a->sprite_h + iround((a->hit_t + a->hit_b) / 2.0);
    bool live = !g->fight.challenge && !g->fight.no_push;

    if (v->blocking) {
        v->anim_timer = 1;
        g->fight.snd_pending = 10;
        spawn_fx(g, 14, 0, 0, cx, cy);
        if (v->facing == 1) move_x(g, v, -10.0);
        if (v->facing == 2) move_x(g, v, 10.0);
        if (live) { v->health -= a->chip * g->settings.damage_level; v->trail_delay = 20; }
        if (v->health <= 0.0) { g->fight.snd_pending = 9; set_move(g, v, 10); }
        a->attacking = false;
        if (v->health > 0.0) {
            if (a->hit_level == 1) g->fight.hit_stop = osc_random(g, 4) + 2;
            if (a->hit_level == 2) g->fight.hit_stop = osc_random(g, 4) + 4;
        }
    }
    /* a chip KO dropped the guard (setMove 10), so it also lands as a hit */
    if (!v->blocking) {
        int ch = a->char_id, st = a->state, t = a->anim_timer;
        if (v->x < a->x) v->facing = 1;
        if (v->x >= a->x) v->facing = 2;
        if (a->human && v->health > 0.0 && !g->fight.hit_stop && !g->fight.super_pause) a->score += 5;
        if (a->meter < OSC_METER_MAX) a->meter += 2.0 * k_meter_unit;
        if (v->meter < OSC_METER_MAX) v->meter += 1.0 * k_meter_unit;
        if (a->combo_timer <= 35) a->hits = 0;
        a->hits++;
        a->combo_timer = 44;
        if (st == 11 || st == 16) a->combo_timer = 50;
        if (ch == 4 && st == 26) a->combo_timer = 46;
        if (ch == 4 && st == 27) a->combo_timer = 47;
        if (ch == 6 && st == 29) a->combo_timer = 50;
        if (ch == 15 && st == 27) a->combo_timer = 50;
        if (a->hits > a->max_combo) a->max_combo = a->hits;
        if (live) { v->health -= a->damage * g->settings.damage_level; v->trail_delay = 20; }
        if (g->settings.cfg_flash) {
            if (a->hit_level == 1 && osc_random(g, 5) == 0) g->fight.flash = 6;
            if (a->hit_level == 2 && osc_random(g, 2) == 0) g->fight.flash = 6;
        }
        if (v->health <= 0.0) { g->fight.snd_pending = 8; set_move(g, v, 10); }
        /* spinning throws: the victim turns 20 degrees a tick */
        if ((ch == 4 && st == 29) || (ch == 5 && st == 29 && t >= 15) || (ch == 6 && st == 29 && t >= 22) ||
            (ch == 9 && st == 29 && t >= 14) || (ch == 10 && st == 29 && t >= 15) ||
            (ch == 11 && st == 29 && t >= 20) || (ch == 12 && st == 28 && t >= 22)) {
            if (v->facing == 1) v->rot_step = -20;
            if (v->facing == 2) v->rot_step = 20;
        }
        spawn_fx(g, 1, 0, 0, cx, cy);
        if ((g->settings.cfg_blood && a->hit_level == 2 && osc_random(g, 5) == 0) || g->settings.unlocks.satanas)
            spawn_fx(g, 10, 0, 0, v->x, v->y - (v->sprite_h - 40));
        set_move(g, v, (st == 4 || st == 6 || st == 8 || st == 9 || st == 10) ? 9 : 8);
        if (a->knockdown || v->vy != 0 || v->y < OSC_GROUND_Y) set_move(g, v, 10);
        /* how far the victim slides (+0x72) and flies (+0x78) */
        v->hitstun = 4;
        if (ch == 1 && st == 26 && a->hit_level == 2) v->hitstun = 20;
        if (ch == 2 && st == 26 && a->hit_level == 2) v->hitstun = 13;
        if (ch == 4 && st == 29) v->hitstun = 20;
        if (ch == 6 && st == 27 && a->hit_level == 2) v->hitstun = 25;
        if (ch == 7 && st == 27 && a->hit_level == 2) v->hitstun = 18;
        if (ch == 9 && st == 29 && t >= 15) v->hitstun = 25;
        if (ch == 10 && st == 28 && a->hit_level == 2) v->hitstun = 13;
        if (ch == 12 && st == 26 && a->hit_level == 2) v->hitstun = 25;
        if (ch == 4 && st == 29) v->vy = -30;
        if (ch == 5 && st == 29 && t >= 15) v->vy = -30;
        if (ch == 15 && st == 27) v->vy = -26;
        if (a->hit_level == 1) g->fight.snd_pending = 7;
        if (a->hit_level == 2) g->fight.snd_pending = 8;
        if (a->hit_level == 3) g->fight.snd_pending = 9;
        if (!g->fight.first_hit) {
            a->msg_index = 1; a->msg_timer = 50;   /* "Primeiro Ataque" */
            if (a->human && !g->fight.hit_stop && !g->fight.super_pause) a->score += 300;
            g->fight.first_hit = true;
        }
        a->on_top = true; v->on_top = false;
        a->attacking = false; v->attacking = false;
        /* multi-hit specials chain into their next part */
        if (ch == 2 && st == 28) { a->state = 29; a->anim_timer = 0; a->anim_dur = 30; v->attacking = false; }
        if (ch == 2 && a->state == 30) { a->state = 31; a->anim_timer = 0; a->anim_dur = 40; a->hits++; v->attacking = false; }
        if (ch == 3 && a->state == 29) { a->state = 30; a->anim_timer = 0; a->anim_dur = 30; v->attacking = false; }
        if (ch == 7 && a->state == 29) { a->state = 30; a->anim_timer = 0; a->anim_dur = 15; v->attacking = false; }
        if (v->health > 0.0) {
            if (a->hit_level == 1) g->fight.hit_stop = osc_random(g, 4) + 5;
            if (a->hit_level == 2) g->fight.hit_stop = osc_random(g, 4) + 7;
        }
        select_frame(g, v);
    }
    /* the attacker bounces off a cornered victim */
    if (!g->fight.no_push) {
        if (v->x >= g->fight.cam_x + 370) move_x(g, a, -12.0);
        if (v->x <= g->fight.cam_x + 30) move_x(g, a, 12.0);
    }
}
