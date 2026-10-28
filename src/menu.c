/* F1 configuration menu (state 2) and F2 key configuration (state 3)
 * (07 "F1 configuration menu"). Drawing, input and value handling follow the
 * x86 code of the timer handler (0x4a0178 state 2, 0x4a1b55 state 3) and
 * FormKeyDown (0x49e2e8); 07b-layout.md "Menus reviewed against the binary"
 * has the call counts. The settings file itself lives in config.c. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "config.h"
#include "exetext.h"
#include "assets.h"
#include "fight.h"
#include "game.h"
#include "wav.h"

/* ---- helpers --------------------------------------------------------------- */
/* the original's texto (0x48e720) with align '-' and size 11; every row is first drawn white and the
 * current choices are drawn again on top in RGB(130,130,255) */
static const osc_rgb k_blue = {130, 130, 255};
static void ltxt(osc_game *g, int x, int y, const char *s, osc_rgb c) { texto(g, x, y, s, '-', 11, c); }
static void cfgtxt(osc_game *g, int x, int y, int line, osc_rgb c) { ltxt(g, x, y, osc_text(OSC_MEMO_CONFIG, line, g->settings.lang), c); }

/* where the F1 menu came back to: leaving it only sets g_state := 1, the attract cycle carries on */
static int s_resume_tick, s_resume_cam, s_resume_ciclo, s_n_music;

/* ---- state 2: F1 menu ------------------------------------------------------ */
void osc_menu_enter(osc_game *g, int which)
{
    if (g->flow.screen == OSC_SCREEN_ATTRACT) { s_resume_tick = g->flow.screen_tick; s_resume_cam = g->flow.menu_cam; s_resume_ciclo = g->fight.ciclo; }
    g->flow.screen = which == 2 ? OSC_SCREEN_MENU_F1 : OSC_SCREEN_MENU_F2;
    g->flow.menu_cursor = 1;
    g->fight.p[1].in_prev = g->fight.p[2].in_prev = 0xff;
    if (which == 2) {
        g->flow.snd_test = g->flow.mus_test = 1;   /* g_sndIndexA / g_sndFlagB := 1 */
        /* 0x4b58a3: FormCreate counts Musicas/Musica-NN.wav */
        s_n_music = 0;
        for (int n = 1; n < 100; n++) {
            char p[1200];
            SDL_PathInfo info;
            snprintf(p, sizeof p, "%s/Musicas/Musica-%02d.wav", g->data_dir, n);
            if (!SDL_GetPathInfo(p, &info)) break;
            s_n_music = n;
        }
    }
}

static void menu_leave(osc_game *g)
{
    if (g->flow.screen == OSC_SCREEN_MENU_F1) {
        osc_audio_stop_music();          /* the test sound and music are stopped, then both indexes := 0 */
        g->flow.snd_test = g->flow.mus_test = 0;
    }
    osc_config_save(g);
    g->flow.screen = OSC_SCREEN_ATTRACT;
    g->flow.screen_tick = s_resume_tick; g->flow.menu_cam = s_resume_cam; g->fight.ciclo = s_resume_ciclo;
}

/* FPU extended arithmetic of the original: x +/- 0.1 (an 80-bit constant) stored back as a double */
static void damage_step(osc_game *g, int d)
{
    g->settings.damage_level = (double)((long double)g->settings.damage_level + (d < 0 ? -0.1L : 0.1L));
}

