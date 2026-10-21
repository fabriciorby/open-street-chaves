/* Screens outside the fight: character select (state 4), VS (state 5),
 * continue (state 9), and the dispatch between them (07-screens-and-text.md).
 * Text is drawn with SDL's debug font until the original font is handled. */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "game.h"

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
        osc_text_at(g, p == 1 ? 58.f : 342.f, 150, osc_text(OSC_MEMO_NOMES, c, g->lang), true);
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
        /* game over -> fresh select */
        g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
        g->sel_locked[1] = g->sel_locked[2] = 0; g->sel_timer = 30 * 25; g->fight_no = 0;
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
void osc_game_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    switch (g->screen) {
    case OSC_SCREEN_SELECT: select_tick(g, in1, in2); break;
    case OSC_SCREEN_VS: vs_tick(g, in1, in2); break;
    case OSC_SCREEN_CONTINUE: continue_tick(g, in1, in2); break;
    default:
        osc_fight_tick(g, in1, in2);
        if (g->match_over) {
            int winner = g->match_over;
            g->match_over = 0;
            bool human_lost = (winner == 1 && g->p[2].human) || (winner == 2 && g->p[1].human);
            if (!g->p2_in && winner == 1) {
                /* arcade ladder: next opponent */
                g->fight_no++;
                if (g->fight_no > 8) { g->fight_no = 0; g->screen = OSC_SCREEN_SELECT; g->sel_sub = 0; g->sel_locked[1] = g->sel_locked[2] = 0; g->sel_timer = 750; break; }
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
    default: osc_fight_render(g); break;
    }
}
