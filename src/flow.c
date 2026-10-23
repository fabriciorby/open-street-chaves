/* Screens outside the fight: character select (state 4), VS (state 5),
 * continue (state 9), and the dispatch between them (07-screens-and-text.md).
 * Text is drawn with SDL's debug font until the original font is handled. */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "font.h"
#include "game.h"
#include "canvas.h"
#include "wav.h"
#include <math.h>

static int lround_i(double v) { return (int)SDL_lround(v); }

void osc_text_at(osc_game *g, float x, float y, const char *s, bool centered)
{
    if (!s || !*s) return;
    if (osc_font_ready()) { osc_text_ex(g, x, y, s, centered ? 'c' : 'l', 8, 255, 255, 255); return; }
    /* the debug font is ASCII only: strip UTF-8 accents to the base letter */
    char buf[128];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < sizeof buf - 1; p++) {
        if (*p < 0x80) buf[n++] = (char)*p;
        else if ((*p & 0xe0) == 0xc0 && p[1]) { unsigned cp = ((*p & 0x1f) << 6) | (p[1] & 0x3f); p++;
            const char *map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            buf[n++] = (cp >= 0xc0 && cp <= 0xff) ? map[cp - 0xc0] : '?'; }
    }
    buf[n] = 0;
    if (centered) x -= (float)(n * 8) / 2.f;
    SDL_SetRenderDrawColor(g->ren, 255, 255, 255, 255);
    SDL_RenderDebugText(g->ren, x, y, buf);
}

static void fill(osc_game *g, float x1, float y1, float x2, float y2, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_FRect rc = {SDL_min(x1, x2), SDL_min(y1, y2), SDL_fabsf(x2 - x1), SDL_fabsf(y2 - y1)};
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderFillRect(g->ren, &rc);
}

static void rect(osc_game *g, float x1, float y1, float x2, float y2, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_FRect rc = {x1, y1, x2 - x1, y2 - y1};
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderRect(g->ren, &rc);
}

/* ---- character select (DXTimer1Timer state 4, 0x4a22f0..0x4a342f) ------------- */
static int cell_x(int sel) { return 49 * ((sel - 1) % 8) + 3; }    /* at full scroll */
static int cell_y(int sel) { return sel <= 8 ? 163 : 223; }

static void select_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t ins[3] = {0, in1, in2};
    g->screen_tick++;
    g->no_push = false;
    osc_audio_stop_music();
    if (g->sel_sub < 300) {                       /* g_selectSub 1: slide in by 20 */
        g->sel_sub += 20;
        if (g->sel_sub >= 300) g->sel_sub = 300;
        return;
    }
    if (g->timer_sec >= 1 && --g->timer_sub <= 0) { g->timer_sec--; g->timer_sub = 25; }
    if (++g->ciclo >= 25) g->ciclo = 0;
    for (int p = 1; p <= 2; p++) {
        osc_fighter *f = &g->p[p];
        bool human = p == 1 ? g->p1_in : g->p2_in;
        uint8_t in = ins[p], pressed = (uint8_t)(in & ~f->in_prev);
        f->in_prev = in;
        if (!human) { g->sel_locked[p] = 1; continue; }
        if (g->sel_locked[p]) continue;
        if (pressed & OSC_IN_LEFT) { g->sel[p]--; osc_play_snd(g, 1); }
        if (pressed & OSC_IN_RIGHT) { g->sel[p]++; osc_play_snd(g, 1); }
        if ((pressed & OSC_IN_UP) && g->sel[p] > 8) { g->sel[p] -= 8; osc_play_snd(g, 1); }
        if ((pressed & OSC_IN_DOWN) && g->sel[p] <= 8) { g->sel[p] += 8; osc_play_snd(g, 1); }
        if (pressed & OSC_IN_B1) { g->sel_locked[p] = 1; osc_play_snd(g, 2); }
        if (g->sel[p] < 1) g->sel[p] = 15;
        if (g->sel[p] > 15) g->sel[p] = 1;
    }
    if (g->timer_sec == 0 && !(g->sel_locked[1] && g->sel_locked[2])) {
        g->sel_locked[1] = g->sel_locked[2] = 1;
        osc_play_snd(g, 2);
    }
    if (g->sel_locked[1] && g->sel_locked[2]) {
        bool both = g->p1_in && g->p2_in;
        if (!both) {                        /* criaadversario for the lone human */
            int h = g->p1_in ? 1 : 2;
            osc_make_ladder(g, g->sel[h]);
            g->sel[3 - h] = g->ladder[g->fight_no];
        }
        g->p[1].human = g->p1_in;
        g->p[2].human = g->p2_in;
        g->p[1].char_id = g->sel[1]; g->p[2].char_id = g->sel[2];
        g->screen = both ? OSC_SCREEN_FIGHT : OSC_SCREEN_VS;
        g->screen_tick = 0; g->ciclo = 0;
        if (g->screen == OSC_SCREEN_FIGHT) osc_fight_start(g, g->sel[1], g->sel[2], 0);
    }
}