static void f1_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* edge of each device against last tick's state (Players[].0xac.. bytes); P1 = keyboard, P2 = joystick */
    uint8_t e1 = (uint8_t)(in1 & ~g->fight.p[1].in_prev), e2 = (uint8_t)(in2 & ~g->fight.p[2].in_prev);
    g->fight.p[1].in_prev = in1; g->fight.p[2].in_prev = in2;
    uint8_t c = (uint8_t)g->flow.menu_cursor;   /* a Byte: P1 and P2 both pressing up at 1 gives 255 */
    if (e1 & OSC_IN_UP) c--;
    if (e1 & OSC_IN_DOWN) c++;
    if (e2 & OSC_IN_UP) c--;
    if (e2 & OSC_IN_DOWN) c++;
    if ((e1 | e2) & OSC_IN_LEFT) {
        switch (c) {
        case 1: g->settings.level--; break;
        case 2: g->settings.cfg_time_mode--; break;
        case 3: g->settings.cfg_power--; break;
        case 4: damage_step(g, -1); break;
        case 5: g->settings.cfg_blood = true; break;
        case 6: g->settings.rounds_to_win--; break;
        case 7: g->settings.lang = 0; break;
        case 8: if (g->flow.snd_test > 1) g->flow.snd_test--; break;
        case 9: if (g->flow.mus_test > 1) g->flow.mus_test--; break;
        case 10: g->settings.cfg_flash = true; break;
        case 11: g->settings.show_level = true; break;
        case 12: g->settings.show_credits = true; break;
        }
    }
    if ((e1 | e2) & OSC_IN_RIGHT) {
        switch (c) {
        case 1: g->settings.level++; break;
        case 2: g->settings.cfg_time_mode++; break;
        case 3: g->settings.cfg_power++; break;
        case 4: damage_step(g, 1); break;
        case 5: g->settings.cfg_blood = false; break;
        case 6: g->settings.rounds_to_win++; break;
        case 7: g->settings.lang = 1; break;
        case 8: if (g->flow.snd_test < g->assets.n_sounds) g->flow.snd_test++; break;
        case 9: if (g->flow.mus_test < s_n_music) g->flow.mus_test++; break;
        case 10: g->settings.cfg_flash = false; break;
        case 11: g->settings.show_level = false; break;
        case 12: g->settings.show_credits = false; break;
        }
    }
    if ((e1 | e2) & OSC_IN_B1) {   /* only the first button plays */
        if (c == 8 && g->assets.n_sounds > g->flow.snd_test - 1) osc_play_snd(g, g->flow.snd_test - 1);
        if (c == 9) osc_play_music(g, g->flow.mus_test);
    }
    if ((e1 | e2) & OSC_IN_B2) osc_audio_stop_music();   /* the second button stops the music, on any row */
    /* the clamps run after the input; the cursor wraps */
    if (c < 1) c = 12;
    if (c > 12) c = 1;
    g->flow.menu_cursor = c;
    g->settings.cfg_time_mode = clampi(g->settings.cfg_time_mode, 1, 4);
    g->settings.level = clampi(g->settings.level, 1, 8);
    g->settings.cfg_power = clampi(g->settings.cfg_power, 1, 4);
    if (0.3L > (long double)g->settings.damage_level) g->settings.damage_level = 0.3;
    if (g->settings.damage_level > 2.0) g->settings.damage_level = 2.0;
    g->settings.rounds_to_win = clampi(g->settings.rounds_to_win, 1, 3);
    if (g->flow.snd_test < 1) g->flow.snd_test = 1;
    if (g->flow.snd_test > g->assets.n_sounds) g->flow.snd_test = g->assets.n_sounds;
}

