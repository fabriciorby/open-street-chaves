/* Stage objects (0x4a618e..0x4a6622). */
#include <stdlib.h>
#include <string.h>

#include "render/canvas.h"
#include "fight/fight_internal.h"

/* ---- stage objects (0x4a618e..0x4a6622) ------------------------------------------- */
void stage_objects(osc_game *g)
{
    bool paused = g->fight.hit_stop || g->fight.super_pause;
    /* something crossing the street on stages 1 and 2 */
    if ((g->fight.stage_id == 1 || (g->fight.stage_id == 2 && !g->fight.no_push)) && !g->fight.mover.active &&
        g->fight.cam_x > 40 && g->fight.cam_x < 160 && osc_random(g, g->fight.mover.freq) == 0) {
        g->fight.mover.x = g->fight.cam_x - 80; g->fight.mover.y = 146; g->fight.mover.speed = 7;
        g->fight.mover.kind = osc_random(g, 2) + 1;
        g->fight.mover.active = true;
    }
    if (g->fight.mover.active) {
        if (!paused) g->fight.mover.x += g->fight.mover.speed;
        if (g->fight.mover.x > g->fight.cam_x + 420) g->fight.mover.active = false;
        /* ... and dropping something on the way */
        if (g->fight.mover.kind != 0 && g->fight.mover.x > g->fight.cam_x + 150 && g->fight.mover.x < g->fight.cam_x + 250 &&
            osc_random(g, 5) == 0 && !g->fight.super_pause && !g->fight.hit_stop && !g->fight.no_push) {
            spawn_fx(g, g->fight.mover.kind == 1 ? 3 : 4, 0, 0, g->fight.mover.x + 13, g->fight.mover.y - 5);
            g->fight.mover.kind = 0;
        }
    }
    /* stage 2: a fighter knocked into the breakable thing smashes it */
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->fight.p[i];
        int l = iround(f->x) + f->hurt_l, t = iround(f->y) - f->sprite_h + f->hurt_t;
        int r = iround(f->x) + f->hurt_r, b = iround(f->y) - f->sprite_h + f->hurt_b;
        if (f->state != 19) continue;
        if (g->fight.crate.x + g->fight.crate.r1 >= l && g->fight.crate.y + g->fight.crate.b1 >= t &&
            g->fight.crate.x + g->fight.crate.r0 <= r && g->fight.crate.y + g->fight.crate.b0 <= b && g->fight.crate.icon) {
            g->fight.crate.icon = 0;
            g->fight.snd_pending = 9;
            for (int k = 0; k < 30; k++) {
                int rx = osc_random(g, 40), ry = osc_random(g, 20);
                spawn_fx(g, 21, 0, 0, g->fight.crate.x + 20 + rx, g->fight.crate.y + 20 + ry);
            }
        }
    }
}
