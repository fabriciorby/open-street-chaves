/* Screens outside the fight: character select (state 4), VS (state 5),
 * continue (state 9), and the dispatch between them (07-screens-and-text.md).
 * Text is drawn with SDL's debug font until the original font is handled. */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "game.h"

static int lround_i(double v) { return (int)SDL_lround(v); }

void osc_text_at(osc_game *g, float x, float y, const char *s, bool centered)
{
    if (!s || !*s) return;
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

/* ---- character select ------------------------------------------------------- */
static void cell_pos(int i, float *x, float *y)   /* i = 0..14 */
{
    *x = 3.f + 49.f * (float)(i % 8);
    *y = i < 8 ? 163.f : 223.f;
}

static void select_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t ins[3] = {0, in1, in2};
    g->screen_tick++;
    if (g->sel_sub < 300) { g->sel_sub += 20; return; }   /* scroll-in */
    for (int p = 1; p <= 2; p++) {
        osc_fighter *f = &g->p[p];
        bool human = p == 1 ? g->p1_in : g->p2_in;
        uint8_t in = ins[p], pressed = (uint8_t)(in & ~f->in_prev);
        f->in_prev = in;
        if (!human) { g->sel_locked[p] = 1; continue; }
        if (g->sel_locked[p]) continue;
        if (pressed & OSC_IN_LEFT) g->sel[p]--;
        if (pressed & OSC_IN_RIGHT) g->sel[p]++;
        if ((pressed & OSC_IN_UP) && g->sel[p] > 8) g->sel[p] -= 8;
        if ((pressed & OSC_IN_DOWN) && g->sel[p] < 9) g->sel[p] += 8;
        if (g->sel[p] < 1) g->sel[p] = 15;
        if (g->sel[p] > 15) g->sel[p] = 1;
        if (pressed & OSC_IN_B1) g->sel_locked[p] = 1;
    }
    if (g->sel_timer > 0) g->sel_timer--;
    if (g->sel_timer == 0) g->sel_locked[1] = g->sel_locked[2] = 1;
    if (g->sel_locked[1] && g->sel_locked[2]) {
        if (!g->p2_in) {
            osc_make_ladder(g, g->sel[1]);
            g->fight_no = 1;
            g->sel[2] = g->ladder[1];
        }
        g->p[1].human = g->p1_in;
        g->p[2].human = g->p2_in;
        g->p[1].char_id = g->sel[1]; g->p[2].char_id = g->sel[2];
        g->screen = g->p2_in ? OSC_SCREEN_FIGHT : OSC_SCREEN_VS;
        g->screen_tick = 0;
        if (g->screen == OSC_SCREEN_FIGHT) osc_fight_start(g, g->sel[1], g->sel[2], 0);
    }
}

static void select_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 20, 20, 60, 255);
    SDL_RenderClear(g->ren);
    float scroll = (float)(g->sel_sub - 300);
    osc_text_at(g, 200, 20, osc_text(OSC_MEMO_TEXTO, 7, g->lang), true);   /* SELECIONE SEU LUTADOR */
    char buf[32];
    snprintf(buf, sizeof buf, "%s %d", osc_text(OSC_MEMO_TEXTO, 8, g->lang), g->sel_timer / 25);
    osc_text_at(g, 200, 36, buf, true);
    for (int i = 0; i < 15; i++) {
        float x, y;
        cell_pos(i, &x, &y);
        x += scroll;
        SDL_FRect d = {x, y, 50, 60};
        if (g->face_small[i + 1]) SDL_RenderTexture(g->ren, g->face_small[i + 1], NULL, &d);
        else fill(g, x, y, x + 50, y + 60, 80, 80, 80);
    }
    for (int p = 1; p <= 2; p++) {
        float x, y;
        cell_pos(g->sel[p] - 1, &x, &y);
        x += scroll;
        bool blink = (g->screen_tick / 4) & 1;
        if (g->sel_locked[p] || blink)
            rect(g, x - 1, y - 1, x + 51, y + 61, p == 1 ? 255 : 80, p == 1 ? 220 : 160, p == 1 ? 60 : 255);
        snprintf(buf, sizeof buf, "%dP", p);
        osc_text_at(g, x + 25, y + 63, buf, true);
        /* big face + name on each side */
        int c = g->sel[p];
        if (g->face_big[c]) {
            SDL_FRect d = {p == 1 ? 10.f : 390.f - (float)g->face_big_w[c], 50, (float)g->face_big_w[c], (float)g->face_big_h[c]};
            SDL_RenderTexture(g->ren, g->face_big[c], NULL, &d);
        }
        osc_text_at(g, p == 1 ? 58.f : 342.f, 50.f + (float)g->face_big_h[c] + 4.f, osc_text(OSC_MEMO_NOMES, c, g->lang), true);
    }
}