static void f1_render(osc_game *g)
{
    char buf[48];
    int c = g->flow.menu_cursor;
    const osc_rgb w = k_white;
    barra(g, 0, 0, 400, 300, k_black);
    texto(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 1, g->settings.lang), 'c', 15, w);
    /* 1 Nível do Jogo */
    cfgtxt(g, 10, 60, 2, w);
    for (int i = 1; i <= 8; i++) { snprintf(buf, sizeof buf, "%d", i); ltxt(g, 140 + 10 * (i - 1), 60, buf, w); }
    /* 2 Tempo do Round */
    cfgtxt(g, 10, 75, 3, w);
    ltxt(g, 140, 75, "30", w); ltxt(g, 190, 75, "60", w); ltxt(g, 240, 75, "90", w); ltxt(g, 290, 75, "--", w);
    /* 3 Poder */
    cfgtxt(g, 10, 90, 6, w);
    cfgtxt(g, 140, 90, 7, w); cfgtxt(g, 215, 90, 8, w); cfgtxt(g, 280, 90, 9, w); cfgtxt(g, 345, 90, 10, w);
    /* 4 Nível de Dano: 78 px per 1.0, a 2 px red band on top, and the percentage (Round: half to even) */
    cfgtxt(g, 10, 105, 11, w);
    barraredonda(g, 140, 110, 300, 121, 5, w, k_black);
    int bw = (int)lrintl(78.0L * g->settings.damage_level);
    barra(g, 142, 112, 142 + bw, 119, rgb(255, 0, 0));
    barra(g, 142, 113, 142 + bw, 114, rgb(255, 100, 100));
    snprintf(buf, sizeof buf, "%lld%%", (long long)llrintl(g->settings.damage_level * 100.0L));
    ltxt(g, 310, 105, buf, w);
    /* 5 Sangue, 6 Rounds, 7 Linguagem */
    cfgtxt(g, 10, 120, 12, w); cfgtxt(g, 140, 120, 4, w); cfgtxt(g, 230, 120, 5, w);
    cfgtxt(g, 10, 135, 13, w);
    for (int i = 1; i <= 3; i++) { snprintf(buf, sizeof buf, "%d", i); ltxt(g, 140 + 30 * (i - 1), 135, buf, w); }
    cfgtxt(g, 10, 150, 14, w); cfgtxt(g, 140, 150, 15, w); cfgtxt(g, 230, 150, 16, w);
    /* 8, 9 tests: "n / max" */
    cfgtxt(g, 10, 165, 17, w);
    snprintf(buf, sizeof buf, "%d / %d", g->flow.snd_test, g->assets.n_sounds); ltxt(g, 140, 165, buf, w);
    cfgtxt(g, 10, 180, 18, w);
    snprintf(buf, sizeof buf, "%d / %d", g->flow.mus_test, s_n_music); ltxt(g, 140, 180, buf, w);
    /* 10..12 Flash, Mostrar Nível, Mostrar Creditos */
    for (int i = 0; i < 3; i++) {
        cfgtxt(g, 10, 195 + 15 * i, 19 + i, w);
        cfgtxt(g, 140, 195 + 15 * i, 4, w); cfgtxt(g, 230, 195 + 15 * i, 5, w);
    }
    texto(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 22, g->settings.lang), 'c', 11, w);   /* F1-SAIR */
    /* the cursor row's label and every current choice again in blue */
    static const int label_line[13] = {0, 2, 3, 6, 11, 12, 13, 14, 17, 18, 19, 20, 21};
    if (c >= 1 && c <= 12) cfgtxt(g, 10, 45 + 15 * c, label_line[c], k_blue);
    snprintf(buf, sizeof buf, "%d", g->settings.level); ltxt(g, 140 + 10 * (g->settings.level - 1), 60, buf, k_blue);
    static const char *times[5] = {"", "30", "60", "90", "--"};
    if (g->settings.cfg_time_mode >= 1 && g->settings.cfg_time_mode <= 4) ltxt(g, 140 + 50 * (g->settings.cfg_time_mode - 1), 75, times[g->settings.cfg_time_mode], k_blue);
    static const int power_x[5] = {0, 140, 215, 280, 345};
    if (g->settings.cfg_power >= 1 && g->settings.cfg_power <= 4) cfgtxt(g, power_x[g->settings.cfg_power], 90, 6 + g->settings.cfg_power, k_blue);
    cfgtxt(g, g->settings.cfg_blood ? 140 : 230, 120, g->settings.cfg_blood ? 4 : 5, k_blue);
    snprintf(buf, sizeof buf, "%d", g->settings.rounds_to_win); ltxt(g, 140 + 30 * (g->settings.rounds_to_win - 1), 135, buf, k_blue);
    cfgtxt(g, g->settings.lang == 0 ? 140 : 230, 150, g->settings.lang == 0 ? 15 : 16, k_blue);
    const bool flag[3] = {g->settings.cfg_flash, g->settings.show_level, g->settings.show_credits};
    for (int i = 0; i < 3; i++) cfgtxt(g, flag[i] ? 140 : 230, 195 + 15 * i, flag[i] ? 4 : 5, k_blue);
}

