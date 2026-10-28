/* Attract / title (state 1) and the demo fight start. */
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

static void blit(osc_game *g, SDL_Texture *t, int w, int h, float x, float y)
{
    if (!t) return;
    SDL_FRect d = {x, y, (float)w, (float)h};
    SDL_RenderTexture(g->ren, t, NULL, &d);
}
static void blit_ab(osc_game *g, int i, float x, float y)
{
    if (i >= 0 && i < g->assets.n_abertura) blit(g, g->assets.abertura[i], g->assets.abertura_w[i], g->assets.abertura_h[i], x, y);
}

/* ---- state 1: attract / title (07b-layout.md "Attract") ------------------- */
void osc_go_attract(osc_game *g, int tick)
{
    g->flow.screen = OSC_SCREEN_ATTRACT;
    g->flow.screen_tick = tick;
    g->flow.menu_cam = tick > 235 ? -2 * (tick - 235) : 0;   /* where the scroll would be at that tick */
    if (g->flow.menu_cam < -200) g->flow.menu_cam = -200;
    g->fight.ciclo = 0;
    g->flow.demo = 0;
    g->flow.p1_in = g->flow.p2_in = false;
    g->fight.p[1].human = g->fight.p[2].human = false;
}

/* State 1 -> 7 (0x49fdb1..0x4a00d4): a CPU-vs-CPU fight with no round intro (g_roundStart = 72), no
 * HUD (0x4b44d2/d4 = -120 / 60 push both bars off screen), no jump-in and no music. */
static void start_demo(osc_game *g)
{
    int stage = lround_i(osc_random(g, 120) / 10.0) + 1;
    if (stage < 1) stage = 1;
    if (stage > 12) stage = 12;
    int c1 = lround_i(osc_random(g, 150) / 10.0) + 1, c2 = lround_i(osc_random(g, 150) / 10.0) + 1;
    if (c1 < 1) c1 = 1; if (c1 > 15) c1 = 15;
    if (c2 < 1) c2 = 1; if (c2 > 15) c2 = 15;
    g->flow.p1_in = g->flow.p2_in = false;
    g->fight.p[1].human = g->fight.p[2].human = false;
    g->fight.p[1].char_id = c1; g->fight.p[2].char_id = c2;
    g->flow.sel[1] = c1; g->flow.sel[2] = c2;
    g->flow.demo = 600;
    g->flow.screen = OSC_SCREEN_FIGHT; g->flow.screen_tick = 0;
    osc_fight_start(g, c1, c2, stage);
    g->fight.no_push = false;
    g->fight.timer_sec = 99; g->fight.timer_sub = 25;
    g->fight.round_start = 72; g->fight.round_end = 0;
    g->fight.hud_y = -120; g->fight.hud_y2 = 60;
    g->fight.first_hit = true;
}

void osc_attract_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    (void)in1; (void)in2;
    /* The original draws a tick and then updates; the port updates and then draws, so the events the
     * original runs while drawing (c == 100, the punch sound at 200) fire on the value just reached,
     * like the camera and the demo trigger, which the original tests after the increment. */
    if (g->flow.credits > 0) { g->flow.screen_tick = 500; g->flow.menu_cam = -200; }
    g->fight.ciclo++;
    if (g->fight.ciclo > 20) g->fight.ciclo = 0;
    if (g->flow.screen_tick < 1000) g->flow.screen_tick++;
    int c = g->flow.screen_tick;
    if (c == 100) g->flow.menu_cam = 0;
    if (c == 200 && g->assets.n_sounds > 9) osc_play_snd(g, 9);
    if (c > 235) g->flow.menu_cam -= 2;
    if (g->flow.menu_cam < -200) g->flow.menu_cam = -200;
    if (c > 650) start_demo(g);
}