static void select_render(osc_game *g)
{
    int c2 = g->sel_sub, k = (int)SDL_lround(c2 * 1.35);
    barra(g, 0, 0, 400, 300, k_black);
    barra(g, 0, 300 - c2, 200, 600 - c2, rgb(120, 120, 255));
    barra(g, 200, c2 - 300, 400, c2, rgb(255, 100, 100));
    /* Icone 11/12 (by language) tiled and drifting down with g_timerB */
    int ic = g->lang == 0 ? 11 : 12;
    if (ic < g->n_icone)
        for (int j = 0; j < 4; j++)
            for (int i = 0; i < 2; i++) {
                draw_tex(g, g->icone[ic], (float)(145 * j - 67), (float)(200 * i - 200 + 8 * g->timer_sub), 255, false);
                draw_tex(g, g->icone[ic], (float)(145 * j), (float)(200 * i - 100 + 8 * g->timer_sub), 255, false);
            }
    barra(g, k - 410, 156, k, 290, rgb(30, 50, 70));
    barra(g, k - 410, 158, k, 160, k_white);
    barra(g, k - 410, 286, k, 288, k_white);
    /* the grid: every face at alpha 60 */
    for (int i = 0; i < 15; i++) {
        int x = k + 49 * (i % 8) - 402, y = i < 8 ? 163 : 223;
        draw_tex_rect(g, g->face_small[i + 1], x, y, x + 50, y + 60, 60);
    }
    if (g->sel_sub < 300) return;
    char buf[32];
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->p1_in : g->p2_in)) continue;
        int c = g->sel[p];
        draw_tex(g, g->face_big[c], p == 1 ? 52.f : 252.f, 25.f, 255, false);
        if (!g->sel_locked[p])
            draw_tex(g, g->face_small[c], (float)(k + 49 * ((c - 1) % 8) - 402), (float)cell_y(c), 255, false);
        texto(g, p == 1 ? 100 : 300, 130 - (int)SDL_lround(cos(g->ciclo / 4.3) * 4.0),
              osc_text(OSC_MEMO_NOMES, c, g->lang), 'c', 12, rgb(180, 220, 160));
    }
    int kt = (int)SDL_lround(cos(g->timer_sub / 4.0) * 20.0);
    texto(g, 200, (int)SDL_lround(c2 * 0.3) - 96, osc_text(OSC_MEMO_TEXTO, 7, g->lang), 'c', 18,
          rgb(230 + kt, 230 + kt, 255));                                                   /* SELECIONE SEU LUTADOR */
    osc_rgb tc = g->timer_sec <= 5 && (g->timer_sub & 1) == 0 ? rgb(250, 220, 220) : rgb(250, 150, 150);
    snprintf(buf, sizeof buf, "%d", g->timer_sec);
    texto(g, 200, 15, buf, 'c', 22, tc);
    /* "1P" / "2P" over the cursor cell while choosing */
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->p1_in : g->p2_in) || g->sel_locked[p]) continue;
        int c = g->sel[p], add = (g->ciclo & 1) == 0 ? 50 : 0;
        snprintf(buf, sizeof buf, "%dP", p);
        if (p == 1) texto(g, cell_x(c), cell_y(c) - 3, buf, '-', 10, rgb(100 + add, 100 + add, 200 + add));
        else texto(g, cell_x(c) + 35, cell_y(c) - 3, buf, '-', 10, rgb(200 + add, 50 + add, 50 + add));
    }
}

/* ---- VS / ladder (state 5, 0x4a342f..0x4a3f9f) ---------------------------------- */
static void vs_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    int c2 = g->screen_tick;
    if (g->fight_no < 1) g->fight_no = 1;
    if (g->fight_no > 8) g->fight_no = 8;
    osc_audio_stop_music();
    if (c2 == 0) {
        /* the CPU opponent picks the stage (osc_stage_for) and it is shown as a preview */
        int opp = g->p1_in ? 2 : 1;
        g->sel[opp] = g->ladder[g->fight_no];
        osc_load_stage(g, osc_stage_for(g, g->sel[opp]));
    }
    if (++g->ciclo > 15) g->ciclo = 0;
    if (++g->screen_tick > 1000) g->screen_tick = 1000;
    uint8_t held = (uint8_t)((g->p1_in ? in1 : 0) | (g->p2_in ? in2 : 0));
    if ((held & OSC_IN_B1) && g->screen_tick > 0) g->screen_tick += 5;
    if (c2 > 150) osc_fight_start(g, g->sel[1], g->sel[2], g->stage_id);
}

