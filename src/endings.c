/* State 10 (ending) and 11 (credits), driven by docs/spec/07e-endings.txt
 * through osc_endings[] (07b-layout.md "Endings"). */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "assets.h"
#include "effects.h"
#include "fight_draw.h"
#include "fight.h"
#include "game.h"
#include "canvas.h"
#include "tables_gen.h"

static const int k_base[16] = {0, 0, 44, 88, 125, 166, 210, 254, 298, 342, 386, 430, 474, 518, 562, 606};

/* The picture of the tick last run: the original draws, spawns and rolls the flash colours inside
 * DXTimer1Timer, so the work is done in the tick and render only replays it (render can run
 * more or less often than the tick). */
static int s_c;               /* counter of the last tick, as the original drew it */
static int s_flash_g[OSC_N_ENDINGS], s_flash_b[OSC_N_ENDINGS];

void osc_ending_start(osc_game *g, int ch)
{
    g->flow.ending_char = ch < 1 || ch > 15 ? 1 : ch;
    g->flow.screen = OSC_SCREEN_ENDING;
    g->flow.screen_tick = 0; s_c = 0;
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
        if (r->ch != g->flow.ending_char || r->from != r->to || r->from > tick) continue;
        if (r->op == OSC_END_STAGE) osc_load_stage(g, r->a);
        else if (r->op == OSC_END_CHAR) osc_load_fighter(g, r->a, r->b);
    }
    g->flow.screen_tick = tick; s_c = tick;
}

static bool row_active(const osc_ending_row *r, int c)
{
    return c >= r->from && c <= r->to;
}

static void spawn_row(osc_game *g, int type)
{
    int x, y;
    if (type == 1) { x = osc_random(g, 150) + 130; y = osc_random(g, 100) + 100; }
    else if (type == 25) { x = osc_random(g, 60) + 250; y = osc_random(g, 20) + 110; }
    else { x = osc_random(g, 100) + 150; y = osc_random(g, 30) + 60; }
    osc_spawn_fx(g, type, x, y);
}

void osc_ending_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    if (g->flow.screen == OSC_SCREEN_CREDITS) {
        /* state 11 draws, then counts: 201 frames, then state 9 with both countdown bytes at 0 ("FIM DO JOGO") */
        g->flow.screen_tick++;
        if (g->flow.screen_tick > 200) {
            g->flow.screen = OSC_SCREEN_CONTINUE; g->flow.screen_tick = 0;
            g->timer_sec = 0; g->timer_sub = 0;
        }
        return;
    }
    int c = g->flow.screen_tick;
    g->p[1].in_prev = in1; g->p[2].in_prev = in2;
    /* loads, spawns and flash colours of this tick, in the order of the code */
    for (int i = 0; i < OSC_N_ENDINGS; i++) {
        const osc_ending_row *r = &osc_endings[i];
        if (r->ch != g->flow.ending_char || !row_active(r, c)) continue;
        if (r->from == r->to && r->op == OSC_END_STAGE) osc_load_stage(g, r->a);
        else if (r->from == r->to && r->op == OSC_END_CHAR) osc_load_fighter(g, r->a, r->b);
        else if (r->op == OSC_END_SPAWN && !(r->chance && osc_random(g, r->chance) != 0)) spawn_row(g, r->a);
        else if (r->op == OSC_END_FLASH) { s_flash_g[i] = osc_random(g, 255); s_flash_b[i] = osc_random(g, 255); }
    }
    s_c = c;
    osc_fx_tick(g);
    g->flow.screen_tick++;
    if (g->flow.screen_tick > 2200) {
        g->flow.screen = OSC_SCREEN_CREDITS; g->flow.screen_tick = 0; g->ciclo = 0;
        return;
    }
    /* a held button 1 of a human player skips to the end of the 100-tick block (level, not edge) */
    if (((in1 & OSC_IN_B1) && g->p[1].human) || ((in2 & OSC_IN_B1) && g->p[2].human)) {
        for (int i = 0; i < 21; i++)
            if (g->flow.screen_tick > i * 100 + 5 && g->flow.screen_tick < i * 100 + 85) { g->flow.screen_tick = i * 100 + 85; break; }
    }
}

