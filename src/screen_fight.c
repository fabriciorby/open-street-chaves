/* Fight screen (state 7) and the end of a match; the loser/ladder logic is here because it only runs
 * from the fight tick and decides which screen follows (VS, name entry, continue, ending). */
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

/* End of a match (DXTimer1Timer round end, re == 100 and re > 220): the loser drops out
 * (beating the CPU advances the ladder); with a human left the ladder goes on, past
 * fight 8 that human sees the ending; with nobody left it is continue time. Either way a
 * score above the 10th record asks for a name first (state 12). */
static void after_match(osc_game *g, int winner)
{
    int loser = 3 - winner;
    if (!g->fight.p[loser].human) g->fight.fight_no++;
    g->fight.p[loser].human = false;
    if (loser == 1) g->flow.p1_in = false; else g->flow.p2_in = false;
    bool any = g->fight.p[1].human || g->fight.p[2].human;
    if (any && g->fight.fight_no <= 8) {
        if (!g->flow.ladder[1]) osc_make_ladder(g, g->fight.p[winner].char_id);   /* 2P winner goes on alone */
        g->flow.sel[winner] = g->fight.p[winner].char_id;
        g->flow.screen = OSC_SCREEN_VS; g->flow.screen_tick = 0; g->fight.ciclo = 0;
        return;
    }
    g->flow.name_after = any ? OSC_SCREEN_ENDING : OSC_SCREEN_CONTINUE;
    g->flow.ending_char = g->fight.p[winner].char_id;
    bool ask = false;
    for (int p = 1; p <= 2; p++) {
        g->flow.name_active[p] = g->fight.p[p].score > g->flow.records[9].score;
        if (!g->flow.name_active[p]) continue;
        memcpy(g->flow.name[p], "AAA", 4);
        g->flow.name_pos[p] = 1;
        g->fight.p[p].in_prev = 0xff;
        ask = true;
    }
    if (ask) { g->flow.screen = OSC_SCREEN_NAME; g->flow.screen_tick = 0; g->fight.ciclo = 0; return; }
    osc_name_leave(g);
}

/* The fight screen (state 7, also the demo): osc_fight_tick, then the demo countdown or the
 * end-of-match transition. Any state value without a screen of its own lands here too. */
void osc_fight_screen_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    osc_fight_tick(g, in1, in2);
    if (g->flow.demo > 0) {
        g->flow.demo--;
        if (g->flow.demo < 6) { g->flow.demo = 0; g->flow.screen = OSC_SCREEN_RECORDS; g->flow.screen_tick = 0; g->flow.menu_cam = 0; g->fight.ciclo = 0; }
        return;
    }
    if (g->flow.match_over) {
        int winner = g->flow.match_over;
        g->flow.match_over = 0;
        after_match(g, winner);
    }
}

void osc_fight_screen_render(osc_game *g)
{
    osc_fight_render(g);
    if (g->flow.demo > 0) {
        /* 0x4aa00f..0x4aa264: venetian wipe over the last 40 ticks, the blinking prompt (size 10),
         * and the title logo (Abertura 12/13, not Icone) at half size in the bottom right corner */
        if (g->flow.demo < 40)
            for (int i = 0; i < 16; i++) barra(g, 0, 20 * i, 400, 20 * i + 40 - g->flow.demo, k_black);
        if (g->fight.ciclo < 20) texto(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 5, g->settings.lang), 'c', 10, k_white);
        int li = g->settings.lang == 1 ? 13 : 12;
        if (g->flow.demo < 500 && li < g->assets.n_abertura && g->assets.abertura[li]) {
            int hw = lround_i(g->assets.abertura_w[li] / 2.0), hh = lround_i(g->assets.abertura_h[li] / 2.0);
            SDL_FRect d = {(float)(390 - hw), (float)(280 - hh), (float)hw, (float)hh};
            SDL_RenderTexture(g->ren, g->assets.abertura[li], NULL, &d);
        }
    }
}