static void vs_render(osc_game *g)
{
    int c2 = g->screen_tick;
    for (int i = 0; i <= 60; i++) barra(g, 0, 10 * i, 400, 10 * i + 10, rgb(2 * i + 10, 3 * i + 20, 4 * i + 30));
    for (int p = 1; p <= 2; p++) {
        if (!(p == 1 ? g->p1_in : g->p2_in)) continue;
        int tx_x = c2 < 30 ? 200 - (c2 - 30) * 20 : c2 <= 100 ? 200 : 200 - (c2 - 100) * 20;
        texto(g, tx_x, 135, osc_text(OSC_MEMO_TEXTO, 19, g->lang), 'c', 26, rgb(150, 255, 180));   /* PRÓXIMA BATALHA */
        if (c2 > 40) texto(g, 200, 180, osc_text(OSC_MEMO_NOMES, g->ladder[g->fight_no], g->lang), 'c', 15, k_white);
        for (int i = 1; i <= 8; i++) {
            int c = g->ladder[i];
            float y = c2 < 50 ? (float)(470 - 5 * c2) : 220.f;
            bool shown = i <= g->fight_no || g->unlock_churros;
            if (shown && c >= 1 && c <= 15) draw_tex(g, g->face_small[c], (float)(48 * i - 40), y, 255, true);
            if (i < g->fight_no && 10 < g->n_icone)
                draw_tex(g, g->icone[10], (float)(48 * i - 35), c2 < 50 ? (float)(485 - 5 * c2) : 235.f, 255, false);
            if (!shown && 9 < g->n_icone) draw_tex(g, g->icone[9], (float)(48 * i - 40), y, 255, false);
        }
    }
    if (c2 >= 50 && g->ciclo <= 10 && 8 < g->n_icone)
        draw_tex(g, g->icone[8], (float)(48 * g->fight_no - 20), 215.f, 255, false);
    if (c2 > 60 && g->stage) {
        draw_tex_rect(g, g->stage, 80, 6, 320, 134, 255);
        texto(g, 200, 115, osc_text(OSC_MEMO_CENARIOS, g->stage_id - 1, g->lang), 'c', 9, k_white);
    }
    if (c2 > 120)
        for (int i = 0; i < 16; i++) barra(g, 0, 20 * i, 400, 20 * i + c2 - 120, k_black);
}

/* ---- continue (state 9, 0x4ab647..0x4ab8fe) --------------------------------------- */
static void continue_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    (void)in1; (void)in2;   /* continuing takes a coin and a start button (handle_keys) */
    g->screen_tick++;
    if (g->timer_sec > 0 || g->timer_sub > 0) {
        if (++g->ciclo > 10) g->ciclo = 0;
    }
    if (g->timer_sub > 0) g->timer_sub--;
    if (g->timer_sub == 0 && g->timer_sec > 0) { g->timer_sec--; g->timer_sub = 25; }
    if (g->timer_sec == 0 && g->timer_sub == 0) {
        if (++g->ciclo > 150) {   /* game over -> records (state 8), then attract */
            g->screen = OSC_SCREEN_RECORDS; g->screen_tick = 0; g->menu_cam = 0; g->ciclo = 0;
            g->p1_in = g->p2_in = false;
            g->p[1].human = g->p[2].human = false;
        }
    }
}

static void continue_render(osc_game *g)
{
    char buf[16];
    for (int i = 0; i <= 60; i++) barra(g, 0, 5 * i, 400, 5 * i + 5, rgb(3 * i + 10, 3 * i + 50, 3 * i + 30));
    if (g->timer_sec > 0 || g->timer_sub > 0) {
        texto(g, 200, 20, osc_text(OSC_MEMO_TEXTO, 23, g->lang), 'c', 35, rgb(255, 120, 120));   /* CONTINUE ? */
        if (g->timer_sec > 3 || g->ciclo < 8) {
            snprintf(buf, sizeof buf, "%d", g->timer_sec);
            texto(g, 320, 70, buf, 'c', 120, rgb(255, 25 * g->timer_sec, 25 * g->timer_sec));
        }
    } else {
        texto(g, 200, 110, osc_text(OSC_MEMO_TEXTO, 22, g->lang), 'c', 36, rgb(255, 0, 0));   /* FIM DO JOGO */
        if (g->ciclo < 15) barra(g, 0, 10 * g->ciclo, 400, 300 - 10 * g->ciclo, k_white);
    }
}

/* ---- dispatch ---------------------------------------------------------------------- */