static void credits_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    osc_text_ex(g, 200, 50, osc_text(OSC_MEMO_TEXTO, 24, g->settings.lang), 'c', 15, 255, 255, 255);   /* OBRIGADO POR JOGAR */
    int li = g->settings.lang == 1 ? 13 : 12;
    if (li < g->assets.n_abertura && g->assets.abertura[li]) {
        SDL_FRect d = {89, 80, (float)g->assets.abertura_w[li], (float)g->assets.abertura_h[li]};
        SDL_RenderTexture(g->ren, g->assets.abertura[li], NULL, &d);
    }
    osc_text_ex(g, 200, 240, osc_text(OSC_MEMO_TEXTO, 25, g->settings.lang), 'c', 10, 255, 255, 255);
    osc_text_ex(g, 200, 260, osc_text(OSC_MEMO_TEXTO, 30, g->settings.lang), 'c', 10, 255, 255, 255);
}

void osc_ending_render(osc_game *g)
{
    if (g->flow.screen == OSC_SCREEN_CREDITS) { credits_render(g); return; }
    int c = s_c;
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->assets.stage) {
        SDL_FRect d = {-80, -100, (float)g->assets.stage_w, (float)g->assets.stage_h};
        SDL_RenderTexture(g->ren, g->assets.stage, NULL, &d);
    }
    for (int i = 0; i < OSC_N_ENDINGS; i++) {
        const osc_ending_row *r = &osc_endings[i];
        if (r->ch != g->flow.ending_char || !row_active(r, c)) continue;
        switch (r->op) {
        case OSC_END_FIGHTER: osc_draw_fighter_at(g, r->a, r->b, r->c, r->d, r->e); break;
        case OSC_END_SHADOW:
            if (g->assets.n_fx_tex > 0 && g->assets.fx_tex[0]) {
                SDL_FRect d = {(float)r->a, (float)r->b, (float)(r->c - r->a), (float)(r->d - r->b)};   /* l t r b */
                SDL_SetTextureAlphaMod(g->assets.fx_tex[0], 70);
                SDL_RenderTexture(g->ren, g->assets.fx_tex[0], NULL, &d);
                SDL_SetTextureAlphaMod(g->assets.fx_tex[0], 255);
            }
            break;
        case OSC_END_FINAL:
            if (r->a >= 0 && r->a < g->assets.n_final && g->assets.final_tex[r->a]) {
                SDL_FRect d = {(float)r->b, (float)r->c, (float)g->assets.final_w[r->a], (float)g->assets.final_h[r->a]};
                SDL_RenderTexture(g->ren, g->assets.final_tex[r->a], NULL, &d);
            }
            break;
        case OSC_END_BLACK: barra(g, 0, 0, 400, 300, k_black); break;
        case OSC_END_FLASH: barra(g, 50, 50, 350, 220, rgb(255, s_flash_g[i], s_flash_b[i])); break;
        case OSC_END_SPAWN: break;   /* spawned by the tick */
        default: break;
        }
    }
    osc_fx_render(g);
    /* letterbox */
    barra(g, 0, 0, 400, 50, k_black);
    barra(g, 0, 220, 400, 300, k_black);
    barra(g, 0, 50, 50, 220, k_black);
    barra(g, 350, 50, 400, 220, k_black);
    /* dialogue: segment k of 200 ticks, speaker tag + 3 lines */
    if (c >= 2200) return;   /* the dialogue is drawn for c < 2200 */
    int k = c / 200;
    int base = k_base[g->flow.ending_char] + 4 * k;
    const char *tag = osc_text(OSC_MEMO_DIALOGO, base, g->settings.lang);
    int speaker = (tag && tag[0] >= 'A' && tag[0] <= 'O') ? tag[0] - 'A' + 1 : 0;
    if (speaker) {
        if (g->assets.face_small[speaker]) {
            SDL_FRect d = {20, 222, 50, 60};
            SDL_RenderTexture(g->ren, g->assets.face_small[speaker], NULL, &d);
        }
        for (int l = 0; l < 3; l++)
            osc_text_ex(g, 80, (float)(228 + 17 * l), osc_text(OSC_MEMO_DIALOGO, base + 1 + l, g->settings.lang), 'l', 12, 255, 255, 255);
    }
}