/* ---- VS / ladder ---------------------------------------------------------------- */
static void vs_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->screen_tick++;
    if ((in1 | in2) & OSC_IN_B1) g->screen_tick += 5;
    if (g->screen_tick >= 150) {
        osc_fight_start(g, g->sel[1], g->sel[2], 0);
    }
}

static void vs_render(osc_game *g)
{
    for (int i = 0; i < 60; i++) fill(g, 0, (float)i * 5, 400, (float)i * 5 + 5, (Uint8)(20 + i), (Uint8)(10 + i / 2), (Uint8)(60 + i * 2));
    osc_text_at(g, 200, 8, osc_text(OSC_MEMO_TEXTO, 19, g->lang), true);     /* PROXIMA BATALHA */
    for (int slot = 1; slot <= 8; slot++) {
        int c = g->ladder[slot];
        float x = 48.f * (float)slot - 40.f, y = 40;
        SDL_FRect d = {x, y, 50, 60};
        bool beaten = slot < g->fight_no, current = slot == g->fight_no;
        if (c && g->face_small[c] && (beaten || current || (g->screen_tick / 8) % 3 == 0))
            SDL_RenderTexture(g->ren, g->face_small[c], NULL, &d);
        else fill(g, x, y, x + 50, y + 60, 40, 40, 40);
        if (current && (g->screen_tick / 4) & 1) rect(g, x - 1, y - 1, x + 51, y + 61, 255, 255, 0);
    }
    int c1 = g->sel[1], c2 = g->sel[2];
    if (g->face_big[c1]) { SDL_FRect d = {20, 120, (float)g->face_big_w[c1], (float)g->face_big_h[c1]}; SDL_RenderTexture(g->ren, g->face_big[c1], NULL, &d); }
    if (g->face_big[c2]) { SDL_FRect d = {380.f - (float)g->face_big_w[c2], 120, (float)g->face_big_w[c2], (float)g->face_big_h[c2]}; SDL_RenderTexture(g->ren, g->face_big[c2], NULL, &d); }
    osc_text_at(g, 68, 250, osc_text(OSC_MEMO_NOMES, c1, g->lang), true);
    osc_text_at(g, 332, 250, osc_text(OSC_MEMO_NOMES, c2, g->lang), true);
    osc_text_at(g, 200, 180, "VS", true);
    if (g->screen_tick > 60) {
        int st = g->stage_id ? g->stage_id : 1;
        osc_text_at(g, 200, 275, osc_text(OSC_MEMO_CENARIOS, st - 1, g->lang), true);
    }
}

/* ---- continue ------------------------------------------------------------------- */
static void continue_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->screen_tick++;
    if (g->timer_sub > 0) g->timer_sub--;
    if (g->timer_sub == 0 && g->timer_sec > 0) { g->timer_sec--; g->timer_sub = 25; }
    if ((in1 | in2) & OSC_IN_B1) {
        /* continue: back to the select screen, same ladder position */
        g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 300;
        g->sel_locked[1] = g->sel_locked[2] = 0; g->sel_timer = 30 * 25;
        g->p[1].score = 0;
        return;
    }
    if (g->timer_sec == 0 && g->timer_sub == 0 && g->screen_tick > 150) {
        /* game over -> records (state 8), then attract */
        g->screen = OSC_SCREEN_RECORDS; g->screen_tick = 0; g->menu_cam = 0; g->ciclo = 0;
        g->p1_in = g->p2_in = false; g->fight_no = 0;
        g->p[1].score = g->p[2].score = 0;
    }
}

static void continue_render(osc_game *g)
{
    osc_fight_render(g);
    fill(g, 100, 100, 300, 200, 0, 0, 0);
    rect(g, 100, 100, 300, 200, 255, 255, 255);
    char buf[16];
    if (g->timer_sec > 0 || g->timer_sub > 0) {
        osc_text_at(g, 200, 120, osc_text(OSC_MEMO_TEXTO, 23, g->lang), true);   /* CONTINUE ? */
        snprintf(buf, sizeof buf, "%d", g->timer_sec);
        osc_text_at(g, 200, 150, buf, true);
    } else {
        osc_text_at(g, 200, 140, osc_text(OSC_MEMO_TEXTO, 22, g->lang), true);   /* FIM DO JOGO */
    }
}

/* ---- dispatch ---------------------------------------------------------------------- */

/* TForm1.texto (0x48e720): Comic Sans MS at `size` points with a black shadow at
 * (+1,+1). Until a bitmap font is produced we scale SDL's 8 px debug font by
 * size/6 (roughly points -> pixels at 96 dpi). */