/* TForm1.texto (0x48e720): Comic Sans MS at `size` points with a black shadow at
 * (+1,+1). Until a bitmap font is produced we scale SDL's 8 px debug font by
 * size/6 (roughly points -> pixels at 96 dpi). */
void osc_text_ex(osc_game *g, float x, float y, const char *s, char align, int size, Uint8 r, Uint8 gg, Uint8 b)
{
    if (!s || !*s) return;
    if (osc_font_ready()) {
        float w = osc_font_width(s, size);
        if (align == 'c') x -= w / 2.f;
        else if (align == 'e') x -= w;
        osc_font_draw(g->ren, x, y, s, size, r, gg, b);
        return;
    }
    char buf[160];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < sizeof buf - 1; p++) {
        if (*p < 0x80) buf[n++] = (char)*p;
        else if ((*p & 0xe0) == 0xc0 && p[1]) { unsigned cp = ((*p & 0x1f) << 6) | (p[1] & 0x3f); p++;
            const char *map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            buf[n++] = (cp >= 0xc0 && cp <= 0xff) ? map[cp - 0xc0] : '?'; }
    }
    buf[n] = 0;
    /* Comic Sans at `size` pt is about size*1.3 px tall and size*0.8 px per letter */
    float sx = (float)size / 10.f, sy = (float)size / 8.f;
    if (sx < 0.7f) sx = 0.7f;
    if (sy < 0.9f) sy = 0.9f;
    float w = (float)n * 8.f * sx;
    if (align == 'c') x -= w / 2.f;
    else if (align == 'e') x -= w;
    SDL_SetRenderScale(g->ren, sx, sy);
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderDebugText(g->ren, (x + 1.f) / sx, (y + 1.f) / sy, buf);
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderDebugText(g->ren, x / sx, y / sy, buf);
    SDL_SetRenderScale(g->ren, 1.f, 1.f);
}

static void blit(osc_game *g, SDL_Texture *t, int w, int h, float x, float y)
{
    if (!t) return;
    SDL_FRect d = {x, y, (float)w, (float)h};
    SDL_RenderTexture(g->ren, t, NULL, &d);
}
static void blit_ab(osc_game *g, int i, float x, float y)
{
    if (i >= 0 && i < g->n_abertura) blit(g, g->abertura[i], g->abertura_w[i], g->abertura_h[i], x, y);
}

/* ---- state 1: attract / title (07b-layout.md "Attract") ------------------- */
void osc_go_attract(osc_game *g, int tick)
{
    g->screen = OSC_SCREEN_ATTRACT;
    g->screen_tick = tick;
    g->menu_cam = tick > 235 ? -2 * (tick - 235) : 0;   /* where the scroll would be at that tick */
    if (g->menu_cam < -200) g->menu_cam = -200;
    g->ciclo = 0;
    g->demo = 0;
    g->p1_in = g->p2_in = false;
    g->p[1].human = g->p[2].human = false;
}

static void start_demo(osc_game *g)
{
    int stage = lround_i(osc_random(g, 120) / 10.0) + 1;
    if (stage < 1) stage = 1;
    if (stage > 12) stage = 12;
    int c1 = lround_i(osc_random(g, 150) / 10.0) + 1, c2 = lround_i(osc_random(g, 150) / 10.0) + 1;
    if (c1 < 1) c1 = 1; if (c1 > 15) c1 = 15;
    if (c2 < 1) c2 = 1; if (c2 > 15) c2 = 15;
    g->p1_in = g->p2_in = false;
    g->p[1].human = g->p[2].human = false;
    g->p[1].char_id = c1; g->p[2].char_id = c2;
    g->sel[1] = c1; g->sel[2] = c2;
    g->demo = 600;
    g->screen = OSC_SCREEN_FIGHT; g->screen_tick = 0;
    osc_fight_start(g, c1, c2, stage);
}

static void attract_tick(osc_game *g)
{
    int c = g->screen_tick;
    if (c == 100) g->menu_cam = 0;
    if (c == 200 && g->n_sounds > 9) osc_play_snd(g, 9);
    if (g->credits > 0) { g->screen_tick = c = 500; g->menu_cam = -200; }
    g->ciclo++;
    if (g->ciclo > 20) g->ciclo = 0;
    if (g->screen_tick < 1000) g->screen_tick++;
    if (c > 235) g->menu_cam -= 2;
    if (g->menu_cam < -200) g->menu_cam = -200;
    if (c > 650) start_demo(g);
}