void osc_attract_render(osc_game *g)
{
    int c = g->flow.screen_tick, cam = g->flow.menu_cam;
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (c < 100) {
        barra(g, 0, 0, 400, 300, rgb(120, 120, 250));
        blit_ab(g, 14, 110, 10);                                               /* Vencedores */
        osc_text_ex(g, 200, 210, osc_text(OSC_MEMO_TEXTO, 12, g->settings.lang), 'c', 16, 250, 200, 0);
    }
    if (c >= 100 && c <= 370) {
        for (int i = 0; i < 21; i++) barra(g, 0, 15 * i, 400, 15 * i + 15, rgb(120 + 2 * i, 120 + 2 * i, 200));
        int c2 = lround_i(cam / 2.0);
        blit_ab(g, 2, 50, (float)(50 - c2)); blit_ab(g, 2, 350, (float)(150 - c2)); blit_ab(g, 2, 300, (float)(180 - c2));
        for (int j = 0; j < 5; j++) {
            blit_ab(g, 3, 50, (float)(47 * j + 73 - c2));
            blit_ab(g, 3, 350, (float)(47 * j + 173 - c2));
            blit_ab(g, 3, 300, (float)(47 * j + 203 - c2));
        }
        for (int j = 0; j < 7; j++) { blit_ab(g, 0, 100, (float)(80 * j - 200 - cam)); blit_ab(g, 0, 305, (float)(80 * j - 200 - cam)); }
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 16; j++) blit_ab(g, 1, (float)(39 * i + 113), (float)(32 * j - 200 - cam));
        barra(g, 147, -155 - cam, 270, -48 - cam, k_black);
        retangulo(g, 149, -153 - cam, 268, -50 - cam, k_white);
        for (int i = 0; i < 8; i++) blit_ab(g, 4, (float)(50 * i), (float)(220 - cam));
        int bob = g->fight.ciclo < 10 ? 4 : 0;
        for (int i = 0; i < 9; i++) {
            blit_ab(g, 7, (float)(50 * i), (float)(245 - cam + bob));
            blit_ab(g, 5, (float)(50 * i - 15), (float)(245 - cam + bob));
            blit_ab(g, 6, (float)(50 * i + 15), (float)(245 - cam + bob));
        }
        int bob2 = g->fight.ciclo > 10 ? 5 : 0;
        if (c < 200) { blit_ab(g, 8, 210, (float)(185 + bob2 - cam)); blit_ab(g, 9, 50, (float)(178 + bob2 - cam)); }
        else {
            blit_ab(g, 11, (float)(60 - 4 * (c - 200)), (float)(180 + 2 * (c - 200) - cam));   /* Seu Madruga caindo */
            blit_ab(g, 10, 100, (float)(190 - cam));                                          /* Chaves dando soco */
        }
    }
    /* logo zoom: k = 4 until 400, grows to 23 at 419, 20 at 420 (the second test wins), shrinks to 2 at 438 */
    int k = c < 400 ? 4 : c < 420 ? c - 396 : c <= 438 ? 440 - c : 2;
    if (k > 0) {
        int k1 = lround_i(223.0 / k), k2 = lround_i(117.0 / k);
        int li = g->settings.lang == 1 ? 13 : 12;
        if (li < g->assets.n_abertura && g->assets.abertura[li]) {
            SDL_FRect d = {(float)(205 - k1), (float)(-100 - cam - k2), (float)(2 * k1), (float)(2 * k2)};
            SDL_RenderTexture(g->ren, g->assets.abertura[li], NULL, &d);
        }
    }
    if (c > 440) {
        osc_text_ex(g, 320, 140, "V1.5A", 'l', 8, 255, 255, 255);
        const char *sub = osc_text(OSC_MEMO_TEXTO, 21, g->settings.lang);                 /* O LUTADOR DA VILA */
        if (c < 500) osc_text_ex(g, (float)((c & 1) ? 200 - 4 * (500 - c) : 200 + 4 * (500 - c)), 170, sub, 'c', 12, 90, 90, 255);
        else osc_text_ex(g, 200, 170, sub, 'c', 12, 90, 90, 255);
    }
    if (c > 440) {                                                             /* the credit block is inside the c > 440 test */
        blit_ab(g, 15, 5, 280);                                                /* Cybergamba */
        osc_text_ex(g, 200, 253, osc_text(OSC_MEMO_TEXTO, 25, g->settings.lang), 'c', 8, 255, 255, 255);
        osc_text_ex(g, 200, 267, osc_text(OSC_MEMO_TEXTO, 30, g->settings.lang), 'c', 8, 255, 255, 255);
    }
    if (g->fight.ciclo < 12)
        osc_text_ex(g, 200, 235, osc_text(OSC_MEMO_TEXTO, g->flow.credits == 0 ? 5 : 6, g->settings.lang), 'c', 10, 255, 255, 255);
}
