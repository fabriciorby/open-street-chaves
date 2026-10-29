/* CPU opponent: TForm1.cpu (0x4931ac), with the AI probability helper that
 * defend() (combat.c) shares. */
#include <stdlib.h>
#include <string.h>

#include "render/canvas.h"
#include "fight/fight_internal.h"

/* AI probability helper: Random(round(C * k)) == 0 (05 "AI difficulty"). */
bool chance(osc_game *g, double c, double k)
{
    int n = iround(c * k);
    if (n < 1) n = 1;
    return osc_random(g, n) == 0;
}

/* ---- CPU opponent: TForm1.cpu (0x4931ac), calls setMove directly ------------------
 * Difficulty k = (8 - level) * 0.6 + 1.3 - 0.1 * fightNo; a rule with constant C
 * fires with Random(Round(C * k)) == 0. Per-character rows: 05d-cpu-table.txt. */
void cpu_think(osc_game *g, osc_fighter *f)
{
    osc_fighter *o = opp(g, f);
    int me = (f == &g->fight.p[1]) ? 1 : 2;
    int dist = abs(iround(f->x) - iround(o->x));
    int oy = iround(o->y);

    /* a hostile effect with a hit box is live (projectile incoming) */
    bool proj = false;
    int pd = 0;
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_effect *e = &g->fight.fx[i];
        if (e->has_hitbox && e->owner != me && e->type) {
            proj = true;
            pd = abs(iround(f->x) - iround(e->x));
        }
    }
    double k = (8 - g->settings.level) * 0.6 + 1.3 - 0.1 * g->fight.fight_no;

    /* preferred distance re-rolled with chance 1/13 */
    static const int pref[6] = {100, 150, 200, 250, 300, 350};
    if (osc_random(g, 13) == 0) f->ai_pref = pref[osc_random(g, 6)];
    if (abs(dist - f->ai_pref) <= 50) f->ai_pref = dist;

    /* walk toward the opponent when farther than preferred, away otherwise */
    if (!f->blocking && !proj) {
        if (dist >= f->ai_pref && f->facing == 1) set_move(g, f, 2);
        if (dist < f->ai_pref && f->facing == 1) set_move(g, f, 1);
        if (dist >= f->ai_pref && f->facing == 2) set_move(g, f, 1);
        if (dist < f->ai_pref && f->facing == 2) set_move(g, f, 2);
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
    if (f->blocking) return;
    for (int i = 0; i < OSC_N_CPU; i++) {
        const osc_cpu_row *r = &osc_cpu[i];
        if (r->ch != f->char_id) continue;
        if ((r->cond & 1) && proj) continue;
        if ((r->cond & 2) && !proj) continue;
        int v = (r->cond & 2) ? pd : dist;
        if (r->greater ? !(v > r->thr) : !(v < r->thr)) continue;
        if ((r->cond & 4) && !(o->y < OSC_GROUND_Y)) continue;
        if ((r->cond & 8) && !(f->y < OSC_GROUND_Y - 100)) continue;
        if ((r->cond & 16) && !(oy < OSC_GROUND_Y - 50)) continue;   /* opponent's rounded y */
        if (!chance(g, r->c, k)) continue;
        set_move(g, f, r->move + (r->n > 1 ? osc_random(g, r->n) : 0));
    }
}
