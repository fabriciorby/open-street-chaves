/* VS / ladder (state 5). */
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

/* ---- VS / ladder (state 5, 0x4a342f..0x4a3f9f) ---------------------------------- */
void osc_vs_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    int c2 = g->flow.screen_tick;
    if (g->fight.fight_no < 1) g->fight.fight_no = 1;
    if (g->fight.fight_no > 8) g->fight.fight_no = 8;
    osc_audio_stop_music();
    if (c2 == 0) {
        /* the CPU opponent picks the stage (osc_stage_for) and it is shown as a preview */
        int opp = g->flow.p1_in ? 2 : 1;
        g->flow.sel[opp] = g->flow.ladder[g->fight.fight_no];
        osc_load_stage(g, osc_stage_for(g, g->flow.sel[opp]));
    }
    if (++g->fight.ciclo > 15) g->fight.ciclo = 0;
    if (++g->flow.screen_tick > 1000) g->flow.screen_tick = 1000;
    uint8_t held = (uint8_t)((g->flow.p1_in ? in1 : 0) | (g->flow.p2_in ? in2 : 0));
    if ((held & OSC_IN_B1) && g->flow.screen_tick > 0) g->flow.screen_tick += 5;
    if (c2 > 150) osc_fight_start(g, g->flow.sel[1], g->flow.sel[2], g->fight.stage_id);
    if (g->settings.unlocks.fadiga) osc_ending_start(g, g->flow.sel[g->flow.p1_in ? 1 : 2]);   /* Fadiga.dat: state 10 */
}

void osc_vs_render(osc_game *g)
{
    int c2 = g->flow.screen_tick;
    for (int i = 0; i <= 60; i++) barra(g, 0, 10 * i, 400, 10 * i + 10, rgb(2 * i + 10, 3 * i + 20, 4 * i + 30));
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->flow.p1_in : g->flow.p2_in)) continue;
        int tx_x = c2 < 30 ? 200 - (c2 - 30) * 20 : c2 <= 100 ? 200 : 200 - (c2 - 100) * 20;
        texto(g, tx_x, 135, osc_text(OSC_MEMO_TEXTO, 19, g->settings.lang), 'c', 26, rgb(150, 255, 180));   /* PRÓXIMA BATALHA */
        if (c2 > 40) texto(g, 200, 180, osc_text(OSC_MEMO_NOMES, g->flow.ladder[g->fight.fight_no], g->settings.lang), 'c', 15, k_white);
        for (int i = 1; i <= 8; i++) {
            int c = g->flow.ladder[i];
            float y = c2 < 50 ? (float)(470 - 5 * c2) : 220.f;
            bool shown = i <= g->fight.fight_no || g->settings.unlocks.churros;
            if (shown && c >= 1 && c <= 15) draw_tex(g, g->assets.face_small[c], (float)(48 * i - 40), y, 255, true);
            if (i < g->fight.fight_no && 10 < g->assets.n_icone)
                draw_tex(g, g->assets.icone[10], (float)(48 * i - 35), c2 < 50 ? (float)(485 - 5 * c2) : 235.f, 255, false);
            if (!shown && 9 < g->assets.n_icone) draw_tex(g, g->assets.icone[9], (float)(48 * i - 40), y, 255, false);
        }
    }
    if (c2 >= 50 && g->fight.ciclo <= 10 && 8 < g->assets.n_icone)
        draw_tex(g, g->assets.icone[8], (float)(48 * g->fight.fight_no - 20), 215.f, 255, false);
    if (c2 > 60 && g->assets.stage) {
        draw_tex_rect(g, g->assets.stage, 80, 6, 320, 134, 255);
        texto(g, 200, 115, osc_text(OSC_MEMO_CENARIOS, g->fight.stage_id - 1, g->settings.lang), 'c', 9, k_white);
    }
    if (c2 > 120)
        for (int i = 0; i < 16; i++) barra(g, 0, 20 * i, 400, 20 * i + c2 - 120, k_black);
}
