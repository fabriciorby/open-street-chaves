/* Screens outside the fight: character select (state 4), VS (state 5),
 * continue (state 9), and the dispatch between them (07-screens-and-text.md).
 * Text is drawn with SDL's debug font until the original font is handled. */
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
#include <math.h>

static int lround_i(double v) { return (int)rint(v); }   /* Delphi Round: half to even */

/* ---- character select (DXTimer1Timer state 4, 0x4a22f0..0x4a342f) ------------- */
static int cell_x(int sel) { return 49 * ((sel - 1) % 8) + 3; }    /* at full scroll */
static int cell_y(int sel) { return sel <= 8 ? 163 : 223; }

static void select_tick(osc_game *g, uint8_t in1, uint8_t in2)
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

static void select_render(osc_game *g)
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

/* ---- VS / ladder (state 5, 0x4a342f..0x4a3f9f) ---------------------------------- */
static void vs_tick(osc_game *g, uint8_t in1, uint8_t in2)
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

static void vs_render(osc_game *g)
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

/* ---- continue (state 9, 0x4ab647..0x4ab8fe) --------------------------------------- */
static void continue_tick(osc_game *g, uint8_t in1, uint8_t in2)
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

static void continue_render(osc_game *g)
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

/* ---- dispatch ---------------------------------------------------------------------- */

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

static void attract_tick(osc_game *g)
{
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

static void attract_render(osc_game *g)
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

/* ---- state 8: game over / top 10 (07b-layout.md "Records") ---------------- */
static void records_tick(osc_game *g)
{
    g->fight.ciclo++;
    if (g->fight.ciclo > 20) g->fight.ciclo = 0;
    if (g->flow.screen_tick < 1000) g->flow.screen_tick++;
    if (g->flow.screen_tick > 80) g->flow.menu_cam -= 5;
    if (g->flow.menu_cam < -320) g->flow.menu_cam = -320;
    if (g->flow.screen_tick > 280) osc_go_attract(g, 0);
}

static void records_render(osc_game *g)
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

static void leave_name_entry(osc_game *g)
{
    for (int p = 1; p <= 2; p++) if (g->flow.name_active[p]) { insert_record(g, p); g->flow.name_active[p] = false; }
    if (g->flow.name_after == OSC_SCREEN_ENDING) { osc_ending_start(g, g->flow.ending_char); return; }
    g->flow.screen = g->flow.name_after; g->flow.screen_tick = 0; g->fight.ciclo = 0; g->flow.menu_cam = 0;
    if (g->flow.screen == OSC_SCREEN_CONTINUE) { g->fight.timer_sec = 9; g->fight.timer_sub = 25; }
    if (g->flow.screen == OSC_SCREEN_RECORDS) { g->flow.p1_in = g->flow.p2_in = false; }
}

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
    leave_name_entry(g);
}

static void name_tick(osc_game *g, uint8_t in1, uint8_t in2)
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
        leave_name_entry(g);
    }
}

static void name_render(osc_game *g)
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

/* ---- coins and start buttons (FormKeyDown '5', '1', '2') ------------------ */
static void join_player(osc_game *g, int p)
{
    g->flow.credits--;
    if (p == 1) g->flow.p1_in = true; else g->flow.p2_in = true;
    g->fight.p[p].score = 0; g->fight.p[p].wins = 0;
    g->flow.sel[p] = p;
    g->flow.sel_locked[p] = 0;
    g->fight.timer_sec = 30; g->fight.timer_sub = 25;   /* FormKeyDown '1'/'2' */
}

