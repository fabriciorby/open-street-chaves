/* Stage objects (0x4a618e..0x4a6622). */
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "fight_internal.h"

/* ---- stage objects (0x4a618e..0x4a6622) ------------------------------------------- */
void stage_objects(osc_game *g)
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