void osc_text_ex(osc_game *g, float x, float y, const char *s, char align, int size, Uint8 r, Uint8 gg, Uint8 b)
{
    if (!s || !*s) return;
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
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    for (int i = 0; i < 21; i++)   /* [approx] gradient operands not recovered */
        fill(g, 0, (float)(20 * i), 400, (float)(20 * i + 20), (Uint8)(40 + 4 * i), (Uint8)(30 + 3 * i), (Uint8)(120 + 5 * i));
    for (int i = 1; i <= 10; i++) {
        const osc_record *r = &g->records[i - 1];
        float y = (float)(60 * i - 350 - g->menu_cam);
        bool hl = i == 1 && (g->ciclo & 1) == 0;
        Uint8 cr = 255, cg = hl ? 255 : 255, cb = hl ? 0 : 255;
        char num[16], sc[16];
        snprintf(num, sizeof num, "%d -", i);
        snprintf(sc, sizeof sc, "%d", r->score);
        osc_text_ex(g, 50, y, num, 'l', 12, cr, cg, cb);
        osc_text_ex(g, 100, y, r->name, 'l', 12, cr, cg, cb);
        if (r->char_id >= 1 && r->char_id <= 15 && g->face_small[r->char_id]) blit(g, g->face_small[r->char_id], 50, 60, 160, y - 20);
        osc_text_ex(g, 230, y, sc, 'l', 12, cr, cg, cb);
    }
    if (g->screen_tick > 240) fill(g, 0, 0, 400, 320, 0, 0, 0);
    if (g->ciclo < 18) osc_text_ex(g, 200, 5, osc_text(OSC_MEMO_TEXTO, 20, g->lang), 'c', 12, 255, 255, 255);
    if (g->ciclo < 10) osc_text_ex(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 5, g->lang), 'c', 12, 255, 255, 255);
}

/* ---- coins and start buttons (FormKeyDown '5', '1', '2') ------------------ */
static void join_player(osc_game *g, int p)
{
    g->credits--;
    if (p == 1) g->p1_in = true; else g->p2_in = true;
    g->p[p].score = 0; g->p[p].wins = 0;
    g->sel[p] = p;
    g->sel_locked[p] = 0;
    g->sel_timer = 30 * 25;
}

static void handle_keys(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* convenience for keyboards and pads: the first button on the title inserts a coin and starts */
    if (g->screen == OSC_SCREEN_ATTRACT || g->screen == OSC_SCREEN_RECORDS || g->demo > 0) {
        uint8_t p1 = (uint8_t)(in1 & ~g->p[1].in_prev), p2 = (uint8_t)(in2 & ~g->p[2].in_prev);
        g->p[1].in_prev = in1; g->p[2].in_prev = in2;
        if (p1 & OSC_IN_B1) { if (g->credits == 0) g->key_coin = true; g->key_start1 = true; }
        if (p2 & OSC_IN_B1) { if (g->credits == 0) g->key_coin = true; g->key_start2 = true; }
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
            g->fight_no = 1;
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
            g->sel_locked[1] = g->sel_locked[2] = 0;
            g->ai_p2 = !g->p2_in;
        } else if (g->screen == OSC_SCREEN_SELECT && !g->no_push) {
            join_player(g, p);
            g->ai_p2 = !g->p2_in;
        } else if (g->screen == OSC_SCREEN_CONTINUE && g->timer_sec > 0) {
            join_player(g, p);
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 300;
            g->sel_locked[1] = g->sel_locked[2] = 0;
            g->ai_p2 = !g->p2_in;
        }
    }
}

void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    handle_keys(g, in1, in2);
    switch (g->screen) {
    case OSC_SCREEN_ATTRACT: attract_tick(g); break;
    case OSC_SCREEN_RECORDS: records_tick(g); break;
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
            bool human_lost = (winner == 1 && g->p[2].human) || (winner == 2 && g->p[1].human);
            if (!g->p2_in && winner == 1) {
                /* arcade ladder: next opponent */
                g->fight_no++;
                if (g->fight_no > 8) { g->fight_no = 0; g->screen = OSC_SCREEN_RECORDS; g->screen_tick = 0; g->menu_cam = 0; g->ciclo = 0; g->p1_in = g->p2_in = false; break; }   /* ending (state 10) not ported yet */
                g->sel[2] = g->ladder[g->fight_no];
                g->screen = OSC_SCREEN_VS; g->screen_tick = 0;
                g->stage_id = osc_stage_for(g, g->sel[2]);
            } else if (human_lost || g->p2_in) {
                g->screen = OSC_SCREEN_CONTINUE; g->screen_tick = 0;
                g->timer_sec = 9; g->timer_sub = 25;
            }
        }
        break;
    }
}

void osc_game_render(osc_game *g)
{
    switch (g->screen) {
    case OSC_SCREEN_SELECT: select_render(g); break;
    case OSC_SCREEN_VS: vs_render(g); break;
    case OSC_SCREEN_CONTINUE: continue_render(g); break;
    case OSC_SCREEN_ATTRACT: attract_render(g); break;
    case OSC_SCREEN_RECORDS: records_render(g); break;
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
