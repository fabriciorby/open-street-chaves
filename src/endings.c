/* State 10 (ending) and 11 (credits), driven by docs/spec/07e-endings.txt
 * through osc_endings[] (07b-layout.md "Endings"). */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "game.h"
#include "tables_gen.h"

static const int k_base[16] = {0, 0, 44, 88, 125, 166, 210, 254, 298, 342, 386, 430, 474, 518, 562, 606};

static void fillr(osc_game *g, float x1, float y1, float x2, float y2, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_FRect d = {x1, y1, x2 - x1, y2 - y1};
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderFillRect(g->ren, &d);
}

void osc_ending_start(osc_game *g, int ch)
{
    g->ending_char = ch < 1 || ch > 15 ? 1 : ch;
    g->screen = OSC_SCREEN_ENDING;
    g->screen_tick = 0;
    g->cam_x = 0; g->cam_y = 0;
    memset(g->fx, 0, sizeof g->fx);
    g->p[1].in_prev = g->p[2].in_prev = 0xff;
    g->p[1].facing = 1; g->p[2].facing = 2;
}

/* debug/preview: jump to a tick, replaying the loads before it */
void osc_ending_seek(osc_game *g, int tick)
{
    for (int i = 0; i < OSC_N_ENDINGS; i++) {
        const osc_ending_row *r = &osc_endings[i];
        if (r->ch != g->ending_char || r->from != r->to || r->from > tick) continue;
        if (r->op == OSC_END_STAGE) osc_load_stage(g, r->a);
        else if (r->op == OSC_END_CHAR) osc_load_fighter(g, r->a, r->b);
    }
    g->screen_tick = tick;
}

static bool row_active(const osc_ending_row *r, int c)
{
    if (r->alt) return c <= r->to || c >= r->from;
    return c >= r->from && c <= r->to;
}

void osc_ending_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    if (g->screen == OSC_SCREEN_CREDITS) {
        g->screen_tick++;
        if (g->screen_tick > 200) {
            g->screen = OSC_SCREEN_CONTINUE; g->screen_tick = 0;
            g->timer_sec = 9; g->timer_sub = 25;
        }
        return;
    }
    int c = g->screen_tick;
    uint8_t pressed = (uint8_t)((in1 & ~g->p[1].in_prev) | (in2 & ~g->p[2].in_prev));
    g->p[1].in_prev = in1; g->p[2].in_prev = in2;
    /* loads at exact ticks */
    for (int i = 0; i < OSC_N_ENDINGS; i++) {
        const osc_ending_row *r = &osc_endings[i];
        if (r->ch != g->ending_char || r->from != r->to || r->from != c) continue;
        if (r->op == OSC_END_STAGE) osc_load_stage(g, r->a);
        else if (r->op == OSC_END_CHAR) osc_load_fighter(g, r->a, r->b);
    }
    osc_fx_tick(g);
    g->screen_tick++;
    if (g->screen_tick > 2200) {
        g->screen = OSC_SCREEN_CREDITS; g->screen_tick = 0; g->ciclo = 0;
        return;
    }
    if (pressed & (OSC_IN_B1 | OSC_IN_B2 | OSC_IN_B3 | OSC_IN_B4)) {
        for (int i = 0; i < 21; i++)
            if (g->screen_tick > i * 100 + 5 && g->screen_tick < i * 100 + 85) { g->screen_tick = i * 100 + 85; break; }
    }
}

static void credits_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    osc_text_ex(g, 200, 50, osc_text(OSC_MEMO_TEXTO, 24, g->lang), 'c', 15, 255, 255, 255);   /* OBRIGADO POR JOGAR */
    int li = g->lang == 1 ? 13 : 12;
    if (li < g->n_abertura && g->abertura[li]) {
        SDL_FRect d = {89, 80, (float)g->abertura_w[li], (float)g->abertura_h[li]};
        SDL_RenderTexture(g->ren, g->abertura[li], NULL, &d);
    }
    osc_text_ex(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 25, g->lang), 'c', 10, 255, 255, 255);
    osc_text_ex(g, 200, 260, osc_text(OSC_MEMO_TEXTO, 30, g->lang), 'c', 10, 255, 255, 255);
}

void osc_ending_render(osc_game *g)
{
    if (g->screen == OSC_SCREEN_CREDITS) { credits_render(g); return; }
    int c = g->screen_tick;
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->stage) {
        SDL_FRect d = {-80, -100, (float)g->stage_w, (float)g->stage_h};
        SDL_RenderTexture(g->ren, g->stage, NULL, &d);
    }
    for (int i = 0; i < OSC_N_ENDINGS; i++) {
        const osc_ending_row *r = &osc_endings[i];
        if (r->ch != g->ending_char || !row_active(r, c)) continue;
        if (r->chance && osc_random(g, r->chance) != 0) continue;
        switch (r->op) {
        case OSC_END_FIGHTER: osc_draw_fighter_at(g, r->a, r->b, r->c, r->d, r->e); break;
        case OSC_END_SHADOW:
            if (g->n_fx_tex > 0 && g->fx_tex[0]) {
                SDL_FRect d = {(float)r->a, (float)r->b, (float)(r->c - r->a), (float)(r->d - r->b)};
                SDL_SetTextureAlphaMod(g->fx_tex[0], 70);
                SDL_RenderTexture(g->ren, g->fx_tex[0], NULL, &d);
                SDL_SetTextureAlphaMod(g->fx_tex[0], 255);
            }
            break;
        case OSC_END_FINAL:
            if (r->a >= 0 && r->a < g->n_final && g->final_tex[r->a]) {
                SDL_FRect d = {(float)r->b, (float)r->c, (float)g->final_w[r->a], (float)g->final_h[r->a]};
                SDL_RenderTexture(g->ren, g->final_tex[r->a], NULL, &d);
            }
            break;
        case OSC_END_BLACK: fillr(g, 0, 0, 400, 300, 0, 0, 0); break;
        case OSC_END_FLASH: fillr(g, 50, 50, 350, 220, 255, (Uint8)osc_random(g, 255), (Uint8)osc_random(g, 255)); break;
        case OSC_END_SPAWN:
            if (r->a == 1) osc_spawn_fx(g, 1, osc_random(g, 150) + 130, osc_random(g, 100) + 100);
            else osc_spawn_fx(g, r->a, osc_random(g, 100) + 150, osc_random(g, 30) + 60);
            break;
        default: break;
        }
    }
    osc_fx_render(g);
    /* letterbox */
    fillr(g, 0, 0, 400, 50, 0, 0, 0);
    fillr(g, 0, 220, 400, 300, 0, 0, 0);
    fillr(g, 0, 50, 50, 220, 0, 0, 0);
    fillr(g, 350, 50, 400, 220, 0, 0, 0);
    /* dialogue: segment k of 200 ticks, speaker tag + 3 lines */
    int k = c / 200;
    if (k > 10) k = 10;
    int base = k_base[g->ending_char] + 4 * k;
    const char *tag = osc_text(OSC_MEMO_DIALOGO, base, g->lang);
    int speaker = (tag && tag[0] >= 'A' && tag[0] <= 'O') ? tag[0] - 'A' + 1 : 0;
    if (speaker) {
        if (g->face_small[speaker]) {
            SDL_FRect d = {20, 222, 50, 60};
            SDL_RenderTexture(g->ren, g->face_small[speaker], NULL, &d);
        }
        for (int l = 0; l < 3; l++)
            osc_text_ex(g, 80, (float)(228 + 17 * l), osc_text(OSC_MEMO_DIALOGO, base + 1 + l, g->lang), 'l', 12, 255, 255, 255);
    }
}