static void attract_render(osc_game *g)
{
    int c = g->screen_tick, cam = g->menu_cam;
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (c < 100) {
        fill(g, 0, 0, 400, 300, 120, 120, 250);
        blit_ab(g, 14, 110, 10);                                               /* Vencedores */
        osc_text_ex(g, 200, 210, osc_text(OSC_MEMO_TEXTO, 12, g->lang), 'c', 16, 250, 200, 0);
    }
    if (c >= 100 && c <= 370) {
        for (int i = 0; i < 21; i++) fill(g, 0, (float)(15 * i), 400, (float)(15 * i + 15), (Uint8)(120 + 2 * i), (Uint8)(120 + 2 * i), 200);
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
        fill(g, 147, (float)(-155 - cam), 270, (float)(-48 - cam), 0, 0, 0);
        rect(g, 149, (float)(-153 - cam), 268, (float)(-50 - cam), 255, 255, 255);
        for (int i = 0; i < 8; i++) blit_ab(g, 4, (float)(50 * i), (float)(220 - cam));
        int bob = g->ciclo < 10 ? 4 : 0;
        for (int i = 0; i < 9; i++) {
            blit_ab(g, 7, (float)(50 * i), (float)(245 - cam + bob));
            blit_ab(g, 5, (float)(50 * i - 15), (float)(245 - cam + bob));
            blit_ab(g, 6, (float)(50 * i + 15), (float)(245 - cam + bob));
        }
        int bob2 = g->ciclo > 10 ? 5 : 0;
        if (c < 200) { blit_ab(g, 8, 210, (float)(185 + bob2 - cam)); blit_ab(g, 9, 50, (float)(178 + bob2 - cam)); }
        else {
            blit_ab(g, 11, (float)(60 - 4 * (c - 200)), (float)(180 + 2 * (c - 200) - cam));   /* Seu Madruga caindo */
            blit_ab(g, 10, 100, (float)(190 - cam));                                          /* Chaves dando soco */
        }
    }
    /* logo zoom: k = 4 until 400, grows to 24 at 420, shrinks to 2 at 438 */
    int k = c < 400 ? 4 : c <= 420 ? c - 396 : c <= 438 ? 440 - c : 2;
    if (k > 0) {
        int k1 = lround_i(223.0 / k), k2 = lround_i(117.0 / k);
        int li = g->lang == 1 ? 13 : 12;
        if (li < g->n_abertura && g->abertura[li]) {
            SDL_FRect d = {(float)(205 - k1), (float)(-100 - cam - k2), (float)(2 * k1), (float)(2 * k2)};
            SDL_RenderTexture(g->ren, g->abertura[li], NULL, &d);
        }
    }
    if (c > 440) {
        osc_text_ex(g, 320, 140, "V1.5A", 'l', 8, 255, 255, 255);
        const char *sub = osc_text(OSC_MEMO_TEXTO, 21, g->lang);                 /* O LUTADOR DA VILA */
        if (c < 500) osc_text_ex(g, (float)((c & 1) ? 200 - 4 * (500 - c) : 200 + 4 * (500 - c)), 170, sub, 'c', 12, 90, 90, 255);
        else osc_text_ex(g, 200, 170, sub, 'c', 12, 90, 90, 255);
    }
    blit_ab(g, 15, 5, 280);                                                    /* Cybergamba */
    osc_text_ex(g, 200, 253, osc_text(OSC_MEMO_TEXTO, 25, g->lang), 'c', 8, 255, 255, 255);
    osc_text_ex(g, 200, 267, osc_text(OSC_MEMO_TEXTO, 30, g->lang), 'c', 8, 255, 255, 255);
    if (g->ciclo < 12)
        osc_text_ex(g, 200, 235, osc_text(OSC_MEMO_TEXTO, g->credits == 0 ? 5 : 6, g->lang), 'c', 12, 255, 255, 255);
}

/* ---- state 8: game over / top 10 (07b-layout.md "Records") ---------------- */
static void records_tick(osc_game *g)
{
    g->ciclo++;
    if (g->ciclo > 20) g->ciclo = 0;
    if (g->screen_tick < 1000) g->screen_tick++;
    if (g->screen_tick > 80) g->menu_cam -= 5;
    if (g->menu_cam < -320) g->menu_cam = -320;
    if (g->screen_tick > 280) osc_go_attract(g, 0);
}