static void handle_keys(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* convenience for keyboards and pads: the first button on the title inserts a coin and starts */
    if (g->flow.screen == OSC_SCREEN_ATTRACT || g->flow.screen == OSC_SCREEN_RECORDS || g->flow.screen == OSC_SCREEN_CONTINUE || g->flow.demo > 0) {
        uint8_t p1 = (uint8_t)(in1 & ~g->fight.p[1].in_prev), p2 = (uint8_t)(in2 & ~g->fight.p[2].in_prev);
        g->fight.p[1].in_prev = in1; g->fight.p[2].in_prev = in2;
        if (p1 & OSC_IN_B1) { if (g->flow.credits == 0) g->flow.key_coin = true; g->flow.key_start1 = true; }
        if (p2 & OSC_IN_B1) { if (g->flow.credits == 0) g->flow.key_coin = true; g->flow.key_start2 = true; }
    }
    if (g->flow.key_f1 || g->flow.key_f2) {
        bool f1 = g->flow.key_f1;
        if (g->flow.screen == OSC_SCREEN_ATTRACT) { g->flow.key_f1 = g->flow.key_f2 = false; osc_menu_enter(g, f1 ? 2 : 3); return; }
        /* in the menus the flags are the exit keys (menu.c); in any other state F1/F2 do nothing */
        if (g->flow.screen != OSC_SCREEN_MENU_F1 && g->flow.screen != OSC_SCREEN_MENU_F2) g->flow.key_f1 = g->flow.key_f2 = false;
    }
    if (g->flow.key_coin) {
        g->flow.key_coin = false;
        if (g->flow.credits < 99) {
            g->flow.credits++;
            if (g->assets.n_sounds > 0) osc_play_snd(g, 0);
            if (g->flow.demo > 0 || g->flow.screen == OSC_SCREEN_RECORDS) osc_go_attract(g, 500);
        }
    }
    for (int p = 1; p <= 2; p++) {
        bool *key = p == 1 ? &g->flow.key_start1 : &g->flow.key_start2;
        if (!*key) continue;
        *key = false;
        bool in = p == 1 ? g->flow.p1_in : g->flow.p2_in;
        if (g->flow.credits == 0 || in) continue;
        if (g->flow.screen == OSC_SCREEN_ATTRACT) {
            join_player(g, p);
            g->fight.fight_no = g->settings.unlocks.gentalha ? 8 : 1;
            g->flow.screen = OSC_SCREEN_SELECT; g->flow.screen_tick = 0; g->flow.sel_sub = 0;
            g->flow.sel_locked[1] = g->flow.sel_locked[2] = 0;
            g->settings.ai_p2 = !g->flow.p2_in;
        } else if (g->flow.screen == OSC_SCREEN_SELECT && !g->fight.no_push) {
            join_player(g, p);
            g->settings.ai_p2 = !g->flow.p2_in;
        } else if (g->flow.screen == OSC_SCREEN_FIGHT && g->flow.demo == 0 && !g->fight.no_push && g->fight.challenge == 0) {
            join_player(g, p);
            g->fight.p[p].human = true;
            g->fight.challenge = 100;
        } else if (g->flow.screen == OSC_SCREEN_CONTINUE && g->fight.timer_sec > 0 && g->fight.timer_sub > 0) {
            /* FormKeyDown state 9: back to the same ladder (VS) or, before any fight, to select */
            int keep = g->fight.p[p].char_id;
            join_player(g, p);
            g->flow.sel[p] = keep ? keep : p;   /* same fighter, same ladder */
            g->fight.p[p].human = true;
            g->flow.sel_locked[p] = 1;
            g->fight.ciclo = 0; g->flow.screen_tick = 0;
            g->settings.ai_p2 = !g->flow.p2_in;
            if (g->flow.ladder[1]) g->flow.screen = OSC_SCREEN_VS;
            else { g->flow.screen = OSC_SCREEN_SELECT; g->flow.sel_sub = 0; g->flow.sel_locked[p] = 0; }
        }
    }
}

static void game_tick_body(osc_game *g, uint8_t in1, uint8_t in2)
{
    handle_keys(g, in1, in2);
    switch (g->flow.screen) {
    case OSC_SCREEN_ATTRACT: attract_tick(g); break;
    case OSC_SCREEN_RECORDS: records_tick(g); break;
    case OSC_SCREEN_NAME: name_tick(g, in1, in2); break;
    case OSC_SCREEN_MENU_F1: case OSC_SCREEN_MENU_F2: osc_menu_tick(g, in1, in2); break;
    case OSC_SCREEN_ENDING: case OSC_SCREEN_CREDITS: osc_ending_tick(g, in1, in2); break;
    case OSC_SCREEN_SELECT: select_tick(g, in1, in2); break;
    case OSC_SCREEN_VS: vs_tick(g, in1, in2); break;
    case OSC_SCREEN_CONTINUE: continue_tick(g, in1, in2); break;
    default:
        osc_fight_tick(g, in1, in2);
        if (g->flow.demo > 0) {
            g->flow.demo--;
            if (g->flow.demo < 6) { g->flow.demo = 0; g->flow.screen = OSC_SCREEN_RECORDS; g->flow.screen_tick = 0; g->flow.menu_cam = 0; g->fight.ciclo = 0; }
            break;
        }
        if (g->flow.match_over) {
            int winner = g->flow.match_over;
            g->flow.match_over = 0;
            after_match(g, winner);
        }
        break;
    }
}

