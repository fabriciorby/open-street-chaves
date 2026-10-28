/* Name entry for the records (state 12). */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "font.h"
#include "assets.h"
#include "fight_draw.h"
#include "fight.h"
#include "game.h"
#include "canvas.h"
#include "arcade.h"
#include "text.h"
#include "wav.h"
#include "screens.h"
#include <math.h>

/* ---- state 12: name entry for the records (07b "Name entry") -------------- */
static void insert_record(osc_game *g, int p)
{
    /* criarecorde(10, name) + ordenarecordista: replace the last slot, sort by score */
    osc_record *r = &g->flow.records[9];
    memcpy(r->name, g->flow.name[p], 4);
    r->char_id = g->fight.p[p].char_id;
    r->score = g->fight.p[p].score;
    for (int i = 9; i > 0 && g->flow.records[i].score > g->flow.records[i - 1].score; i--) {
        osc_record t = g->flow.records[i]; g->flow.records[i] = g->flow.records[i - 1]; g->flow.records[i - 1] = t;
    }
}

void osc_name_leave(osc_game *g)
{
    for (int p = 1; p <= 2; p++) if (g->flow.name_active[p]) { insert_record(g, p); g->flow.name_active[p] = false; }
    if (g->flow.name_after == OSC_SCREEN_ENDING) { osc_ending_start(g, g->flow.ending_char); return; }
    g->flow.screen = g->flow.name_after; g->flow.screen_tick = 0; g->fight.ciclo = 0; g->flow.menu_cam = 0;
    if (g->flow.screen == OSC_SCREEN_CONTINUE) { g->fight.timer_sec = 9; g->fight.timer_sub = 25; }
    if (g->flow.screen == OSC_SCREEN_RECORDS) { g->flow.p1_in = g->flow.p2_in = false; }
}

void osc_name_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t ins[3] = {0, in1, in2};
    g->fight.ciclo++;
    if (g->fight.ciclo > 20) g->fight.ciclo = 0;
    if (g->flow.screen_tick < 1000) g->flow.screen_tick++;
    bool any = false;
    for (int p = 1; p <= 2; p++) {
        if (!g->flow.name_active[p]) continue;
        uint8_t pressed = (uint8_t)(ins[p] & ~g->fight.p[p].in_prev);
        g->fight.p[p].in_prev = ins[p];
        int *pos = &g->flow.name_pos[p];
        if (*pos > 3) continue;
        any = true;
        if (pressed & OSC_IN_LEFT) (*pos)--;
        if (pressed & OSC_IN_RIGHT) (*pos)++;
        if (*pos < 1) *pos = 1;
        if (*pos > 3) *pos = 3;
        char *c = &g->flow.name[p][*pos - 1];
        if (pressed & OSC_IN_UP) (*c)++;
        if (pressed & OSC_IN_DOWN) (*c)--;
        if (*c < '0') *c = '0';
        if (*c > 'Z') *c = 'Z';
        if (pressed & OSC_IN_B1) (*pos)++;
    }
    /* the first player to pass the third letter (or the 40 s limit) ends the screen; only
     * the names that were finished are recorded (state 12, 0x4ae0c4) */
    bool done = !any || g->flow.screen_tick >= 1000;
    for (int p = 1; p <= 2; p++) if (g->flow.name_active[p] && g->flow.name_pos[p] > 3) done = true;
    if (done) {
        if (g->flow.screen_tick < 1000)
            for (int p = 1; p <= 2; p++) if (g->flow.name_active[p] && g->flow.name_pos[p] <= 3) g->flow.name_active[p] = false;
        osc_name_leave(g);
    }
}

void osc_name_render(osc_game *g)
{
    barra(g, 0, 0, 400, 300, k_black);
    for (int p = 1; p <= 2; p++) {
        if (!g->flow.name_active[p]) continue;
        int x0 = (p - 1) * 200 + 20;
        for (int i = 0; i < 3; i++) {
            char l[2] = {g->flow.name[p][i], 0};
            texto(g, x0 + 15 * i, 5, l, '-', 15, k_white);
        }
        if (g->fight.ciclo < 15 && g->flow.name_pos[p] <= 3) texto(g, x0 + 15 * (g->flow.name_pos[p] - 1), 4, "-", '-', 27, k_white);
    }
}