static void records_render(osc_game *g)
{
    for (int i = 0; i < 21; i++) barra(g, 0, 20 * i, 400, 20 * i + 20, rgb(5 * i + 100, 5 * i + 130, 5 * i + 160));
    for (int i = 1; i <= 10; i++) {
        const osc_record *r = &g->records[i - 1];
        int y = 60 * i - 350 - g->menu_cam;
        int hl = i == 1 && (g->ciclo & 1) == 0 ? 80 : 0;   /* the leader blinks */
        osc_rgb c = rgb(255, 255 - hl, 255 - hl);
        char num[16], sc[16];
        snprintf(num, sizeof num, i < 10 ? "0%d -" : "%d -", i);
        snprintf(sc, sizeof sc, "%d", r->score);
        texto(g, 50, y, num, '-', 15, c);
        texto(g, 100, y, r->name, '-', 15, c);
        if (r->char_id >= 1 && r->char_id <= 15) draw_tex(g, g->face_small[r->char_id], 160.f, (float)(y - 20), 255, false);
        texto(g, 230, y, sc, '-', 15, c);
    }
    if (g->screen_tick > 240)
        for (int i = 0; i < 16; i++) barra(g, 0, 20 * i, 400, 20 * i + g->screen_tick - 240, k_black);
    if (g->ciclo < 18) texto(g, 200, 5, osc_text(OSC_MEMO_TEXTO, 20, g->lang), 'c', 12, k_white);    /* OS 10 MELHORES */
    if (g->ciclo < 10) texto(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 5, g->lang), 'c', 10, k_white);   /* INSIRA FICHAS */
}


/* ---- state 12: name entry for the records (07b "Name entry") -------------- */
static void insert_record(osc_game *g, int p)
{
    /* criarecorde(10, name) + ordenarecordista: replace the last slot, sort by score */
    osc_record *r = &g->records[9];
    memcpy(r->name, g->name[p], 4);
    r->char_id = g->p[p].char_id;
    r->score = g->p[p].score;
    for (int i = 9; i > 0 && g->records[i].score > g->records[i - 1].score; i--) {
        osc_record t = g->records[i]; g->records[i] = g->records[i - 1]; g->records[i - 1] = t;
    }
}

static void leave_name_entry(osc_game *g)
{
    for (int p = 1; p <= 2; p++) if (g->name_active[p]) { insert_record(g, p); g->name_active[p] = false; }
    if (g->name_after == OSC_SCREEN_ENDING) { osc_ending_start(g, g->ending_char); return; }
    g->screen = g->name_after; g->screen_tick = 0; g->ciclo = 0; g->menu_cam = 0;
    if (g->screen == OSC_SCREEN_CONTINUE) { g->timer_sec = 9; g->timer_sub = 25; }
    if (g->screen == OSC_SCREEN_RECORDS) { g->p1_in = g->p2_in = false; }
}

/* End of a match (DXTimer1Timer round end, re == 100 and re > 220): the loser drops out
 * (beating the CPU advances the ladder); with a human left the ladder goes on, past
 * fight 8 that human sees the ending; with nobody left it is continue time. Either way a
 * score above the 10th record asks for a name first (state 12). */
static void after_match(osc_game *g, int winner)
{
    int loser = 3 - winner;
    if (!g->p[loser].human) g->fight_no++;
    g->p[loser].human = false;
    if (loser == 1) g->p1_in = false; else g->p2_in = false;
    bool any = g->p[1].human || g->p[2].human;
    if (any && g->fight_no <= 8) {
        if (!g->ladder[1]) osc_make_ladder(g, g->p[winner].char_id);   /* 2P winner goes on alone */
        g->sel[winner] = g->p[winner].char_id;
        g->screen = OSC_SCREEN_VS; g->screen_tick = 0; g->ciclo = 0;
        return;
    }
    g->name_after = any ? OSC_SCREEN_ENDING : OSC_SCREEN_CONTINUE;
    g->ending_char = g->p[winner].char_id;
    bool ask = false;
    for (int p = 1; p <= 2; p++) {
        g->name_active[p] = g->p[p].score > g->records[9].score;
        if (!g->name_active[p]) continue;
        memcpy(g->name[p], "AAA", 4);
        g->name_pos[p] = 1;
        g->p[p].in_prev = 0xff;
        ask = true;
    }
    if (ask) { g->screen = OSC_SCREEN_NAME; g->screen_tick = 0; g->ciclo = 0; return; }
    leave_name_entry(g);
}

