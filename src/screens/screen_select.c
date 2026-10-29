/* Character select (state 4). */
#include <stdio.h>
#include <string.h>

#include "data/exetext.h"
#include "render/font.h"
#include "data/assets.h"
#include "render/fight_draw.h"
#include "fight/fight.h"
#include "game.h"
#include "render/canvas.h"
#include "screens/arcade.h"
#include "render/text.h"
#include "data/wav.h"
#include "screens/screens.h"
#include <math.h>

/* ---- character select (DXTimer1Timer state 4, 0x4a22f0..0x4a342f) ------------- */
static int cell_x(int sel) { return 49 * ((sel - 1) % 8) + 3; }    /* at full scroll */
static int cell_y(int sel) { return sel <= 8 ? 163 : 223; }

void osc_select_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t ins[3] = {0, in1, in2};
    g->flow.screen_tick++;
    g->fight.no_push = false;
    osc_audio_stop_music();
    if (g->flow.sel_sub < 300) {                       /* g_selectSub 1: slide in by 20 */
        g->flow.sel_sub += 20;
        if (g->flow.sel_sub >= 300) g->flow.sel_sub = 300;
        return;
    }
    if (g->fight.timer_sec >= 1 && --g->fight.timer_sub <= 0) { g->fight.timer_sec--; g->fight.timer_sub = 25; }
    if (++g->fight.ciclo >= 25) g->fight.ciclo = 0;
    for (int p = 1; p <= 2; p++) {
        osc_fighter *f = &g->fight.p[p];
        bool human = p == 1 ? g->flow.p1_in : g->flow.p2_in;
        uint8_t in = ins[p], pressed = (uint8_t)(in & ~f->in_prev);
        f->in_prev = in;
        if (!human) { g->flow.sel_locked[p] = 1; continue; }
        if (g->flow.sel_locked[p]) continue;
        if (pressed & OSC_IN_LEFT) { g->flow.sel[p]--; osc_play_snd(g, 1); }
        if (pressed & OSC_IN_RIGHT) { g->flow.sel[p]++; osc_play_snd(g, 1); }
        if ((pressed & OSC_IN_UP) && g->flow.sel[p] > 8) { g->flow.sel[p] -= 8; osc_play_snd(g, 1); }
        if ((pressed & OSC_IN_DOWN) && g->flow.sel[p] <= 8) { g->flow.sel[p] += 8; osc_play_snd(g, 1); }
        if (pressed & OSC_IN_B1) { g->flow.sel_locked[p] = 1; osc_play_snd(g, 2); }
        if (g->flow.sel[p] < 1) g->flow.sel[p] = 15;
        if (g->flow.sel[p] > 15) g->flow.sel[p] = 1;
    }
    if (g->fight.timer_sec == 0 && !(g->flow.sel_locked[1] && g->flow.sel_locked[2])) {
        g->flow.sel_locked[1] = g->flow.sel_locked[2] = 1;
        osc_play_snd(g, 2);
    }
    if (g->flow.sel_locked[1] && g->flow.sel_locked[2]) {
        bool both = g->flow.p1_in && g->flow.p2_in;
        if (!both) {                        /* criaadversario for the lone human */
            int h = g->flow.p1_in ? 1 : 2;
            osc_make_ladder(g, g->flow.sel[h]);
            g->flow.sel[3 - h] = g->flow.ladder[g->fight.fight_no];
        }
        g->fight.p[1].human = g->flow.p1_in;
        g->fight.p[2].human = g->flow.p2_in;
        g->fight.p[1].char_id = g->flow.sel[1]; g->fight.p[2].char_id = g->flow.sel[2];
        g->flow.screen = both ? OSC_SCREEN_FIGHT : OSC_SCREEN_VS;
        g->flow.screen_tick = 0; g->fight.ciclo = 0;
        if (g->flow.screen == OSC_SCREEN_FIGHT) osc_fight_start(g, g->flow.sel[1], g->flow.sel[2], 0);
    }
}

void osc_select_render(osc_game *g)
{
    int c2 = g->flow.sel_sub, k = (int)SDL_lround(c2 * 1.35);
    barra(g, 0, 0, 400, 300, k_black);
    barra(g, 0, 300 - c2, 200, 600 - c2, rgb(120, 120, 255));
    barra(g, 200, c2 - 300, 400, c2, rgb(255, 100, 100));
    /* Icone 11/12 (by language) tiled and drifting down with g_timerB */
    int ic = g->settings.lang == 0 ? 11 : 12;
    if (ic < g->assets.n_icone)
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 2; i++) {
                draw_tex(g, g->assets.icone[ic], (float)(145 * j - 67), (float)(200 * i - 200 + 8 * g->fight.timer_sub), 255, false);
                draw_tex(g, g->assets.icone[ic], (float)(145 * j), (float)(200 * i - 100 + 8 * g->fight.timer_sub), 255, false);
            }
    barra(g, k - 410, 156, k, 290, rgb(30, 50, 70));
    barra(g, k - 410, 158, k, 160, k_white);
    barra(g, k - 410, 286, k, 288, k_white);
    /* the grid: every face at alpha 60 */
    for (int i = 0; i < 15; i++) {
        int x = k + 49 * (i % 8) - 402, y = i < 8 ? 163 : 223;
        draw_tex_rect(g, g->assets.face_small[i + 1], x, y, x + 50, y + 60, 60);
    }
    if (g->flow.sel_sub < 300) return;
    char buf[32];
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->flow.p1_in : g->flow.p2_in)) continue;
        int c = g->flow.sel[p];
        draw_tex(g, g->assets.face_big[c], p == 1 ? 52.f : 252.f, 25.f, 255, false);
        if (!g->flow.sel_locked[p])
            draw_tex(g, g->assets.face_small[c], (float)(k + 49 * ((c - 1) % 8) - 402), (float)cell_y(c), 255, false);
        texto(g, p == 1 ? 100 : 300, 130 - (int)SDL_lround(cos(g->fight.ciclo / 4.3) * 4.0),
              osc_text(OSC_MEMO_NOMES, c, g->settings.lang), 'c', 12, rgb(180, 220, 160));
    }
    int kt = (int)SDL_lround(cos(g->fight.timer_sub / 4.0) * 20.0);
    texto(g, 200, (int)SDL_lround(c2 * 0.3) - 96, osc_text(OSC_MEMO_TEXTO, 7, g->settings.lang), 'c', 18,
          rgb(230 + kt, 230 + kt, 255));                                                   /* SELECIONE SEU LUTADOR */
    osc_rgb tc = g->fight.timer_sec <= 5 && (g->fight.timer_sub & 1) == 0 ? rgb(250, 220, 220) : rgb(250, 150, 150);
    snprintf(buf, sizeof buf, "%d", g->fight.timer_sec);
    texto(g, 200, 15, buf, 'c', 22, tc);
    /* "1P" / "2P" over the cursor cell while choosing */
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->flow.p1_in : g->flow.p2_in) || g->flow.sel_locked[p]) continue;
        int c = g->flow.sel[p], add = (g->fight.ciclo & 1) == 0 ? 50 : 0;
        snprintf(buf, sizeof buf, "%dP", p);
        if (p == 1) texto(g, cell_x(c), cell_y(c) - 3, buf, '-', 10, rgb(100 + add, 100 + add, 200 + add));
        else texto(g, cell_x(c) + 35, cell_y(c) - 3, buf, '-', 10, rgb(200 + add, 50 + add, 50 + add));
    }
}
