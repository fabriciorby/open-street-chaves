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
#include "screens.h"
#include <math.h>

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
    case OSC_SCREEN_ATTRACT: osc_attract_tick(g, in1, in2); break;
    case OSC_SCREEN_RECORDS: osc_records_tick(g, in1, in2); break;
    case OSC_SCREEN_NAME: name_tick(g, in1, in2); break;
    case OSC_SCREEN_MENU_F1: case OSC_SCREEN_MENU_F2: osc_menu_tick(g, in1, in2); break;
    case OSC_SCREEN_ENDING: case OSC_SCREEN_CREDITS: osc_ending_tick(g, in1, in2); break;
    case OSC_SCREEN_SELECT: osc_select_tick(g, in1, in2); break;
    case OSC_SCREEN_VS: osc_vs_tick(g, in1, in2); break;
    case OSC_SCREEN_CONTINUE: osc_continue_tick(g, in1, in2); break;
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
    case OSC_SCREEN_SELECT: osc_select_render(g); break;
    case OSC_SCREEN_VS: osc_vs_render(g); break;
    case OSC_SCREEN_CONTINUE: osc_continue_render(g); break;
    case OSC_SCREEN_ATTRACT: osc_attract_render(g); break;
    case OSC_SCREEN_RECORDS: osc_records_render(g); break;
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