static void name_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t ins[3] = {0, in1, in2};
    g->ciclo++;
    if (g->ciclo > 20) g->ciclo = 0;
    if (g->screen_tick < 1000) g->screen_tick++;
    bool any = false;
    for (int p = 1; p <= 2; p++) {
        if (!g->name_active[p]) continue;
        uint8_t pressed = (uint8_t)(ins[p] & ~g->p[p].in_prev);
        g->p[p].in_prev = ins[p];
        int *pos = &g->name_pos[p];
        if (*pos > 3) continue;
        any = true;
        if (pressed & OSC_IN_LEFT) (*pos)--;
        if (pressed & OSC_IN_RIGHT) (*pos)++;
        if (*pos < 1) *pos = 1;
        if (*pos > 3) *pos = 3;
        char *c = &g->name[p][*pos - 1];
        if (pressed & OSC_IN_UP) (*c)++;
        if (pressed & OSC_IN_DOWN) (*c)--;
        if (*c < '0') *c = '0';
        if (*c > 'Z') *c = 'Z';
        if (pressed & OSC_IN_B1) (*pos)++;
    }
    /* the first player to pass the third letter (or the 40 s limit) ends the screen; only
     * the names that were finished are recorded (state 12, 0x4ae0c4) */
    bool done = !any || g->screen_tick >= 1000;
    for (int p = 1; p <= 2; p++) if (g->name_active[p] && g->name_pos[p] > 3) done = true;
    if (done) {
        if (g->screen_tick < 1000)
            for (int p = 1; p <= 2; p++) if (g->name_active[p] && g->name_pos[p] <= 3) g->name_active[p] = false;
        leave_name_entry(g);
    }
}

static void name_render(osc_game *g)
{
    barra(g, 0, 0, 400, 300, k_black);
    for (int p = 1; p <= 2; p++) {
        if (!g->name_active[p]) continue;
        int x0 = (p - 1) * 200 + 20;
        for (int i = 0; i < 3; i++) {
            char l[2] = {g->name[p][i], 0};
            texto(g, x0 + 15 * i, 5, l, '-', 15, k_white);
        }
        if (g->ciclo < 15 && g->name_pos[p] <= 3) texto(g, x0 + 15 * (g->name_pos[p] - 1), 4, "-", '-', 27, k_white);
    }
}

/* ---- coins and start buttons (FormKeyDown '5', '1', '2') ------------------ */
static void join_player(osc_game *g, int p)
{
    g->credits--;
    if (p == 1) g->p1_in = true; else g->p2_in = true;
    g->p[p].score = 0; g->p[p].wins = 0;
    g->sel[p] = p;
    g->sel_locked[p] = 0;
    g->timer_sec = 30; g->timer_sub = 25;   /* FormKeyDown '1'/'2' */
}

static void handle_keys(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* convenience for keyboards and pads: the first button on the title inserts a coin and starts */
    if (g->screen == OSC_SCREEN_ATTRACT || g->screen == OSC_SCREEN_RECORDS || g->screen == OSC_SCREEN_CONTINUE || g->demo > 0) {
        uint8_t p1 = (uint8_t)(in1 & ~g->p[1].in_prev), p2 = (uint8_t)(in2 & ~g->p[2].in_prev);
        g->p[1].in_prev = in1; g->p[2].in_prev = in2;
        if (p1 & OSC_IN_B1) { if (g->credits == 0) g->key_coin = true; g->key_start1 = true; }
        if (p2 & OSC_IN_B1) { if (g->credits == 0) g->key_coin = true; g->key_start2 = true; }
    }
    if (g->key_f1 || g->key_f2) {
        bool f1 = g->key_f1;
        g->key_f1 = g->key_f2 = false;
        if (g->screen == OSC_SCREEN_ATTRACT) { osc_menu_enter(g, f1 ? 2 : 3); return; }
    }
    if (g->key_coin) {
        g->key_coin = false;
        if (g->credits < 99) {
            g->credits++;
            if (g->n_sounds > 0) osc_play_snd(g, 0);
            if (g->demo > 0 || g->screen == OSC_SCREEN_RECORDS) osc_go_attract(g, 500);
        }
    }
    for (int p = 1; p <= 2; p++) {
        bool *key = p == 1 ? &g->key_start1 : &g->key_start2;
        if (!*key) continue;
        *key = false;
        bool in = p == 1 ? g->p1_in : g->p2_in;
        if (g->credits == 0 || in) continue;
        if (g->screen == OSC_SCREEN_ATTRACT) {
            join_player(g, p);
            g->fight_no = g->unlock_gentalha ? 8 : 1;
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
            g->sel_locked[1] = g->sel_locked[2] = 0;
            g->ai_p2 = !g->p2_in;
        } else if (g->screen == OSC_SCREEN_SELECT && !g->no_push) {
            join_player(g, p);
            g->ai_p2 = !g->p2_in;
        } else if (g->screen == OSC_SCREEN_FIGHT && g->demo == 0 && !g->no_push && g->challenge == 0) {
            join_player(g, p);
            g->p[p].human = true;
            g->challenge = 100;
        } else if (g->screen == OSC_SCREEN_CONTINUE && g->timer_sec > 0 && g->timer_sub > 0) {
            /* FormKeyDown state 9: back to the same ladder (VS) or, before any fight, to select */
            int keep = g->p[p].char_id;
            join_player(g, p);
            g->sel[p] = keep ? keep : p;   /* same fighter, same ladder */
            g->p[p].human = true;
            g->sel_locked[p] = 1;
            g->ciclo = 0; g->screen_tick = 0;
            g->ai_p2 = !g->p2_in;
            if (g->ladder[1]) g->screen = OSC_SCREEN_VS;
            else { g->screen = OSC_SCREEN_SELECT; g->sel_sub = 0; g->sel_locked[p] = 0; }
        }
    }
}

