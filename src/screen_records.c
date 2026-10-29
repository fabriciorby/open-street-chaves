/* Game over / top 10 (state 8). */
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

/* ---- state 8: game over / top 10 (07b-layout.md "Records") ---------------- */
void osc_records_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    (void)in1; (void)in2;
    g->fight.ciclo++;
    if (g->fight.ciclo > 20) g->fight.ciclo = 0;
    if (g->flow.screen_tick < 1000) g->flow.screen_tick++;
    if (g->flow.screen_tick > 80) g->flow.menu_cam -= 5;
    if (g->flow.menu_cam < -320) g->flow.menu_cam = -320;
    if (g->flow.screen_tick > 280) osc_go_attract(g, 0);
}

void osc_records_render(osc_game *g)
{
    for (int i = 0; i < 21; i++) barra(g, 0, 20 * i, 400, 20 * i + 20, rgb(5 * i + 100, 5 * i + 130, 5 * i + 160));
    for (int i = 1; i <= 10; i++) {
        const osc_record *r = &g->flow.records[i - 1];
        int y = 60 * i - 350 - g->flow.menu_cam;
        int hl = i == 1 && (g->fight.ciclo & 1) == 0 ? 80 : 0;   /* the leader blinks */
        osc_rgb c = rgb(255, 255 - hl, 255 - hl);
        char num[16], sc[16];
        snprintf(num, sizeof num, i < 10 ? "0%d -" : "%d -", i);
        snprintf(sc, sizeof sc, "%d", r->score);
        texto(g, 50, y, num, '-', 15, c);
        texto(g, 100, y, r->name, '-', 15, c);
        if (r->char_id >= 1 && r->char_id <= 15) draw_tex(g, g->assets.face_small[r->char_id], 160.f, (float)(y - 20), 255, false);
        texto(g, 230, y, sc, '-', 15, c);
    }
    if (g->flow.screen_tick > 240)
        for (int i = 0; i < 16; i++) barra(g, 0, 20 * i, 400, 20 * i + g->flow.screen_tick - 240, k_black);
    if (g->fight.ciclo < 18) texto(g, 200, 5, osc_text(OSC_MEMO_TEXTO, 20, g->settings.lang), 'c', 12, k_white);    /* OS 10 MELHORES */
    if (g->fight.ciclo < 10) texto(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 5, g->settings.lang), 'c', 10, k_white);   /* INSIRA FICHAS */
}