/* Port rule: P2's default strong punch is G, so G only opens the move list when neither player's
 * controls use it (the original has no such clash, its P2 keys are different). */
static bool g_is_bound(const osc_game *g)
{
    for (int p = 0; p < 2; p++)
        for (int k = 0; k < 8; k++)
            if (g->settings.keymap[p][k] == SDL_SCANCODE_G) return true;
    return false;
}

/* FormKeyDown (0x49e2e8): speed, picture shift, pause and the move list */
void osc_game_key(osc_game *g, SDL_Keycode key)
{
    if (key == SDLK_MINUS && g->flow.speed_delay < 0x50) g->flow.speed_delay++;
    if (key == SDLK_EQUALS && g->flow.speed_delay >= 1) g->flow.speed_delay--;
    if (key == SDLK_MINUS || key == SDLK_EQUALS) g->flow.speed_overlay = 30;
    if (key == SDLK_COMMA && g->flow.view_dx > -40) g->flow.view_dx--;
    if (key == SDLK_PERIOD && g->flow.view_dx < 40) g->flow.view_dx++;
    bool p = key == SDLK_P, gk = key == SDLK_G && g->flow.screen == OSC_SCREEN_FIGHT && !g_is_bound(g);
    if (p || gk) {
        if (!g->fight.paused) {
            g->fight.paused = true;
            if (gk) g->flow.show_moves = true;
        } else {
            g->fight.timer_off = false;   /* DXTimer1.Enabled := True */
            g->fight.paused = false;
            g->flow.show_moves = false;
        }
    }
}

/* DXTimer1Timer: the whole tick, then the VELOCIDADE bar (0x4ae1bd) and, if g_paused,
 * DXTimer1.Enabled := False so the last frame stays on screen. */
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    if (g->fight.timer_off) return;
    game_tick_body(g, in1, in2);
    g->flow.speed_overlay_draw = g->flow.speed_overlay > 0 && g->flow.screen != OSC_SCREEN_MENU_F1 && g->flow.screen != OSC_SCREEN_MENU_F2;
    if (g->flow.speed_overlay_draw) g->flow.speed_overlay--;
    if (g->fight.paused) g->fight.timer_off = true;
}

static void speed_bar(osc_game *g)
{
    texto(g, 200, 260, osc_text_const(1642), 'c', 8, k_white);   /* VELOCIDADE */
    int v = 238 - (int)nearbyint(g->flow.speed_delay / 1.05);
    barra(g, 160, 278, 240, 280, k_white);
    barra(g, v, 275, v + 2, 283, rgb(255, 50, 50));
}

static void osc_game_render_screen(osc_game *g);

static void footer(osc_game *g)
{
    char buf[32];
    if (g->settings.show_level) {
        snprintf(buf, sizeof buf, "%s%d", osc_text(OSC_MEMO_TEXTO, 17, g->settings.lang), g->settings.level);   /* NIVEL-n */
        osc_text_ex(g, 200, 285, buf, 'c', 8, 255, 255, 255);
    }
    if (g->settings.show_credits) {
        osc_text_ex(g, 300, 285, osc_text(OSC_MEMO_TEXTO, 18, g->settings.lang), 'c', 8, 255, 255, 255);   /* CREDITOS */
        snprintf(buf, sizeof buf, "%d", g->flow.credits);
        osc_text_ex(g, 395, 285, buf, 'e', 8, 255, 255, 255);
    }
}

void osc_game_render(osc_game *g)
{
    osc_game_render_screen(g);
    if (g->flow.screen != OSC_SCREEN_MENU_F1 && g->flow.screen != OSC_SCREEN_MENU_F2) footer(g);
    if (g->flow.speed_overlay_draw) speed_bar(g);
}

static void osc_game_render_screen(osc_game *g)
{
    switch (g->flow.screen) {
    case OSC_SCREEN_SELECT: select_render(g); break;
    case OSC_SCREEN_VS: vs_render(g); break;
    case OSC_SCREEN_CONTINUE: continue_render(g); break;
    case OSC_SCREEN_ATTRACT: attract_render(g); break;
    case OSC_SCREEN_RECORDS: records_render(g); break;
    case OSC_SCREEN_NAME: name_render(g); break;
    case OSC_SCREEN_MENU_F1: case OSC_SCREEN_MENU_F2: osc_menu_render(g); break;
    case OSC_SCREEN_ENDING: case OSC_SCREEN_CREDITS: osc_ending_render(g); break;
    default:
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
        break;
    }
}
