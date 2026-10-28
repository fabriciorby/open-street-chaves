/* Screen dispatcher (DXTimer1Timer / FormKeyDown / render): the coin and start keys, the per-screen
 * table, the VELOCIDADE bar and the footer. Each screen lives in its own screen_*.c
 * (07-screens-and-text.md); shared declarations are in screens.h. */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "font.h"
#include "assets.h"
#include "fight.h"
#include "wav.h"
#include "game.h"
#include "canvas.h"
#include "text.h"
#include "screens.h"
#include <math.h>

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

/* Screen dispatch table, indexed by OSC_SCREEN_*. Slots that are NULL (0, 6 and anything out of
 * range) fall through to the fight screen, exactly like the `default:` of the switch this replaces. */
typedef struct {
    void (*tick)(osc_game *g, uint8_t in1, uint8_t in2);
    void (*render)(osc_game *g);
} screen_ops;

static const screen_ops k_screens[] = {
    [OSC_SCREEN_ATTRACT]  = {osc_attract_tick,       osc_attract_render},
    [OSC_SCREEN_MENU_F1]  = {osc_menu_tick,          osc_menu_render},
    [OSC_SCREEN_MENU_F2]  = {osc_menu_tick,          osc_menu_render},
    [OSC_SCREEN_SELECT]   = {osc_select_tick,        osc_select_render},
    [OSC_SCREEN_VS]       = {osc_vs_tick,            osc_vs_render},
    [OSC_SCREEN_FIGHT]    = {osc_fight_screen_tick,  osc_fight_screen_render},
    [OSC_SCREEN_RECORDS]  = {osc_records_tick,       osc_records_render},
    [OSC_SCREEN_CONTINUE] = {osc_continue_tick,      osc_continue_render},
    [OSC_SCREEN_ENDING]   = {osc_ending_tick,        osc_ending_render},
    [OSC_SCREEN_CREDITS]  = {osc_ending_tick,        osc_ending_render},
    [OSC_SCREEN_NAME]     = {osc_name_tick,          osc_name_render},
};

static const screen_ops *screen_of(const osc_game *g)
{
    unsigned s = (unsigned)g->flow.screen;
    if (s < sizeof k_screens / sizeof k_screens[0] && k_screens[s].tick) return &k_screens[s];
    return &k_screens[OSC_SCREEN_FIGHT];
}

static void game_tick_body(osc_game *g, uint8_t in1, uint8_t in2)
{
    handle_keys(g, in1, in2);
    screen_of(g)->tick(g, in1, in2);
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
    screen_of(g)->render(g);
    if (g->flow.screen != OSC_SCREEN_MENU_F1 && g->flow.screen != OSC_SCREEN_MENU_F2) footer(g);
    if (g->flow.speed_overlay_draw) speed_bar(g);
}
