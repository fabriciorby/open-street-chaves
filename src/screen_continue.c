/* Continue (state 9). */
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

/* ---- continue (state 9, 0x4ab647..0x4ab8fe) --------------------------------------- */
void osc_continue_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    (void)in1; (void)in2;   /* continuing takes a coin and a start button (handle_keys) */
    g->flow.screen_tick++;
    if (g->fight.timer_sec > 0 || g->fight.timer_sub > 0) {
        if (++g->fight.ciclo > 10) g->fight.ciclo = 0;
    }
    if (g->fight.timer_sub > 0) g->fight.timer_sub--;
    if (g->fight.timer_sub == 0 && g->fight.timer_sec > 0) { g->fight.timer_sec--; g->fight.timer_sub = 25; }
    if (g->fight.timer_sec == 0 && g->fight.timer_sub == 0) {
        if (++g->fight.ciclo > 150) {   /* game over -> records (state 8), then attract */
            g->flow.screen = OSC_SCREEN_RECORDS; g->flow.screen_tick = 0; g->flow.menu_cam = 0; g->fight.ciclo = 0;
            g->flow.p1_in = g->flow.p2_in = false;
            g->fight.p[1].human = g->fight.p[2].human = false;
        }
    }
}

void osc_continue_render(osc_game *g)
{
    char buf[16];
    for (int i = 0; i <= 60; i++) barra(g, 0, 5 * i, 400, 5 * i + 5, rgb(3 * i + 10, 3 * i + 50, 3 * i + 30));
    if (g->fight.timer_sec > 0 || g->fight.timer_sub > 0) {
        texto(g, 200, 20, osc_text(OSC_MEMO_TEXTO, 23, g->settings.lang), 'c', 35, rgb(255, 120, 120));   /* CONTINUE ? */
        if (g->fight.timer_sec > 3 || g->fight.ciclo < 8) {
            snprintf(buf, sizeof buf, "%d", g->fight.timer_sec);
            texto(g, 320, 70, buf, 'c', 120, rgb(255, 25 * g->fight.timer_sec, 25 * g->fight.timer_sec));
        }
    } else {
        texto(g, 200, 110, osc_text(OSC_MEMO_TEXTO, 22, g->settings.lang), 'c', 36, rgb(255, 0, 0));   /* FIM DO JOGO */
        if (g->fight.ciclo < 15) barra(g, 0, 10 * g->fight.ciclo, 400, 300 - 10 * g->fight.ciclo, k_white);
    }
}