/* ---- state 3: F2 key configuration (P1 keyboard) --------------------------- */
/* FormKeyDown (0x49e789): while in state 3 a letter, digit, arrow or numpad key (VK 41..5A, 30..39, 25..28,
 * 60..69) is stored in DXInput's assign slot of the current row and the cursor moves on, with no wrap (the
 * timer clamps it to 9, after which keys do nothing). Auto-repeat counts: WM_KEYDOWN repeats. */
static bool f2_key_ok(SDL_Scancode k)
{
    return (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_0) || (k >= SDL_SCANCODE_RIGHT && k <= SDL_SCANCODE_UP) ||
           (k >= SDL_SCANCODE_KP_1 && k <= SDL_SCANCODE_KP_0);
}

void osc_menu_key(osc_game *g, SDL_Scancode k)
{
    if (g->flow.screen != OSC_SCREEN_MENU_F2 || !f2_key_ok(k)) return;
    uint8_t c = (uint8_t)g->flow.menu_cursor;
    if (c >= 1 && c <= 8) g->settings.keymap[0][c - 1] = k;
    g->flow.menu_cursor = (uint8_t)(c + 1);
}

/* the character Delphi's Chr(VK) gives for the key; the numpad's VK 60..69 are ` and a..i */
static char vk_char(SDL_Scancode k)
{
    if (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_Z) return (char)('A' + (k - SDL_SCANCODE_A));
    if (k >= SDL_SCANCODE_1 && k <= SDL_SCANCODE_9) return (char)('1' + (k - SDL_SCANCODE_1));
    if (k == SDL_SCANCODE_0) return '0';
    if (k >= SDL_SCANCODE_KP_1 && k <= SDL_SCANCODE_KP_9) return (char)('a' + (k - SDL_SCANCODE_KP_1));
    if (k == SDL_SCANCODE_KP_0) return '`';
    return 0;
}

static void f2_tick(osc_game *g)
{
    if (g->flow.menu_cursor < 1) g->flow.menu_cursor = 1;
    if (g->flow.menu_cursor > 9) g->flow.menu_cursor = 9;
}

static void f2_render(osc_game *g)
{
    const osc_rgb w = k_white;
    barra(g, 0, 0, 400, 300, k_black);
    texto(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 23, g->settings.lang), 'c', 15, w);
    for (int i = 1; i <= 8; i++) cfgtxt(g, 50, 60 + 20 * i, 23 + i, w);
    for (int i = 1; i <= 8; i++) {
        SDL_Scancode k = g->settings.keymap[0][i - 1];
        int icon = k == SDL_SCANCODE_UP ? 0 : k == SDL_SCANCODE_DOWN ? 1 : k == SDL_SCANCODE_RIGHT ? 2 : k == SDL_SCANCODE_LEFT ? 3 : -1;
        if (icon >= 0) {
            if (icon < g->assets.n_icone && g->assets.icone[icon]) draw_tex(g, g->assets.icone[icon], 200, (float)(85 + 20 * (i - 1)), 255, false);
        } else {
            char s[2] = {vk_char(k), 0};
            ltxt(g, 200, 60 + 20 * i, s[0] ? s : SDL_GetScancodeName(k), w);
        }
    }
    if (g->flow.menu_cursor >= 1 && g->flow.menu_cursor <= 8) cfgtxt(g, 50, 60 + 20 * g->flow.menu_cursor, 23 + g->flow.menu_cursor, k_blue);
    texto(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 32, g->settings.lang), 'c', 11, w);   /* F2-SAIR */
}

void osc_menu_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* F1 leaves state 2 only and F2 leaves state 3 only; the other key does nothing there */
    bool leave = g->flow.screen == OSC_SCREEN_MENU_F1 ? g->flow.key_f1 : g->flow.key_f2;
    g->flow.key_f1 = g->flow.key_f2 = false;
    if (leave) { menu_leave(g); return; }
    if (g->flow.screen == OSC_SCREEN_MENU_F1) f1_tick(g, in1, in2); else f2_tick(g);
}

void osc_menu_render(osc_game *g)
{
    if (g->flow.screen == OSC_SCREEN_MENU_F1) f1_render(g); else f2_render(g);
}