static void game_tick_body(osc_game *g, uint8_t in1, uint8_t in2)
{
    handle_keys(g, in1, in2);
    switch (g->screen) {
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
        if (g->demo > 0) {
            g->demo--;
            if (g->demo < 6) { g->demo = 0; g->screen = OSC_SCREEN_RECORDS; g->screen_tick = 0; g->menu_cam = 0; g->ciclo = 0; }
            break;
        }
        if (g->match_over) {
            int winner = g->match_over;
            g->match_over = 0;
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
            if (g->keymap[p][k] == SDL_SCANCODE_G) return true;
    return false;
}

/* FormKeyDown (0x49e2e8): speed, picture shift, pause and the move list */
void osc_game_key(osc_game *g, SDL_Keycode key)
{
    if (key == SDLK_MINUS && g->speed_delay < 0x50) g->speed_delay++;
    if (key == SDLK_EQUALS && g->speed_delay >= 1) g->speed_delay--;
    if (key == SDLK_MINUS || key == SDLK_EQUALS) g->speed_overlay = 30;
    if (key == SDLK_COMMA && g->view_dx > -40) g->view_dx--;
    if (key == SDLK_PERIOD && g->view_dx < 40) g->view_dx++;
    bool p = key == SDLK_P, gk = key == SDLK_G && g->screen == OSC_SCREEN_FIGHT && !g_is_bound(g);
    if (p || gk) {
        if (!g->paused) {
            g->paused = true;
            if (gk) g->show_moves = true;
        } else {
            g->timer_off = false;   /* DXTimer1.Enabled := True */
            g->paused = false;
            g->show_moves = false;
        }
    }
}

/* DXTimer1Timer: the whole tick, then the VELOCIDADE bar (0x4ae1bd) and, if g_paused,
 * DXTimer1.Enabled := False so the last frame stays on screen. */
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    if (g->timer_off) return;
    game_tick_body(g, in1, in2);
    g->speed_overlay_draw = g->speed_overlay > 0 && g->screen != OSC_SCREEN_MENU_F1 && g->screen != OSC_SCREEN_MENU_F2;
    if (g->speed_overlay_draw) g->speed_overlay--;
    if (g->paused) g->timer_off = true;
}

static void speed_bar(osc_game *g)
{
    texto(g, 200, 260, osc_text_const(1642), 'c', 8, k_white);   /* VELOCIDADE */
    int v = 238 - (int)nearbyint(g->speed_delay / 1.05);
    barra(g, 160, 278, 240, 280, k_white);
    barra(g, v, 275, v + 2, 283, rgb(255, 50, 50));
}

static void osc_game_render_screen(osc_game *g);

static void footer(osc_game *g)
{
    char buf[32];
    if (g->show_level) {
        snprintf(buf, sizeof buf, "%s%d", osc_text(OSC_MEMO_TEXTO, 17, g->lang), g->level);   /* NIVEL-n */
        osc_text_ex(g, 200, 285, buf, 'c', 8, 255, 255, 255);
    }
    if (g->show_credits) {
        osc_text_ex(g, 300, 285, osc_text(OSC_MEMO_TEXTO, 18, g->lang), 'c', 8, 255, 255, 255);   /* CREDITOS */
        snprintf(buf, sizeof buf, "%d", g->credits);
        osc_text_ex(g, 395, 285, buf, 'e', 8, 255, 255, 255);
    }
}

void osc_game_render(osc_game *g)
{
    osc_game_render_screen(g);
    if (g->screen != OSC_SCREEN_MENU_F1 && g->screen != OSC_SCREEN_MENU_F2) footer(g);
    if (g->speed_overlay_draw) speed_bar(g);
}

static void osc_game_render_screen(osc_game *g)
{
    switch (g->screen) {
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
        if (g->demo > 0) {
            if (g->demo < 40) fill(g, 0, 0, 400, 320, 0, 0, 0);
            if (g->ciclo < 20) osc_text_ex(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 5, g->lang), 'c', 12, 255, 255, 255);
            int li = g->lang == 1 ? 12 : 11;   /* Logo Transparente */
            if (g->demo < 500 && li < g->n_icone) blit(g, g->icone[li], g->icone_w[li], g->icone_h[li], (float)(390 - g->icone_w[li]), (float)(280 - g->icone_h[li]));
        }
        break;
    }
}
